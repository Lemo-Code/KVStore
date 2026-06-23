// zero FiberMutex & Fiber sync primitives — fiber-aware synchronization
//
// Regular OS-level mutexes block the calling thread, which defeats the
// purpose of M:N fiber scheduling — a blocked fiber prevents the scheduler
// from running other fibers on that thread.
//
// Fiber-aware synchronization primitives yield the fiber back to the
// scheduler instead of blocking the OS thread. When the resource becomes
// available, the fiber is re-scheduled.
//
// All primitives in this file:
//   FiberMutex           - Mutual exclusion lock
//   FiberConditionVariable - Condition variable for FiberMutex
//   FiberSemaphore       - Counting semaphore
//   FiberSharedMutex     - Reader-writer lock
//
// All operations require a running fiber context. Calling from a raw OS
// thread will fall back to blocking behaviour.
#pragma once

#include <queue>
#include "zero/fiber/fiber.h"
#include "zero/thread/spinlock.h"
#include "zero/thread/mutex.h"
#include "zero/base/noncopyable.h"

namespace zero {

class Scheduler;

// ============================================================
// FiberMutex — fiber-aware mutual exclusion
// ============================================================
//
// When a fiber calls lock() on a contended mutex, it is placed in a
// wait queue and yields to the scheduler. When the mutex holder calls
// unlock(), the next waiting fiber is dequeued and scheduled.
//
// Recursive locking is NOT supported (same as std::mutex semantics).
//
// Thread-safe: can be used by fibers on different OS threads (the
// internal wait queue is protected by a SpinLock).
class FiberMutex : public Noncopyable {
public:
    FiberMutex() noexcept = default;
    ~FiberMutex();

    // Acquire the mutex. If held by another fiber, the calling fiber
    // is placed in the wait queue and yields to the scheduler.
    // Returns true on success.
    bool lock();

    // Attempt to acquire the mutex without blocking.
    // Returns true if the mutex was acquired.
    bool try_lock();

    // Release the mutex. If fibers are waiting, the next one in the
    // queue is scheduled for wakeup.
    void unlock();

    // Whether the mutex is currently held (racy — for debugging only).
    bool is_locked() const noexcept;

private:
    std::atomic<bool> locked_{false};
    Fiber* owner_ = nullptr;

    // Wait queue — protected by internal spinlock
    mutable SpinLock wait_lock_;
    std::queue<Fiber::Ptr> waiters_;

    // Helper to schedule a fiber
    static void schedule_fiber(Fiber::Ptr fiber);
};

// ============================================================
// FiberConditionVariable — fiber-aware condition variable
// ============================================================
//
// Works with FiberMutex (UniqueLock<FiberMutex>). When wait() is
// called, the mutex is atomically released and the fiber yields.
// Upon wakeup (via notify_one/notify_all), the fiber re-acquires
// the mutex before returning.
//
// Supports spurious wakeups (standard CV semantics).
class FiberConditionVariable : public Noncopyable {
public:
    FiberConditionVariable() noexcept = default;
    ~FiberConditionVariable();

    // Wait for a notification. The mutex must be locked by the calling
    // fiber. Atomically releases the mutex and yields until notified.
    // On return, the mutex is re-locked.
    //
    // Usage:
    //   UniqueLock<FiberMutex> lk(mtx);
    //   while (!condition) {
    //       cv.wait(lk);
    //   }
    void wait(UniqueLock<FiberMutex>& lock);

    // Wait with a predicate (convenience wrapper).
    // Equivalent to: while (!pred()) wait(lock);
    template <typename Predicate>
    void wait(UniqueLock<FiberMutex>& lock, Predicate pred) {
        while (!pred()) {
            wait(lock);
        }
    }

    // Wait for a notification or timeout (milliseconds).
    // Returns true if notified, false if timed out.
    bool wait_for(UniqueLock<FiberMutex>& lock, int64_t timeout_ms);

    // Wait with a predicate and timeout.
    template <typename Predicate>
    bool wait_for(UniqueLock<FiberMutex>& lock, int64_t timeout_ms,
                  Predicate pred) {
        if (pred()) return true;
        // Note: timer-based timeout requires scheduler integration.
        // For now, we approximate with a simplistic polling approach.
        while (!pred()) {
            if (!wait_for(lock, timeout_ms)) {
                return pred();
            }
        }
        return true;
    }

    // Wake one waiting fiber.
    void notify_one();

    // Wake all waiting fibers.
    void notify_all();

private:
    mutable SpinLock wait_lock_;
    std::queue<Fiber::Ptr> waiters_;
};

// ============================================================
// FiberSemaphore — fiber-aware counting semaphore
// ============================================================
//
// A counting semaphore initialized with a given number of permits.
// acquire() decrements the count; if no permits are available, the
// fiber yields until a permit is released.
// release() increments the count and wakes a waiting fiber.
//
// This is the fiber equivalent of a POSIX semaphore.
class FiberSemaphore : public Noncopyable {
public:
    // Create a semaphore with `count` initial permits.
    explicit FiberSemaphore(int64_t count = 0) noexcept
        : count_(count) {}

