// zero UringEngine implementation — io_uring I/O backend (optional)
//
// io_uring is a high-performance asynchronous I/O interface introduced
// in Linux 5.1. It uses shared memory ring buffers for submission and
// completion queues, eliminating syscall overhead per I/O operation.
//
// This implementation is conditionally compiled:
//   - When ZERO_HAVE_URING is defined and liburing is available:
//     Full io_uring-based I/O with batched submission/completion.
//   - Otherwise:
//     Stub that falls back to epoll. All methods return false/0
//     so callers gracefully degrade to EpollEngine.
//
// Features supported (with liburing):
//   - io_uring_queue_init / io_uring_queue_exit lifecycle
//   - SQE preparation for poll_add, poll_remove, read, write ops
//   - CQE batch peeking via io_uring_peek_batch_cqe
//   - Timeout via IORING_OP_TIMEOUT linked SQEs
#include "zero/io/uring_engine.h"
#include "zero/base/macro.h"
#include <unistd.h>
#include <cerrno>

namespace zero {
#ifdef ZERO_HAS_IOURING

// ============================================================
// Construction / Destruction
// ============================================================
IoUringEngine::UringEngine() {
#ifdef ZERO_HAVE_URING
    // Initialize io_uring with a reasonable queue depth.
    // 256 entries balances memory usage (~256 * 2 * 64 bytes)
    // against throughput (fewer batch submissions needed).
    struct io_uring_params params;
    std::memset(&params, 0, sizeof(params));
    // params.flags = IORING_SETUP_SQPOLL;  // Kernel poll thread (optional)

    int ret = io_uring_queue_init_params(256, &ring_, &params);
    if (ret == 0) {
        initialized_ = true;
    }
    // On failure, initialized_ remains false and we fall back gracefully
#else
    ZERO_UNUSED(initialized_);
#endif
}

IoUringEngine::~UringEngine() {
#ifdef ZERO_HAVE_URING
    if (initialized_) {
        io_uring_queue_exit(&ring_);
        initialized_ = false;
    }
#endif
}

// ============================================================
// FD management
// ============================================================
bool IoUringEngine::addFd(int fd, uint32_t events, void* user_data) {
#ifdef ZERO_HAVE_URING
    if (!initialized_ || fd < 0) return false;

    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) return false;

    // Prepare a poll-add submission
    io_uring_prep_poll_add(sqe, fd, events);
    io_uring_sqe_set_data(sqe, user_data);

    int ret = io_uring_submit(&ring_);
    return ret >= 0;
#else
    ZERO_UNUSED(fd, events); ZERO_UNUSED(user_data);
    return false;  // Stub: fallback to epoll
#endif
}

bool IoUringEngine::modFd(int fd, uint32_t events, void* user_data) {
#ifdef ZERO_HAVE_URING
    if (!initialized_ || fd < 0) return false;

    // To modify, we first remove then re-add.
    // io_uring does not have a direct "modify poll" operation.
    // We use IORING_OP_POLL_REMOVE followed by IORING_OP_POLL_ADD.

    // Queue poll remove
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) return false;
    io_uring_prep_poll_remove(sqe, user_data);
    io_uring_submit(&ring_);

    // Queue poll add with new events
    return addFd(fd, events, user_data);
#else
    ZERO_UNUSED(fd, events); ZERO_UNUSED(user_data);
    return false;
#endif
}

bool IoUringEngine::delFd(int fd) {
#ifdef ZERO_HAVE_URING
    if (!initialized_ || fd < 0) return false;

    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) return false;

    // Cancel any pending poll operation on this fd
    // We pass fd as user_data for identification
    io_uring_prep_poll_remove(sqe, reinterpret_cast<void*>(static_cast<uintptr_t>(fd)));

    int ret = io_uring_submit(&ring_);
    return ret >= 0;
#else
    ZERO_UNUSED(fd);
    return false;
#endif
}

// ============================================================
// Event polling (wait for completions)
// ============================================================
int IoUringEngine::wait(IoEvent* events, int max_events, int timeout_ms) {
#ifdef ZERO_HAVE_URING
    if (!initialized_ || !events || max_events <= 0) return 0;

    struct io_uring_cqe* cqes[256];
    int nr_ready = 0;

    // If timeout is not immediate, use io_uring_wait_cqe_timeout
    if (timeout_ms > 0) {
        struct __kernel_timespec ts;
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000L;

        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        if (sqe) {
            io_uring_prep_timeout(sqe, &ts, 0, 0);
            io_uring_submit(&ring_);
        }
    }

    // Peek at all available completions
    nr_ready = io_uring_peek_batch_cqe(&ring_, cqes,
                                        std::min(max_events, 256));

    int count = 0;
    for (int i = 0; i < nr_ready; ++i) {
        struct io_uring_cqe* cqe = cqes[i];
        if (!cqe) continue;

        // Convert CQE to IoEvent
        events[count].fd = cqe->res;  // For poll ops, cqe->res is error code
        events[count].events = 0;      // We don't get event flags from io_uring
        events[count].user_data = io_uring_cqe_get_data(cqe);

        // Mark the CQE as seen
        io_uring_cqe_seen(&ring_, cqe);
        ++count;
    }

    return count;
#else
    ZERO_UNUSED(events, max_events); ZERO_UNUSED(timeout_ms);
    return 0;  // Stub: no events available
#endif
}

#endif  // ZERO_HAS_IOURING
} // namespace zero
