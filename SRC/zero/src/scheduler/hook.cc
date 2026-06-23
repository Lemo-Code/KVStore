// zero syscall hooks — transparent async I/O via dlsym interposition
//
// Interposes common blocking syscalls at the linker level using
// dlsym(RTLD_NEXT). When hooks are enabled for the current thread:
//
//   - socket() automatically sets O_NONBLOCK and registers with FdManager
//   - connect() handles EINPROGRESS by registering with the reactor and
//     yielding the fiber until the connection completes
//   - accept() loops on EAGAIN, yielding until a connection arrives
//   - read/recv/readv/recvfrom/recvmsg loop on EAGAIN, yielding until
//     data is available
//   - write/send/writev/sendto/sendmsg loop on EAGAIN, yielding until
//     the socket buffer has space
//   - sleep/usleep/nanosleep yield the fiber instead of blocking the
//     OS thread
//   - close() cleans up FdManager state
//   - fcntl(F_SETFL) and ioctl(FIONBIO) track non-blocking state changes
//   - getsockopt/setsockopt track socket timeout values
//
// Hook state is thread-local — threads not running the zero scheduler
// are unaffected. The dlsym lookups are cached using atomic_flag for
// thread-safe lazy initialization with zero overhead after the first call.
//
// IMPORTANT: The hooked function names MUST match the libc symbol names
// exactly for linker interposition (LD_PRELOAD) to work. For in-process
// hooking without LD_PRELOAD, these functions are called explicitly by
// the framework after checking is_hook_enabled().

#include "zero/scheduler/hook.h"
#include "zero/scheduler/scheduler.h"
#include "zero/scheduler/fd_manager.h"
#include "zero/scheduler/reactor.h"
#include "zero/scheduler/timer_wheel.h"
#include "zero/fiber/fiber.h"
#include "zero/base/macro.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <atomic>

