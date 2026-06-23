// zero SpinLock — atomic_flag spinlock with exponential backoff
// Uses x86 PAUSE or ARM YIELD/WFE for power-efficient spinning.
// Includes throughput-optimized ScopedLock.
#pragma once

#include <atomic>
#include <utility>

#include "zero/base/noncopyable.h"

namespace zero {

class SpinLock : public Noncopyable {
public:
    SpinLock() noexcept = default;

    // Acquire the lock, spinning with exponential backoff.
    // On x86, uses PAUSE instruction to improve SMT throughput and
    // reduce power consumption.
    // On ARM64, uses YIELD hint.
    void lock() noexcept {
        unsigned backoff = 1;
        constexpr unsigned kMaxBackoff = 1024;

        while (locked_.exchange(true, std::memory_order_acquire)) {
            // Exponential backoff with PAUSE/YIELD
            for (unsigned i = 0; i < backoff; ++i) {
                cpu_relax();
            }
            if (backoff < kMaxBackoff) {
                backoff <<= 1;
            }
        }
    }

    // Attempt to acquire the lock without blocking.
    // Returns true if the lock was acquired, false otherwise.
    bool try_lock() noexcept {
        return !locked_.exchange(true, std::memory_order_acquire);
    }

    // Release the lock.
    void unlock() noexcept {
        locked_.store(false, std::memory_order_release);
    }

    // Whether the lock is currently held (for debugging only — racy)
    bool is_locked() const noexcept {
        return locked_.load(std::memory_order_relaxed);
    }

private:
    // Platform-specific CPU relaxation instruction
    static void cpu_relax() noexcept {
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
        __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
        __asm__ volatile("yield" ::: "memory");
#elif defined(__powerpc__) || defined(__ppc__)
        __asm__ volatile("or 27,27,27" ::: "memory");
#else
        // Generic fallback: prevent compiler from optimizing away
        __asm__ volatile("" ::: "memory");
#endif
    }

    std::atomic<bool> locked_{false};
};

// RAII lock guard for SpinLock
class ScopedSpinLock {
public:
    explicit ScopedSpinLock(SpinLock& lock) noexcept : lock_(lock) {
        lock_.lock();
    }

    ~ScopedSpinLock() noexcept {
        lock_.unlock();
    }

    ScopedSpinLock(const ScopedSpinLock&) = delete;
    ScopedSpinLock& operator=(const ScopedSpinLock&) = delete;

private:
    SpinLock& lock_;
};

// SpinLock with try-lock semantics
class TrySpinLock {
public:
    explicit TrySpinLock(SpinLock& lock) noexcept
        : lock_(&lock), locked_(lock.try_lock()) {}

    ~TrySpinLock() noexcept {
        if (locked_) {
            lock_->unlock();
        }
    }

    bool is_locked() const noexcept { return locked_; }
    explicit operator bool() const noexcept { return locked_; }

    TrySpinLock(const TrySpinLock&) = delete;
    TrySpinLock& operator=(const TrySpinLock&) = delete;

private:
    SpinLock* lock_;
    bool locked_;
};

} // namespace zero
