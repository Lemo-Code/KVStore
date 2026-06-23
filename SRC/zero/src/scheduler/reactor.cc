// zero Reactor implementation — per-thread epoll-based event loop
//
// Each scheduler thread owns a dedicated Reactor that monitors all file
// descriptors assigned to that thread. Using edge-triggered epoll (EPOLLET)
// for low-overhead I/O readiness notification.
//
// Key responsibilities:
//   - Polling for I/O readiness via epoll_wait
//   - Timer management via an integrated 5-level TimerWheel
//   - Cross-thread wakeup via eventfd (for work-stealing and shutdown)
//   - FD event registration, modification, and deregistration
//   - Maintaining a per-FD callback and waiting-fiber map for dispatch
//
// The reactor maintains an internal std::unordered_map from fd to
// FdEventInfo. Each FdEventInfo records:
//   - The user-provided EventCallback (invoked on readiness)
//   - A waiting Fiber::Ptr (resumed and re-scheduled on readiness)
//   - The current epoll event mask
//   - Opaque user_data pointer for round-tripping
//
// Edge-triggered semantics (EPOLLET): the reactor receives exactly one
// notification when an fd transitions from "not ready" to "ready." The
// application must read/write until EAGAIN before the fd becomes
// "not ready" again, at which point the next state change triggers
// another notification. This is more efficient than level-triggered
// (default) because it avoids repeated notifications for idle fds.

#include "zero/scheduler/reactor.h"
#include "zero/scheduler/timer_wheel.h"
#include "zero/scheduler/scheduler.h"
#include "zero/fiber/fiber.h"
#include "zero/base/macro.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include <map>

namespace zero {

// ============================================================
// FdEventInfo — per-FD reactor state
// ============================================================

struct FdEventInfo {
    Reactor::EventCallback cb;            // User callback on readiness
    Fiber::Ptr             waiting_fiber; // Fiber to wake on readiness
    uint32_t               events = 0;    // Currently registered epoll events
    void*                  user_data = nullptr; // Opaque round-tripped pointer
};

// ============================================================
// Per-reactor FD map
// ============================================================
//
// Since the Reactor header does not declare a member for the fd map,
// we maintain it as a file-scope data structure keyed by Reactor*.
// Each Reactor instance has exactly one map; the maps are created on
// first access and destroyed when the Reactor is destructed.
//
// We use std::map (red-black tree) rather than std::unordered_map
// because std::map guarantees that references/pointers to elements
// are never invalidated by insertions or deletions of other elements.
// This is essential because we store pointers to map elements in
// epoll_event.data.ptr for O(1) round-trip dispatch. std::unordered_map
// would invalidate these pointers on rehash.
//
// The O(log N) lookup vs O(1) is negligible for typical reactor
// workloads (tens to low thousands of registered fds per thread).

namespace {

using FdMap = std::map<int, FdEventInfo>;

FdMap& get_fd_map(Reactor* reactor) {
    static std::map<Reactor*, FdMap> s_all_maps;
    return s_all_maps[reactor];
}

void remove_fd_map(Reactor* reactor) {
    static std::map<Reactor*, FdMap> s_all_maps;
    s_all_maps.erase(reactor);
}

} // anonymous namespace

// ============================================================
// Constructor
// ============================================================

Reactor::Reactor() {
    // Create the epoll instance with the close-on-exec flag. This
    // prevents the fd from leaking into child processes after fork().
    // On Linux 2.6.27+, epoll_create1 supersedes epoll_create.
    epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) {
        fprintf(stderr, "Reactor: epoll_create1 failed: %s (errno=%d)\n",
                strerror(errno), errno);
        panic("Reactor: epoll_create1 failed");
    }