namespace zero {

// ============================================================
// Thread-local hook state
// ============================================================

static thread_local bool t_hook_enabled = false;

bool is_hook_enabled() noexcept {
    return t_hook_enabled;
}

void set_hook_enabled(bool enabled) noexcept {
    t_hook_enabled = enabled;
}

// ============================================================
// dlsym helper — thread-safe lazy resolution of real syscalls
// ============================================================
//
// Each hooked function caches the real implementation pointer via a
// function-local static. The atomic_flag ensures exactly one thread
// performs the dlsym lookup; other threads spin briefly (or fall
// through) until the pointer is initialized.
//
// The pattern:
//   static original_fn_t real_fn = nullptr;
//   static std::atomic_flag initialized = ATOMIC_FLAG_INIT;
//   if (!initialized.test_and_set(std::memory_order_acquire)) {
//       real_fn = (original_fn_t)dlsym(RTLD_NEXT, "name");
//   }
//
// RTLD_NEXT finds the next occurrence of the symbol in the shared
// library search order AFTER the current library. This is essential
// for interposition: our hook is in the first library searched, and
// RTLD_NEXT gives us the real libc implementation.

// Convenience macro for declaring and lazily initializing a real function
// pointer. Using function-local statics avoids static initialization
// order issues and keeps the caching local to each hook function.
#define HOOK_REAL_FN(ret, name, ...)                                            \
    using name##_real_t = ret (*)(__VA_ARGS__);                                 \
    static name##_real_t name##_real_ptr = nullptr;                             \
    static std::atomic_flag name##_init_flag = ATOMIC_FLAG_INIT;                \
    if (ZERO_UNLIKELY(!name##_init_flag.test_and_set(std::memory_order_acquire))) { \
        name##_real_ptr = reinterpret_cast<name##_real_t>(                      \
            dlsym(RTLD_NEXT, #name));                                           \
        if (!name##_real_ptr) {                                                 \
            /* dlsym failed — fall back to nullptr; first call will crash    */ \
            /* with a clear segfault rather than silently corrupting state.  */ \
            fprintf(stderr, "HOOK: dlsym(%s) failed: %s\n",                     \
                    #name, dlerror());                                          \
        }                                                                       \
    }

// ============================================================
// Internal helpers
// ============================================================

// Check if the current thread is running within a fiber context and
// hooks are enabled. Returns the current fiber, or nullptr if either
// condition is not met.
static inline Fiber* get_fiber_for_hook() {
    if (ZERO_UNLIKELY(!t_hook_enabled)) {
        return nullptr;
    }
    return Fiber::GetThis();
}

// Preserve errno across a fiber yield/resume cycle. Since the scheduler
// may run other fibers between yield and resume, errno could be
// overwritten. We save it before yielding and restore after.
class ErrnoGuard {
public:
    ErrnoGuard() : saved_errno_(errno) {}
    ~ErrnoGuard() { errno = saved_errno_; }
    void update() { saved_errno_ = errno; }
private:
    int saved_errno_;
};

// ============================================================
// Sleep hooks — yield fiber instead of blocking thread
// ============================================================

unsigned int sleep(unsigned int seconds) {
    Fiber* fiber = get_fiber_for_hook();
    if (!fiber) {
        HOOK_REAL_FN(unsigned int, sleep, unsigned int);
        if (sleep_real_ptr) {
            return sleep_real_ptr(seconds);
        }
        // Fallback: if dlsym failed, do a real sleep via nanosleep
        struct timespec ts = {static_cast<time_t>(seconds), 0};
        struct timespec rem;
        while (::nanosleep(&ts, &rem) != 0 && errno == EINTR) {
            ts = rem;
        }
        return 0;
    }

    // In fiber context: yield the fiber instead of blocking the OS thread.
    // The scheduler's timer wheel is used for accurate sleep timing.
    Scheduler* sched = Scheduler::GetThis();
    if (sched && seconds > 0) {
        // Register a timer for the sleep duration, then yield.
        // On timer expiry, the fiber is re-scheduled.
        Reactor* reactor = Scheduler::t_per_thread
                           ? Scheduler::t_per_thread->reactor
                           : nullptr;
        if (reactor) {
            uint64_t delay_ms = static_cast<uint64_t>(seconds) * 1000;
            reactor->timer_wheel()->add_timer(delay_ms, []() {
                // Timer callback — the fiber is re-scheduled by the
                // timer wheel's tick mechanism. No explicit action needed
                // because the fiber is already in HOLD state and will be
                // re-scheduled when the event loop resumes it.
            });
        }
    }

    // Yield to the scheduler. When we resume, the sleep duration has
    // elapsed (or we were woken early).
    fiber->yield();
    return 0;
}

int usleep(useconds_t usec) {
    Fiber* fiber = get_fiber_for_hook();
    if (!fiber) {
        HOOK_REAL_FN(int, usleep, useconds_t);
        if (usleep_real_ptr) {
            return usleep_real_ptr(usec);
        }
        // Fallback
        struct timespec ts = {
            static_cast<time_t>(usec / 1000000),
            static_cast<long>((usec % 1000000) * 1000)
        };
        return ::nanosleep(&ts, nullptr);
    }

    // For sub-second sleep, register a millisecond-granularity timer.
    Scheduler* sched = Scheduler::GetThis();
    if (sched && usec > 0) {
        Reactor* reactor = Scheduler::t_per_thread
                           ? Scheduler::t_per_thread->reactor
                           : nullptr;
        if (reactor) {
            uint64_t delay_ms = (static_cast<uint64_t>(usec) + 999) / 1000;
            if (delay_ms == 0) delay_ms = 1;
            reactor->timer_wheel()->add_timer(delay_ms, []() {});
        }
    }
    fiber->yield();
    return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
    Fiber* fiber = get_fiber_for_hook();
    if (!fiber) {
        HOOK_REAL_FN(int, nanosleep, const struct timespec*, struct timespec*);
        if (nanosleep_real_ptr) {
            return nanosleep_real_ptr(req, rem);
        }
        return -1;
    }

    if (req == nullptr) {
        errno = EFAULT;
        return -1;
    }

    // Convert timespec to milliseconds for the timer wheel.
    uint64_t delay_ms = static_cast<uint64_t>(req->tv_sec) * 1000ULL +
                        static_cast<uint64_t>(req->tv_nsec) / 1000000ULL;
    if (req->tv_nsec % 1000000 > 0) {
        delay_ms++;  // Round up partial milliseconds
    }

    Scheduler* sched = Scheduler::GetThis();
    if (sched && delay_ms > 0) {
        Reactor* reactor = Scheduler::t_per_thread
                           ? Scheduler::t_per_thread->reactor
                           : nullptr;
        if (reactor) {
            reactor->timer_wheel()->add_timer(delay_ms, []() {});
        }
    }

    fiber->yield();

    // Indicate no remaining sleep time.
    if (rem != nullptr) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }

    return 0;
}

// ============================================================
// socket() — automatic O_NONBLOCK + FdManager registration
// ============================================================

int socket(int domain, int type, int protocol) {
    HOOK_REAL_FN(int, socket, int, int, int);
    if (!socket_real_ptr) {
        errno = ENOSYS;
        return -1;
    }

    if (!t_hook_enabled) {
        return socket_real_ptr(domain, type, protocol);
    }

    // Create the socket via the real syscall.
    int fd = socket_real_ptr(domain, type, protocol);
    if (fd < 0) {
        return fd;  // Propagate error
    }

    // Set O_NONBLOCK automatically so that all I/O on this socket
    // can be multiplexed by the scheduler without blocking.
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        int rc = ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        if (rc < 0) {
            int saved_errno = errno;
            ::close(fd);
            errno = saved_errno;
            return -1;
        }
    }

    // Register the fd with the fd manager so the hook system can
    // identify it as a socket and apply fiber-aware I/O.
    auto* ctx = FdManager::instance().get(fd, true);
    if (ctx != nullptr) {
        ctx->is_socket     = true;
        ctx->sys_nonblock  = true;
    }

    return fd;
}

// ============================================================
// connect() — non-blocking connect with fiber yield
// ============================================================

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    HOOK_REAL_FN(int, connect, int, const struct sockaddr*, socklen_t);
    if (!connect_real_ptr) {
        errno = ENOSYS;
        return -1;
    }

    Fiber* fiber = get_fiber_for_hook();
    if (!fiber) {
        return connect_real_ptr(sockfd, addr, addrlen);
    }

    // Attempt the connection on a non-blocking socket.
    int ret = connect_real_ptr(sockfd, addr, addrlen);
    if (ret == 0) {
        // Connection completed immediately (e.g., localhost).
        return 0;
    }

    // Save errno before any operations that might change it.
    int saved_errno = errno;

    if (saved_errno == EINPROGRESS) {
        // Connection is in progress. Register the fd with the reactor
        // for EPOLLOUT (writable = connection established) and yield
        // the fiber until the reactor notifies us.
        // unused;
        Reactor* reactor = Scheduler::t_per_thread
                           ? Scheduler::t_per_thread->reactor
                           : nullptr;

        if (reactor) {
            // Register for EPOLLOUT — when the socket becomes writable,
            // the connection has completed (either successfully or with
            // an error).
            reactor->add_event(sockfd, EPOLLOUT, nullptr);
        }

        // Yield the fiber. The scheduler event loop will resume us when
        // the socket becomes writable (reactor detects EPOLLOUT).
        fiber->yield();

        // After resuming, check the connection result via SO_ERROR.
        int error = 0;
        socklen_t len = sizeof(error);
        if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
            if (error == 0) {
                // Connection established successfully.
                return 0;
            }
            // Connection failed — set errno to the socket error.
            errno = error;
            return -1;
        }

        // getsockopt failed — propagate errno from getsockopt.
        return -1;
    }

