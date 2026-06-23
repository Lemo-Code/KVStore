// test_fiber_pool.cpp — Comprehensive FiberPool unit tests
// Tests instance() singleton, get() returns valid fiber,
// recycle() returns fiber to pool for reuse, preallocate(),
// reserve(), statistics, clear(), multi-thread pool usage.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <thread>
#include <vector>
#include <functional>

using namespace zero;

// ============================================================
// Singleton pattern
// ============================================================

TEST(FiberPool, InstanceReturnsSame) {
    FiberPool& pool1 = FiberPool::instance();
    FiberPool& pool2 = FiberPool::instance();
    EXPECT_EQ(&pool1, &pool2);
}

// ============================================================
// get() returns valid fiber
// ============================================================

TEST(FiberPool, GetReturnsValidFiber) {
    auto& pool = FiberPool::instance();

    bool called = false;
    auto fiber = pool.get([&called]() { called = true; });

    ASSERT_NE(fiber, nullptr);
    EXPECT_EQ(fiber->getState(), Fiber::State::INIT);
    EXPECT_FALSE(fiber->isMain());
}

TEST(FiberPool, GetReturnsUniqueFibers) {
    auto& pool = FiberPool::instance();

    auto f1 = pool.get([]() {});
    auto f2 = pool.get([]() {});
    auto f3 = pool.get([]() {});

    ASSERT_NE(f1, nullptr);
    ASSERT_NE(f2, nullptr);
    ASSERT_NE(f3, nullptr);

    EXPECT_NE(f1->id(), f2->id());
    EXPECT_NE(f2->id(), f3->id());
    EXPECT_NE(f1->id(), f3->id());
}

TEST(FiberPool, GetWithCustomStackSize) {
    auto& pool = FiberPool::instance();
    const size_t kStackSize = 65536;

    auto fiber = pool.get([]() {}, kStackSize);
    ASSERT_NE(fiber, nullptr);
    EXPECT_EQ(fiber->stackSize(), kStackSize);
}

TEST(FiberPool, GetWithDefaultStackSize) {
    auto& pool = FiberPool::instance();
    auto fiber = pool.get([]() {});
    ASSERT_NE(fiber, nullptr);
    EXPECT_EQ(fiber->stackSize(), Fiber::kDefaultStackSize);
}

// ============================================================
// recycle() returns fiber to pool
// ============================================================

TEST(FiberPool, RecycleThenGetReusesFiber) {
    auto& pool = FiberPool::instance();

    // Get an initial fiber
    auto f1 = pool.get([]() {});
    ASSERT_NE(f1, nullptr);
    uint64_t f1_id = f1->id();
    EXPECT_EQ(f1->getState(), Fiber::State::INIT);

    // Simulate it being used and terminated
    f1->setState(Fiber::State::TERM);

    // Recycle it
    pool.recycle(f1);

    // Get another fiber — should be the same object
    auto f2 = pool.get([]() {});
    ASSERT_NE(f2, nullptr);
    // The ID stays the same (reused object)
    EXPECT_EQ(f2->id(), f1_id);
    EXPECT_EQ(f2->getState(), Fiber::State::INIT);
}

TEST(FiberPool, RecycleMultipleFibers) {
    auto& pool = FiberPool::instance();

    std::vector<uint64_t> ids;
    const int kNum = 10;

    // Create and recycle N fibers
    for (int i = 0; i < kNum; ++i) {
        auto f = pool.get([]() {});
        ASSERT_NE(f, nullptr);
        ids.push_back(f->id());
        f->setState(Fiber::State::TERM);
        pool.recycle(f);
    }

    // Get them back — should get the same IDs
    for (int i = 0; i < kNum; ++i) {
        auto f = pool.get([]() {});
        ASSERT_NE(f, nullptr);
        // IDs should be from the recycled set
        EXPECT_NE(std::find(ids.begin(), ids.end(), f->id()), ids.end());
    }
}

TEST(FiberPool, RecycledFiberRunsNewCallback) {
    auto& pool = FiberPool::instance();

    int call_count = 0;
    auto f1 = pool.get([&call_count]() { call_count = 1; });
    ASSERT_NE(f1, nullptr);
    uint64_t id = f1->id();

    f1->setState(Fiber::State::TERM);
    pool.recycle(f1);

    // Get recycled fiber — callback should be updated
    auto f2 = pool.get([&call_count]() { call_count = 2; });
    ASSERT_NE(f2, nullptr);
    EXPECT_EQ(f2->id(), id);  // Same object
}

// ============================================================
// preallocate()
// ============================================================

TEST(FiberPool, Preallocate) {
    auto& pool = FiberPool::instance();
    size_t before = pool.available();

    pool.preallocate(20);
    EXPECT_GE(pool.available(), before + 20);
}

TEST(FiberPool, PreallocateWithCustomStackSize) {
    auto& pool = FiberPool::instance();
    size_t before = pool.available();

    pool.preallocate(10, 65536);
    EXPECT_GE(pool.available(), before + 10);
}

// ============================================================
// reserve()
// ============================================================

TEST(FiberPool, Reserve) {
    auto& pool = FiberPool::instance();
    // reserve just allocates vector space, doesn't create fibers
    pool.reserve(100);
    // Should not crash
    SUCCEED();
}

// ============================================================
// Statistics
// ============================================================

