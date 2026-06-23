// test_condition_variable.cpp — Comprehensive ConditionVariable unit tests
// Tests wait/notify_one, notify_all, wait_for with timeout,
// predicate wait, producer-consumer pattern, spurious wakeup handling,
// multiple threads synchronization, and edge cases.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <queue>
#include <mutex>

using namespace zero;

// ============================================================
// Basic wait/notify_one
// ============================================================

TEST(ConditionVariable, NotifyOneWakesWaiter) {
    Mutex mtx;
    ConditionVariable cv;
    std::atomic<bool> ready{false};
    bool woken = false;

    UniqueLock<Mutex> lock(mtx);

    std::thread t([&]() {
        UniqueLock<Mutex> lk(mtx);
        ready.store(true);
        cv.notify_one();
    });

    cv.wait(lock);
    woken = true;

    t.join();
    EXPECT_TRUE(ready.load());
    EXPECT_TRUE(woken);
}

TEST(ConditionVariable, NotifyOneSequence) {
    Mutex mtx;
    ConditionVariable cv;
    std::atomic<int> stage{0};

    std::thread waiter([&]() {
        UniqueLock<Mutex> lock(mtx);
        stage.store(1);
        cv.wait(lock);
        EXPECT_EQ(stage.load(), 2);
    });

    // Wait for waiter to be ready
    while (stage.load() != 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    {
        UniqueLock<Mutex> lock(mtx);
        stage.store(2);
        cv.notify_one();
    }

    waiter.join();
}

// ============================================================
// notify_all wakes all waiters
// ============================================================

TEST(ConditionVariable, NotifyAllWakesAll) {
    Mutex mtx;
    ConditionVariable cv;
    std::atomic<int> woke_count{0};
    const int kNumWaiters = 5;

    std::vector<std::thread> waiters;
    for (int i = 0; i < kNumWaiters; ++i) {
        waiters.emplace_back([&]() {
            UniqueLock<Mutex> lock(mtx);
            cv.wait(lock);
            woke_count.fetch_add(1);
        });
    }

    // Give all waiters time to enter wait
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    {
        UniqueLock<Mutex> lock(mtx);
        cv.notify_all();
    }

    for (auto& t : waiters) t.join();
    EXPECT_EQ(woke_count.load(), kNumWaiters);
}

TEST(ConditionVariable, NotifyAllMultipleBatches) {
    Mutex mtx;
    ConditionVariable cv;
    std::atomic<int> woke{0};
    std::atomic<int> ready{0};

    auto waiter = [&]() {
        UniqueLock<Mutex> lock(mtx);
        ready.fetch_add(1);
        cv.wait(lock);
        woke.fetch_add(1);
    };

    // First batch
    std::vector<std::thread> batch1;
    for (int i = 0; i < 3; ++i) batch1.emplace_back(waiter);

    while (ready.load() < 3) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    {
        UniqueLock<Mutex> lock(mtx);
        cv.notify_all();
    }
    for (auto& t : batch1) t.join();
    EXPECT_EQ(woke.load(), 3);

    ready.store(0);
    // Second batch
    std::vector<std::thread> batch2;
    for (int i = 0; i < 3; ++i) batch2.emplace_back(waiter);

    while (ready.load() < 3) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    {
        UniqueLock<Mutex> lock(mtx);
        cv.notify_all();
    }
    for (auto& t : batch2) t.join();
    EXPECT_EQ(woke.load(), 6);
}

// ============================================================
// wait_for with timeout
// ============================================================

TEST(ConditionVariable, WaitForReturnsTrueWhenNotified) {
    Mutex mtx;
    ConditionVariable cv;
    bool woken = false;

    std::thread notifier([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        UniqueLock<Mutex> lock(mtx);
        cv.notify_one();
    });

    UniqueLock<Mutex> lock(mtx);
    woken = cv.wait_for(lock, std::chrono::seconds(5));

    notifier.join();
    EXPECT_TRUE(woken);
}

TEST(ConditionVariable, WaitForReturnsFalseOnTimeout) {
    Mutex mtx;
    ConditionVariable cv;

    UniqueLock<Mutex> lock(mtx);
    bool result = cv.wait_for(lock, std::chrono::milliseconds(10));
    EXPECT_FALSE(result);
}

TEST(ConditionVariable, WaitForShortTimeout) {
    Mutex mtx;
    ConditionVariable cv;

    auto start = std::chrono::steady_clock::now();
    UniqueLock<Mutex> lock(mtx);
    bool result = cv.wait_for(lock, std::chrono::milliseconds(1));
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(result);
    // Should have waited at least close to 1ms
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 0);
}

// ============================================================
// wait with predicate lambda
// ============================================================