    // Create the wakeup eventfd. eventfd is a lightweight alternative to
    // pipe(2) — it uses a single fd with a 64-bit kernel counter. Writing
    // increments the counter; reading resets it and returns the value.
    //
    // EFD_NONBLOCK: reads/writes don't block if the counter is 0/MAX.
    // EFD_CLOEXEC:   prevents leaking into child processes.
    wakeup_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wakeup_fd_ < 0) {
        fprintf(stderr, "Reactor: eventfd failed: %s (errno=%d)\n",
                strerror(errno), errno);
        ::close(epoll_fd_);
        epoll_fd_ = -1;
        panic("Reactor: eventfd failed");
    }

    // Register the wakeup fd with edge-triggered EPOLLIN.
    // We use ev.data.fd = wakeup_fd_ so that poll() can identify the
    // wakeup event by comparing fd values. For user-registered fds,
    // we use ev.data.ptr to point to the FdEventInfo.
    struct epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.events   = EPOLLIN | EPOLLET;
    ev.data.ptr = nullptr;  // nullptr signals "this is the wakeup fd"

    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wakeup_fd_, &ev) != 0) {
        fprintf(stderr, "Reactor: epoll_ctl(wakeup) failed: %s (errno=%d)\n",
                strerror(errno), errno);
        ::close(wakeup_fd_);
        ::close(epoll_fd_);
        wakeup_fd_ = -1;
        epoll_fd_  = -1;
        panic("Reactor: failed to register wakeup fd");
    }

    // Create the integrated timer wheel. One wheel per reactor ensures
    // timer operations are thread-local with no synchronization overhead.
    timer_wheel_ = new TimerWheel();

    // Pre-allocate the epoll_event buffer. 256 events per poll() call
    // is sufficient for high-throughput servers; events beyond this
    // limit will be returned on the next poll() iteration.
    events_.resize(static_cast<size_t>(kMaxEvents));
}

// ============================================================
// Destructor
// ============================================================

Reactor::~Reactor() {
    // Destroy the timer wheel first. Timer callbacks may reference fibers
    // or other reactor state that becomes invalid after cleanup.
    delete timer_wheel_;
    timer_wheel_ = nullptr;

    // Close the wakeup eventfd. The kernel automatically removes this
    // fd from the epoll interest list upon close.
    if (wakeup_fd_ >= 0) {
        ::close(wakeup_fd_);
        wakeup_fd_ = -1;
    }

    // Close the epoll fd. All registered fds are automatically removed
    // from the kernel's interest list. We do NOT close user fds — that
    // is the responsibility of the fd owners.
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }

    // Remove our fd map from the global registry.
    remove_fd_map(this);
}

// ============================================================
// add_event — register an fd with epoll
// ============================================================

bool Reactor::add_event(int fd, uint32_t events, void* user_data) {
    if (fd < 0 || epoll_fd_ < 0) {
        return false;
    }

    // Create or update the FdEventInfo for this fd.
    auto& info = get_fd_map(this)[fd];
    info.events    = events;
    info.user_data = user_data;

    struct epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.events   = events | EPOLLET;
    ev.data.ptr = &info;  // Store pointer to our map entry

    int ret = ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
    if (ret != 0) {
        if (errno == EEXIST) {
            // Already registered — fall through to modify.
            // This is a common code path when switching between
            // EPOLLIN and EPOLLOUT interest (e.g., after a partial write).
            get_fd_map(this).erase(fd);
            return mod_event(fd, events, user_data);
        }
        // Other errors: EBADF (bad fd), ENOMEM (kernel out of memory),
        // ENOSPC (epoll instance full), EPERM (fd does not support epoll).
        get_fd_map(this).erase(fd);
        return false;
    }

    return true;
}

// ============================================================
// mod_event — modify an existing fd's epoll event mask
// ============================================================

bool Reactor::mod_event(int fd, uint32_t events, void* user_data) {
    if (fd < 0 || epoll_fd_ < 0) {
        return false;
    }

    auto& map = get_fd_map(this);
    auto it = map.find(fd);
    if (it == map.end()) {
        // Not registered yet — add it first.
        return add_event(fd, events, user_data);
    }

    it->second.events    = events;
    it->second.user_data = user_data;

    struct epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.events   = events | EPOLLET;
    ev.data.ptr = &it->second;

    int ret = ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
    if (ret != 0) {
        if (errno == ENOENT) {
            // fd was removed (possibly closed) — re-add it.
            map.erase(it);
            return add_event(fd, events, user_data);
        }
        return false;
    }

    return true;
}

// ============================================================
// delEvent — remove an fd from epoll
// ============================================================

bool Reactor::del_event(int fd) {
    if (fd < 0 || epoll_fd_ < 0) {
        return false;
    }

    // Remove from our internal map regardless of epoll_ctl outcome.
    get_fd_map(this).erase(fd);

    // EPOLL_CTL_DEL: the kernel identifies the registration by fd alone.
    // Providing a nullptr event pointer is valid on Linux 2.6.9+.
    int ret = ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    if (ret != 0) {
        if (errno == ENOENT) {
            // Not registered — harmless, the fd may have been auto-removed
            // when closed, or it was never registered.
            return true;
        }
        // EBADF: fd is invalid.
        // EINVAL: epoll_fd is invalid or fd is the epoll_fd itself.
        return false;
    }

    return true;
}

