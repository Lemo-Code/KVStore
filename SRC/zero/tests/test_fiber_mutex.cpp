// test_fiber_mutex.cpp — Comprehensive fiber synchronization unit tests
// Tests FiberMutex lock/unlock/try_lock, FiberLockGuard (RAII),
// FiberUniqueLock, FiberConditionVariable wait/notify_one/notify_all,
// FiberSemaphore acquire/try_acquire/release/drain,
// FiberSharedMutex shared/exclusive lock/try_lock,
// FiberSharedLockGuard/FiberExclusiveLockGuard.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <thread>
#include <vector>
#include <chrono>

using namespace zero;

// ============================================================
// FiberMutex — lock/unlock
// ============================================================

TEST(FiberMutex, LockUnlock) {
    FiberMutex mtx;
    mtx.lock();
    mtx.unlock();
    SUCCEED();
}

TEST(FiberMutex, TryLockSuccess) {
    FiberMutex mtx;
    EXPECT_TRUE(mtx.try_lock());
    mtx.unlock();
}

TEST(FiberMutex, TryLockFailsWhenHeld) {
    FiberMutex mtx;
    mtx.lock();
    EXPECT_FALSE(mtx.try_lock());
    mtx.unlock();
}

TEST(FiberMutex, TryLockAfterUnlock) {
    FiberMutex mtx;
    mtx.lock();
    mtx.unlock();
    EXPECT_TRUE(mtx.try_lock());
    mtx.unlock();
}

TEST(FiberMutex, IsLocked) {
    FiberMutex mtx;
    EXPECT_FALSE(mtx.is_locked());  // Initially unlocked

    mtx.lock();
    EXPECT_TRUE(mtx.is_locked());

    mtx.unlock();
    EXPECT_FALSE(mtx.is_locked());
}

// ============================================================
// FiberMutex — LockGuard RAII
// ============================================================

TEST(FiberMutex, LockGuardAutoUnlock) {
    FiberMutex mtx;
    {
        FiberLockGuard guard(mtx);
        EXPECT_TRUE(mtx.is_locked());
    }
    EXPECT_FALSE(mtx.is_locked());
}

TEST(FiberMutex, LockGuardTryLockAfterGuardScope) {
    FiberMutex mtx;
    {
        FiberLockGuard guard(mtx);
        EXPECT_FALSE(mtx.try_lock());
    }
    EXPECT_TRUE(mtx.try_lock());
    mtx.unlock();
}

TEST(FiberMutex, LockGuardNested) {
    FiberMutex mtx;
    {
        FiberLockGuard outer(mtx);
        EXPECT_TRUE(mtx.is_locked());
        {
            // Inner: mutex is still held
            EXPECT_FALSE(mtx.try_lock());
        }
    }
    EXPECT_FALSE(mtx.is_locked());
}

// ============================================================
// FiberMutex — FiberUniqueLock
// ============================================================

TEST(FiberMutex, UniqueLockImmediate) {
    FiberMutex mtx;
    {
        FiberUniqueLock lock(mtx);
        EXPECT_TRUE(lock.owns_lock());
        EXPECT_TRUE(mtx.is_locked());
    }
    EXPECT_FALSE(mtx.is_locked());
}

TEST(FiberMutex, UniqueLockDefer) {
    FiberMutex mtx;
    {
        FiberUniqueLock lock(mtx, FiberUniqueLock::defer_lock);
        EXPECT_FALSE(lock.owns_lock());
        EXPECT_FALSE(mtx.is_locked());

        lock.lock();
        EXPECT_TRUE(lock.owns_lock());
        EXPECT_TRUE(mtx.is_locked());

        lock.unlock();
        EXPECT_FALSE(lock.owns_lock());
    }
}

TEST(FiberMutex, UniqueLockTryToLock) {
    FiberMutex mtx;
    {
        FiberUniqueLock lock(mtx, FiberUniqueLock::try_to_lock);
        EXPECT_TRUE(lock.owns_lock());
    }

    // Lock held by another
    FiberMutex mtx2;
    mtx2.lock();
    {
        FiberUniqueLock lock(mtx2, FiberUniqueLock::try_to_lock);
        EXPECT_FALSE(lock.owns_lock());
    }
    mtx2.unlock();
}

