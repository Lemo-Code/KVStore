// zero FdManager implementation — file descriptor context tracking
//
// Tracks the state of every file descriptor (up to 65536=65536) in the
// process. Each fd has an associated FdContext recording:
//   - Socket vs regular file classification (affects hook behavior)
//   - Non-blocking status (distinguishing user-set vs hook-set)
//   - Per-direction socket timeouts (SO_RCVTIMEO / SO_SNDTIMEO equivalents)
//   - I/O statistics (bytes read/written, last I/O timestamp)
//
// Concurrency: the fd space is sharded into 64 buckets (1024 fds each)
// with one SpinLock per bucket. This provides fine-grained locking:
// operations on fds in different buckets proceed in parallel without
// contention. Within a bucket, the lock is held only briefly during
// context creation/lookup.
//
// The hook system consults FdManager to decide whether to apply
// fiber-aware non-blocking I/O to a given fd.

#include "zero/scheduler/fd_manager.h"
#include "zero/thread/spinlock.h"
#include "zero/base/macro.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <string.h>
#include <atomic>

namespace zero {

// ============================================================
// Configuration
// ============================================================

// Number of lock shards. Must be a power of 2 for efficient modulo.
static constexpr int kNumShards = 64;

// FDs per shard.
static constexpr int kFdsPerShard = 65536 / kNumShards;  // 1024

static_assert(65536 % kNumShards == 0,
              "65536 must be divisible by kNumShards");
static_assert((kNumShards & (kNumShards - 1)) == 0,
              "kNumShards must be a power of two");

// ============================================================
// Extended per-FD state (private to this translation unit)
// ============================================================

// Additional per-fd fields beyond those in the public FdContext.
// Stored in parallel arrays indexed by fd for cache-friendly access.
struct FdExtraState {
    uint32_t event_flags = 0;    // Currently registered epoll events
    uint64_t last_io_time = 0;  // Monotonic timestamp of last I/O op
    size_t   bytes_read = 0;     // Cumulative bytes received
    size_t   bytes_written = 0;  // Cumulative bytes sent
    bool     marked_closed = false; // True if close() has been called
};

// Parallel arrays indexed by fd. Each array element corresponds to
// the fd at that index. Null FdContext* means "not tracked."
static FdExtraState s_extra[65536];

// ============================================================
// Sharded locks for thread-safe fd access
// ============================================================

// One SpinLock per shard. Operations on fd N lock shard N / kFdsPerShard.
static SpinLock s_locks[kNumShards];

// Get the shard index for a given fd.
static inline int shard_index(int fd) {
    return fd / kFdsPerShard;
}

// RAII helper: locks the shard for a given fd.
class ShardLock {
public:
    explicit ShardLock(int fd) : lock_(s_locks[shard_index(fd)]) {
        lock_.lock();
    }
    ~ShardLock() {
        lock_.unlock();
    }
    ShardLock(const ShardLock&) = delete;
    ShardLock& operator=(const ShardLock&) = delete;
private:
    SpinLock& lock_;
};

// ============================================================
// Statistics
// ============================================================

static std::atomic<size_t> s_total_fds{0};
static std::atomic<size_t> s_peak_fds{0};
static std::atomic<uint64_t> s_total_allocations{0};
static std::atomic<uint64_t> s_total_deallocations{0};

// ============================================================
// Singleton accessor
// ============================================================

FdManager& FdManager::instance() {
    // Meyer's singleton — guaranteed thread-safe by C++11 [stmt.dcl].
    static FdManager mgr;
    return mgr;
}

// ============================================================
// Constructor
// ============================================================

FdManager::FdManager() {
    // Zero-initialize the entire contexts_ array.
    memset(contexts_, 0, sizeof(contexts_));

    // Pre-populate the standard I/O descriptors (0, 1, 2).
    // These are always open at process start and should be tracked
    // to prevent the hook system from treating them as sockets.
    for (int fd = 0; fd <= 2; ++fd) {
        auto* ctx = new FdContext();
        ctx->fd            = fd;
        ctx->is_socket     = false;
        ctx->sys_nonblock  = false;
        ctx->user_nonblock = false;
        ctx->recv_timeout_ms = -1;
        ctx->send_timeout_ms = -1;

        ShardLock guard(fd);
        contexts_[fd] = ctx;
    }

    s_total_fds.store(3, std::memory_order_relaxed);
    s_peak_fds.store(3, std::memory_order_relaxed);
}

// ============================================================
// Destructor
// ============================================================

FdManager::~FdManager() {
    // Delete all registered contexts. We do not lock each shard
    // individually because at destruction time no other threads
    // should be accessing the fd manager.
    for (int fd = 0; fd < 65536; ++fd) {
        delete contexts_[fd];
        contexts_[fd] = nullptr;
    }
}

// ============================================================
// get() — retrieve or create an FdContext for the given fd
// ============================================================

FdManager::FdContext* FdManager::get(int fd, bool auto_create) {
    // Validate the fd is within the trackable range.
    if (fd < 0 || fd >= 65536) {
        return nullptr;
    }

    // Fast path: context already exists. Since pointer reads are atomic
    // on all supported platforms, we can check without locking.
    if (contexts_[fd] != nullptr) {
        return contexts_[fd];
    }

    if (!auto_create) {
        return nullptr;
    }

    // Slow path: create a new context under the shard lock.
    ShardLock guard(fd);

    // Double-check after acquiring the lock — another thread may have
    // created the context between our fast-path check and lock acquisition.
    if (contexts_[fd] != nullptr) {
        return contexts_[fd];
    }

    // Allocate and initialize a new context.
    auto* ctx = new FdContext();
    ctx->fd               = fd;
    ctx->is_socket        = false;
    ctx->sys_nonblock     = false;
    ctx->user_nonblock    = false;
    ctx->recv_timeout_ms  = -1;
    ctx->send_timeout_ms  = -1;

    // Auto-detect whether this fd is a socket. getsockopt(SO_TYPE)
    // succeeds for sockets and fails for regular files, pipes, etc.
    int sock_type = 0;
    socklen_t optlen = sizeof(sock_type);
    if (::getsockopt(fd, SOL_SOCKET, SO_TYPE,
                     &sock_type, &optlen) == 0) {
        ctx->is_socket = true;
    }

    // Check the current non-blocking flag via fcntl.
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0 && (flags & O_NONBLOCK)) {
        // The fd is already non-blocking. We record it as user-set
        // because we didn't set it via the hook system.
        ctx->user_nonblock = true;
    }