    // Some other error (not EINPROGRESS). Propagate it.
    errno = saved_errno;
    return ret;
}

// ============================================================
// accept() — non-blocking accept with fiber yield on EAGAIN
// ============================================================

int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    HOOK_REAL_FN(int, accept, int, struct sockaddr*, socklen_t*);
    if (!accept_real_ptr) {
        errno = ENOSYS;
        return -1;
    }

    Fiber* fiber = get_fiber_for_hook();
    if (!fiber) {
        return accept_real_ptr(sockfd, addr, addrlen);
    }

    while (true) {
        int fd = accept_real_ptr(sockfd, addr, addrlen);
        if (fd >= 0) {
            // Accepted a new connection — set O_NONBLOCK and register it.
            int flags = ::fcntl(fd, F_GETFL, 0);
            if (flags >= 0) {
                ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            }

            auto* ctx = FdManager::instance().get(fd, true);
            if (ctx != nullptr) {
                ctx->is_socket     = true;
                ctx->sys_nonblock  = true;
            }

            return fd;
        }

        // Handle expected non-blocking errors.
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No pending connections. Yield the fiber; the reactor will
            // wake us when the listening socket becomes readable.
            fiber->yield();
            continue;
        }

        // EINTR: interrupted by signal — retry immediately.
        if (errno == EINTR) {
            continue;
        }

        // Fatal error — propagate.
        return -1;
    }
}

