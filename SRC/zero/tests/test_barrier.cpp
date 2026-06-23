// test_barrier.cpp — Comprehensive Barrier unit tests
// Tests Barrier with N threads, wait blocks until all arrive,
// multiple phases, repeated use, and with 2/4/8 threads.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <thread>
#include <vector>

using namespace zero;

// ============================================================
// Basic synchronization
// ============================================================

TEST(Barrier, TwoThreads) {
    Barrier barrier(2);
    std::atomic<int> phase{0};

    std::thread t([&]() {
        phase.store(1);
        barrier.wait();
        EXPECT_EQ(phase.load(), 1);
    });

    barrier.wait();
    EXPECT_EQ(phase.load(), 1);
    t.join();
}

TEST(Barrier, ThreeThreads) {
    Barrier barrier(3);
    std::atomic<int> arrived{0};

    auto worker = [&]() {
        arrived.fetch_add(1);
        barrier.wait();
        EXPECT_EQ(arrived.load(), 3);
    };

    std::thread t1(worker), t2(worker);
    worker();
    t1.join();
    t2.join();
}

// ============================================================
// Multi-thread barrier with 4/8 threads
// ============================================================

TEST(Barrier, FourThreads) {
    Barrier barrier(4);
    std::atomic<int> arrived{0};

    auto worker = [&]() {
        arrived.fetch_add(1);
        barrier.wait();
        EXPECT_EQ(arrived.load(), 4);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back(worker);
    }
    worker();
    for (auto& t : threads) t.join();
}

TEST(Barrier, EightThreads) {
    Barrier barrier(8);
    std::atomic<int> arrived{0};

    auto worker = [&]() {
        arrived.fetch_add(1);
        barrier.wait();
        EXPECT_EQ(arrived.load(), 8);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 7; ++i) {
        threads.emplace_back(worker);
    }
    worker();
    for (auto& t : threads) t.join();
}

// ============================================================
// wait() return value (leader thread)
// ============================================================

TEST(Barrier, LeaderThreadReturnsTrue) {
    Barrier barrier(3);
    std::atomic<int> leader_count{0};

    auto worker = [&]() {
        if (barrier.wait()) {
            leader_count.fetch_add(1);
        }
    };

    std::thread t1(worker), t2(worker);
    worker();
    t1.join();
    t2.join();

    // Exactly one thread should be the leader
    EXPECT_EQ(leader_count.load(), 1);
}

// ============================================================
// Repeated use (multiple phases)
// ============================================================

TEST(Barrier, RepeatedUse) {
    Barrier barrier(2);
    for (int i = 0; i < 20; ++i) {
        std::thread t([&]() { barrier.wait(); });
        barrier.wait();
        t.join();
    }
    SUCCEED();
}

TEST(Barrier, MultiplePhases) {
    const int num_threads = 4;
    const int phases = 10;
    Barrier barrier(num_threads);
    std::atomic<int> phase{0};
    std::vector<std::atomic<int>> counts(phases);

    auto worker = [&]() {
        for (int i = 0; i < phases; ++i) {
            barrier.wait();
            counts[i].fetch_add(1);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) t.join();

    for (int i = 0; i < phases; ++i) {
        EXPECT_EQ(counts[i].load(), num_threads);
    }
}

// ============================================================
// Many iterations
// ============================================================

TEST(Barrier, ManyIterations) {
    const int num_threads = 4;
    const int iterations = 500;
    Barrier barrier(num_threads);
    std::atomic<int> count{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < iterations; ++j) {
                barrier.wait();
                count.fetch_add(1);
            }
        });
    }
    for (auto& t : threads) t.join();
    EXPECT_EQ(count.load(), num_threads * iterations);
}

// ============================================================
// Full thread count with 16 threads
// ============================================================

TEST(Barrier, SixteenThreads) {
    const int N = 16;
    Barrier barrier(N);
    std::atomic<int> arrived{0};

    auto worker = [&]() {
        arrived.fetch_add(1);
        barrier.wait();
        EXPECT_EQ(arrived.load(), N);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < N - 1; ++i) {
        threads.emplace_back(worker);
    }
    worker();
    for (auto& t : threads) t.join();
}

// ============================================================
// count()
// ============================================================

TEST(Barrier, Count) {
    Barrier barrier(5);
    EXPECT_EQ(barrier.count(), 5u);
}

// ============================================================
// arrive_and_wait alias
// ============================================================

TEST(Barrier, ArriveAndWait) {
    Barrier barrier(2);
    std::thread t([&]() { barrier.arrive_and_wait(); });
    barrier.arrive_and_wait();
    t.join();
    SUCCEED();
}

// ============================================================
// Long-running stability
// ============================================================

TEST(Barrier, LongRunning) {
    const int num_threads = 4;
    const int iterations = 500;
    Barrier barrier(num_threads);
    std::atomic<int> count{0};

    auto worker = [&]() {
        for (int i = 0; i < iterations; ++i) {
            barrier.wait();        // Sync start
            count.fetch_add(1);
            barrier.wait();        // Sync end (all done counting)
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) t.join();
    EXPECT_EQ(count.load(), num_threads * iterations);
}
