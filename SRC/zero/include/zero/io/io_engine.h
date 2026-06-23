// zero IIOEngine — abstract I/O multiplexing backend interface
//
// Zero supports pluggable I/O backends. The primary backend is epoll
// (Linux), with optional io_uring (Linux 5.1+) for even lower latency.
//
// The interface is used by the Reactor to monitor file descriptors for
// readability, writability, and error conditions. Each backend is
// responsible for translating its native event representation into
// the zero::IoEvent format.
#pragma once

#include <cstdint>
#include <sys/epoll.h>  // For EPOLLIN/EPOLLOUT/EPOLLERR constants

namespace zero {

// ============================================================
// IoEvent — unified event structure for all backends
// ============================================================

struct IoEvent {
    int fd = -1;           // File descriptor that triggered
    uint32_t events = 0;  // Bitmask of EPOLLIN, EPOLLOUT, EPOLLERR,
                            // EPOLLHUP, EPOLLRDHUP, etc.
    void* user_data = nullptr;  // Opaque pointer set at registration time
};

// ============================================================
// IIOEngine — abstract I/O backend
// ============================================================

class IIOEngine {
public:
    virtual ~IIOEngine() = default;

    // Register a file descriptor with the given event mask.
    // events: bitmask of EPOLLIN, EPOLLOUT, EPOLLERR, EPOLLET, etc.
    // user_data: returned in IoEvent when the fd is ready.
    // Returns true on success.
    virtual bool add_fd(int fd, uint32_t events,
                         void* user_data = nullptr) = 0;

    // Modify the event mask for an already-registered fd.
    virtual bool mod_fd(int fd, uint32_t events,
                         void* user_data = nullptr) = 0;

    // Remove a file descriptor from monitoring.
    virtual bool del_fd(int fd) = 0;

    // Wait for events. Blocks for up to `timeout_ms` milliseconds.
    // - timeout_ms > 0: block for at most timeout_ms ms
    // - timeout_ms == 0: return immediately (poll)
    // - timeout_ms < 0: block indefinitely until events arrive
    //
    // Fills `events` array with up to `max_events` events.
    // Returns the number of events placed in the array, 0 on timeout,
    // or -1 on error.
    virtual int wait(IoEvent* events, int max_events,
                      int timeout_ms) = 0;

    // Get the native fd for this backend (epoll fd, io_uring fd, etc.).
    // Used for nested event loop integration.
    virtual int native_fd() const noexcept = 0;

    // Wake up a blocked wait() from another thread.
    virtual void wakeup() noexcept = 0;
};

} // namespace zero