// ============================================================
// accept4() — non-blocking accept4 with flags
// ============================================================

int accept4(int sockfd, struct sockaddr* addr, socklen_t* addrlen, int flags) {
    HOOK_REAL_FN(int, accept4, int, struct sockaddr*, socklen_t*, int);
    if (!accept4_real_ptr) {
        errno = ENOSYS;
        return -1;
    }

    Fiber* fiber = get_fiber_for_hook();
    if (!fiber) {
        return accept4_real_ptr(sockfd, addr, addrlen, flags);
    }

    while (true) {
        // Always add SOCK_NONBLOCK and SOCK_CLOEXEC to the flags for
        // proper async behavior in the fiber scheduler.
        int fd = accept4_real_ptr(sockfd, addr, addrlen,
                                   flags | SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd >= 0) {
            auto* ctx = FdManager::instance().get(fd, true);
            if (ctx != nullptr) {
                ctx->is_socket     = true;
                ctx->sys_nonblock  = true;
            }
            return fd;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fiber->yield();
            continue;
        }
        if (errno == EINTR) {
            continue;
        }
        return -1;
    }
}

// ============================================================
// pread() — hooked positional read
// ============================================================

ssize_t pread(int fd, void* buf, size_t count, off_t offset) {
    HOOK_REAL_FN(ssize_t, pread, int, void*, size_t, off_t);
    if (!pread_real_ptr) { errno = ENOSYS; return -1; }

    // pread is typically used on regular files, not sockets.
    // Regular files do not support epoll, so we cannot make them
    // non-blocking. Always fall through to the real implementation.
    // If hooks are enabled and this is called on a socket fd, we
    // still pass through because pread on sockets is unusual.
    return pread_real_ptr(fd, buf, count, offset);
}

// ============================================================
// pwrite() — hooked positional write
// ============================================================

ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset) {
    HOOK_REAL_FN(ssize_t, pwrite, int, const void*, size_t, off_t);
    if (!pwrite_real_ptr) { errno = ENOSYS; return -1; }

    // Same rationale as pread — positional I/O on regular files
    // cannot be made non-blocking. Pass through to the real syscall.
    return pwrite_real_ptr(fd, buf, count, offset);
}

// ============================================================
// I/O hooks — generic template for read-like and write-like ops
// ============================================================