    // Initialize the extended state.
    s_extra[fd] = FdExtraState{};

    contexts_[fd] = ctx;

    // Update statistics.
    size_t prev = s_total_fds.fetch_add(1, std::memory_order_relaxed) + 1;
    s_total_allocations.fetch_add(1, std::memory_order_relaxed);

    // Track peak concurrently open fds.
    size_t peak = s_peak_fds.load(std::memory_order_relaxed);
    while (prev > peak &&
           !s_peak_fds.compare_exchange_weak(peak, prev,
                                              std::memory_order_relaxed)) {
        // Retry until we successfully update the peak.
    }

    return ctx;
}

// ============================================================
// remove() — delete the context for a closed fd
// ============================================================

void FdManager::remove(int fd) {
    if (fd < 0 || fd >= 65536) {
        return;
    }

    ShardLock guard(fd);

    if (contexts_[fd] != nullptr) {
        // Mark as closed in the extended state.
        s_extra[fd].marked_closed = true;

        delete contexts_[fd];
        contexts_[fd] = nullptr;

        // Reset the extended state.
        s_extra[fd] = FdExtraState{};

        s_total_fds.fetch_sub(1, std::memory_order_relaxed);
        s_total_deallocations.fetch_add(1, std::memory_order_relaxed);
    }
}

// ============================================================
// Extended helper methods (beyond the public header API)
// ============================================================

namespace fd_helpers {

bool isSocket(int fd) {
    if (fd < 0 || fd >= 65536) return false;
    ShardLock guard(fd);
    auto* ctx = FdManager::instance().get(fd, false);
    return ctx != nullptr && ctx->is_socket;
}

void markClosed(int fd) {
    if (fd < 0 || fd >= 65536) return;
    ShardLock guard(fd);
    s_extra[fd].marked_closed = true;
}

bool isMarkedClosed(int fd) {
    if (fd < 0 || fd >= 65536) return true;
    ShardLock guard(fd);
    return s_extra[fd].marked_closed;
}

void setTimeout(int fd, int64_t recv_ms, int64_t send_ms) {
    if (fd < 0 || fd >= 65536) return;
    ShardLock guard(fd);
    auto* ctx = FdManager::instance().get(fd, true);
    if (ctx) {
        if (recv_ms >= -1) ctx->recv_timeout_ms = recv_ms;
        if (send_ms >= -1) ctx->send_timeout_ms = send_ms;
    }
}

void recordRead(int fd, size_t nbytes) {
    if (fd < 0 || fd >= 65536) return;
    ShardLock guard(fd);
    s_extra[fd].bytes_read += nbytes;
    s_extra[fd].last_io_time = 1;  // Placeholder timestamp
}

void recordWrite(int fd, size_t nbytes) {
    if (fd < 0 || fd >= 65536) return;
    ShardLock guard(fd);
    s_extra[fd].bytes_written += nbytes;
    s_extra[fd].last_io_time = 1;
}

void setEventFlags(int fd, uint32_t flags) {
    if (fd < 0 || fd >= 65536) return;
    ShardLock guard(fd);
    s_extra[fd].event_flags = flags;
}

uint32_t getEventFlags(int fd) {
    if (fd < 0 || fd >= 65536) return 0;
    ShardLock guard(fd);
    return s_extra[fd].event_flags;
}

size_t getBytesRead(int fd) {
    if (fd < 0 || fd >= 65536) return 0;
    ShardLock guard(fd);
    return s_extra[fd].bytes_read;
}

size_t getBytesWritten(int fd) {
    if (fd < 0 || fd >= 65536) return 0;
    ShardLock guard(fd);
    return s_extra[fd].bytes_written;
}

void markNonblockByHook(int fd) {
    if (fd < 0 || fd >= 65536) return;
    ShardLock guard(fd);
    auto* ctx = FdManager::instance().get(fd, true);
    if (ctx) {
        ctx->sys_nonblock = true;
    }
}

void markNonblockByUser(int fd) {
    if (fd < 0 || fd >= 65536) return;
    ShardLock guard(fd);
    auto* ctx = FdManager::instance().get(fd, true);
    if (ctx) {
        ctx->user_nonblock = true;
    }
}

bool isHookManaged(int fd) {
    if (fd < 0 || fd >= 65536) return false;
    ShardLock guard(fd);
    auto* ctx = FdManager::instance().get(fd, false);
    return ctx != nullptr && ctx->is_socket && ctx->sys_nonblock;
}

} // namespace fd_helpers

} // namespace zero