TEST(FiberMutex, UniqueLockAdopt) {
    FiberMutex mtx;
    mtx.lock();
    {
        FiberUniqueLock lock(mtx, FiberUniqueLock::adopt_lock);
        EXPECT_TRUE(lock.owns_lock());
        EXPECT_TRUE(mtx.is_locked());
    }
    // Destructor unlocks
    EXPECT_FALSE(mtx.is_locked());
}

TEST(FiberMutex, UniqueLockMove) {
    FiberMutex mtx;
    FiberUniqueLock a(mtx);
    EXPECT_TRUE(a.owns_lock());

    FiberUniqueLock b = std::move(a);
    EXPECT_FALSE(a.owns_lock());
    EXPECT_TRUE(b.owns_lock());

    b.unlock();
    EXPECT_FALSE(mtx.is_locked());
}

Test(FiberMutex, UniqueLockRelease) {
    FiberMutex mtx;
    FiberUniqueLock lock(mtx);
    EXPECT_TRUE(lock.owns_lock());

    lock.release();
    EXPECT_FALSE(lock.owns_lock());

    // The mutex is still locked — we released responsibility
    EXPECT_TRUE(mtx.is_locked());
    mtx.unlock();  // Manual unlock
}

// ============================================================
// FiberConditionVariable — wait/notify_one
// ============================================================

TEST(FiberConditionVariable, NotifyOne) {
    FiberMutex mtx;
    FiberConditionVariable cv;
    std::atomic<bool> ready{false};

    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        {
            FiberLockGuard guard(mtx);
            ready.store(true);
        }
        cv.notify_one();
    });

    FiberUniqueLock lock(mtx);
    cv.wait(lock);  // Will fall back to blocking wait on real thread
    EXPECT_TRUE(ready.load());

    t.join();
}

TEST(FiberConditionVariable, NotifyAll) {
    FiberMutex mtx;
    FiberConditionVariable cv;
    std::atomic<int> woke{0};
    const int kWaiters = 4;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int i = 0; i < kWaiters; ++i) {
        threads.emplace_back([&]() {
            FiberUniqueLock lock(mtx);
            start.store(true);
            while (woke.load() == 0) {
                cv.wait(lock);
            }
            woke.fetch_add(1);
        });
    }

    // Wait for all threads to be waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    cv.notify_all();

    for (auto& t : threads) t.join();
    EXPECT_EQ(woke.load(), kWaiters);
}

TEST(FiberConditionVariable, WaitWithPredicate) {
    FiberMutex mtx;
    FiberConditionVariable cv;
    std::atomic<bool> ready{false};

    std::thread notifier([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ready.store(true);
        cv.notify_one();
    });

    FiberUniqueLock lock(mtx);
    cv.wait(lock, [&ready]() { return ready.load(); });
    EXPECT_TRUE(ready.load());

    notifier.join();
}

TEST(FiberConditionVariable, WaitForTimeout) {
    FiberMutex mtx;
    FiberConditionVariable cv;

    FiberUniqueLock lock(mtx);
    bool result = cv.wait_for(lock, 10);  // 10ms timeout
    EXPECT_FALSE(result);  // Should timeout
}

TEST(FiberConditionVariable, WaitForPredicateTimeout) {
    FiberMutex mtx;
    FiberConditionVariable cv;

    FiberUniqueLock lock(mtx);
    bool result = cv.wait_for(lock, 10, []() { return false; });
    EXPECT_FALSE(result);  // Predicate never true, should timeout
}

// ============================================================
// FiberSemaphore
// ============================================================

TEST(FiberSemaphore, AcquireRelease) {
    FiberSemaphore sem(1);  // Start with 1 permit
    sem.acquire();  // Takes it
    sem.release();  // Puts it back
    sem.acquire();  // Can take again
    SUCCEED();
}

TEST(FiberSemaphore, TryAcquireSuccess) {
    FiberSemaphore sem(1);
    EXPECT_TRUE(sem.try_acquire());
    sem.release();
}

TEST(FiberSemaphore, TryAcquireFail) {
    FiberSemaphore sem(0);
    EXPECT_FALSE(sem.try_acquire());
}

TEST(FiberSemaphore, MultipleReleases) {
    FiberSemaphore sem(0);
    sem.release(3);

    EXPECT_TRUE(sem.try_acquire());
    EXPECT_TRUE(sem.try_acquire());
    EXPECT_TRUE(sem.try_acquire());
    EXPECT_FALSE(sem.try_acquire());
}