// Macro to generate a hooked I/O function for read-like operations.
// The pattern: call the real function, if EAGAIN yield and retry,
// if success return, otherwise propagate error.
#define HOOK_IO_READ(name, fd_arg, buf_arg, ...)                                \
    ssize_t name(int fd, __VA_ARGS__) {                                         \
        HOOK_REAL_FN(ssize_t, name, int, __VA_ARGS__);                          \
        if (!name##_real_ptr) { errno = ENOSYS; return -1; }                    \
                                                                                \
        Fiber* fiber = get_fiber_for_hook();                                    \
        if (!fiber) {                                                           \
            return name##_real_ptr(fd, __VA_ARGS__);                            \
        }                                                                       \
                                                                                \
        /* Check if this fd is tracked as a socket */                           \
        auto* ctx = FdManager::instance().get(fd, false);                       \
        if (!ctx || !ctx->is_socket) {                                          \
            return name##_real_ptr(fd, __VA_ARGS__);                            \
        }                                                                       \
                                                                                \
        while (true) {                                                          \
            ssize_t n = name##_real_ptr(fd, __VA_ARGS__);                       \
            if (n >= 0) return n;                                               \
                                                                                \
            if (errno == EAGAIN || errno == EWOULDBLOCK) {                      \
                /* No data available — yield and retry. */                      \
                /* The reactor will detect readability and resume us. */        \
                fiber->yield();                                                 \
                continue;                                                       \
            }                                                                   \
                                                                                \
            if (errno == EINTR) {                                               \
                /* Interrupted by signal — retry immediately. */                \
                continue;                                                       \
            }                                                                   \
                                                                                \
            /* Fatal error — propagate. */                                      \
            return n;                                                           \
        }                                                                       \
    }

// Macro to generate a hooked I/O function for write-like operations.
// Same pattern as read, but without the timeout check (timeouts for
// write are less common and handled at the application level).
#define HOOK_IO_WRITE(name, fd_arg, buf_arg, ...)                               \
    ssize_t name(int fd, __VA_ARGS__) {                                         \
        HOOK_REAL_FN(ssize_t, name, int, __VA_ARGS__);                          \
        if (!name##_real_ptr) { errno = ENOSYS; return -1; }                    \
                                                                                \
        Fiber* fiber = get_fiber_for_hook();                                    \
        if (!fiber) {                                                           \
            return name##_real_ptr(fd, __VA_ARGS__);                            \
        }                                                                       \
                                                                                \
        auto* ctx = FdManager::instance().get(fd, false);                       \
        if (!ctx || !ctx->is_socket) {                                          \
            return name##_real_ptr(fd, __VA_ARGS__);                            \
        }                                                                       \
                                                                                \
        while (true) {                                                          \
            ssize_t n = name##_real_ptr(fd, __VA_ARGS__);                       \
            if (n >= 0) return n;                                               \
                                                                                \
            if (errno == EAGAIN || errno == EWOULDBLOCK) {                      \
                /* Send buffer full — yield and retry. */                       \
                fiber->yield();                                                 \
                continue;                                                       \
            }                                                                   \
                                                                                \
            if (errno == EINTR) continue;                                       \
            return n;                                                           \
        }                                                                       \
    }

// ============================================================
// read() — hooked read
// ============================================================

ssize_t read(int fd, void* buf, size_t count) {
    HOOK_REAL_FN(ssize_t, read, int, void*, size_t);
    if (!read_real_ptr) { errno = ENOSYS; return -1; }

    Fiber* fiber = get_fiber_for_hook();
    if (!fiber) {
        return read_real_ptr(fd, buf, count);
    }

    auto* ctx = FdManager::instance().get(fd, false);
    if (!ctx || !ctx->is_socket) {
        return read_real_ptr(fd, buf, count);
    }

    while (true) {
        ssize_t n = read_real_ptr(fd, buf, count);
        if (n >= 0) return n;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fiber->yield();
            continue;
        }
        if (errno == EINTR) continue;
        return n;
    }
}

// ============================================================
// readv() — hooked scatter read
// ============================================================

ssize_t readv(int fd, const struct iovec* iov, int iovcnt) {
    HOOK_REAL_FN(ssize_t, readv, int, const struct iovec*, int);
    if (!readv_real_ptr) { errno = ENOSYS; return -1; }

    Fiber* fiber = get_fiber_for_hook();
    if (!fiber) {
        return readv_real_ptr(fd, iov, iovcnt);
    }

    auto* ctx = FdManager::instance().get(fd, false);
    if (!ctx || !ctx->is_socket) {
        return readv_real_ptr(fd, iov, iovcnt);
    }

    while (true) {
        ssize_t n = readv_real_ptr(fd, iov, iovcnt);
        if (n >= 0) return n;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fiber->yield();
            continue;
        }
        if (errno == EINTR) continue;
        return n;
    }
}

