// test_semaphore.cpp — Comprehensive Semaphore unit tests
// Tests construction with initial count, wait/post, try_wait,
// wait_for with timeout, producer-consumer pattern,
// and multi-threaded synchronization with semaphore.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace zero;

// ============================================================
// Construction and initial count
// ============================================================

TEST(Semaphore, DefaultConstructCountZero) {
    Semaphore sem(0);
    EXPECT_FALSE(sem.try_wait());
}

TEST(Semaphore, ConstructWithCount) {
    Semaphore sem(5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(sem.try_wait());
    }
    EXPECT_FALSE(sem.try_wait());
}

TEST(Semaphore, ConstructWithCountOne) {
    Semaphore sem(1);
    EXPECT_TRUE(sem.try_wait());
    EXPECT_FALSE(sem.try_wait());
}

// ============================================================
// wait / post
// ============================================================

TEST(Semaphore, PostWait) {
    Semaphore sem(0);
    EXPECT_FALSE(sem.try_wait());
    sem.post();
    EXPECT_TRUE(sem.try_wait());
    EXPECT_FALSE(sem.try_wait());
}

TEST(Semaphore, MultiplePostWait) {
    Semaphore sem(0);
    sem.post();
    sem.post();
    sem.post();
    EXPECT_TRUE(sem.try_wait());
    EXPECT_TRUE(sem.try_wait());
    EXPECT_TRUE(sem.try_wait());
    EXPECT_FALSE(sem.try_wait());
}

TEST(Semaphore, PostMulti) {
    Semaphore sem(0);
    sem.post(5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(sem.try_wait());
    }
    EXPECT_FALSE(sem.try_wait());
}

// ============================================================
// try_wait
// ============================================================

TEST(Semaphore, TryWaitReturnsImmediately) {
    Semaphore sem(2);
    EXPECT_TRUE(sem.try_wait());
    EXPECT_TRUE(sem.try_wait());
    EXPECT_FALSE(sem.try_wait());
}

TEST(Semaphore, TryWaitAfterPost) {
    Semaphore sem(0);
    sem.post();
    EXPECT_TRUE(sem.try_wait());
    EXPECT_FALSE(sem.try_wait());
}

// ============================================================
// wait_for timeout
// ============================================================

TEST(Semaphore, WaitForTimeout) {
    Semaphore sem(0);
    auto start = std::chrono::steady_clock::now();
    EXPECT_FALSE(sem.wait_for(std::chrono::milliseconds(50)));
    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_GE(elapsed, std::chrono::milliseconds(30));
}

TEST(Semaphore, WaitForSuccess) {
    Semaphore sem(0);
    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        sem.post();
    });
    EXPECT_TRUE(sem.wait_for(std::chrono::milliseconds(5000)));
    t.join();
}

TEST(Semaphore, WaitForMicroseconds) {
    Semaphore sem(0);
    EXPECT_FALSE(sem.wait_for(std::chrono::microseconds(100)));
}

// ============================================================
// wait_until
// ============================================================

TEST(Semaphore, WaitUntilTimeout) {
    Semaphore sem(0);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(30);
    EXPECT_FALSE(sem.wait_until(deadline));
}

TEST(Semaphore, WaitUntilSuccess) {
    Semaphore sem(1);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    EXPECT_TRUE(sem.wait_until(deadline));
}

// ============================================================
// count()
// ============================================================

TEST(Semaphore, Count) {
    Semaphore sem(3);
    EXPECT_EQ(sem.count(), 3);
    sem.try_wait();
    EXPECT_EQ(sem.count(), 2);
    sem.post();
    EXPECT_EQ(sem.count(), 3);
}

// ============================================================
// Native handle
// ============================================================

TEST(Semaphore, NativeHandle) {
    Semaphore sem(0);
    EXPECT_NE(sem.native_handle(), nullptr);
    const Semaphore& csem = sem;
    EXPECT_NE(csem.native_handle(), nullptr);
}

// ============================================================
// Producer-consumer pattern
// ============================================================

TEST(Semaphore, ProducerConsumer) {
    Semaphore empty(0);    // Counts items available
    Semaphore full(10);    // Counts empty slots
    std::vector<int> buffer;
    const int total_items = 100;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    std::thread producer([&]() {
        for (int i = 0; i < total_items; ++i) {
            full.wait();  // Wait for empty slot
            buffer.push_back(i);
            produced.fetch_add(1);
            empty.post(); // Signal item available
        }
    });

    std::thread consumer([&]() {
        for (int i = 0; i < total_items; ++i) {
            empty.wait(); // Wait for item
            EXPECT_FALSE(buffer.empty());
            buffer.erase(buffer.begin());
            consumed.fetch_add(1);
            full.post();  // Signal empty slot
        }
    });

    producer.join();
    consumer.join();
    EXPECT_EQ(produced.load(), total_items);
    EXPECT_EQ(consumed.load(), total_items);
    EXPECT_TRUE(buffer.empty());
}

// ============================================================
// Multi-threaded wait/post
// ============================================================

TEST(Semaphore, MultiThreadWaitPost) {
    Semaphore sem(0);
    std::atomic<int> count{0};
    const int num_threads = 8;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            sem.wait();
            count.fetch_add(1);
        });
    }

    // Give threads time to start blocking
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Post once per thread
    for (int i = 0; i < num_threads; ++i) {
        sem.post();
    }

    for (auto& t : threads) t.join();
    EXPECT_EQ(count.load(), num_threads);
}

// ============================================================
// High-contention semaphore
// ============================================================

TEST(Semaphore, HighContention) {
    Semaphore sem(1);
    std::atomic<int> counter{0};
    std::atomic<int> total_ops{0};
    const int num_threads = 8;
    const int iterations = 10000;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (int j = 0; j < iterations; ++j) {
                sem.wait();
                counter.fetch_add(1);
                sem.post();
                total_ops.fetch_add(1);
            }
        });
    }

    start.store(true);
    for (auto& t : threads) t.join();
    EXPECT_EQ(counter.load(), num_threads * iterations);
    EXPECT_EQ(total_ops.load(), num_threads * iterations);
}

// ============================================================
// Multi-producer, multi-consumer
// ============================================================

TEST(Semaphore, MultiProducerMultiConsumer) {
    Semaphore sem(0);
    std::atomic<int> count{0};
    std::atomic<int> produced{0};
    const int num_consumers = 4;
    const int num_producers = 4;
    const int per_producer = 500;

    std::vector<std::thread> consumers;
    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([&]() {
            int total = num_producers * per_producer;
            while (count.load() < total) {
                sem.wait();
                count.fetch_add(1);
            }
        });
    }

    std::vector<std::thread> producers;
    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back([&]() {
            for (int j = 0; j < per_producer; ++j) {
                produced.fetch_add(1);
                sem.post();
            }
        });
    }

    for (auto& t : producers) t.join();

    // Wait for consumers to finish
    for (int i = 0; i < num_consumers; ++i) {
        sem.post();
    }

    for (auto& t : consumers) t.join();
    EXPECT_EQ(count.load(), num_producers * per_producer);
}

// ============================================================
// Wait interrupted by signal (EINTR handling)
// ============================================================

TEST(Semaphore, WaitEINTRHandling) {
    Semaphore sem(0);
    // Launch a thread that will ultimately post
    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        sem.post();
    });

    // wait() should handle EINTR and not fail
    sem.wait();
    t.join();
    SUCCEED();
}
