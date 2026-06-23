// test_spinlock.cpp — Comprehensive SpinLock unit tests
// Tests lock/unlock, try_lock, ScopedSpinLock/TrySpinLock RAII,
// and multi-threaded shared counter increment.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <thread>
#include <vector>

using namespace zero;

// ============================================================
// Basic lock/unlock
// ============================================================

TEST(SpinLock, LockUnlock) {
    SpinLock lock;
    lock.lock();
    lock.unlock();
    SUCCEED();
}

TEST(SpinLock, LockUnlockSequence) {
    SpinLock lock;
    for (int i = 0; i < 1000; ++i) {
        lock.lock();
        lock.unlock();
    }
    SUCCEED();
}

// ============================================================
// try_lock
// ============================================================

TEST(SpinLock, TryLockSuccess) {
    SpinLock lock;
    EXPECT_TRUE(lock.try_lock());
    lock.unlock();
}

TEST(SpinLock, TryLockFailsWhenLocked) {
    SpinLock lock;
    lock.lock();
    EXPECT_FALSE(lock.try_lock());
    lock.unlock();
}

TEST(SpinLock, TryLockSucceedsAfterUnlock) {
    SpinLock lock;
    lock.lock();
    lock.unlock();
    EXPECT_TRUE(lock.try_lock());
    lock.unlock();
}

// ============================================================
// is_locked (racy, debug only)
// ============================================================

TEST(SpinLock, IsLocked) {
    SpinLock lock;
    EXPECT_FALSE(lock.is_locked());
    lock.lock();
    EXPECT_TRUE(lock.is_locked());
    lock.unlock();
    EXPECT_FALSE(lock.is_locked());
}

// ============================================================
// ScopedSpinLock RAII
// ============================================================

TEST(SpinLock, ScopedSpinLockAutoUnlock) {
    SpinLock lock;
    {
        ScopedSpinLock guard(lock);
        EXPECT_FALSE(lock.try_lock());
    }
    // Lock should be released
    EXPECT_TRUE(lock.try_lock());
    lock.unlock();
}

TEST(SpinLock, ScopedSpinLockNestedScope) {
    SpinLock lock;
    {
        ScopedSpinLock guard(lock);
        EXPECT_TRUE(lock.is_locked());
        {
            // Can't re-acquire
            EXPECT_FALSE(lock.try_lock());
        }
    }
    // Released after outer scope
    EXPECT_FALSE(lock.is_locked());
}

// ============================================================
// TrySpinLock
// ============================================================

TEST(SpinLock, TrySpinLockSuccess) {
    SpinLock lock;
    {
        TrySpinLock guard(lock);
        EXPECT_TRUE(guard.is_locked());
        EXPECT_TRUE(static_cast<bool>(guard));
    }
    // Released
    EXPECT_FALSE(lock.is_locked());
}

TEST(SpinLock, TrySpinLockFail) {
    SpinLock lock;
    lock.lock();
    {
        TrySpinLock guard(lock);
        EXPECT_FALSE(guard.is_locked());
        EXPECT_FALSE(static_cast<bool>(guard));
    }
    // Still held by us
    lock.unlock();
}

// ============================================================
// Generic LockGuard with SpinLock
// ============================================================

TEST(SpinLock, LockGuardGeneric) {
    SpinLock lock;
    {
        LockGuard<SpinLock> guard(lock);
        EXPECT_FALSE(lock.try_lock());
    }
    EXPECT_TRUE(lock.try_lock());
    lock.unlock();
}

// ============================================================
// Multi-threaded counter increment
// ============================================================

TEST(SpinLock, MultiThreadIncrement) {
    SpinLock lock;
    int counter = 0;
    const int num_threads = 8;
    const int per_thread = 100000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < per_thread; ++i) {
                lock.lock();
                ++counter;
                lock.unlock();
            }
        });
    }

    for (auto& th : threads) th.join();
    EXPECT_EQ(counter, num_threads * per_thread);
}

TEST(SpinLock, MultiThreadGuardIncrement) {
    SpinLock lock;
    int counter = 0;
    const int num_threads = 8;
    const int per_thread = 100000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < per_thread; ++i) {
                LockGuard<SpinLock> guard(lock);
                ++counter;
            }
        });
    }

    for (auto& th : threads) th.join();
    EXPECT_EQ(counter, num_threads * per_thread);
}

TEST(SpinLock, TryLockContention) {
    SpinLock lock;
    std::atomic<int> successes{0};
    std::atomic<int> failures{0};
    const int num_threads = 4;
    const int per_thread = 50000;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (int i = 0; i < per_thread; ++i) {
                if (lock.try_lock()) {
                    successes.fetch_add(1);
                    lock.unlock();
                } else {
                    failures.fetch_add(1);
                }
            }
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();

    // In heavy contention, both successes and failures should occur
    EXPECT_GT(successes.load(), 0);
    EXPECT_GT(failures.load(), 0);
}

// ============================================================
// Two locks fixed order (deadlock prevention test)
// ============================================================

TEST(SpinLock, TwoLocksFixedOrder) {
    SpinLock lockA, lockB;
    std::atomic<int> count{0};
    const int iterations = 10000;

    auto worker = [&]() {
        for (int i = 0; i < iterations; ++i) {
            lockA.lock();
            lockB.lock();
            count.fetch_add(1);
            lockB.unlock();
            lockA.unlock();
        }
    };

    std::thread t1(worker), t2(worker);
    t1.join();
    t2.join();
    EXPECT_EQ(count.load(), 2 * iterations);
}

// ============================================================
// TryLock with backoff
// ============================================================

TEST(SpinLock, TryLockBackoff) {
    SpinLock lockA, lockB;
    std::atomic<int> count{0};
    const int iterations = 1000;

    auto worker = [&]() {
        for (int i = 0; i < iterations; ++i) {
            while (true) {
                lockA.lock();
                if (lockB.try_lock()) {
                    count.fetch_add(1);
                    lockB.unlock();
                    lockA.unlock();
                    break;
                }
                lockA.unlock();
                std::this_thread::yield();
            }
        }
    };

    std::thread t1(worker), t2(worker);
    t1.join();
    t2.join();
    EXPECT_EQ(count.load(), 2 * iterations);
}