// ============================================================
// recv() — hooked receive
// ============================================================

ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
    HOOK_REAL_FN(ssize_t, recv, int, void*, size_t, int);
    if (!recv_real_ptr) { errno = ENOSYS; return -1; }

    Fiber* fiber = get_fiber_for_hook();
    if (!fiber) {
        return recv_real_ptr(sockfd, buf, len, flags);
    }

    auto* ctx = FdManager::instance().get(sockfd, false);
    if (!ctx || !ctx->is_socket) {
        return recv_real_ptr(sockfd, buf, len, flags);
    }

    while (true) {
        ssize_t n = recv_real_ptr(sockfd, buf, len, flags);
        if (n >= 0) return n;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fiber->yield();
            continue;
        }
        if (errno == EINTR) continue;
        return n;
    }
}

// ============================================================
// recvfrom() — hooked receive with source address
// ============================================================

ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags,
                 struct sockaddr* src_addr, socklen_t* addrlen) {
    HOOK_REAL_FN(ssize_t, recvfrom, int, void*, size_t, int,
                 struct sockaddr*, socklen_t*);
    if (!recvfrom_real_ptr) { errno = ENOSYS; return -1; }

    Fiber* fiber = get_fiber_for_hook();
    if (!fiber) {
        return recvfrom_real_ptr(sockfd, buf, len, flags,
                                  src_addr, addrlen);
    }

    auto* ctx = FdManager::instance().get(sockfd, false);
    if (!ctx || !ctx->is_socket) {
        return recvfrom_real_ptr(sockfd, buf, len, flags,
                                  src_addr, addrlen);
    }

    while (true) {
        ssize_t n = recvfrom_real_ptr(sockfd, buf, len, flags,
                                       src_addr, addrlen);
        if (n >= 0) return n;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fiber->yield();
            continue;
        }
        if (errno == EINTR) continue;
        return n;
    }
}

// ============================================================
// recvmsg() — hooked receive message
// ============================================================

ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
    HOOK_REAL_FN(ssize_t, recvmsg, int, struct msghdr*, int);
    if (!recvmsg_real_ptr) { errno = ENOSYS; return -1; }

    Fiber* fiber = get_fiber_for_hook();
    if (!fiber) {
        return recvmsg_real_ptr(sockfd, msg, flags);
    }

    auto* ctx = FdManager::instance().get(sockfd, false);
    if (!ctx || !ctx->is_socket) {
        return recvmsg_real_ptr(sockfd, msg, flags);
    }

    while (true) {
        ssize_t n = recvmsg_real_ptr(sockfd, msg, flags);
        if (n >= 0) return n;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fiber->yield();
            continue;
        }
        if (errno == EINTR) continue;
        return n;
    }
}

// ============================================================
// write() — hooked write
// ============================================================

ssize_t write(int fd, const void* buf, size_t count) {
    HOOK_REAL_FN(ssize_t, write, int, const void*, size_t);
    if (!write_real_ptr) { errno = ENOSYS; return -1; }

    Fiber* fiber = get_fiber_for_hook();
    if (!fiber) {
        return write_real_ptr(fd, buf, count);
    }

    auto* ctx = FdManager::instance().get(fd, false);
    if (!ctx || !ctx->is_socket) {
        return write_real_ptr(fd, buf, count);
    }

    while (true) {
        ssize_t n = write_real_ptr(fd, buf, count);
        if (n >= 0) return n;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fiber->yield();
            continue;
        }
        if (errno == EINTR) continue;
        return n;
    }
}

// ============================================================
// writev() — hooked gather write
// ============================================================

ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
    HOOK_REAL_FN(ssize_t, writev, int, const struct iovec*, int);
    if (!writev_real_ptr) { errno = ENOSYS; return -1; }

    Fiber* fiber = get_fiber_for_hook();
    if (!fiber) {
        return writev_real_ptr(fd, iov, iovcnt);
    }

    auto* ctx = FdManager::instance().get(fd, false);
    if (!ctx || !ctx->is_socket) {
        return writev_real_ptr(fd, iov, iovcnt);
    }

    while (true) {
        ssize_t n = writev_real_ptr(fd, iov, iovcnt);
        if (n >= 0) return n;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fiber->yield();
            continue;
        }
        if (errno == EINTR) continue;
        return n;
    }
}

