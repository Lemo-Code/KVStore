// test_rwlock.cpp — Comprehensive RWLock unit tests
// Tests lock_shared/unlock_shared, lock/unlock (exclusive),
// try_lock_shared/try_lock, ReadLockGuard/WriteLockGuard RAII,
// UpgradeLockGuard (read-to-write upgrade/downgrade),
// multiple concurrent readers, and writer exclusivity.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <thread>
#include <vector>
#include <chrono>

using namespace zero;

// ============================================================
// Basic exclusive (write) lock
// ============================================================

TEST(RWLock, ExclusiveLockUnlock) {
    RWLock rwlock;
    rwlock.lock();
    rwlock.unlock();
    SUCCEED();
}

TEST(RWLock, ExclusiveTryLockSuccess) {
    RWLock rwlock;
    EXPECT_TRUE(rwlock.try_lock());
    rwlock.unlock();
}

TEST(RWLock, ExclusiveTryLockFailsWhenLocked) {
    RWLock rwlock;
    rwlock.lock();
    EXPECT_FALSE(rwlock.try_lock());
    rwlock.unlock();
}

TEST(RWLock, ExclusiveLockBlocksSharedLock) {
    RWLock rwlock;
    rwlock.lock();
    EXPECT_FALSE(rwlock.try_lock_shared());
    rwlock.unlock();
}

// ============================================================
// Shared (read) lock
// ============================================================

TEST(RWLock, SharedLockUnlock) {
    RWLock rwlock;
    rwlock.lock_shared();
    rwlock.unlock_shared();
    SUCCEED();
}

TEST(RWLock, SharedTryLockSuccess) {
    RWLock rwlock;
    EXPECT_TRUE(rwlock.try_lock_shared());
    rwlock.unlock_shared();
}

// ============================================================
// Multiple concurrent readers
// ============================================================

TEST(RWLock, MultipleConcurrentReaders) {
    RWLock rwlock;
    rwlock.lock_shared();
    // While one reader holds shared lock, another reader can also acquire
    EXPECT_TRUE(rwlock.try_lock_shared());
    rwlock.unlock_shared();
    rwlock.unlock_shared();
}

TEST(RWLock, ReadersBlockWriter) {
    RWLock rwlock;
    rwlock.lock_shared();
    EXPECT_FALSE(rwlock.try_lock());
    rwlock.unlock_shared();
}

TEST(RWLock, WriterBlocksReaders) {
    RWLock rwlock;
    rwlock.lock();
    EXPECT_FALSE(rwlock.try_lock_shared());
    rwlock.unlock();
}

// ============================================================
// Write unlock allows readers
// ============================================================

TEST(RWLock, WriteUnlockAllowsReaders) {
    RWLock rwlock;
    rwlock.lock();
    rwlock.unlock();
    EXPECT_TRUE(rwlock.try_lock_shared());
    rwlock.unlock_shared();
}

// ============================================================
// ReadLockGuard RAII
// ============================================================

TEST(RWLock, ReadLockGuard) {
    RWLock rwlock;
    {
        ReadLockGuard guard(rwlock);
        // Another reader can still acquire
        EXPECT_TRUE(rwlock.try_lock_shared());
        rwlock.unlock_shared();
        // Writer can't acquire
        EXPECT_FALSE(rwlock.try_lock());
    }
    // Lock released
    EXPECT_TRUE(rwlock.try_lock());
    rwlock.unlock();
}

// ============================================================
// WriteLockGuard RAII
// ============================================================

TEST(RWLock, WriteLockGuard) {
    RWLock rwlock;
    {
        WriteLockGuard guard(rwlock);
        // No readers allowed
        EXPECT_FALSE(rwlock.try_lock_shared());
        // No writers allowed
        EXPECT_FALSE(rwlock.try_lock());
    }
    // Lock released
    EXPECT_TRUE(rwlock.try_lock_shared());
    rwlock.unlock_shared();
}

// ============================================================
// UpgradeLockGuard — read-to-write upgrade/downgrade
// ============================================================

TEST(RWLock, UpgradeLockGuardStartsRead) {
    RWLock rwlock;
    UpgradeLockGuard guard(rwlock);
    EXPECT_FALSE(guard.is_write_locked());
    // Others can still read
    EXPECT_TRUE(rwlock.try_lock_shared());
    rwlock.unlock_shared();
}

