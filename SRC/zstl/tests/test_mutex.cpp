// ============================================================================
// zstl Mutex Unit Tests
// Tests: mutex (lock/unlock/try_lock), recursive_mutex, timed_mutex,
// lock_guard, unique_lock (all constructors: defer/try_to/adopt,
// lock/unlock/try_lock/mutex/release/owns_lock/swap),
// zstl::lock (multi-lock deadlock avoidance), try_lock,
// call_once/once_flag,
// condition_variable (wait/notify_one/notify_all/wait_for with predicate).
// Test basic thread synchronization.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

// ============================================================
// mutex
// ============================================================

TEST(MutexTest, DefaultConstructor) {
    zstl::mutex m;
    SUCCEED();
}

TEST(MutexTest, LockUnlock) {
    zstl::mutex m;
    m.lock();
    m.unlock();
}

TEST(MutexTest, TryLock) {
    zstl::mutex m;
    EXPECT_TRUE(m.try_lock());
    // Second try_lock from same thread is undefined behavior for non-recursive mutex
    // Don't test that.
    m.unlock();
}

TEST(MutexTest, TryLockContention) {
    zstl::mutex m;
    m.lock();

    std::atomic<bool> result(false);
    std::thread t([&]() {
        result.store(m.try_lock());
        if (result.load()) {
            m.unlock();
        }
    });

    t.join();
    EXPECT_FALSE(result.load()); // thread should fail to acquire

    m.unlock();
}

// ============================================================
// recursive_mutex
// ============================================================

TEST(MutexTest, RecursiveMutexLockMultiple) {
    zstl::recursive_mutex m;
    m.lock();
    m.lock(); // re-entrant lock
    m.unlock();
    m.unlock();
}

TEST(MutexTest, RecursiveMutexTryLock) {
    zstl::recursive_mutex m;
    m.lock();
    EXPECT_TRUE(m.try_lock()); // re-entrant try_lock should succeed
    m.unlock();
    m.unlock();
}

// ============================================================
// timed_mutex
// ============================================================

TEST(MutexTest, TimedMutexBasic) {
    zstl::timed_mutex m;
    m.lock();
    m.unlock();
}

TEST(MutexTest, TimedMutexTryLockFor) {
    zstl::timed_mutex m;
    EXPECT_TRUE(m.try_lock_for(std::chrono::milliseconds(100)));
    m.unlock();
}

TEST(MutexTest, TimedMutexTryLockForTimeout) {
    zstl::timed_mutex m;
    m.lock();

    std::atomic<bool> acquired(false);
    std::thread t([&]() {
        acquired.store(m.try_lock_for(std::chrono::milliseconds(50)));
        if (acquired.load()) {
            m.unlock();
        }
    });

    t.join();
    EXPECT_FALSE(acquired.load());
    m.unlock();
}

// ============================================================
// lock_guard
// ============================================================

TEST(MutexTest, LockGuardBasic) {
    zstl::mutex m;
    {
        zstl::lock_guard<zstl::mutex> guard(m);
        // mutex is locked
    }
    // mutex is unlocked
    // Verify we can lock again
    EXPECT_TRUE(m.try_lock());
    m.unlock();
}

TEST(MutexTest, LockGuardAdopt) {
    zstl::mutex m;
    m.lock();
    {
        zstl::lock_guard<zstl::mutex> guard(m, zstl::adopt_lock);
        // mutex is already locked, guard adopts ownership
    }
    // guard destroyed, mutex unlocked
    EXPECT_TRUE(m.try_lock());
    m.unlock();
}

TEST(MutexTest, LockGuardDeduction) {
    zstl::mutex m;
    zstl::lock_guard guard(m); // CTAD
    // locked
}

// ============================================================
// unique_lock
// ============================================================

TEST(UniqueLockTest, DefaultConstructor) {
    zstl::unique_lock<zstl::mutex> lock;
    EXPECT_FALSE(lock.owns_lock());
    EXPECT_EQ(lock.mutex(), nullptr);
}

TEST(UniqueLockTest, LockConstructor) {
    zstl::mutex m;
    zstl::unique_lock<zstl::mutex> lock(m);
    EXPECT_TRUE(lock.owns_lock());
    EXPECT_EQ(lock.mutex(), &m);
    EXPECT_TRUE(static_cast<bool>(lock));
}

TEST(UniqueLockTest, DeferConstructor) {
    zstl::mutex m;
    zstl::unique_lock<zstl::mutex> lock(m, zstl::defer_lock);
    EXPECT_FALSE(lock.owns_lock());
    EXPECT_EQ(lock.mutex(), &m);
}

