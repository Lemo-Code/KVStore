// test_work_stealing_queue.cpp — Unit tests for the Chase-Lev lock-free
// work-stealing deque.
//
// Tests: push/pop from owner (LIFO), steal from thief (FIFO), empty queue
// pop/steal returns null, full queue, concurrent push/pop/steal,
// size/empty queries.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <thread>
#include <vector>

using namespace zero;

// Helper: create a simple fiber (we just need Fiber::Ptr objects for the queue)
static Fiber::Ptr make_dummy_fiber() {
    return Fiber::Create([]() {});
}

// =====================================================================
// Construction tests
// =====================================================================

TEST(WorkStealingQueueTest, Construct) {
    WorkStealingQueue q(64);
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
    EXPECT_EQ(q.capacity(), 64u);
}

TEST(WorkStealingQueueTest, ConstructWithVariousCapacities) {
    for (size_t cap : {2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 1024u}) {
        WorkStealingQueue q(cap);
        EXPECT_EQ(q.capacity(), cap);
        EXPECT_TRUE(q.empty());
        EXPECT_EQ(q.size(), 0u);
    }
}

// =====================================================================
// Push/Pop (owner LIFO) tests
// =====================================================================

TEST(WorkStealingQueueTest, PushThenPopSingle) {
    WorkStealingQueue q(64);
    auto f1 = make_dummy_fiber();

    q.push(f1);
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(q.size(), 1u);

    auto popped = q.pop();
    EXPECT_EQ(popped, f1);
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
}

TEST(WorkStealingQueueTest, PushThenPop_Multiple_LIFO) {
    WorkStealingQueue q(64);
    auto f1 = make_dummy_fiber();
    auto f2 = make_dummy_fiber();
    auto f3 = make_dummy_fiber();

    q.push(f1);
    q.push(f2);
    q.push(f3);

    EXPECT_EQ(q.size(), 3u);

    // LIFO: last pushed is first popped
    EXPECT_EQ(q.pop(), f3);
    EXPECT_EQ(q.pop(), f2);
    EXPECT_EQ(q.pop(), f1);
    EXPECT_TRUE(q.empty());
}

TEST(WorkStealingQueueTest, PushUntilFull) {
    WorkStealingQueue q(8);

    for (int i = 0; i < 8; ++i) {
        auto f = make_dummy_fiber();
        q.push(f);
    }

    EXPECT_EQ(q.size(), 8u);
    EXPECT_FALSE(q.empty());

    // Pop all back
    for (int i = 0; i < 8; ++i) {
        auto f = q.pop();
        EXPECT_NE(f, nullptr);
    }

    EXPECT_TRUE(q.empty());
}

// =====================================================================
// Empty queue tests
// =====================================================================

TEST(WorkStealingQueueTest, PopFromEmptyReturnsNull) {
    WorkStealingQueue q(64);
    EXPECT_EQ(q.pop(), nullptr);
    EXPECT_TRUE(q.empty());
}

TEST(WorkStealingQueueTest, StealFromEmptyReturnsNull) {
    WorkStealingQueue q(64);
    EXPECT_EQ(q.steal(), nullptr);
    EXPECT_TRUE(q.empty());
}

TEST(WorkStealingQueueTest, PopUntilEmpty) {
    WorkStealingQueue q(8);

    auto f1 = make_dummy_fiber();
    auto f2 = make_dummy_fiber();
    q.push(f1);
    q.push(f2);

    EXPECT_EQ(q.pop(), f2);
    EXPECT_EQ(q.pop(), f1);
    EXPECT_EQ(q.pop(), nullptr);
    EXPECT_TRUE(q.empty());
}

// =====================================================================
// Steal (thief FIFO) tests
// =====================================================================

TEST(WorkStealingQueueTest, StealFromSingleElement) {
    WorkStealingQueue q(64);
    auto f1 = make_dummy_fiber();

    q.push(f1);
    EXPECT_EQ(q.size(), 1u);

    auto stolen = q.steal();
    EXPECT_EQ(stolen, f1);
    EXPECT_TRUE(q.empty());
}

TEST(WorkStealingQueueTest, StealOrderIsFIFO) {
    WorkStealingQueue q(64);
    auto f1 = make_dummy_fiber();
    auto f2 = make_dummy_fiber();
    auto f3 = make_dummy_fiber();

    q.push(f1);
    q.push(f2);
    q.push(f3);

    // Steal takes from the top (FIFO): first pushed is first stolen
    EXPECT_EQ(q.steal(), f1);
    EXPECT_EQ(q.steal(), f2);
    EXPECT_EQ(q.steal(), f3);
    EXPECT_TRUE(q.empty());
}