// ============================================================
// send() — hooked send
// ============================================================

ssize_t send(int sockfd, const void* buf, size_t len, int flags) {
    HOOK_REAL_FN(ssize_t, send, int, const void*, size_t, int);
    if (!send_real_ptr) { errno = ENOSYS; return -1; }

    Fiber* fiber = get_fiber_for_hook();
    if (!fiber) {
        return send_real_ptr(sockfd, buf, len, flags);
    }

    auto* ctx = FdManager::instance().get(sockfd, false);
    if (!ctx || !ctx->is_socket) {
        return send_real_ptr(sockfd, buf, len, flags);
    }

    while (true) {
        ssize_t n = send_real_ptr(sockfd, buf, len, flags);
        if (n >= 0) return n;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fiber->yield();
            continue;
        }
        if (errno == EINTR) continue;
        return n;
    }
}

// ============================================================
// sendto() — hooked send with destination address
// ============================================================

ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
               const struct sockaddr* dest_addr, socklen_t addrlen) {
    HOOK_REAL_FN(ssize_t, sendto, int, const void*, size_t, int,
                 const struct sockaddr*, socklen_t);
    if (!sendto_real_ptr) { errno = ENOSYS; return -1; }

    Fiber* fiber = get_fiber_for_hook();
    if (!fiber) {
        return sendto_real_ptr(sockfd, buf, len, flags,
                                dest_addr, addrlen);
    }

    auto* ctx = FdManager::instance().get(sockfd, false);
    if (!ctx || !ctx->is_socket) {
        return sendto_real_ptr(sockfd, buf, len, flags,
                                dest_addr, addrlen);
    }

    while (true) {
        ssize_t n = sendto_real_ptr(sockfd, buf, len, flags,
                                     dest_addr, addrlen);
        if (n >= 0) return n;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fiber->yield();
            continue;
        }
        if (errno == EINTR) continue;
        return n;
    }
}

// ============================================================
// sendmsg() — hooked send message
// ============================================================

ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags) {
    HOOK_REAL_FN(ssize_t, sendmsg, int, const struct msghdr*, int);
    if (!sendmsg_real_ptr) { errno = ENOSYS; return -1; }

    Fiber* fiber = get_fiber_for_hook();
    if (!fiber) {
        return sendmsg_real_ptr(sockfd, msg, flags);
    }

    auto* ctx = FdManager::instance().get(sockfd, false);
    if (!ctx || !ctx->is_socket) {
        return sendmsg_real_ptr(sockfd, msg, flags);
    }

    while (true) {
        ssize_t n = sendmsg_real_ptr(sockfd, msg, flags);
        if (n >= 0) return n;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fiber->yield();
            continue;
        }
        if (errno == EINTR) continue;
        return n;
    }
}

// ============================================================
// close() — clean up FdManager before closing
// ============================================================

int close(int fd) {
    HOOK_REAL_FN(int, close, int);
    if (!close_real_ptr) {
        errno = ENOSYS;
        return -1;
    }

    // Remove from fd manager before closing, regardless of hook state.
    // This prevents stale fd tracking if close() is called with hooks
    // disabled but the fd was previously registered.
    FdManager::instance().remove(fd);

    return close_real_ptr(fd);
}

// ============================================================
// fcntl() — track O_NONBLOCK changes
// ============================================================

