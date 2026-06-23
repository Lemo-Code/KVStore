// zero Semaphore — counting semaphore for thread synchronization
// Wraps POSIX sem_t for portability. Supports wait, try_wait,
// timed wait (wait_for), and post (signal).
// Initial count defaults to 0 (blocking until posted).
#pragma once

#include <semaphore.h>
#include <chrono>
#include <ctime>
#include <cerrno>

#include "zero/base/noncopyable.h"

namespace zero {

class Semaphore : public Noncopyable {
public:
    // Create a semaphore with an initial count.
    // count=0: threads calling wait() will block until post().
    // count=N: up to N threads can wait() without blocking.
    explicit Semaphore(unsigned int initial = 0) {
        sem_init(&sem_, 0, static_cast<int>(initial));
    }

    ~Semaphore() {
        sem_destroy(&sem_);
    }

    // Wait (decrement) — blocks if count is 0.
    void wait() {
        int rc;
        do {
            rc = sem_wait(&sem_);
        } while (rc == -1 && errno == EINTR);
    }

    // Try to decrement without blocking.
    // Returns true if successful, false if count is 0.
    bool try_wait() {
        return sem_trywait(&sem_) == 0;
    }

    // Wait with a relative timeout.
    // Returns true if the semaphore was acquired, false on timeout.
    template <typename Rep, typename Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& timeout) {
        using namespace std::chrono;

        auto sec = duration_cast<seconds>(timeout);
        auto ns = duration_cast<nanoseconds>(timeout) -
                  duration_cast<nanoseconds>(sec);

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);

        ts.tv_sec += sec.count();
        ts.tv_nsec += ns.count();
        // Handle nanosecond overflow
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000L;
        }

        return sem_timedwait(&sem_, &ts) == 0;
    }

    // Wait until an absolute time point.
    // Returns true if acquired, false if the time point passed.
    template <typename Clock, typename Duration>
    bool wait_until(
        const std::chrono::time_point<Clock, Duration>& tp) {
        auto now = Clock::now();
        if (now >= tp) {
            return try_wait();
        }
        return wait_for(tp - now);
    }

    // Post (increment) the semaphore, waking one waiting thread.
    void post() {
        sem_post(&sem_);
    }

    // Post to wake N waiting threads simultaneously.
    void post(size_t n) {
        for (size_t i = 0; i < n; ++i) {
            sem_post(&sem_);
        }
    }

    // Get the current count (may be negative if threads are waiting).
    // Returns -1 on error.
    int count() const noexcept {
        int val = 0;
        if (sem_getvalue(const_cast<sem_t*>(&sem_), &val) == 0) {
            return val;
        }
        return -1;
    }

    // Get the native sem_t handle
    sem_t* native_handle() noexcept { return &sem_; }
    const sem_t* native_handle() const noexcept { return &sem_; }

private:
    sem_t sem_;
};

} // namespace zero