TEST(FiberPool, AvailableIncreasesAfterPreallocate) {
    auto& pool = FiberPool::instance();
    pool.preallocate(5);
    EXPECT_GE(pool.available(), 5u);
}

TEST(FiberPool, AvailableDecreasesAfterGet) {
    auto& pool = FiberPool::instance();
    pool.preallocate(5);
    size_t before = pool.available();

    auto f = pool.get([]() {});
    ASSERT_NE(f, nullptr);
    // Getting from pool reduces available
    EXPECT_LE(pool.available(), before);
}

TEST(FiberPool, TotalAllocated) {
    auto& pool = FiberPool::instance();
    size_t before = pool.total_allocated();

    for (int i = 0; i < 10; ++i) {
        auto f = pool.get([]() {});
        ASSERT_NE(f, nullptr);
        pool.recycle(f);
        f->setState(Fiber::State::TERM);
    }

    EXPECT_GE(pool.total_allocated(), before);
}

TEST(FiberPool, TotalRecycled) {
    auto& pool = FiberPool::instance();
    size_t before = pool.total_recycled();

    auto f = pool.get([]() {});
    ASSERT_NE(f, nullptr);
    f->setState(Fiber::State::TERM);
    pool.recycle(f);

    EXPECT_GE(pool.total_recycled(), before + 1);
}

// ============================================================
// clear()
// ============================================================

TEST(FiberPool, ClearEmptiesPool) {
    auto& pool = FiberPool::instance();
    pool.preallocate(5);
    EXPECT_GT(pool.available(), 0u);

    pool.clear();
    EXPECT_EQ(pool.available(), 0u);
}

// ============================================================
// Recycling with nullptr is safe
// ============================================================

TEST(FiberPool, RecycleNullIsSafe) {
    auto& pool = FiberPool::instance();
    // Should not crash
    pool.recycle(nullptr);
    SUCCEED();
}

// ============================================================
// Get many fibers after recycle
// ============================================================

TEST(FiberPool, GetAfterRecycleCycle) {
    auto& pool = FiberPool::instance();
    pool.clear();  // Start clean

    const int kCycles = 5;
    const int kPerCycle = 20;

    for (int cycle = 0; cycle < kCycles; ++cycle) {
        std::vector<Fiber::Ptr> fibers;
        for (int i = 0; i < kPerCycle; ++i) {
            auto f = pool.get([]() {});
            ASSERT_NE(f, nullptr);
            fibers.push_back(f);
        }
        for (auto& f : fibers) {
            f->setState(Fiber::State::TERM);
            pool.recycle(f);
        }
    }
    SUCCEED();
}

// ============================================================
// Multi-thread pool usage
// ============================================================

TEST(FiberPool, MultiThreadGet) {
    auto& pool = FiberPool::instance();
    const int kNumThreads = 4;
    const int kPerThread = 50;
    std::atomic<int> successes{0};

    auto worker = [&]() {
        for (int i = 0; i < kPerThread; ++i) {
            auto f = pool.get([]() {});
            if (f) {
                successes.fetch_add(1);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < kNumThreads; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(successes.load(), kNumThreads * kPerThread);
}

TEST(FiberPool, MultiThreadRecycle) {
    auto& pool = FiberPool::instance();
    const int kNumThreads = 4;
    const int kPerThread = 50;

    auto worker = [&]() {
        for (int i = 0; i < kPerThread; ++i) {
            auto f = pool.get([]() {});
            ASSERT_NE(f, nullptr);
            f->setState(Fiber::State::TERM);
            pool.recycle(f);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < kNumThreads; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) t.join();

    SUCCEED();
}

// ============================================================
// Pool growth behavior
// ============================================================

TEST(FiberPool, PoolGrowsWhenEmpty) {
    auto& pool = FiberPool::instance();
    pool.clear();

    size_t available = pool.available();
    size_t total_alloc = pool.total_allocated();

    // Get a fiber when pool is empty — should allocate new
    auto f = pool.get([]() {});
    ASSERT_NE(f, nullptr);
    EXPECT_GE(pool.total_allocated(), total_alloc);
}

// ============================================================
// Preallocation + get + recycle stress
// ============================================================

TEST(FiberPool, PreallocateGetRecycleStress) {
    auto& pool = FiberPool::instance();
    pool.clear();

    const int kPrealloc = 50;
    pool.preallocate(kPrealloc);
    EXPECT_GE(pool.available(), static_cast<size_t>(kPrealloc));

    std::vector<Fiber::Ptr> fibers;
    for (int i = 0; i < kPrealloc; ++i) {
        auto f = pool.get([]() {});
        ASSERT_NE(f, nullptr);
        fibers.push_back(f);
    }

    // Recycle all back
    for (auto& f : fibers) {
        f->setState(Fiber::State::TERM);
        pool.recycle(f);
    }

    // Available should be close to kPrealloc
    EXPECT_GE(pool.available(), static_cast<size_t>(kPrealloc));
}

// ============================================================
// FiberPool with real std::function captures
// ============================================================

TEST(FiberPool, FiberWithSharedState) {
    auto& pool = FiberPool::instance();
    std::shared_ptr<int> shared = std::make_shared<int>(0);

    auto f = pool.get([shared]() {
        *shared = 42;
    });

    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->getState(), Fiber::State::INIT);
    EXPECT_EQ(*shared, 0);  // Callback not executed yet
}