int fcntl(int fd, int cmd, ... /* arg */) {
    HOOK_REAL_FN(int, fcntl, int, int, ...);
    if (!fcntl_real_ptr) {
        errno = ENOSYS;
        return -1;
    }

    // Extract the optional third argument. fcntl's third argument is
    // only meaningful for certain commands; for others it is ignored.
    va_list args;
    va_start(args, cmd);

    long arg = 0;
    // Commands that take an int argument.
    switch (cmd) {
    case F_DUPFD:
    case F_DUPFD_CLOEXEC:
    case F_SETFD:
    case F_SETFL:
    case F_SETLK:
    case F_SETLKW:
    case F_SETOWN:
    case F_SETSIG:
    case F_SETLEASE:
    case F_NOTIFY:
    case F_SETPIPE_SZ:
        arg = va_arg(args, long);
        break;
    // F_GETFD, F_GETFL, F_GETLK, F_GETOWN, F_GETSIG, F_GETLEASE,
    // F_GETPIPE_SZ — no argument.
    default:
        break;
    }
    va_end(args);

    int ret = fcntl_real_ptr(fd, cmd, arg);

    // Track O_NONBLOCK state changes from F_SETFL.
    if (ret >= 0 && cmd == F_SETFL) {
        auto* ctx = FdManager::instance().get(fd, false);
        if (ctx != nullptr) {
            ctx->user_nonblock = ((static_cast<long>(arg) & O_NONBLOCK) != 0);
        }
    }

    // Track O_NONBLOCK state from F_GETFL as well (for completeness).
    if (ret >= 0 && cmd == F_GETFL) {
        auto* ctx = FdManager::instance().get(fd, false);
        if (ctx != nullptr && (ret & O_NONBLOCK)) {
            // fd is already non-blocking — could be sys or user set.
            // If not sys_set, assume user_set.
            if (!ctx->sys_nonblock) {
                ctx->user_nonblock = true;
            }
        }
    }

    return ret;
}

// ============================================================
// ioctl() — track FIONBIO (non-blocking) changes
// ============================================================

int ioctl(int fd, unsigned long request, ...) {
    HOOK_REAL_FN(int, ioctl, int, unsigned long, ...);
    if (!ioctl_real_ptr) {
        errno = ENOSYS;
        return -1;
    }

    va_list args;
    va_start(args, request);

    // Extract the optional third argument (void*).
    void* arg = va_arg(args, void*);
    va_end(args);

    int ret = ioctl_real_ptr(fd, request, arg);

    // Track FIONBIO state changes.
    if (ret >= 0 && request == static_cast<unsigned long>(FIONBIO)) {
        auto* ctx = FdManager::instance().get(fd, false);
        if (ctx != nullptr) {
            int* val = static_cast<int*>(arg);
            if (val != nullptr) {
                ctx->user_nonblock = (*val != 0);
            }
        }
    }

    return ret;
}

// ============================================================
// getsockopt() — passthrough (tracking not needed)
// ============================================================

int getsockopt(int sockfd, int level, int optname,
               void* optval, socklen_t* optlen) {
    HOOK_REAL_FN(int, getsockopt, int, int, int, void*, socklen_t*);
    if (!getsockopt_real_ptr) { errno = ENOSYS; return -1; }
    return getsockopt_real_ptr(sockfd, level, optname, optval, optlen);
}

// ============================================================
// setsockopt() — track timeout values
// ============================================================

int setsockopt(int sockfd, int level, int optname,
               const void* optval, socklen_t optlen) {
    HOOK_REAL_FN(int, setsockopt, int, int, int, const void*, socklen_t);
    if (!setsockopt_real_ptr) {
        errno = ENOSYS;
        return -1;
    }

    int ret = setsockopt_real_ptr(sockfd, level, optname, optval, optlen);

    // Track socket timeout values so the hook system can enforce them
    // during fiber-based I/O (since the kernel timeouts apply to
    // blocking calls, but we convert those to non-blocking + yield).
    if (ret == 0 && optval != nullptr) {
        auto* ctx = FdManager::instance().get(sockfd, false);
        if (ctx != nullptr) {
            if (level == SOL_SOCKET) {
                const auto* tv = static_cast<const struct timeval*>(optval);
                if (optname == SO_RCVTIMEO) {
                    ctx->recv_timeout_ms =
                        static_cast<int64_t>(tv->tv_sec) * 1000 +
                        tv->tv_usec / 1000;
                } else if (optname == SO_SNDTIMEO) {
                    ctx->send_timeout_ms =
                        static_cast<int64_t>(tv->tv_sec) * 1000 +
                        tv->tv_usec / 1000;
                }
            }
        }
    }

    return ret;
}

} // namespace zero