// ============================================================
// poll() — wait for and dispatch I/O events
// ============================================================

int Reactor::poll(int timeout_ms) {
    if (epoll_fd_ < 0) {
        return 0;
    }

    // Clamp the timeout to 1ms minimum for negative values.
    if (timeout_ms < 0) {
        timeout_ms = 1;
    }

    int nfds = ::epoll_wait(epoll_fd_, events_.data(), kMaxEvents, timeout_ms);
    if (nfds < 0) {
        // EINTR: a signal interrupted the wait. Treat as zero events so
        // the event loop processes timers and checks for shutdown.
        if (errno == EINTR) {
            return 0;
        }
        // Other errors typically indicate a programming error.
        return -1;
    }

    // Process each ready event.
    for (int i = 0; i < nfds; ++i) {
        const struct epoll_event& ev = events_[static_cast<size_t>(i)];

        // Identify the wakeup fd. We stored ev.data.ptr = nullptr for
        // the wakeup fd in the constructor; all user-registered fds
        // have a non-null FdEventInfo pointer.
        if (ev.data.ptr == nullptr) {
            // Wakeup event — drain the eventfd counter.
            // Multiple wakeups may have accumulated (e.g., several
            // work-stealing attempts between polls). Draining them
            // all in one loop avoids redundant wakeups.
            uint64_t drain = 0;
            while (::read(wakeup_fd_, &drain, sizeof(drain)) > 0) {
                // Empty loop body — just draining.
            }
            continue;
        }

        // Retrieve the FdEventInfo stored at registration time.
        auto* info = static_cast<FdEventInfo*>(ev.data.ptr);
        if (info == nullptr) {
            continue;  // Should not happen
        }

        uint32_t ready_events = ev.events;

        // Handle error/hangup events. EPOLLERR and EPOLLHUP are
        // always reported by the kernel even if not requested.
        // We propagate them to the callback so the application can
        // check SO_ERROR or handle EOF gracefully.
        if (ready_events & (EPOLLERR | EPOLLHUP)) {
            if (info->cb) {
                info->cb(ready_events, info->user_data);
            }
        }

        // Invoke the user's callback if present.
        if (info->cb && (ready_events & info->events)) {
            info->cb(ready_events, info->user_data);
        }

        // Resume the waiting fiber if one is registered on this fd.
        // The fiber was placed in HOLD state by the hook system when
        // it hit EAGAIN; resuming it allows the I/O operation to retry.
        if (info->waiting_fiber) {
            Fiber::Ptr fiber = std::move(info->waiting_fiber);
            info->waiting_fiber.reset();

            if (fiber->getState() == Fiber::State::HOLD) {
                // Transition to READY and re-schedule via the current
                // scheduler. The fiber will retry its I/O when it runs.
                fiber->setState(Fiber::State::READY);
                Scheduler* sched = Scheduler::GetThis();
                if (sched != nullptr) {
                    sched->schedule(std::move(fiber));
                }
            }
        }
    }

    // Tick the timer wheel after processing I/O events. Expired timer
    // callbacks may schedule new fibers, which is handled by the timer
    // wheel's tick() implementation (it invokes callbacks directly).
    timer_wheel_->tick();

    return nfds;
}

// ============================================================
// wakeup() — cross-thread notification
// ============================================================

void Reactor::wakeup() noexcept {
    if (wakeup_fd_ < 0) {
        return;
    }

    // Write 1 to the eventfd counter. This causes epoll_wait to return
    // immediately with the wakeup fd marked readable, even if the calling
    // thread is in a different scheduler than the one owning this reactor.
    //
    // We use a uint64_t value of 1. The actual value is irrelevant — we
    // just need to trigger EPOLLIN. Multiple consecutive wakeups before
    // the reactor drains the counter may cause the write to fail with
    // EAGAIN (counter at UINT64_MAX-1). This is benign: the fd is already
    // readable, so epoll_wait will return regardless.
    uint64_t val = 1;
    ssize_t ret = ::write(wakeup_fd_, &val, sizeof(val));
    ZERO_UNUSED(ret);

    // Errors from eventfd write are silently ignored:
    //   EAGAIN: counter full → fd already readable
    //   EINTR:  signal interrupted → unlikely but harmless
    //   EBADF:  wakeup_fd_ closed (during shutdown) → harmless
}

} // namespace zero