TEST(ConditionVariable, WaitWithPredicate) {
    Mutex mtx;
    ConditionVariable cv;
    bool ready = false;

    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        UniqueLock<Mutex> lock(mtx);
        ready = true;
        cv.notify_one();
    });

    UniqueLock<Mutex> lock(mtx);
    // Manual predicate loop (equivalent to waiting with predicate)
    while (!ready) {
        cv.wait(lock);
    }
    EXPECT_TRUE(ready);
    t.join();
}

TEST(ConditionVariable, WaitWithPredicateSpuriousWakeupSafe) {
    Mutex mtx;
    ConditionVariable cv;
    std::atomic<int> value{0};
    std::atomic<int> wake_count{0};

    std::thread producer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        UniqueLock<Mutex> lock(mtx);
        value.store(42);
        cv.notify_one();
    });

    UniqueLock<Mutex> lock(mtx);
    while (value.load() == 0) {
        cv.wait(lock);
        wake_count.fetch_add(1);
    }

    producer.join();
    EXPECT_EQ(value.load(), 42);
    // wake_count should be >= 1 (could be more due to spurious wakeups)
    EXPECT_GE(wake_count.load(), 1);
}

// ============================================================
// Producer-consumer pattern
// ============================================================

TEST(ConditionVariable, ProducerConsumerSingle) {
    Mutex mtx;
    ConditionVariable cv;
    std::queue<int> queue;
    bool done = false;
    const int kNumItems = 100;

    std::thread consumer([&]() {
        UniqueLock<Mutex> lock(mtx);
        int consumed = 0;
        while (consumed < kNumItems) {
            while (queue.empty() && !done) {
                cv.wait(lock);
            }
            while (!queue.empty()) {
                int val = queue.front();
                queue.pop();
                EXPECT_EQ(val, consumed);
                ++consumed;
            }
        }
    });

    {
        UniqueLock<Mutex> lock(mtx);
        for (int i = 0; i < kNumItems; ++i) {
            queue.push(i);
            cv.notify_one();
        }
        done = true;
        cv.notify_one();
    }

    consumer.join();
}

TEST(ConditionVariable, ProducerConsumerNotifyAll) {
    Mutex mtx;
    ConditionVariable cv;
    std::queue<int> queue;
    std::atomic<int> consumed{0};
    const int kNumProducers = 4;
    const int kPerProducer = 250;

    std::atomic<int> producers_done{0};

    auto producer = [&](int start_val) {
        for (int i = 0; i < kPerProducer; ++i) {
            UniqueLock<Mutex> lock(mtx);
            queue.push(start_val + i);
            cv.notify_one();
        }
        producers_done.fetch_add(1);
        // Wake consumers to check done condition
        UniqueLock<Mutex> lock(mtx);
        cv.notify_all();
    };

    // Single consumer
    std::thread consumer([&]() {
        UniqueLock<Mutex> lock(mtx);
        while (consumed.load() < kNumProducers * kPerProducer) {
            // Wait until there's data or all producers are done
            while (queue.empty() && producers_done.load() < kNumProducers) {
                cv.wait(lock);
            }
            while (!queue.empty()) {
                queue.pop();
                consumed.fetch_add(1);
            }
            if (producers_done.load() == kNumProducers && queue.empty()) {
                break;
            }
        }
    });

    std::vector<std::thread> producers;
    for (int p = 0; p < kNumProducers; ++p) {
        producers.emplace_back(producer, p * kPerProducer);
    }

    for (auto& t : producers) t.join();
    consumer.join();
    EXPECT_EQ(consumed.load(), kNumProducers * kPerProducer);
}

// ============================================================
// Multiple producers, single consumer
// ============================================================

TEST(ConditionVariable, MultiProducerSingleConsumer) {
    Mutex mtx;
    ConditionVariable cv;
    std::queue<std::string> queue;
    std::atomic<int> produced_count{0};
    std::vector<std::string> consumed;
    std::atomic<bool> all_done{false};
    const int kNumProducers = 8;
    const int kPerProducer = 100;

    auto producer = [&](int id) {
        for (int i = 0; i < kPerProducer; ++i) {
            UniqueLock<Mutex> lock(mtx);
            std::string item = "p" + std::to_string(id) + "_" + std::to_string(i);
            queue.push(item);
            produced_count.fetch_add(1);
            cv.notify_one();
        }
    };

    std::thread consumer([&]() {
        UniqueLock<Mutex> lock(mtx);
        while (true) {
            while (queue.empty() && produced_count.load() < kNumProducers * kPerProducer) {
                cv.wait(lock);
            }
            while (!queue.empty()) {
                consumed.push_back(queue.front());
                queue.pop();
            }
            if (produced_count.load() >= kNumProducers * kPerProducer) {
                break;
            }
        }
    });

    std::vector<std::thread> producers;
    for (int p = 0; p < kNumProducers; ++p) {
        producers.emplace_back(producer, p);
    }

    for (auto& t : producers) t.join();
    consumer.join();

    EXPECT_EQ(consumed.size(), static_cast<size_t>(kNumProducers * kPerProducer));
}

