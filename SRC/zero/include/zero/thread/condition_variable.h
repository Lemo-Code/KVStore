// zero ConditionVariable — thread coordination primitive
// Wraps pthread_cond_t. Works with UniqueLock<Mutex>.
// Supports wait, timed wait (wait_for, wait_until), notify_one,
// and notify_all.
#pragma once

#include <pthread.h>
#include "zero/thread/mutex.h"
#include <chrono>
#include "zero/thread/mutex.h"
#include <ctime>
#include "zero/thread/mutex.h"
#include <cerrno>
#include "zero/thread/mutex.h"

#include "zero/base/noncopyable.h"
#include "zero/thread/mutex.h"
#include "zero/base/macro.h"
#include "zero/thread/mutex.h"

namespace zero {

class ConditionVariable : public Noncopyable {
public:
    ConditionVariable() {
        pthread_condattr_t attr;
        pthread_condattr_init(&attr);
        // Use monotonic clock for wait_for/wait_until to be immune to
        // system clock adjustments.
        pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
        pthread_cond_init(&cond_, &attr);
        pthread_condattr_destroy(&attr);
    }

    ~ConditionVariable() {
        pthread_cond_destroy(&cond_);
    }

    // Block until notified. The mutex must be locked by the caller;
    // it is atomically released while waiting and re-acquired on return.
    template <typename MutexType>
    void wait(UniqueLock<MutexType>& lock) {
        ZERO_ASSERT(lock.owns_lock());
        ZERO_ASSERT(lock.mutex() != nullptr);
        pthread_cond_wait(&cond_, lock.mutex()->native_handle());
    }

    // Block until notified or relative timeout expires.
    // Returns true if notified, false if timed out.
    template <typename MutexType, typename Rep, typename Period>
    bool wait_for(UniqueLock<MutexType>& lock,
                   const std::chrono::duration<Rep, Period>& timeout) {
        ZERO_ASSERT(lock.owns_lock());

        using namespace std::chrono;
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);

        auto total_ns =
            duration_cast<nanoseconds>(timeout).count();

        // Using nanoseconds directly to avoid overflow
        ts.tv_sec += static_cast<time_t>(total_ns / 1000000000LL);
        ts.tv_nsec += static_cast<long>(total_ns % 1000000000LL);
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000L;
        }

        int rc = pthread_cond_timedwait(
            &cond_, lock.mutex()->native_handle(), &ts);
        return rc == 0;
    }

    // Block until notified or absolute time point is reached.
    // Returns true if notified, false if timeout.
    template <typename MutexType, typename Clock, typename Duration>
    bool wait_until(
        UniqueLock<MutexType>& lock,
        const std::chrono::time_point<Clock, Duration>& tp) {
        auto now = Clock::now();
        if (now >= tp) {
            return false;
        }
        return wait_for(lock, tp - now);
    }

    // Wake one waiting thread.
    void notify_one() noexcept {
        pthread_cond_signal(&cond_);
    }

    // Wake all waiting threads.
    void notify_all() noexcept {
        pthread_cond_broadcast(&cond_);
    }

    // Get the native handle
    pthread_cond_t* native_handle() noexcept { return &cond_; }
    const pthread_cond_t* native_handle() const noexcept { return &cond_; }

private:
    pthread_cond_t cond_;
};

} // namespace zero