TEST(FiberSemaphore, Available) {
    FiberSemaphore sem(5);
    EXPECT_EQ(sem.available(), 5);

    sem.acquire();
    EXPECT_EQ(sem.available(), 4);

    sem.acquire();
    EXPECT_EQ(sem.available(), 3);

    sem.release(2);
    EXPECT_EQ(sem.available(), 5);
}

TEST(FiberSemaphore, Drain) {
    FiberSemaphore sem(10);
    sem.acquire();
    sem.acquire();
    sem.acquire();

    int64_t drained = sem.drain();
    EXPECT_EQ(drained, 7);
    EXPECT_EQ(sem.available(), 0);
}

TEST(FiberSemaphore, ReleaseMultipleThreads) {
    FiberSemaphore sem(0);
    std::atomic<int> acquired{0};
    const int kThreads = 4;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&]() {
            start.store(true);
            sem.acquire();
            acquired.fetch_add(1);
        });
    }

    // Wait for all threads to be waiting
    while (!start.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    sem.release(kThreads);

    for (auto& t : threads) t.join();
    EXPECT_EQ(acquired.load(), kThreads);
}

TEST(FiberSemaphore, TryAcquireForTimeout) {
    FiberSemaphore sem(0);
    // Should time out
    bool result = sem.try_acquire_for(10);
    EXPECT_FALSE(result);
}

// ============================================================
// FiberSharedMutex — exclusive lock
// ============================================================

TEST(FiberSharedMutex, ExclusiveLockUnlock) {
    FiberSharedMutex mtx;

    mtx.lock();
    EXPECT_TRUE(mtx.is_write_locked());
    mtx.unlock();
    EXPECT_FALSE(mtx.is_write_locked());
}

TEST(FiberSharedMutex, ExclusiveTryLock) {
    FiberSharedMutex mtx;

    EXPECT_TRUE(mtx.try_lock());
    EXPECT_TRUE(mtx.is_write_locked());

    EXPECT_FALSE(mtx.try_lock());  // Already held

    mtx.unlock();
    EXPECT_TRUE(mtx.try_lock());
    mtx.unlock();
}

// ============================================================
// FiberSharedMutex — shared lock
// ============================================================

TEST(FiberSharedMutex, SharedLockUnlock) {
    FiberSharedMutex mtx;

    mtx.lock_shared();
    EXPECT_TRUE(mtx.is_read_locked());
    EXPECT_FALSE(mtx.is_write_locked());

    mtx.unlock_shared();
    EXPECT_FALSE(mtx.is_read_locked());
}

TEST(FiberSharedMutex, SharedTryLock) {
    FiberSharedMutex mtx;

    EXPECT_TRUE(mtx.try_lock_shared());
    EXPECT_TRUE(mtx.is_read_locked());

    EXPECT_TRUE(mtx.try_lock_shared());  // Multiple readers ok
    EXPECT_TRUE(mtx.is_read_locked());

    mtx.unlock_shared();
    mtx.unlock_shared();
    EXPECT_FALSE(mtx.is_read_locked());
}

TEST(FiberSharedMutex, WriteBlocksRead) {
    FiberSharedMutex mtx;
    mtx.lock();  // Exclusive lock

    // Try to acquire shared — should fail
    EXPECT_FALSE(mtx.try_lock_shared());

    mtx.unlock();

    // Now shared should work
    EXPECT_TRUE(mtx.try_lock_shared());
    mtx.unlock_shared();
}

TEST(FiberSharedMutex, ReadBlocksWrite) {
    FiberSharedMutex mtx;
    mtx.lock_shared();  // Shared lock

    // Try to acquire exclusive — should fail
    EXPECT_FALSE(mtx.try_lock());

    mtx.unlock_shared();

    // Now exclusive should work
    EXPECT_TRUE(mtx.try_lock());
    mtx.unlock();
}

TEST(FiberSharedMutex, MultipleConcurrentReaders) {
    FiberSharedMutex mtx;

    EXPECT_TRUE(mtx.try_lock_shared());
    EXPECT_TRUE(mtx.try_lock_shared());
    EXPECT_TRUE(mtx.try_lock_shared());
    EXPECT_TRUE(mtx.is_read_locked());

    mtx.unlock_shared();
    mtx.unlock_shared();
    mtx.unlock_shared();

    EXPECT_FALSE(mtx.is_read_locked());
}