TEST(UniqueLockTest, TryToLockConstructor) {
    zstl::mutex m;
    zstl::unique_lock<zstl::mutex> lock(m, zstl::try_to_lock);
    EXPECT_TRUE(lock.owns_lock());
}

TEST(UniqueLockTest, AdoptConstructor) {
    zstl::mutex m;
    m.lock();
    zstl::unique_lock<zstl::mutex> lock(m, zstl::adopt_lock);
    EXPECT_TRUE(lock.owns_lock());
    EXPECT_EQ(lock.mutex(), &m);
}

TEST(UniqueLockTest, MoveConstructor) {
    zstl::mutex m;
    zstl::unique_lock<zstl::mutex> lock1(m);
    EXPECT_TRUE(lock1.owns_lock());

    zstl::unique_lock<zstl::mutex> lock2(zstl::move(lock1));
    EXPECT_TRUE(lock2.owns_lock());
    EXPECT_EQ(lock2.mutex(), &m);
    EXPECT_FALSE(lock1.owns_lock());
    EXPECT_EQ(lock1.mutex(), nullptr);
}

TEST(UniqueLockTest, MoveAssignment) {
    zstl::mutex m1, m2;
    zstl::unique_lock<zstl::mutex> lock1(m1);
    zstl::unique_lock<zstl::mutex> lock2(m2);

    lock2 = zstl::move(lock1);
    EXPECT_TRUE(lock2.owns_lock());
    EXPECT_EQ(lock2.mutex(), &m1);
    EXPECT_FALSE(lock1.owns_lock());
}

TEST(UniqueLockTest, LockUnlock) {
    zstl::mutex m;
    zstl::unique_lock<zstl::mutex> lock(m, zstl::defer_lock);

    EXPECT_FALSE(lock.owns_lock());
    lock.lock();
    EXPECT_TRUE(lock.owns_lock());
    lock.unlock();
    EXPECT_FALSE(lock.owns_lock());
}

TEST(UniqueLockTest, TryLock) {
    zstl::mutex m;
    zstl::unique_lock<zstl::mutex> lock(m, zstl::defer_lock);

    EXPECT_TRUE(lock.try_lock());
    EXPECT_TRUE(lock.owns_lock());
}

TEST(UniqueLockTest, Release) {
    zstl::mutex m;
    zstl::unique_lock<zstl::mutex> lock(m);
    EXPECT_TRUE(lock.owns_lock());

    zstl::mutex* mp = lock.release();
    EXPECT_EQ(mp, &m);
    EXPECT_FALSE(lock.owns_lock());
    EXPECT_EQ(lock.mutex(), nullptr);

    // Must manually unlock now
    mp->unlock();
}

TEST(UniqueLockTest, Swap) {
    zstl::mutex m1, m2;
    zstl::unique_lock<zstl::mutex> lock1(m1);
    zstl::unique_lock<zstl::mutex> lock2(m2, zstl::defer_lock);

    lock1.swap(lock2);
    EXPECT_FALSE(lock1.owns_lock());
    EXPECT_EQ(lock1.mutex(), &m2);
    EXPECT_TRUE(lock2.owns_lock());
    EXPECT_EQ(lock2.mutex(), &m1);

    lock2.unlock();
}

TEST(UniqueLockTest, NonMemberSwap) {
    zstl::mutex m1, m2;
    zstl::unique_lock<zstl::mutex> lock1(m1);
    zstl::unique_lock<zstl::mutex> lock2(m2, zstl::defer_lock);

    zstl::swap(lock1, lock2);
    EXPECT_FALSE(lock1.owns_lock());
    EXPECT_TRUE(lock2.owns_lock());

    lock2.unlock();
}

// ============================================================
// unique_lock timed constructors
// ============================================================

TEST(UniqueLockTest, TimedDurationConstructor) {
    zstl::timed_mutex m;
    zstl::unique_lock<zstl::timed_mutex> lock(m, std::chrono::milliseconds(100));
    EXPECT_TRUE(lock.owns_lock());
}

TEST(UniqueLockTest, TimedTryLockFor) {
    zstl::timed_mutex m;
    zstl::unique_lock<zstl::timed_mutex> lock(m, zstl::defer_lock);
    EXPECT_TRUE(lock.try_lock_for(std::chrono::milliseconds(50)));
}

// ============================================================
// zstl::lock (multi-lock)
// ============================================================

TEST(MutexTest, LockTwo) {
    zstl::mutex m1, m2;
    zstl::lock(m1, m2);
    // Both locked
    m2.unlock();
    m1.unlock();
}