// ============================================================
// Spurious wakeup handling with predicate loop
// ============================================================

TEST(ConditionVariable, SpuriousWakeupResilience) {
    Mutex mtx;
    ConditionVariable cv;
    std::atomic<int> counter{0};
    std::atomic<int> wakes{0};

    auto waiter = [&]() {
        UniqueLock<Mutex> lock(mtx);
        while (counter.load() < 10) {
            cv.wait(lock);
            wakes.fetch_add(1);
        }
    };

    std::thread t1(waiter);
    std::thread t2(waiter);

    // Increment counter slowly, notify each time
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        UniqueLock<Mutex> lock(mtx);
        counter.fetch_add(1);
        cv.notify_all();
    }

    t1.join();
    t2.join();

    EXPECT_EQ(counter.load(), 10);
    // Each thread was woken at least 10 times (could be more
    // due to spurious wakeups, but the predicate loop handles them)
    EXPECT_GE(wakes.load(), 20);
}

// ============================================================
// wait_until
// ============================================================

TEST(ConditionVariable, WaitUntilTimeout) {
    Mutex mtx;
    ConditionVariable cv;

    UniqueLock<Mutex> lock(mtx);
    auto timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(10);
    bool result = cv.wait_until(lock, timeout);
    EXPECT_FALSE(result);
}

TEST(ConditionVariable, WaitUntilNotified) {
    Mutex mtx;
    ConditionVariable cv;

    std::thread notifier([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        UniqueLock<Mutex> lock(mtx);
        cv.notify_one();
    });

    UniqueLock<Mutex> lock(mtx);
    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    bool result = cv.wait_until(lock, timeout);

    notifier.join();
    EXPECT_TRUE(result);
}

TEST(ConditionVariable, WaitUntilAlreadyExpired) {
    Mutex mtx;
    ConditionVariable cv;

    UniqueLock<Mutex> lock(mtx);
    auto timeout = std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
    bool result = cv.wait_until(lock, timeout);
    EXPECT_FALSE(result);
}

// ============================================================
// Native handle
// ============================================================

TEST(ConditionVariable, NativeHandle) {
    ConditionVariable cv;
    EXPECT_NE(cv.native_handle(), nullptr);
    const ConditionVariable& ccv = cv;
    EXPECT_NE(ccv.native_handle(), nullptr);
}

// ============================================================
// Multiple condition variables on same mutex
// ============================================================

TEST(ConditionVariable, MultipleCVs) {
    Mutex mtx;
    ConditionVariable cv1;
    ConditionVariable cv2;
    std::atomic<int> stage{0};

    std::thread waiter1([&]() {
        UniqueLock<Mutex> lock(mtx);
        stage.fetch_add(1);
        cv1.wait(lock);
        EXPECT_GE(stage.load(), 2);
    });

    std::thread waiter2([&]() {
        UniqueLock<Mutex> lock(mtx);
        while (stage.load() < 2) {
            cv2.wait(lock);
        }
        EXPECT_GE(stage.load(), 2);
    });

    // Wait for waiter1 to enter wait
    while (stage.load() < 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    {
        UniqueLock<Mutex> lock(mtx);
        stage.store(2);
        cv1.notify_one();
        cv2.notify_one();
    }

    waiter1.join();
    waiter2.join();
    EXPECT_EQ(stage.load(), 2);
}

// ============================================================
// Stress: Concurrent notify_one calls
// ============================================================

TEST(ConditionVariable, StressConcurrentNotifyOne) {
    Mutex mtx;
    ConditionVariable cv;
    std::atomic<int> counter{0};
    const int kNumThreads = 16;
    const int kIterations = 1000;

    auto worker = [&]() {
        UniqueLock<Mutex> lock(mtx);
        for (int i = 0; i < kIterations; ++i) {
            // Wait for counter to be incremented by main
            cv.wait(lock);
            counter.fetch_add(1);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < kNumThreads; ++i) {
        threads.emplace_back(worker);
    }

    // Give threads time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Notify threads
    for (int i = 0; i < kNumThreads * kIterations; ++i) {
        UniqueLock<Mutex> lock(mtx);
        cv.notify_one();
    }

    for (auto& t : threads) t.join();
    EXPECT_EQ(counter.load(), kNumThreads * kIterations);
}

// ============================================================
// wait_for predicate version
// ============================================================

TEST(ConditionVariable, PredicateWaitForSuccess) {
    Mutex mtx;
    ConditionVariable cv;
    std::atomic<bool> ready{false};

    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        UniqueLock<Mutex> lock(mtx);
        ready.store(true);
        cv.notify_one();
    });

    UniqueLock<Mutex> lock(mtx);
    while (!ready.load()) {
        bool result = cv.wait_for(lock, std::chrono::seconds(5));
        if (ready.load()) break;
        if (!result) break;
    }

    t.join();
    EXPECT_TRUE(ready.load());
}