TEST(RWLock, UpgradeLockGuardUpgradeDowngrade) {
    RWLock rwlock;
    {
        UpgradeLockGuard guard(rwlock);
        EXPECT_FALSE(guard.is_write_locked());

        guard.upgrade();
        EXPECT_TRUE(guard.is_write_locked());
        // Now exclusive — no readers or writers
        EXPECT_FALSE(rwlock.try_lock_shared());
        EXPECT_FALSE(rwlock.try_lock());

        guard.downgrade();
        EXPECT_FALSE(guard.is_write_locked());
        // Back to shared — readers allowed
        EXPECT_TRUE(rwlock.try_lock_shared());
        rwlock.unlock_shared();
    }
    // Fully released
    EXPECT_TRUE(rwlock.try_lock());
    rwlock.unlock();
}

TEST(RWLock, UpgradeLockGuardMultipleUpgrades) {
    RWLock rwlock;
    {
        UpgradeLockGuard guard(rwlock);
        // Upgrade, downgrade, upgrade again
        guard.upgrade();
        EXPECT_TRUE(guard.is_write_locked());
        guard.downgrade();
        EXPECT_FALSE(guard.is_write_locked());
        guard.upgrade();
        EXPECT_TRUE(guard.is_write_locked());
    }
    SUCCEED();
}

// ============================================================
// Native handle
// ============================================================

TEST(RWLock, NativeHandle) {
    RWLock rwlock;
    EXPECT_NE(rwlock.native_handle(), nullptr);
    const RWLock& crwlock = rwlock;
    EXPECT_NE(crwlock.native_handle(), nullptr);
}

// ============================================================
// Multi-threaded concurrent readers
// ============================================================

TEST(RWLock, MultiThreadConcurrentReaders) {
    RWLock rwlock;
    std::atomic<int> readers_inside{0};
    std::atomic<int> max_concurrent{0};
    const int num_readers = 8;
    const int per_reader = 10000;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int i = 0; i < num_readers; ++i) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (int j = 0; j < per_reader; ++j) {
                ReadLockGuard guard(rwlock);
                int current = readers_inside.fetch_add(1) + 1;
                int old = max_concurrent.load();
                while (current > old &&
                       !max_concurrent.compare_exchange_weak(old, current)) {}
                readers_inside.fetch_sub(1);
            }
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();
    EXPECT_GT(max_concurrent.load(), 1);
}

// ============================================================
// Multi-threaded writer exclusive
// ============================================================

TEST(RWLock, MultiThreadWriterExclusive) {
    RWLock rwlock;
    int counter = 0;
    const int num_writers = 4;
    const int per_writer = 50000;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int i = 0; i < num_writers; ++i) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (int j = 0; j < per_writer; ++j) {
                WriteLockGuard guard(rwlock);
                ++counter;
            }
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();
    EXPECT_EQ(counter, num_writers * per_writer);
}

// ============================================================
// Mixed readers and writers
// ============================================================

TEST(RWLock, MixedReadersAndWriters) {
    RWLock rwlock;
    std::atomic<int> reads{0};
    std::atomic<int> writes{0};
    const int writer_iterations = 1000;
    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};

    std::vector<std::thread> readers;
    for (int i = 0; i < 8; ++i) {
        readers.emplace_back([&]() {
            while (!start.load()) {}
            while (!stop.load()) {
                ReadLockGuard guard(rwlock);
                reads.fetch_add(1);
            }
        });
    }

    std::thread writer([&]() {
        while (!start.load()) {}
        for (int i = 0; i < writer_iterations; ++i) {
            WriteLockGuard guard(rwlock);
            writes.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    start.store(true);
    writer.join();
    stop.store(true);
    for (auto& th : readers) th.join();

    EXPECT_EQ(writes.load(), writer_iterations);
    EXPECT_GT(reads.load(), 0);
}

// ============================================================
// RWLock read-write alternating
// ============================================================

TEST(RWLock, ReadWriteAlternating) {
    RWLock rwlock;
    std::atomic<int> value{0};
    std::atomic<int> read_count{0};
    std::atomic<int> write_count{0};
    const int num_ops = 5000;
    std::atomic<bool> start{false};

    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&]() {
            while (!start.load()) {}
            for (int j = 0; j < num_ops; ++j) {
                ReadLockGuard guard(rwlock);
                (void)value.load();
                read_count.fetch_add(1);
            }
        });
    }

    std::thread writer([&]() {
        while (!start.load()) {}
        for (int i = 0; i < num_ops; ++i) {
            WriteLockGuard guard(rwlock);
            value.store(i);
            write_count.fetch_add(1);
        }
    });

    start.store(true);
    for (auto& th : readers) th.join();
    writer.join();
    EXPECT_GE(read_count.load(), 4 * num_ops - 100);
    EXPECT_EQ(write_count.load(), num_ops);
}