// ============================================================
// Scoped lock guards for FiberSharedMutex
// ============================================================

TEST(FiberSharedMutex, ExclusiveLockGuard) {
    FiberSharedMutex mtx;
    {
        FiberExclusiveLockGuard guard(mtx);
        EXPECT_TRUE(mtx.is_write_locked());
    }
    EXPECT_FALSE(mtx.is_write_locked());
}

TEST(FiberSharedMutex, SharedLockGuard) {
    FiberSharedMutex mtx;
    {
        FiberSharedLockGuard guard(mtx);
        EXPECT_TRUE(mtx.is_read_locked());
    }
    EXPECT_FALSE(mtx.is_read_locked());
}

TEST(FiberSharedMutex, SharedAndExclusiveGuardInteraction) {
    FiberSharedMutex mtx;
    {
        FiberSharedLockGuard shared(mtx);
        EXPECT_TRUE(mtx.is_read_locked());
        EXPECT_FALSE(mtx.is_write_locked());

        // Cannot get exclusive lock while shared held
        EXPECT_FALSE(mtx.try_lock());
    }
    // Shared released
    EXPECT_TRUE(mtx.try_lock());
    mtx.unlock();
}

// ============================================================
// Multi-thread FiberMutex
// ============================================================

TEST(FiberMutex, MultiThreadLockUnlock) {
    FiberMutex mtx;
    int counter = 0;
    const int kThreads = 4;
    const int kPerThread = 10000;

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < kPerThread; ++j) {
                FiberLockGuard guard(mtx);
                ++counter;
            }
        });
    }

    for (auto& t : threads) t.join();
    EXPECT_EQ(counter, kThreads * kPerThread);
}

TEST(FiberMutex, MultiThreadTryLock) {
    FiberMutex mtx;
    std::atomic<int> successes{0};
    const int kThreads = 4;
    const int kPerThread = 5000;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&]() {
            while (!start.load());
            for (int j = 0; j < kPerThread; ++j) {
                if (mtx.try_lock()) {
                    successes.fetch_add(1);
                    mtx.unlock();
                }
            }
        });
    }

    start.store(true);
    for (auto& t : threads) t.join();

    EXPECT_GT(successes.load(), 0);
}

// ============================================================
// FiberSemaphore as producer-consumer
// ============================================================

TEST(FiberSemaphore, ProducerConsumer) {
    FiberSemaphore items(0);   // Items available
    FiberSemaphore spaces(10); // Buffer spaces

    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    const int kTotal = 100;

    std::thread producer([&]() {
        for (int i = 0; i < kTotal; ++i) {
            spaces.acquire();
            produced.fetch_add(1);
            items.release();
        }
    });

    std::thread consumer([&]() {
        for (int i = 0; i < kTotal; ++i) {
            items.acquire();
            consumed.fetch_add(1);
            spaces.release();
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(produced.load(), kTotal);
    EXPECT_EQ(consumed.load(), kTotal);
    EXPECT_EQ(items.available(), 0);
    EXPECT_EQ(spaces.available(), 10);
}

// ============================================================
// FiberSharedMutex with threads as writer and readers
// ============================================================

TEST(FiberSharedMutex, WriterAndReaders) {
    FiberSharedMutex mtx;
    std::atomic<int> shared_data{0};
    std::atomic<int> reads{0};
    std::atomic<int> writes{0};
    std::atomic<bool> done{false};

    std::thread writer([&]() {
        for (int i = 0; i < 50; ++i) {
            FiberExclusiveLockGuard guard(mtx);
            shared_data.store(i);
            writes.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        done.store(true);
    });

    std::vector<std::thread> readers;
    for (int r = 0; r < 4; ++r) {
        readers.emplace_back([&]() {
            while (!done.load()) {
                FiberSharedLockGuard guard(mtx);
                int val = shared_data.load();
                EXPECT_GE(val, 0);
                EXPECT_LE(val, 50);
                reads.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    writer.join();
    for (auto& t : readers) t.join();

    EXPECT_EQ(writes.load(), 50);
    EXPECT_GT(reads.load(), 0);
}
