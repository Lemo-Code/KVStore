// test_mutex.cpp — Comprehensive Mutex unit tests
// Tests Mutex lock/unlock/try_lock, LockGuard RAII,
// UniqueLock (all constructors: defer_lock/try_to_lock/adopt_lock,
// lock/unlock/try_lock/release/owns_lock/swap/move),
// and multi-threaded shared resource protection.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <thread>
#include <vector>
#include <chrono>

using namespace zero;

// ============================================================
// Basic lock/unlock/try_lock
// ============================================================

TEST(Mutex, LockUnlock) {
    Mutex mtx;
    mtx.lock();
    mtx.unlock();
    SUCCEED();
}

TEST(Mutex, TryLockSuccess) {
    Mutex mtx;
    EXPECT_TRUE(mtx.try_lock());
    mtx.unlock();
}

TEST(Mutex, TryLockFailsWhenLocked) {
    Mutex mtx;
    mtx.lock();
    EXPECT_FALSE(mtx.try_lock());
    mtx.unlock();
}

TEST(Mutex, TryLockSucceedsAfterUnlock) {
    Mutex mtx;
    mtx.lock();
    mtx.unlock();
    EXPECT_TRUE(mtx.try_lock());
    mtx.unlock();
}

TEST(Mutex, LockUnlockLoop) {
    Mutex mtx;
    for (int i = 0; i < 1000; ++i) {
        mtx.lock();
        mtx.unlock();
    }
    SUCCEED();
}

// ============================================================
// Native handle
// ============================================================

TEST(Mutex, NativeHandle) {
    Mutex mtx;
    EXPECT_NE(mtx.native_handle(), nullptr);
    // const version
    const Mutex& cmtx = mtx;
    EXPECT_NE(cmtx.native_handle(), nullptr);
}

// ============================================================
// LockGuard (RAII)
// ============================================================

TEST(Mutex, LockGuardAutoUnlock) {
    Mutex mtx;
    {
        LockGuard<Mutex> guard(mtx);
        EXPECT_FALSE(mtx.try_lock());
    }
    EXPECT_TRUE(mtx.try_lock());
    mtx.unlock();
}

TEST(Mutex, LockGuardNestedScope) {
    Mutex mtx;
    {
        LockGuard<Mutex> guard(mtx);
        {
            // Can't re-acquire (non-recursive)
            EXPECT_FALSE(mtx.try_lock());
        }
    }
    EXPECT_TRUE(mtx.try_lock());
    mtx.unlock();
}

// ============================================================
// UniqueLock — immediate lock
// ============================================================

TEST(Mutex, UniqueLockImmediate) {
    Mutex mtx;
    {
        UniqueLock<Mutex> lock(mtx);
        EXPECT_TRUE(lock.owns_lock());
        EXPECT_TRUE(static_cast<bool>(lock));
        EXPECT_FALSE(mtx.try_lock());
    }
    // Destructor unlocks
    EXPECT_TRUE(mtx.try_lock());
    mtx.unlock();
}

// ============================================================
// UniqueLock — defer_lock
// ============================================================

TEST(Mutex, UniqueLockDefer) {
    Mutex mtx;
    {
        UniqueLock<Mutex> lock(mtx, UniqueLock<Mutex>::defer_lock);
        EXPECT_FALSE(lock.owns_lock());
        EXPECT_FALSE(static_cast<bool>(lock));

        lock.lock();
        EXPECT_TRUE(lock.owns_lock());
        EXPECT_FALSE(mtx.try_lock());

        lock.unlock();
        EXPECT_FALSE(lock.owns_lock());
        EXPECT_TRUE(mtx.try_lock());
        mtx.unlock();
    }
}

// ============================================================
// UniqueLock — try_to_lock
// ============================================================

TEST(Mutex, UniqueLockTryToLockSuccess) {
    Mutex mtx;
    UniqueLock<Mutex> lock(mtx, UniqueLock<Mutex>::try_to_lock);
    EXPECT_TRUE(lock.owns_lock());
    mtx.unlock(); // Actually the UniqueLock holds it; need to release via lock
    lock.unlock();
}

TEST(Mutex, UniqueLockTryToLockFail) {
    Mutex mtx;
    mtx.lock();
    {
        UniqueLock<Mutex> lock(mtx, UniqueLock<Mutex>::try_to_lock);
        EXPECT_FALSE(lock.owns_lock());
    }
    mtx.unlock();
}

TEST(Mutex, UniqueLockTryLockMethod) {
    Mutex mtx;
    UniqueLock<Mutex> lock(mtx, UniqueLock<Mutex>::defer_lock);
    EXPECT_FALSE(lock.owns_lock());

    EXPECT_TRUE(lock.try_lock());
    EXPECT_TRUE(lock.owns_lock());

    lock.unlock();
    EXPECT_FALSE(lock.owns_lock());
}

// ============================================================
// UniqueLock — adopt_lock
// ============================================================

TEST(Mutex, UniqueLockAdopt) {
    Mutex mtx;
    mtx.lock();
    {
        UniqueLock<Mutex> lock(mtx, UniqueLock<Mutex>::adopt_lock);
        EXPECT_TRUE(lock.owns_lock());
    }
    // Destructor unlocked it
    EXPECT_TRUE(mtx.try_lock());
    mtx.unlock();
}

// ============================================================
// UniqueLock — release
// ============================================================

TEST(Mutex, UniqueLockRelease) {
    Mutex mtx;
    UniqueLock<Mutex> lock(mtx);
    EXPECT_TRUE(lock.owns_lock());

    Mutex* p = lock.release();
    EXPECT_EQ(p, &mtx);
    EXPECT_FALSE(lock.owns_lock());
    EXPECT_EQ(lock.mutex(), nullptr);

    // Must unlock manually now
    p->unlock();
}