TEST(MutexTest, LockThree) {
    zstl::mutex m1, m2, m3;
    zstl::lock(m1, m2, m3);
    m3.unlock();
    m2.unlock();
    m1.unlock();
}

// ============================================================
// zstl::try_lock
// ============================================================

TEST(MutexTest, TryLockSingle) {
    zstl::mutex m;
    int result = zstl::try_lock(m);
    EXPECT_EQ(result, -1); // -1 means success
    m.unlock();
}

TEST(MutexTest, TryLockTwo) {
    zstl::mutex m1, m2;
    int result = zstl::try_lock(m1, m2);
    EXPECT_EQ(result, -1);
    m2.unlock();
    m1.unlock();
}

TEST(MutexTest, TryLockTwoOneHeld) {
    zstl::mutex m1, m2;
    m1.lock();
    int result = zstl::try_lock(m1, m2);
    EXPECT_EQ(result, 0); // Failed on index 0 (m1)
    m1.unlock();
}

// ============================================================
// call_once / once_flag
// ============================================================

TEST(MutexTest, CallOnce) {
    int count = 0;
    zstl::once_flag flag;

    zstl::call_once(flag, [&]() { ++count; });
    EXPECT_EQ(count, 1);

    // Second call should not execute the function
    zstl::call_once(flag, [&]() { ++count; });
    EXPECT_EQ(count, 1);
}

TEST(MutexTest, CallOnceMultiThread) {
    int count = 0;
    zstl::once_flag flag;

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&]() {
            zstl::call_once(flag, [&]() { ++count; });
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(count, 1);
}

// ============================================================
// condition_variable
// ============================================================

TEST(ConditionVariableTest, NotifyOne) {
    zstl::mutex m;
    zstl::condition_variable cv;
    std::atomic<bool> ready(false);

    std::thread t([&]() {
        zstl::unique_lock<zstl::mutex> lock(m);
        cv.wait(lock, [&]() { return ready.load(); });
        EXPECT_TRUE(ready.load());
    });

    {
        zstl::lock_guard<zstl::mutex> lock(m);
        ready.store(true);
    }
    cv.notify_one();

    t.join();
}

TEST(ConditionVariableTest, NotifyAll) {
    zstl::mutex m;
    zstl::condition_variable cv;
    std::atomic<int> counter(0);
    std::atomic<bool> ready(false);

    auto worker = [&]() {
        zstl::unique_lock<zstl::mutex> lock(m);
        cv.wait(lock, [&]() { return ready.load(); });
        counter.fetch_add(1);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back(worker);
    }

    // Give threads time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    {
        zstl::lock_guard<zstl::mutex> lock(m);
        ready.store(true);
    }
    cv.notify_all();

    for (auto& t : threads) t.join();

    EXPECT_EQ(counter.load(), 5);
}

TEST(ConditionVariableTest, WaitFor) {
    zstl::mutex m;
    zstl::condition_variable cv;
    zstl::unique_lock<zstl::mutex> lock(m);

    auto start = std::chrono::steady_clock::now();
    auto result = cv.wait_for(lock, std::chrono::milliseconds(100));
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_EQ(result, std::cv_status::timeout);
    EXPECT_GE(elapsed, std::chrono::milliseconds(50));
}

TEST(ConditionVariableTest, WaitForPredicate) {
    zstl::mutex m;
    zstl::condition_variable cv;
    zstl::unique_lock<zstl::mutex> lock(m);

    std::atomic<bool> flag(false);

    // Predicate returns false, so wait should timeout
    bool result = cv.wait_for(lock, std::chrono::milliseconds(50),
                              [&]() { return flag.load(); });
    EXPECT_FALSE(result);
}

// ============================================================
// Producer-consumer pattern
// ============================================================

TEST(ConditionVariableTest, ProducerConsumer) {
    zstl::mutex m;
    zstl::condition_variable cv;
    std::vector<int> queue;
    bool done = false;

    auto consumer = [&]() {
        int sum = 0;
        zstl::unique_lock<zstl::mutex> lock(m);
        while (!done || !queue.empty()) {
            cv.wait(lock, [&]() { return done || !queue.empty(); });
            while (!queue.empty()) {
                sum += queue.back();
                queue.pop_back();
            }
        }
        return sum;
    };

    auto producer = [&]() {
        for (int i = 1; i <= 10; ++i) {
            {
                zstl::lock_guard<zstl::mutex> lock(m);
                queue.push_back(i);
            }
            cv.notify_one();
        }
        {
            zstl::lock_guard<zstl::mutex> lock(m);
            done = true;
        }
        cv.notify_one();
    };

    std::thread pt(producer);
    std::thread ct(consumer);
    pt.join();
    ct.join();
}