    ~FiberSemaphore();

    // Acquire one permit. If none available, the calling fiber yields.
    void acquire();

    // Attempt to acquire one permit without blocking.
    // Returns true if a permit was acquired.
    bool try_acquire();

    // Acquire with timeout (milliseconds).
    // Returns true if a permit was acquired, false on timeout.
    bool try_acquire_for(int64_t timeout_ms);

    // Release `n` permits (default 1). Wakes waiting fibers.
    void release(int64_t n = 1);

    // Get the current permit count (negative if fibers are waiting).
    // This is racy and should only be used for diagnostics.
    int64_t available() const noexcept;

    // Drain all permits and return the count.
    int64_t drain() noexcept;

private:
    mutable SpinLock lock_;
    std::atomic<int64_t> count_;
    std::queue<Fiber::Ptr> waiters_;
};

// ============================================================
// FiberSharedMutex — fiber-aware reader-writer lock
// ============================================================
//
// Multiple fibers can hold a shared (read) lock concurrently.
// Only one fiber can hold an exclusive (write) lock at a time.
// When a writer is waiting, new readers may be blocked to prevent
// write starvation (writer-preference policy).
//
// All blocking operations yield the calling fiber instead of
// blocking the OS thread.
class FiberSharedMutex : public Noncopyable {
public:
    FiberSharedMutex() noexcept = default;
    ~FiberSharedMutex();

    // ============================================================
    // Exclusive (write) lock
    // ============================================================

    // Acquire exclusive ownership. If the lock is held by any fiber
    // (reader or writer), the calling fiber yields.
    void lock();

    // Attempt to acquire exclusive ownership without blocking.
    bool try_lock();

    // Release exclusive ownership. Wakes waiting fibers.
    void unlock();

    // ============================================================
    // Shared (read) lock
    // ============================================================

    // Acquire shared ownership. Multiple fibers can hold shared
    // ownership concurrently. If a writer holds the lock (or a
    // writer is waiting and the policy is writer-preference),
    // the calling fiber yields.
    void lock_shared();

    // Attempt to acquire shared ownership without blocking.
    bool try_lock_shared();

    // Release shared ownership.
    void unlock_shared();

    // ============================================================
    // Observers (racy — debugging only)
    // ============================================================

    // Whether the lock is held in exclusive mode.
    bool is_write_locked() const noexcept;

    // Whether the lock is held in shared mode.
    bool is_read_locked() const noexcept;

private:
    enum class State : uint8_t {
        Free = 0,        // No owners
        Shared = 1,      // One or more readers hold the lock
        Exclusive = 2,   // One writer holds the lock
    };

    mutable SpinLock lock_;
    State state_ = State::Free;
    int64_t reader_count_ = 0;
    Fiber* writer_owner_ = nullptr;

    // Wait queues
    std::queue<Fiber::Ptr> reader_waiters_;
    std::queue<Fiber::Ptr> writer_waiters_;

    // Writer-preference: when true, new readers are blocked if a
    // writer is waiting (prevents write starvation).
    bool writer_preference_ = true;
};

// ============================================================
// RAII lock guards for fiber mutexes
// ============================================================

// Scoped lock for FiberMutex (acquires in ctor, releases in dtor).
using FiberLockGuard = LockGuard<FiberMutex>;

// Unique lock for FiberMutex (for use with FiberConditionVariable).
using FiberUniqueLock = UniqueLock<FiberMutex>;

// Scoped shared lock for FiberSharedMutex.
class FiberSharedLockGuard {
public:
    explicit FiberSharedLockGuard(FiberSharedMutex& mtx) : mtx_(&mtx) {
        mtx_->lock_shared();
    }

    ~FiberSharedLockGuard() {
        mtx_->unlock_shared();
    }

    FiberSharedLockGuard(const FiberSharedLockGuard&) = delete;
    FiberSharedLockGuard& operator=(const FiberSharedLockGuard&) = delete;

private:
    FiberSharedMutex* mtx_;
};

// Scoped exclusive lock for FiberSharedMutex.
class FiberExclusiveLockGuard {
public:
    explicit FiberExclusiveLockGuard(FiberSharedMutex& mtx) : mtx_(&mtx) {
        mtx_->lock();
    }

    ~FiberExclusiveLockGuard() {
        mtx_->unlock();
    }

    FiberExclusiveLockGuard(const FiberExclusiveLockGuard&) = delete;
    FiberExclusiveLockGuard& operator=(const FiberExclusiveLockGuard&) = delete;

private:
    FiberSharedMutex* mtx_;
};

} // namespace zero