// ============================================================
// UniqueLock — mutex()
// ============================================================

TEST(Mutex, UniqueLockMutex) {
    Mutex mtx;
    UniqueLock<Mutex> lock(mtx);
    EXPECT_EQ(lock.mutex(), &mtx);

    {
        UniqueLock<Mutex> lock2(mtx, UniqueLock<Mutex>::defer_lock);
        EXPECT_EQ(lock2.mutex(), &mtx);
    }
}

// ============================================================
// UniqueLock — move semantics
// ============================================================

TEST(Mutex, UniqueLockMove) {
    Mutex mtx;
    UniqueLock<Mutex> a(mtx);
    EXPECT_TRUE(a.owns_lock());

    UniqueLock<Mutex> b = std::move(a);
    EXPECT_FALSE(a.owns_lock());
    EXPECT_TRUE(b.owns_lock());
    EXPECT_EQ(b.mutex(), &mtx);

    b.unlock();
}

TEST(Mutex, UniqueLockMoveAssign) {
    Mutex mtx;
    UniqueLock<Mutex> a(mtx);
    UniqueLock<Mutex> b;

    b = std::move(a);
    EXPECT_FALSE(a.owns_lock());
    EXPECT_TRUE(b.owns_lock());

    b.unlock();
}

TEST(Mutex, UniqueLockMoveAssignToExisting) {
    Mutex mtx1, mtx2;
    UniqueLock<Mutex> a(mtx1); // Holds mtx1
    UniqueLock<Mutex> b(mtx2); // Holds mtx2

    b = std::move(a); // b unlocks mtx2, takes ownership from a
    // mtx1 should be locked by b
    EXPECT_FALSE(a.owns_lock());
    EXPECT_TRUE(b.owns_lock());
    EXPECT_EQ(b.mutex(), &mtx1);

    // mtx2 should be unlocked
    EXPECT_TRUE(mtx2.try_lock());
    mtx2.unlock();

    b.unlock();
}

// ============================================================
// UniqueLock — owns_lock and operator bool
// ============================================================

TEST(Mutex, UniqueLockOwnsLock) {
    Mutex mtx;

    UniqueLock<Mutex> lock(mtx, UniqueLock<Mutex>::defer_lock);
    EXPECT_FALSE(lock.owns_lock());
    EXPECT_FALSE(static_cast<bool>(lock));

    lock.lock();
    EXPECT_TRUE(lock.owns_lock());
    EXPECT_TRUE(static_cast<bool>(lock));

    lock.unlock();
    EXPECT_FALSE(lock.owns_lock());
}

// ============================================================
// UniqueLock — swap
// ============================================================

TEST(Mutex, UniqueLockSwap) {
    Mutex mtx1, mtx2;
    UniqueLock<Mutex> a(mtx1);
    UniqueLock<Mutex> b(mtx2, UniqueLock<Mutex>::defer_lock);

    EXPECT_TRUE(a.owns_lock());
    EXPECT_FALSE(b.owns_lock());

    // Manual swap via move
    UniqueLock<Mutex> tmp = std::move(a);
    a = std::move(b);
    b = std::move(tmp);

    EXPECT_FALSE(a.owns_lock());
    EXPECT_TRUE(b.owns_lock());
    EXPECT_EQ(b.mutex(), &mtx1);

    b.unlock();
}

// ============================================================
// Multi-threaded counter increment
// ============================================================

TEST(Mutex, MultiThreadIncrement) {
    Mutex mtx;
    int counter = 0;
    const int num_threads = 8;
    const int per_thread = 100000;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < per_thread; ++j) {
                LockGuard<Mutex> guard(mtx);
                ++counter;
            }
        });
    }

    for (auto& th : threads) th.join();
    EXPECT_EQ(counter, num_threads * per_thread);
}

TEST(Mutex, MultiThreadWithTryLock) {
    Mutex mtx;
    std::atomic<int> successes{0};
    const int num_threads = 4;
    const int per_thread = 10000;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (int j = 0; j < per_thread; ++j) {
                if (mtx.try_lock()) {
                    successes.fetch_add(1);
                    mtx.unlock();
                }
            }
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();
    EXPECT_GT(successes.load(), 0);
}

// ============================================================
// Shared resource (queue) protected by mutex
// ============================================================

TEST(Mutex, SharedQueue) {
    Mutex mtx;
    std::vector<int> queue;
    const int num_producers = 4;
    const int per_producer = 1000;

    std::vector<std::thread> threads;
    for (int p = 0; p < num_producers; ++p) {
        threads.emplace_back([&, p]() {
            for (int i = 0; i < per_producer; ++i) {
                LockGuard<Mutex> guard(mtx);
                queue.push_back(p * per_producer + i);
            }
        });
    }

    for (auto& th : threads) th.join();
    EXPECT_EQ(queue.size(), static_cast<size_t>(num_producers * per_producer));
}

// ============================================================
// Multiple unique locks on two mutexes (deadlock prevention with try_lock)
// ============================================================

TEST(Mutex, TwoMutexesTryLockBackoff) {
    Mutex mtxA, mtxB;
    std::atomic<int> count{0};
    const int iterations = 1000;

    auto worker = [&]() {
        for (int i = 0; i < iterations; ++i) {
            while (true) {
                UniqueLock<Mutex> lockA(mtxA);
                UniqueLock<Mutex> lockB(mtxB, UniqueLock<Mutex>::try_to_lock);
                if (lockB.owns_lock()) {
                    count.fetch_add(1);
                    break;
                }
                // lockB failed, lockA released automatically
            }
        }
    };

    std::thread t1(worker), t2(worker);
    t1.join();
    t2.join();
    EXPECT_EQ(count.load(), 2 * iterations);
}
