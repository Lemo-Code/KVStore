/**
 * @file    reactor.h
 * @brief   Per-thread epoll event loop with fiber integration.
 *
 * Reactor is the I/O multiplexing core. Each thread has one Reactor
 * instance that manages epoll, timers, and fiber wakeups.
 *
 * When a fiber blocks on I/O:
 *   1. fd registered with epoll + EPOLLONESHOT
 *   2. Fiber yields
 *   3. Reactor::poll() returns events → fiber woken
 *
 * @ingroup reactor
 */

#pragma once

#include <sys/epoll.h>
#include <functional>
#include <vector>
#include <memory>

namespace zero {

class Fiber;

/**
 * @brief  Per-thread epoll event loop.
 */
class Reactor {
public:
    static const int kMaxEvents = 256;

    Reactor();
    ~Reactor();

    /// Add/modify epoll event for fd. Returns 0 on success.
    int addEvent(int fd, uint32_t events, Fiber* waiter);
    /// Remove fd from epoll.
    int delEvent(int fd);
    /// Cancel a specific event for fd.
    bool cancelEvent(int fd, uint32_t event);

    /// Poll for events. Returns number of ready fds.
    /// timeout_ms: -1 = infinite, 0 = non-blocking, >0 = timeout.
    int poll(int timeout_ms);

    /// Wake up the epoll_wait (for cross-thread notification).
    void notify();

    /// Get per-thread Reactor instance.
    static Reactor* GetCurrent();

private:
    int epoll_fd_;
    int event_fd_; // eventfd for wakeup

    struct EventCtx {
        Fiber* read_waiter  = nullptr;
        Fiber* write_waiter = nullptr;
        uint32_t registered_events = 0;
    };

    std::vector<EventCtx> fd_ctxs_; // indexed by fd (grows as needed)

    void ensureFdCapacity(int fd);
};

} // namespace zero