TEST(WorkStealingQueueTest, MixPopAndSteal) {
    WorkStealingQueue q(64);
    auto f1 = make_dummy_fiber();
    auto f2 = make_dummy_fiber();
    auto f3 = make_dummy_fiber();
    auto f4 = make_dummy_fiber();

    q.push(f1);
    q.push(f2);
    q.push(f3);
    q.push(f4);

    // Owner pops one (LIFO) — gets f4
    EXPECT_EQ(q.pop(), f4);

    // Thief steals (FIFO) — gets f1
    EXPECT_EQ(q.steal(), f1);

    // Owner pops again — gets f3
    EXPECT_EQ(q.pop(), f3);

    // Thief steals — gets f2
    EXPECT_EQ(q.steal(), f2);

    EXPECT_TRUE(q.empty());
}

// =====================================================================
// Size and empty queries
// =====================================================================

TEST(WorkStealingQueueTest, SizeReflectsPushPop) {
    WorkStealingQueue q(64);

    EXPECT_EQ(q.size(), 0u);

    q.push(make_dummy_fiber());
    EXPECT_EQ(q.size(), 1u);

    q.push(make_dummy_fiber());
    EXPECT_EQ(q.size(), 2u);

    q.pop();
    EXPECT_EQ(q.size(), 1u);

    q.pop();
    EXPECT_EQ(q.size(), 0u);
}

TEST(WorkStealingQueueTest, EmptyQueryAfterEachOp) {
    WorkStealingQueue q(8);

    EXPECT_TRUE(q.empty());

    q.push(make_dummy_fiber());
    EXPECT_FALSE(q.empty());

    q.push(make_dummy_fiber());
    EXPECT_FALSE(q.empty());

    q.pop();
    EXPECT_FALSE(q.empty());

    q.pop();
    EXPECT_TRUE(q.empty());
}

// =====================================================================
// Concurrent push/pop/steal tests
// =====================================================================

TEST(WorkStealingQueueTest, ConcurrentSteal) {
    const size_t kCapacity = 1024;
    WorkStealingQueue q(kCapacity);

    // Owner pushes many fibers
    const int kNumFibers = 500;
    for (int i = 0; i < kNumFibers; ++i) {
        q.push(make_dummy_fiber());
    }

    EXPECT_EQ(q.size(), static_cast<size_t>(kNumFibers));

    // Thief thread steals half
    std::atomic<int> stolen_count{0};
    std::thread thief([&q, &stolen_count]() {
        for (int i = 0; i < 250; ++i) {
            auto f = q.steal();
            if (f) {
                stolen_count.fetch_add(1);
            }
        }
    });

    // Owner pops the rest
    int popped = 0;
    for (int i = 0; i < 250; ++i) {
        auto f = q.pop();
        if (f) {
            ++popped;
        }
    }

    thief.join();

    EXPECT_GE(stolen_count.load() + popped, 400);
    EXPECT_EQ(q.size(), static_cast<size_t>(kNumFibers - stolen_count.load() - popped));
}

TEST(WorkStealingQueueTest, MultipleThieves) {
    const size_t kCapacity = 1024;
    WorkStealingQueue q(kCapacity);

    // Owner fills
    const int kNumFibers = 600;
    for (int i = 0; i < kNumFibers; ++i) {
        q.push(make_dummy_fiber());
    }

    std::atomic<int> stolen{0};
    std::atomic<bool> start{false};

    auto thief_fn = [&q, &stolen, &start]() {
        while (!start.load()) {
            std::this_thread::yield();
        }
        for (int i = 0; i < 200; ++i) {
            auto f = q.steal();
            if (f) {
                stolen.fetch_add(1);
            }
        }
    };

    std::thread t1(thief_fn);
    std::thread t2(thief_fn);
    std::thread t3(thief_fn);

    start.store(true);

    t1.join();
    t2.join();
    t3.join();

    // Remaining should be pop-able
    size_t remaining = q.size();
    for (size_t i = 0; i < remaining; ++i) {
        auto f = q.pop();
        if (f) {
            stolen.fetch_add(1);
        }
    }

    EXPECT_EQ(stolen.load(), kNumFibers);
    EXPECT_TRUE(q.empty());
}

// =====================================================================
// Stress tests
// =====================================================================

TEST(WorkStealingQueueTest, PushPopAlternating) {
    WorkStealingQueue q(64);

    for (int round = 0; round < 100; ++round) {
        for (int i = 0; i < 4; ++i) {
            q.push(make_dummy_fiber());
        }
        for (int i = 0; i < 4; ++i) {
            EXPECT_NE(q.pop(), nullptr);
        }
        EXPECT_TRUE(q.empty());
    }
}

TEST(WorkStealingQueueTest, CapacityNeverExceeded) {
    WorkStealingQueue q(16);

    for (int round = 0; round < 50; ++round) {
        for (int i = 0; i < 16; ++i) {
            q.push(make_dummy_fiber());
        }
        EXPECT_EQ(q.size(), 16u);

        for (int i = 0; i < 16; ++i) {
            q.pop();
        }
        EXPECT_TRUE(q.empty());
    }
}
