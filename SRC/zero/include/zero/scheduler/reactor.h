// zero Reactor — per-thread epoll-based I/O event loop
//
// Each scheduler thread owns one Reactor. The Reactor monitors file
// descriptors for readability/writability/errors and dispatches events
// to registered callbacks (typically fibers waiting on I/O).
//
// Integrates with:
//   - TimerWheel: for timer-based events (timeouts, delayed tasks)
//   - FdManager: maps fd -> waiting fiber
//   - eventfd: for cross-thread wakeup (when another thread schedules
//     a fiber onto this thread's queue, it writes to the eventfd to
//     wake the epoll_wait).
//
// The poll() method is the core of the event loop — it blocks in
// epoll_wait with a timeout determined by the nearest timer deadline.
// On events, it resumes the waiting fibers.
#pragma once

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>

#include "zero/base/noncopyable.h"

namespace zero {

class TimerWheel;
class Fiber;

class Reactor : public Noncopyable {
public:
    // Callback type for I/O events: void(uint32_t events, void* user_data)
    // events is a bitmask of EPOLLIN, EPOLLOUT, EPOLLERR, EPOLLHUP, etc.
    using EventCallback = std::function<void(uint32_t events, void* user_data)>;

    // Maximum events returned per epoll_wait call
    static constexpr int kMaxEvents = 256;

    Reactor();
    ~Reactor();

    // ============================================================
    // FD event management
    // ============================================================

    // Add an FD to the epoll interest list.
    // events: bitmask of EPOLLIN, EPOLLOUT, EPOLLERR, EPOLLET, etc.
    // user_data: opaque pointer returned with events (typically FdContext*)
    // Returns true on success.
    bool add_event(int fd, uint32_t events, void* user_data = nullptr);

    // Modify the events for an already-registered FD.
    bool mod_event(int fd, uint32_t events, void* user_data = nullptr);

    // Remove an FD from epoll monitoring.
    bool del_event(int fd);

    // ============================================================
    // Event loop
    // ============================================================

    // Poll for events. Blocks for up to `timeout_ms` milliseconds.
    // For each ready FD, calls the registered event callback
    // (which typically resumes the waiting fiber).
    // Also advances the timer wheel and fires expired timers.
    //
    // Returns the total number of events processed (I/O + timers).
    int poll(int timeout_ms);

    // ============================================================
    // Timer management
    // ============================================================

    // Get the per-reactor timer wheel
    TimerWheel* timer_wheel() noexcept { return timer_wheel_; }
    const TimerWheel* timer_wheel() const noexcept { return timer_wheel_; }

    // ============================================================
    // Wakeup mechanism
    // ============================================================

    // Wake up the event loop from another thread.
    // Writes 1 byte to the internal eventfd, which causes epoll_wait()
    // to return immediately.
    void wakeup() noexcept;

    // ============================================================
    // Observers
    // ============================================================

    int epoll_fd() const noexcept { return epoll_fd_; }
    int wakeup_fd() const noexcept { return wakeup_fd_; }

    // Whether the reactor is active (not shut down)
    bool is_running() const noexcept { return running_; }

    // Stop the reactor loop (called during scheduler shutdown)
    void stop() noexcept { running_ = false; wakeup(); }

private:
    // Process a single epoll event
    void process_event(const epoll_event& ev);

    // Handle the wakeup eventfd
    void handle_wakeup();

    int epoll_fd_ = -1;
    int wakeup_fd_ = -1;  // eventfd for cross-thread wakeup
    TimerWheel* timer_wheel_ = nullptr;

    // Pre-allocated event buffer (avoids allocation in poll loop)
    std::vector<epoll_event> events_;

    bool running_ = true;
};

} // namespace zero
