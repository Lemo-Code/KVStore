// zero Mutex — pthread mutex wrapper with deadlock-safe LockGuard
// Uses PTHREAD_MUTEX_ADAPTIVE_NP for optimized throughput under
// contention (spins briefly before yielding to kernel).
// Provides LockGuard, UniqueLock, and shared try-lock helpers.
// Note: RWLock has been moved to zero/thread/rwlock.h.
#pragma once

#include <pthread.h>
#include <mutex>

#include "zero/base/noncopyable.h"
#include "zero/base/macro.h"

namespace zero {

class Mutex : public Noncopyable {
public:
    Mutex() {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        // Adaptive mutex: spins briefly before kernel-level sleep.
        // Provides better throughput under moderate contention.
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ADAPTIVE_NP);
        pthread_mutex_init(&mutex_, &attr);
        pthread_mutexattr_destroy(&attr);
    }

    ~Mutex() {
        pthread_mutex_destroy(&mutex_);
    }

    // Acquire the lock (blocking)
    void lock() {
        pthread_mutex_lock(&mutex_);
    }

    // Release the lock
    void unlock() {
        pthread_mutex_unlock(&mutex_);
    }

    // Attempt to acquire the lock without blocking.
    // Returns true if the lock was acquired.
    bool try_lock() {
        return pthread_mutex_trylock(&mutex_) == 0;
    }

    // Get the native mutex handle (for use with condition variables etc.)
    pthread_mutex_t* native_handle() noexcept { return &mutex_; }
    const pthread_mutex_t* native_handle() const noexcept { return &mutex_; }

private:
    pthread_mutex_t mutex_;
};

// RAII lock guard — acquires on construction, releases on destruction.
// Simple, lightweight, non-movable. Works with Mutex, RWLock, SpinLock,
// FiberMutex — anything that exposes lock()/unlock().
template <typename LockType>
class LockGuard {
public:
    explicit LockGuard(LockType& lock) : lock_(lock) {
        lock_.lock();
    }

    ~LockGuard() {
        lock_.unlock();
    }

    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;

private:
    LockType& lock_;
};

// Unique lock with deferred, try-to-lock, and adopt modes.
// Can be unlocked and re-locked. Required by ConditionVariable.
template <typename LockType>
class UniqueLock {
public:
    // Tag for deferred lock (don't lock on construction)
    struct defer_lock_t {};
    static constexpr defer_lock_t defer_lock{};

    // Tag for try-to-lock (attempt non-blocking lock on construction)
    struct try_to_lock_t {};
    static constexpr try_to_lock_t try_to_lock{};

    // Tag for adopt lock (assume the lock is already held)
    struct adopt_lock_t {};
    static constexpr adopt_lock_t adopt_lock{};

    // Lock immediately on construction
    explicit UniqueLock(LockType& lock) : lock_(&lock) {
        lock_->lock();
        owns_ = true;
    }

    // Deferred — do not lock
    UniqueLock(LockType& lock, defer_lock_t) noexcept : lock_(&lock) {}

    // Try to lock
    UniqueLock(LockType& lock, try_to_lock_t) : lock_(&lock) {
        owns_ = lock_->try_lock();
    }

    // Adopt — assume already locked
    UniqueLock(LockType& lock, adopt_lock_t) noexcept
        : lock_(&lock), owns_(true) {}

    ~UniqueLock() {
        if (owns_) {
            lock_->unlock();
        }
    }

    // Move-only
    UniqueLock(UniqueLock&& other) noexcept
        : lock_(other.lock_), owns_(other.owns_) {
        other.lock_ = nullptr;
        other.owns_ = false;
    }

    UniqueLock& operator=(UniqueLock&& other) noexcept {
        if (this != &other) {
            if (owns_) lock_->unlock();
            lock_ = other.lock_;
            owns_ = other.owns_;
            other.lock_ = nullptr;
            other.owns_ = false;
        }
        return *this;
    }

    UniqueLock(const UniqueLock&) = delete;
    UniqueLock& operator=(const UniqueLock&) = delete;

    void lock() {
        ZERO_ASSERT(lock_ && !owns_);
        lock_->lock();
        owns_ = true;
    }

    bool try_lock() {
        ZERO_ASSERT(lock_ && !owns_);
        owns_ = lock_->try_lock();
        return owns_;
    }

    void unlock() {
        ZERO_ASSERT(lock_ && owns_);
        lock_->unlock();
        owns_ = false;
    }

    bool owns_lock() const noexcept { return owns_; }
    explicit operator bool() const noexcept { return owns_; }

    LockType* mutex() const noexcept { return lock_; }
    LockType* release() noexcept {
        LockType* l = lock_;
        lock_ = nullptr;
        owns_ = false;
        return l;
    }

private:
    LockType* lock_ = nullptr;
    bool owns_ = false;
};

} // namespace zero
