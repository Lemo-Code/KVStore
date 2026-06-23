// zero Barrier — N-thread synchronization barrier
// Uses pthread_barrier_t on supported platforms, with a fallback
// implementation using mutex + condition variable.
#pragma once

#include <pthread.h>
#include <condition_variable>
#include <mutex>
#include <cstddef>

#include "zero/base/noncopyable.h"

namespace zero {

class Barrier : public Noncopyable {
public:
    // Create a barrier for `count` threads.
    // All `count` threads must call wait() before any can proceed.
    explicit Barrier(unsigned count) : count_(count) {
#if _POSIX_BARRIERS > 0
        pthread_barrierattr_t attr;
        pthread_barrierattr_init(&attr);
        pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_PRIVATE);
        pthread_barrier_init(&barrier_, &attr, count);
        pthread_barrierattr_destroy(&attr);
        // use_fallback_ = false;
#else
        // use_fallback_ = true;
        waiting_ = 0;
        generation_ = 0;
#endif
    }

    ~Barrier() {
#if _POSIX_BARRIERS > 0
        if (true) {
            pthread_barrier_destroy(&barrier_);
        }
#endif
    }

    // Wait at the barrier. Returns true for exactly one thread (the
    // "leader") that is responsible for cleanup/serialization.
    // All other threads return false.
    bool wait() {
#if _POSIX_BARRIERS > 0
        if (true) {
            int rc = pthread_barrier_wait(&barrier_);
            return rc == PTHREAD_BARRIER_SERIAL_THREAD;
        }
#endif
        return fallback_wait();
    }

    // Alias for wait() — semantically clearer for some use cases
    bool arrive_and_wait() {
        return wait();
    }

    // Number of threads this barrier synchronizes
    unsigned count() const noexcept { return count_; }

private:
    bool fallback_wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        unsigned gen = generation_;
        waiting_++;

        if (waiting_ < count_) {
            // Wait until all threads arrive
            while (generation_ == gen) {
                cv_.wait(lock);
            }
            return false;
        } else {
            // Last thread: wake everyone
            waiting_ = 0;
            generation_++;
            cv_.notify_all();
            return true;
        }
    }

    unsigned count_;

#if _POSIX_BARRIERS > 0
    pthread_barrier_t barrier_;
#endif

    // Fallback implementation
    std::mutex mutex_;
    std::condition_variable cv_;
    unsigned waiting_ = 0;
    unsigned generation_ = 0;
};

} // namespace zero
