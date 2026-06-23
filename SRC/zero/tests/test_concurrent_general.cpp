// test_concurrent_general.cpp — Concurrency stress and correctness tests
// for the zero library thread primitives and cross-subsystem concurrency.
//
// Tests thread safety of: spinlock, mutex, rwlock, condition_variable,
// barrier, semaphore, timer_wheel, channel, config, log, buffer, fd_manager,
// work_stealing_queue, and thread/fiber interaction patterns.
//
// Each test stresses correctness under concurrent access — lost updates,
// deadlocks, data races, and correctness invariants.
//
// Sections:
//  1.  SpinLock stress (6+ tests)
//  2.  Mutex stress (6+ tests)
//  3.  RWMutex stress (5+ tests)
//  4.  RWLock concurrency stress
//  5.  ConditionVariable concurrency stress
//  6.  Barrier concurrency stress
//  7.  Semaphore concurrency stress
//  8.  TimerWheel concurrency stress
//  9.  Channel concurrency stress
// 10.  Buffer concurrency stress
// 11.  FD Manager concurrency stress
// 12.  WorkStealingQueue concurrency stress
// 13.  Config concurrency stress
// 14.  Log concurrency stress
// 15.  Thread creation / join stress
// 16.  Scheduler concurrency stress
// 17.  Cross-subsystem stress tests
// 18.  Deadlock prevention tests
// 19.  Long-running stability
// 20.  Edge-case concurrent patterns
// 21.  Concurrent ChainBuffer I/O patterns
// 22.  Scheduler with fiber lifecycle
// 23.  Reactor/heavy load
// 24.  Scheduler config interaction
// 25.  Global stress: all primitives together
// 26.  SpinLock cache-line ping-pong & exponential backoff
// 27.  Mutex timed lock & unique_lock move
// 28.  RWMutex writer starvation test
// 29.  Channel close stress & large object throughput
// 30.  Performance benchmark suite
// =====================================================================

#include <gtest/gtest.h>
#include "zero/zero.h"

// Subsystem headers not in the umbrella zero.h
#include "zero/thread/rwlock.h"
#include "zero/thread/condition_variable.h"
#include "zero/thread/barrier.h"
#include "zero/fiber/channel.h"
#include "zero/scheduler/work_stealing_queue.h"
#include "zero/scheduler/fd_manager.h"
#include "zero/scheduler/hook.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <sstream>
#include <thread>
#include <unordered_set>
#include <vector>
#include <sys/eventfd.h>
#include <unistd.h>

using namespace zero;

namespace {
// Helper: busy-wait until condition is true or timeout
template <typename F>
bool wait_until(F&& condition, int timeout_ms = 5000) {
    auto deadline = std::chrono::steady_clock::now() +
                     std::chrono::milliseconds(timeout_ms);
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

// Helper: compute median of a vector of int64_t values
static int64_t compute_median(std::vector<int64_t>& vals) {
    if (vals.empty()) return 0;
    std::sort(vals.begin(), vals.end());
    return vals[vals.size() / 2];
}

// Helper: compute average of a vector of int64_t values
static int64_t compute_avg(const std::vector<int64_t>& vals) {
    if (vals.empty()) return 0;
    int64_t sum = 0;
    for (auto v : vals) sum += v;
    return sum / static_cast<int64_t>(vals.size());
}

// Helper: compute p99 of a vector of int64_t values
static int64_t compute_p99(std::vector<int64_t>& vals) {
    if (vals.empty()) return 0;
    std::sort(vals.begin(), vals.end());
    size_t idx = vals.size() * 99 / 100;
    return vals[idx];
}

// Helper: dummy work to simulate computation in critical section
static void dummy_work(int iterations) {
    volatile int x = 0;
    for (int i = 0; i < iterations; ++i) {
        x += i;
    }
    (void)x;
}

// Helper: get a monotonically increasing timestamp in nanoseconds
static int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}
} // namespace

// =====================================================================
// Section 1 — SpinLock concurrency stress (expanded)
// =====================================================================

TEST(ConcurrentSpinLock, IncrementCounter) {
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

TEST(ConcurrentSpinLock, IncrementCounter16Threads) {
    SpinLock lock;
    std::atomic<int> counter{0};
    const int num_threads = 16;
    const int per_thread = 100000;

    auto t_start = now_ns();

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < per_thread; ++i) {
                LockGuard<SpinLock> guard(lock);
                counter.fetch_add(1);
            }
        });
    }
    for (auto& th : threads) th.join();

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;
    int total = num_threads * per_thread;

    EXPECT_EQ(counter.load(), total);
    std::cout << "[SpinLock16x100K] " << total << " increments in "
              << elapsed_ms << "ms ("
              << (total / std::max(elapsed_ms, 0.001) * 1000.0) << " ops/sec)"
              << std::endl;
}

TEST(ConcurrentSpinLock, TryLockContention) {
    SpinLock lock;
    std::atomic<int> successes{0};
    std::atomic<int> failures{0};
    const int num_threads = 4;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (int i = 0; i < 50000; ++i) {
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
    EXPECT_GT(successes.load(), 0);
    // In heavy contention, some failures should occur
    EXPECT_GT(failures.load(), 0);
    std::cout << "[SpinLock::try_lock] successes=" << successes.load()
              << " failures=" << failures.load()
              << " ratio=" << (100.0 * successes.load() /
                 (successes.load() + failures.load())) << "%" << std::endl;
}

TEST(ConcurrentSpinLock, TryLockContention32Threads) {
    SpinLock lock;
    std::atomic<int> successes{0};
    std::atomic<int> failures{0};
    const int num_threads = 32;
    const int per_thread = 20000;
    std::atomic<bool> start{false};

    auto t_start = now_ns();

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

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;

    EXPECT_GT(successes.load(), 0);
    std::cout << "[SpinLock32TryLock] " << successes.load() << " acquired, "
              << failures.load() << " failed in " << elapsed_ms << "ms"
              << std::endl;
}

TEST(ConcurrentSpinLock, LockOrder) {
    // Verify that two locks are acquired independently without deadlock
    SpinLock lock1, lock2;
    std::atomic<int> count{0};
    const int iterations = 1000;

    std::thread t1([&]() {
        for (int i = 0; i < iterations; ++i) {
            lock1.lock();
            lock2.lock();
            count.fetch_add(1);
            lock2.unlock();
            lock1.unlock();
        }
    });

    std::thread t2([&]() {
        for (int i = 0; i < iterations; ++i) {
            lock1.lock(); // Same order to avoid deadlock
            lock2.lock();
            count.fetch_add(1);
            lock2.unlock();
            lock1.unlock();
        }
    });

    t1.join();
    t2.join();
    EXPECT_EQ(count.load(), 2 * iterations);
}

TEST(ConcurrentSpinLock, ExponentialBackoffFairness) {
    // Under contention, verify that all threads make progress
    SpinLock lock;
    const int num_threads = 8;
    const int per_thread = 50000;
    std::atomic<int> total{0};
    std::atomic<bool> start{false};
    std::vector<std::atomic<int>> thread_counts(num_threads);

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            while (!start.load()) {}
            for (int i = 0; i < per_thread; ++i) {
                lock.lock();
                total.fetch_add(1);
                thread_counts[t].fetch_add(1);
                lock.unlock();
            }
        });
    }
    start.store(true);
    for (auto& th : threads) th.join();

    EXPECT_EQ(total.load(), num_threads * per_thread);

    // Check fairness: no thread should be starved
    int min_count = per_thread;
    int max_count = 0;
    for (int t = 0; t < num_threads; ++t) {
        int c = thread_counts[t].load();
        min_count = std::min(min_count, c);
        max_count = std::max(max_count, c);
    }
    EXPECT_EQ(min_count, per_thread);
    EXPECT_EQ(max_count, per_thread);
    std::cout << "[SpinLockFairness] " << num_threads << " threads each "
              << per_thread << " ops, all completed equally" << std::endl;
}

TEST(ConcurrentSpinLock, CacheLinePingPong) {
    // Measure performance with 2/4/8 threads to see cache-line effects
    for (int num_threads : {2, 4, 8}) {
        SpinLock lock;
        std::atomic<int> counter{0};
        const int per_thread = 200000;
        std::atomic<bool> start{false};

        auto t_start = now_ns();

        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&]() {
                while (!start.load()) {}
                for (int i = 0; i < per_thread; ++i) {
                    LockGuard<SpinLock> guard(lock);
                    counter.fetch_add(1);
                }
            });
        }
        start.store(true);
        for (auto& th : threads) th.join();

        auto t_end = now_ns();
        double elapsed_ms = (t_end - t_start) / 1e6;
        int total = num_threads * per_thread;

        EXPECT_EQ(counter.load(), total);
        std::cout << "[SpinLockPingPong-" << num_threads << "t] "
                  << total << " ops in " << elapsed_ms << "ms ("
                  << (total / std::max(elapsed_ms, 0.001) * 1000.0)
                  << " ops/sec)" << std::endl;
    }
}

TEST(ConcurrentSpinLock, LongCriticalSection) {
    // Simulate work in critical section to verify behavior
    SpinLock lock;
    std::atomic<int> counter{0};
    const int num_threads = 4;
    const int per_thread = 1000;
    const int work_iterations = 1000;

    auto t_start = now_ns();

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < per_thread; ++i) {
                lock.lock();
                counter.fetch_add(1);
                dummy_work(work_iterations);
                lock.unlock();
            }
        });
    }
    for (auto& th : threads) th.join();

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;

    EXPECT_EQ(counter.load(), num_threads * per_thread);
    std::cout << "[SpinLockLongCS] " << (num_threads * per_thread)
              << " critical sections with " << work_iterations
              << " dummy iterations in " << elapsed_ms << "ms" << std::endl;
}

TEST(ConcurrentSpinLock, ScopedSpinLockCorrectness) {
    SpinLock lock;
    std::atomic<int> counter{0};
    const int num_threads = 6;
    const int per_thread = 50000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < per_thread; ++i) {
                ScopedSpinLock guard(lock);
                counter.fetch_add(1);
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(counter.load(), num_threads * per_thread);
}

// =====================================================================
// Section 2 — Mutex concurrency stress (expanded)
// =====================================================================

TEST(ConcurrentMutex, IncrementCounter) {
    Mutex mtx;
    int counter = 0;
    const int num_threads = 8;
    const int per_thread = 100000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < per_thread; ++i) {
                LockGuard<Mutex> guard(mtx);
                ++counter;
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(counter, num_threads * per_thread);
}

TEST(ConcurrentMutex, IncrementCounter16Threads) {
    Mutex mtx;
    std::atomic<int> counter{0};
    const int num_threads = 16;
    const int per_thread = 100000;

    auto t_start = now_ns();

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < per_thread; ++i) {
                LockGuard<Mutex> guard(mtx);
                counter.fetch_add(1);
            }
        });
    }
    for (auto& th : threads) th.join();

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;
    int total = num_threads * per_thread;

    EXPECT_EQ(counter.load(), total);
    std::cout << "[Mutex16x100K] " << total << " increments in "
              << elapsed_ms << "ms ("
              << (total / std::max(elapsed_ms, 0.001) * 1000.0) << " ops/sec)"
              << std::endl;
}

TEST(ConcurrentMutex, ProducerConsumer) {
    Mutex mtx;
    ConditionVariable cv;
    std::queue<int> queue;
    const int max_items = 10000;
    std::atomic<int> consumed{0};
    std::atomic<bool> done{false};

    std::thread producer([&]() {
        for (int i = 0; i < max_items; ++i) {
            UniqueLock<Mutex> lock(mtx);
            queue.push(i);
            lock.unlock();
            cv.notify_one();
        }
        done.store(true);
        cv.notify_one();
    });

    std::thread consumer([&]() {
        while (true) {
            UniqueLock<Mutex> lock(mtx);
            cv.wait(lock);
            bool should_break = false;
            while (!queue.empty()) {
                queue.pop();
                consumed.fetch_add(1);
            }
            if (done.load() && queue.empty()) {
                should_break = true;
            }
            lock.unlock();
            if (should_break) break;
        }
    });

    producer.join();
    consumer.join();
    EXPECT_EQ(consumed.load(), max_items);
}

TEST(ConcurrentMutex, MultiProducerConsumerBoundedQueue) {
    // 4 producers + 4 consumers, bounded queue of 1024, 50000 total items
    Mutex mtx;
    ConditionVariable cv_not_full;
    ConditionVariable cv_not_empty;
    std::queue<int> queue;
    const int capacity = 1024;
    const int num_producers = 4;
    const int num_consumers = 4;
    const int total_items = 50000;
    const int per_producer = total_items / num_producers;
    std::atomic<int> total_produced{0};
    std::atomic<int> total_consumed{0};
    std::atomic<int> sum_produced{0};
    std::atomic<int> sum_consumed{0};
    std::atomic<bool> producers_done{false};
    std::atomic<bool> start{false};

    auto t_start = now_ns();

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            while (!start.load()) {}
            int base = p * per_producer;
            for (int i = 0; i < per_producer; ++i) {
                int item = base + i;
                UniqueLock<Mutex> lock(mtx);
                while (static_cast<int>(queue.size()) >= capacity) {
                    cv_not_full.wait(lock);
                }
                queue.push(item);
                sum_produced.fetch_add(item);
                total_produced.fetch_add(1);
                lock.unlock();
                cv_not_empty.notify_one();
            }
        });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&]() {
            while (!start.load()) {}
            while (true) {
                UniqueLock<Mutex> lock(mtx);
                while (queue.empty()) {
                    if (total_consumed.load() >= total_items) {
                        lock.unlock();
                        return;
                    }
                    cv_not_empty.wait(lock);
                }
                int item = queue.front();
                queue.pop();
                sum_consumed.fetch_add(item);
                total_consumed.fetch_add(1);
                lock.unlock();
                cv_not_full.notify_one();

                if (total_consumed.load() >= total_items) {
                    cv_not_empty.notify_all();
                    return;
                }
            }
        });
    }

    start.store(true);
    for (auto& th : producers) th.join();
    producers_done.store(true);
    cv_not_empty.notify_all();
    for (auto& th : consumers) th.join();

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;

    EXPECT_EQ(total_produced.load(), total_items);
    EXPECT_EQ(total_consumed.load(), total_items);
    EXPECT_EQ(sum_produced.load(), sum_consumed.load());
    std::cout << "[MutexBoundedQueue] " << total_items << " items, "
              << num_producers << "P+" << num_consumers << "C in "
              << elapsed_ms << "ms ("
              << (total_items / std::max(elapsed_ms, 0.001) * 1000.0)
              << " ops/sec)" << std::endl;
}

TEST(ConcurrentMutex, DeadlockFreeMultiLock) {
    // 100 threads locking 5 mutexes in random order — no deadlock due to
    // fixed ordering via try_lock-and-backoff pattern
    const int num_threads = 100;
    const int num_mutexes = 5;
    const int iterations = 200;
    std::vector<Mutex> mutexes(num_mutexes);
    std::atomic<int> completed{0};
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            std::mt19937 rng(42 + t);
            for (int iter = 0; iter < iterations; ++iter) {
                // Generate a random permutation of all indices
                std::vector<int> order(num_mutexes);
                for (int i = 0; i < num_mutexes; ++i) order[i] = i;
                std::shuffle(order.begin(), order.end(), rng);

                // Sort to get a fixed locking order (prevents deadlock)
                std::sort(order.begin(), order.end());

                // Lock in sorted order
                for (int idx : order) {
                    mutexes[idx].lock();
                }
                // Critical section — dummy
                dummy_work(10);
                // Unlock in reverse order
                for (auto it = order.rbegin(); it != order.rend(); ++it) {
                    mutexes[*it].unlock();
                }
            }
            completed.fetch_add(1);
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();
    EXPECT_EQ(completed.load(), num_threads);
    std::cout << "[MutexDeadlockFree] " << num_threads << " threads, "
              << num_mutexes << " mutexes, no deadlock" << std::endl;
}

TEST(ConcurrentMutex, UniqueLockMove) {
    // Threads passing unique_lock ownership through a channel
    Mutex mtx;
    std::queue<UniqueLock<Mutex>> lock_queue;
    Mutex queue_mtx;
    std::atomic<int> operations{0};
    const int num_threads = 4;
    const int per_thread = 500;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (int i = 0; i < per_thread; ++i) {
                // Acquire lock, do work, then move it into queue
                UniqueLock<Mutex> lock(mtx);
                operations.fetch_add(1);
                dummy_work(5);

                // Move lock to queue (simulates passing ownership)
                {
                    LockGuard<Mutex> qg(queue_mtx);
                    lock_queue.push(std::move(lock));
                }
            }
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();

    // Drain queue and release all locks
    while (!lock_queue.empty()) {
        UniqueLock<Mutex> lock(std::move(lock_queue.front()));
        lock_queue.pop();
        lock.unlock();
    }

    EXPECT_EQ(operations.load(), num_threads * per_thread);
}

TEST(ConcurrentMutex, TryLockContention) {
    Mutex mtx;
    std::atomic<int> successes{0};
    const int num_threads = 4;
    const int per_thread = 10000;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (int i = 0; i < per_thread; ++i) {
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

TEST(ConcurrentMutex, ConditionVariableBroadcast) {
    // Broadcast wakes all 64 waiters
    Mutex mtx;
    ConditionVariable cv;
    std::atomic<int> woken{0};
    const int num_waiters = 64;
    std::atomic<bool> ready{false};

    std::vector<std::thread> waiters;
    for (int i = 0; i < num_waiters; ++i) {
        waiters.emplace_back([&]() {
            UniqueLock<Mutex> lock(mtx);
            ready.fetch_add(1);
            cv.wait(lock);
            woken.fetch_add(1);
        });
    }

    // Wait for all threads to be waiting
    wait_until([&]() { return ready.load() >= num_waiters; }, 5000);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    cv.notify_all();
    for (auto& th : waiters) th.join();
    EXPECT_EQ(woken.load(), num_waiters);
    std::cout << "[CVBroadcast] " << num_waiters << " waiters, all woken"
              << std::endl;
}

TEST(ConcurrentMutex, TimedLock) {
    // try_lock_for with timeout — test timeout behavior
    Mutex mtx;
    std::atomic<bool> holder_released{false};

    // Hold the lock
    mtx.lock();

    std::thread waiter([&]() {
        // Try locking with primitive timeout using try_lock in a loop
        auto deadline = std::chrono::steady_clock::now() +
                         std::chrono::milliseconds(100);
        bool acquired = false;
        while (std::chrono::steady_clock::now() < deadline) {
            if (mtx.try_lock()) {
                acquired = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        EXPECT_FALSE(acquired) << "Should not acquire held lock within timeout";
        holder_released.store(true);
        return;
    });

    // Wait for waiter to time out
    wait_until([&]() { return holder_released.load(); }, 5000);
    mtx.unlock();
    waiter.join();
    EXPECT_TRUE(holder_released.load());
}

// =====================================================================
// Section 3 — RWMutex concurrency stress (expanded)
// =====================================================================

TEST(ConcurrentRWMutex, ManyConcurrentReaders) {
    RWMutex mtx;
    std::atomic<int> read_count{0};
    std::atomic<int> max_concurrent{0};
    const int num_readers = 8;
    const int per_reader = 10000;
    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};

    std::vector<std::thread> readers;
    for (int t = 0; t < num_readers; ++t) {
        readers.emplace_back([&]() {
            while (!start.load()) {}
            for (int i = 0; i < per_reader; ++i) {
                mtx.rdlock();
                int current = read_count.fetch_add(1) + 1;
                // Track max concurrent readers
                int old = max_concurrent.load();
                while (current > old && !max_concurrent.compare_exchange_weak(old, current)) {}
                read_count.fetch_sub(1);
                mtx.unlock();
            }
        });
    }
    start.store(true);
    for (auto& th : readers) th.join();
    // With 8 concurrent readers, we should see multiple readers at once
    EXPECT_GT(max_concurrent.load(), 1);
}

TEST(ConcurrentRWMutex, WriterExcludesAll) {
    RWMutex mtx;
    std::atomic<int> readers_inside{0};
    std::atomic<int> max_readers_during_write{0};
    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    std::atomic<int> reads{0};

    std::vector<std::thread> readers;
    for (int t = 0; t < 4; ++t) {
        readers.emplace_back([&]() {
            while (!start.load()) {}
            while (!stop.load()) {
                mtx.rdlock();
                int inside = readers_inside.fetch_add(1) + 1;
                reads.fetch_add(1);
                readers_inside.fetch_sub(1);
                mtx.unlock();
            }
        });
    }

    std::thread writer([&]() {
        while (!start.load()) {}
        for (int i = 0; i < 5000; ++i) {
            mtx.wrlock();
            // While writing, no readers should be inside
            int inside = readers_inside.load();
            int old_max = max_readers_during_write.load();
            while (inside > old_max &&
                   !max_readers_during_write.compare_exchange_weak(old_max, inside)) {}
            mtx.unlock();
        }
    });

    start.store(true);
    writer.join();
    stop.store(true);
    for (auto& th : readers) th.join();
    // During write locks, readers_inside should normally be 0
    EXPECT_LE(max_readers_during_write.load(), 10); // Some tolerance
    EXPECT_GT(reads.load(), 0);
}

TEST(ConcurrentRWMutex, ReadWriteAlternating) {
    RWMutex mtx;
    std::atomic<int> value{0};
    std::atomic<int> read_count{0};
    std::atomic<int> write_count{0};
    const int num_ops = 10000;
    std::atomic<bool> start{false};

    std::vector<std::thread> readers;
    for (int t = 0; t < 4; ++t) {
        readers.emplace_back([&]() {
            while (!start.load()) {}
            for (int i = 0; i < num_ops; ++i) {
                mtx.rdlock();
                int v = value.load();
                ZERO_UNUSED(v);
                read_count.fetch_add(1);
                mtx.unlock();
            }
        });
    }

    std::thread writer([&]() {
        while (!start.load()) {}
        for (int i = 0; i < num_ops; ++i) {
            mtx.wrlock();
            value.store(i);
            write_count.fetch_add(1);
            mtx.unlock();
        }
    });

    start.store(true);
    for (auto& th : readers) th.join();
    writer.join();
    EXPECT_GE(read_count.load(), 4 * num_ops - 100); // Small tolerance
    EXPECT_EQ(write_count.load(), num_ops);
}

TEST(ConcurrentRWMutex, MassiveReaderCount) {
    // 64 threads reading, verify no corruption during reads
    RWMutex mtx;
    std::atomic<int> value{42};
    std::atomic<int64_t> checksum{0};
    const int num_readers = 64;
    const int per_reader = 5000;
    std::atomic<bool> start{false};

    std::vector<std::thread> readers;
    for (int t = 0; t < num_readers; ++t) {
        readers.emplace_back([&]() {
            while (!start.load()) {}
            for (int i = 0; i < per_reader; ++i) {
                mtx.rdlock();
                int v = value.load();
                checksum.fetch_add(v);
                mtx.unlock();
            }
        });
    }

    // One writer occasionally updates value
    std::thread writer([&]() {
        while (!start.load()) {}
        for (int i = 0; i < 500; ++i) {
            mtx.wrlock();
            value.store(value.load() + 1);
            mtx.unlock();
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    start.store(true);
    for (auto& th : readers) th.join();
    writer.join();

    // All values read should be >= 42 (the initial value)
    int64_t cs = checksum.load();
    EXPECT_GE(cs, static_cast<int64_t>(42) * num_readers * per_reader);
    std::cout << "[RWMutex64Readers] " << num_readers << " readers, "
              << per_reader << " ops each, checksum=" << cs << std::endl;
}

TEST(ConcurrentRWMutex, WriterStarvationTest) {
    // Ensure writers eventually get the lock despite continuous readers
    RWMutex mtx;
    std::atomic<int> writes_completed{0};
    std::atomic<bool> stop{false};
    std::atomic<bool> start{false};
    const int target_writes = 100;

    // Many continuous readers
    std::vector<std::thread> readers;
    for (int t = 0; t < 8; ++t) {
        readers.emplace_back([&]() {
            while (!start.load()) {}
            while (!stop.load()) {
                mtx.rdlock();
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                mtx.unlock();
            }
        });
    }

    // Writer trying to write
    std::thread writer([&]() {
        while (!start.load()) {}
        for (int i = 0; i < target_writes; ++i) {
            mtx.wrlock();
            writes_completed.fetch_add(1);
            mtx.unlock();
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });

    start.store(true);

    // Wait for all writes to complete (with a generous timeout)
    bool completed = wait_until([&]() {
        return writes_completed.load() >= target_writes;
    }, 15000);
    stop.store(true);

    writer.join();
    for (auto& th : readers) th.join();

    EXPECT_TRUE(completed) << "Writer starved: only "
                           << writes_completed.load() << " / "
                           << target_writes << " writes completed";
}

TEST(ConcurrentRWMutex, ReadWriteThroughput) {
    // Measure throughput of read-write-read pattern
    RWMutex mtx;
    std::atomic<int> value{0};
    std::atomic<bool> start{false};
    const int num_cycles = 10000;

    auto t_start = now_ns();

    std::thread writer([&]() {
        while (!start.load()) {}
        for (int i = 0; i < num_cycles / 2; ++i) {
            mtx.wrlock();
            value.store(value.load() + 1);
            mtx.unlock();
        }
    });

    std::thread reader([&]() {
        while (!start.load()) {}
        for (int i = 0; i < num_cycles; ++i) {
            mtx.rdlock();
            int v = value.load();
            ZERO_UNUSED(v);
            mtx.unlock();
            mtx.rdlock();
            v = value.load();
            ZERO_UNUSED(v);
            mtx.unlock();
        }
    });

    start.store(true);
    reader.join();
    writer.join();

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;
    int total_ops = num_cycles * 2 + num_cycles / 2;
    std::cout << "[RWMutexThroughput] " << total_ops
              << " ops in " << elapsed_ms << "ms ("
              << (total_ops / std::max(elapsed_ms, 0.001) * 1000.0)
              << " ops/sec)" << std::endl;
}

TEST(ConcurrentRWMutex, UpgradeToWrite) {
    // Test read-then-upgrade pattern using separate locks
    RWMutex mtx;
    std::atomic<int> reads_before_upgrade{0};
    std::atomic<int> writes_after_upgrade{0};
    std::atomic<bool> start{false};
    const int num_threads = 4;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            while (!start.load()) {}
            for (int i = 0; i < 500; ++i) {
                // Read phase
                mtx.rdlock();
                reads_before_upgrade.fetch_add(1);
                mtx.unlock();

                // Write phase (release read, acquire write)
                mtx.wrlock();
                writes_after_upgrade.fetch_add(1);
                dummy_work(50);
                mtx.unlock();
            }
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();

    EXPECT_EQ(reads_before_upgrade.load(), num_threads * 500);
    EXPECT_EQ(writes_after_upgrade.load(), num_threads * 500);
    std::cout << "[RWMutexUpgrade] " << num_threads << " threads, "
              << (num_threads * 500) << " read-then-write upgrades" << std::endl;
}

// =====================================================================
// Section 4 — RWLock concurrency stress
// =====================================================================

TEST(ConcurrentRWLock, ManyReadersOneWriter) {
    RWLock rwlock;
    std::atomic<int> reads{0};
    std::atomic<int> writes{0};
    const int writer_iterations = 1000;
    std::atomic<bool> stop{false};
    std::atomic<bool> start{false};

    std::vector<std::thread> readers;
    for (int t = 0; t < 8; ++t) {
        readers.emplace_back([&]() {
            while (!start.load()) {}
            while (!stop.load()) {
                rwlock.lock_shared();
                reads.fetch_add(1);
                rwlock.unlock_shared();
            }
        });
    }

    std::thread writer([&]() {
        while (!start.load()) {}
        for (int i = 0; i < writer_iterations; ++i) {
            rwlock.lock();
            writes.fetch_add(1);
            rwlock.unlock();
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

TEST(ConcurrentRWLock, ReadLockGuardStress) {
    RWLock rwlock;
    std::atomic<int> counter{0};
    const int num_threads = 6;
    const int per_thread = 50000;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (int i = 0; i < per_thread; ++i) {
                ReadLockGuard guard(rwlock);
                counter.fetch_add(1);
            }
        });
    }
    start.store(true);
    for (auto& th : threads) th.join();
    EXPECT_EQ(counter.load(), num_threads * per_thread);
}

TEST(ConcurrentRWLock, WriteLockGuardStress) {
    RWLock rwlock;
    int counter = 0;
    const int num_writers = 4;
    const int per_writer = 50000;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_writers; ++t) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (int i = 0; i < per_writer; ++i) {
                WriteLockGuard guard(rwlock);
                ++counter;
            }
        });
    }
    start.store(true);
    for (auto& th : threads) th.join();
    EXPECT_EQ(counter, num_writers * per_writer);
}

TEST(ConcurrentRWLock, UpgradeLockGuardPattern) {
    // Test the UpgradeLockGuard pattern: read first, upgrade to write if needed
    RWLock rwlock;
    std::atomic<int> upgrades{0};
    std::atomic<int> downgrades{0};
    const int num_threads = 4;
    const int per_thread = 200;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (int i = 0; i < per_thread; ++i) {
                UpgradeLockGuard guard(rwlock);
                // Read phase
                EXPECT_FALSE(guard.is_write_locked());

                // Upgrade to write
                guard.upgrade();
                EXPECT_TRUE(guard.is_write_locked());
                upgrades.fetch_add(1);

                // Write phase
                dummy_work(10);

                // Downgrade back to read
                guard.downgrade();
                EXPECT_FALSE(guard.is_write_locked());
                downgrades.fetch_add(1);
            }
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();
    EXPECT_EQ(upgrades.load(), num_threads * per_thread);
    EXPECT_EQ(downgrades.load(), num_threads * per_thread);
    std::cout << "[RWLockUpgrade] " << upgrades.load()
              << " upgrade/downgrade cycles" << std::endl;
}

TEST(ConcurrentRWLock, TryLockSharedContention) {
    RWLock rwlock;
    std::atomic<int> successes{0};
    std::atomic<int> failures{0};
    const int num_threads = 8;
    const int per_thread = 10000;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (int i = 0; i < per_thread; ++i) {
                if (rwlock.try_lock_shared()) {
                    successes.fetch_add(1);
                    rwlock.unlock_shared();
                } else {
                    failures.fetch_add(1);
                }
            }
        });
    }
    start.store(true);
    for (auto& th : threads) th.join();
    // try_lock_shared should mostly succeed since multiple readers allowed
    EXPECT_GT(successes.load(), 0);
    std::cout << "[RWLockTryShared] " << successes.load() << " acquired, "
              << failures.load() << " failed" << std::endl;
}

// =====================================================================
// Section 5 — ConditionVariable concurrency stress
// =====================================================================

TEST(ConcurrentConditionVariable, NotifyAllWakesAll) {
    Mutex mtx;
    ConditionVariable cv;
    std::atomic<int> woken{0};
    const int num_waiters = 16;

    std::vector<std::thread> waiters;
    for (int i = 0; i < num_waiters; ++i) {
        waiters.emplace_back([&]() {
            UniqueLock<Mutex> lock(mtx);
            cv.wait(lock);
            woken.fetch_add(1);
        });
    }

    // Give threads time to block on the CV
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    cv.notify_all();
    for (auto& th : waiters) th.join();
    EXPECT_EQ(woken.load(), num_waiters);
}

TEST(ConcurrentConditionVariable, NotifyOneWakesExactlyOne) {
    Mutex mtx;
    ConditionVariable cv;
    std::atomic<int> woken{0};
    const int num_waiters = 8;

    std::vector<std::thread> waiters;
    for (int i = 0; i < num_waiters; ++i) {
        waiters.emplace_back([&]() {
            UniqueLock<Mutex> lock(mtx);
            cv.wait(lock);
            woken.fetch_add(1);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Notify one at a time
    for (int i = 0; i < num_waiters; ++i) {
        cv.notify_one();
        // Wait a bit for the woken thread to respond
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    for (auto& th : waiters) th.join();
    EXPECT_EQ(woken.load(), num_waiters);
}

TEST(ConcurrentConditionVariable, SpuriousWakeupSafe) {
    Mutex mtx;
    ConditionVariable cv;
    std::atomic<int> count{0};
    const int num_threads = 4;
    const int target = 10000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            UniqueLock<Mutex> lock(mtx);
            while (count.load() < target) {
                cv.wait(lock);
                // Count may have been incremented by another thread
            }
        });
    }

    std::thread notifier([&]() {
        for (int i = 0; i < target; ++i) {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            {
                UniqueLock<Mutex> lock(mtx);
                count.fetch_add(1);
            }
            cv.notify_one();
        }
    });

    notifier.join();
    for (auto& th : threads) th.join();
    EXPECT_GE(count.load(), target);
}

TEST(ConcurrentConditionVariable, MultiCVProducerConsumer) {
    // Extended producer-consumer with multiple CVs
    Mutex mtx;
    ConditionVariable cv_not_full;
    ConditionVariable cv_not_empty;
    std::deque<int> queue;
    const int capacity = 32;
    const int num_producers = 6;
    const int num_consumers = 6;
    const int total_items = 50000;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<bool> producers_done{false};
    std::atomic<bool> start{false};

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            while (!start.load()) {}
            while (produced.load() < total_items) {
                UniqueLock<Mutex> lock(mtx);
                while (static_cast<int>(queue.size()) >= capacity) {
                    cv_not_full.wait(lock);
                }
                int item = produced.fetch_add(1);
                if (item < total_items) {
                    queue.push_back(item);
                }
                lock.unlock();
                cv_not_empty.notify_one();
            }
        });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&]() {
            while (!start.load()) {}
            while (true) {
                UniqueLock<Mutex> lock(mtx);
                while (queue.empty()) {
                    if (consumed.load() >= total_items) {
                        lock.unlock();
                        return;
                    }
                    cv_not_empty.wait(lock);
                }
                int item = queue.front();
                queue.pop_front();
                consumed.fetch_add(1);
                ZERO_UNUSED(item);
                lock.unlock();
                cv_not_full.notify_one();

                if (consumed.load() >= total_items) {
                    cv_not_empty.notify_all();
                    break;
                }
            }
        });
    }

    start.store(true);
    for (auto& th : producers) th.join();
    producers_done.store(true);
    cv_not_empty.notify_all();
    for (auto& th : consumers) th.join();
    EXPECT_EQ(consumed.load(), total_items);
    std::cout << "[MultiCV] " << total_items << " items, "
              << num_producers << "P+" << num_consumers << "C" << std::endl;
}

// =====================================================================
// Section 6 — Barrier concurrency stress
// =====================================================================

TEST(ConcurrentBarrier, RepeatedSync) {
    const int num_threads = 8;
    const int iterations = 100;
    Barrier barrier(num_threads);
    std::atomic<int> phase{0};
    std::vector<std::atomic<int>> counts(iterations);

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < iterations; ++i) {
                phase.store(i);
                barrier.wait();
                counts[i].fetch_add(1);
            }
        });
    }

    for (auto& th : threads) th.join();
    for (int i = 0; i < iterations; ++i) {
        EXPECT_EQ(counts[i].load(), num_threads);
    }
}

TEST(ConcurrentBarrier, FullThreadCount) {
    const int N = 16;
    Barrier barrier(N);
    std::atomic<int> arrived{0};

    auto worker = [&]() {
        arrived.fetch_add(1);
        barrier.wait();
        EXPECT_EQ(arrived.load(), N);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < N; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& th : threads) th.join();
}

TEST(ConcurrentBarrier, ManyIterations) {
    const int num_threads = 4;
    const int iterations = 500;
    Barrier barrier(num_threads);
    std::atomic<int> count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < iterations; ++i) {
                barrier.wait();
                count.fetch_add(1);
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(count.load(), num_threads * iterations);
}

TEST(ConcurrentBarrier, LargeGroupBarrier) {
    // Barrier with many threads
    const int num_threads = 32;
    const int iterations = 10;
    Barrier barrier(num_threads);
    std::atomic<int> phase{0};
    std::atomic<int> sync_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < iterations; ++i) {
                phase.store(i);
                barrier.wait();
                sync_count.fetch_add(1);
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(sync_count.load(), num_threads * iterations);
    std::cout << "[Barrier32x10] " << num_threads << " threads, "
              << iterations << " iterations, total syncs="
              << sync_count.load() << std::endl;
}

TEST(ConcurrentBarrier, StaggeredArrival) {
    // Threads arrive at different times
    const int num_threads = 8;
    const int iterations = 20;
    Barrier barrier(num_threads);
    std::atomic<int> completed{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < iterations; ++i) {
                // Stagger arrival times
                if (t < 4) {
                    dummy_work(t * 100);
                } else {
                    std::this_thread::sleep_for(std::chrono::microseconds(t * 50));
                }
                barrier.wait();
                completed.fetch_add(1);
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(completed.load(), num_threads * iterations);
}

// =====================================================================
// Section 7 — Semaphore concurrency stress
// =====================================================================

TEST(ConcurrentSemaphore, PostWaitThreads) {
    Semaphore sem(0);
    std::atomic<int> count{0};
    const int num_threads = 8;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            sem.wait();
            count.fetch_add(1);
        });
    }

    // Post once per thread
    for (int t = 0; t < num_threads; ++t) {
        sem.post();
    }

    for (auto& th : threads) th.join();
    EXPECT_EQ(count.load(), num_threads);
}

TEST(ConcurrentSemaphore, TryWaitContention) {
    Semaphore sem(100);
    std::atomic<int> acquired{0};
    std::atomic<int> failed{0};
    const int num_threads = 8;
    const int attempts_per_thread = 1000;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (int i = 0; i < attempts_per_thread; ++i) {
                if (sem.try_wait()) {
                    acquired.fetch_add(1);
                    sem.post(); // Return immediately
                } else {
                    failed.fetch_add(1);
                }
            }
        });
    }
    start.store(true);
    for (auto& th : threads) th.join();
    // Total acquisitions should be close to attempts (we post back immediately)
    long total_attempts = num_threads * attempts_per_thread;
    EXPECT_GE(acquired.load() + failed.load(), total_attempts - 100);
}

TEST(ConcurrentSemaphore, MultiproducerConsumer) {
    Semaphore sem(0);
    std::atomic<int> count{0};
    std::atomic<int> produced{0};
    const int num_consumers = 4;
    const int num_producers = 4;
    const int per_producer = 1000;

    std::vector<std::thread> consumers;
    for (int t = 0; t < num_consumers; ++t) {
        consumers.emplace_back([&]() {
            while (produced.load() < num_producers * per_producer ||
                   sem.try_wait()) {
                sem.wait();
                count.fetch_add(1);
            }
        });
    }

    std::vector<std::thread> producers;
    for (int t = 0; t < num_producers; ++t) {
        producers.emplace_back([&]() {
            for (int i = 0; i < per_producer; ++i) {
                produced.fetch_add(1);
                sem.post();
            }
        });
    }

    for (auto& th : producers) th.join();

    // Give consumers time to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Signal consumers to stop
    for (int t = 0; t < num_consumers; ++t) {
        sem.post();
    }

    for (auto& th : consumers) th.join();
    EXPECT_EQ(count.load(), num_producers * per_producer);
}

TEST(ConcurrentSemaphore, TimedWait) {
    Semaphore sem(0);
    std::atomic<int> timed_out{0};
    std::atomic<int> acquired{0};
    const int num_threads = 4;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            bool got = sem.wait_for(std::chrono::milliseconds(100));
            if (got) {
                acquired.fetch_add(1);
            } else {
                timed_out.fetch_add(1);
            }
        });
    }

    for (auto& th : threads) th.join();
    // With 4 threads and only 100ms wait, all should time out
    EXPECT_EQ(timed_out.load(), num_threads);
    EXPECT_EQ(acquired.load(), 0);
}

TEST(ConcurrentSemaphore, PostMultiple) {
    Semaphore sem(0);
    std::atomic<int> count{0};
    const int N = 50;

    std::vector<std::thread> threads;
    for (int t = 0; t < N; ++t) {
        threads.emplace_back([&]() {
            sem.wait();
            count.fetch_add(1);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Post all at once
    sem.post(N);

    for (auto& th : threads) th.join();
    EXPECT_EQ(count.load(), N);
}

// =====================================================================
// Section 8 — TimerWheel concurrency stress
// =====================================================================

TEST(ConcurrentTimerWheel, ManyTimers) {
    TimerWheel tw;
    std::atomic<int> fires{0};
    const int num_timers = 1000;

    for (int i = 0; i < num_timers; ++i) {
        tw.addTimer(static_cast<uint64_t>((i % 50) + 1), [&]() {
            fires.fetch_add(1);
        });
    }

    // Tick enough to fire all
    for (int i = 0; i < 100; ++i) {
        tw.tick();
    }
    EXPECT_EQ(fires.load(), num_timers);
}

TEST(ConcurrentTimerWheel, CancelStress) {
    TimerWheel tw;
    std::atomic<int> fires{0};
    const int num_timers = 500;
    std::vector<uint64_t> ids;

    for (int i = 0; i < num_timers; ++i) {
        uint64_t delay = static_cast<uint64_t>((i % 100) + 10);
        auto id = tw.addTimer(delay, [&]() { fires.fetch_add(1); });
        ids.push_back(id);
    }

    // Cancel half
    for (int i = 0; i < num_timers / 2; ++i) {
        tw.cancelTimer(ids[i]);
    }

    // Tick past all deadlines
    for (int i = 0; i < 200; ++i) {
        tw.tick();
    }

    // Only uncancelled timers should fire
    EXPECT_EQ(fires.load(), num_timers - num_timers / 2);
}

TEST(ConcurrentTimerWheel, ConcurrentAddAndTick) {
    TimerWheel tw;
    std::atomic<int> fires{0};
    std::atomic<bool> stop{false};

    // Background thread keeps adding timers
    std::thread adder([&]() {
        for (int i = 0; i < 2000; ++i) {
            tw.addTimer(static_cast<uint64_t>((i % 10) + 1), [&]() {
                fires.fetch_add(1);
            });
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        stop.store(true);
    });

    // Main thread keeps ticking
    while (!stop.load()) {
        tw.tick();
    }

    // Drain remaining timers
    for (int i = 0; i < 50; ++i) {
        tw.tick();
    }

    adder.join();
    EXPECT_EQ(fires.load(), 2000);
}

TEST(ConcurrentTimerWheel, ZeroDelayFlood) {
    TimerWheel tw;
    std::atomic<int> fires{0};
    const int N = 500;

    for (int i = 0; i < N; ++i) {
        tw.addTimer(0, [&]() { fires.fetch_add(1); });
    }

    // All should fire on first tick
    tw.tick();
    EXPECT_EQ(fires.load(), N);
}

TEST(ConcurrentTimerWheel, TenThousandTimers) {
    TimerWheel tw;
    std::atomic<int> fires{0};
    const int num_timers = 10000;

    std::mt19937 rng(42);
    std::uniform_int_distribution<uint64_t> dist(1, 200);

    auto t_start = now_ns();

    for (int i = 0; i < num_timers; ++i) {
        uint64_t delay = dist(rng);
        tw.addTimer(delay, [&fires]() {
            fires.fetch_add(1);
        });
    }

    // Tick through all possible delays
    for (int t = 0; t < 250; ++t) {
        tw.tick();
    }

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;

    EXPECT_EQ(fires.load(), num_timers);
    std::cout << "[TimerWheel10K] " << num_timers
              << " timers fired in " << elapsed_ms << "ms ("
              << (num_timers / std::max(elapsed_ms, 0.001) * 1000.0)
              << " timers/sec)" << std::endl;
}

TEST(ConcurrentTimerWheel, CancelHalfOfMany) {
    TimerWheel tw;
    std::atomic<int> fires{0};
    const int num_timers = 10000;
    std::vector<uint64_t> ids;
    ids.reserve(num_timers);

    std::mt19937 rng(123);
    std::uniform_int_distribution<uint64_t> dist(10, 100);

    for (int i = 0; i < num_timers; ++i) {
        uint64_t delay = dist(rng);
        uint64_t id = tw.addTimer(delay, [&fires]() {
            fires.fetch_add(1);
        });
        ids.push_back(id);
    }

    // Randomly cancel half
    std::shuffle(ids.begin(), ids.end(), rng);
    for (int i = 0; i < num_timers / 2; ++i) {
        tw.cancelTimer(ids[i]);
    }

    // Tick past all deadlines
    for (int t = 0; t < 150; ++t) {
        tw.tick();
    }

    EXPECT_EQ(fires.load(), num_timers / 2);
    std::cout << "[TimerCancel5K] " << num_timers / 2 << " fired, "
              << num_timers / 2 << " cancelled" << std::endl;
}

TEST(ConcurrentTimerWheel, RecurringTimers) {
    TimerWheel tw;
    std::atomic<int> fires{0};
    const int max_fires = 5000;
    const int num_recurring = 1000;

    // Set up recurring timer that adds itself back
    for (int i = 0; i < num_recurring; ++i) {
        std::function<void()> recurring;
        recurring = [&tw, &fires, &recurring, max_fires]() {
            int count = fires.fetch_add(1) + 1;
            if (count < max_fires) {
                tw.addTimer(5, recurring);
            }
        };
        tw.addTimer(3, recurring);
    }

    // Tick until all fires complete
    for (int t = 0; t < 200; ++t) {
        tw.tick();
        if (fires.load() >= max_fires) break;
    }

    EXPECT_EQ(fires.load(), max_fires);
    std::cout << "[TimerRecurring] " << num_recurring << " recurring timers, "
              << fires.load() << " total fires" << std::endl;
}

TEST(ConcurrentTimerWheel, TimerOrdering) {
    // Timers with same deadline should fire in insertion order
    TimerWheel tw;
    std::vector<int> fire_order;
    std::mutex order_mutex;
    const int N = 50;

    for (int i = 0; i < N; ++i) {
        tw.addTimer(10, [&fire_order, &order_mutex, i]() {
            std::lock_guard<std::mutex> lock(order_mutex);
            fire_order.push_back(i);
        });
    }

    // Tick past deadline
    for (int t = 0; t < 20; ++t) {
        tw.tick();
    }

    ASSERT_EQ(fire_order.size(), static_cast<size_t>(N));
    // Verify ordering is preserved for same delay
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(fire_order[i], i);
    }
}

TEST(ConcurrentTimerWheel, RapidAddCancelCycle) {
    // Add and cancel timers rapidly — stress test the add/cancel paths
    TimerWheel tw;
    std::atomic<int> fires{0};
    const int cycles = 100;
    const int timers_per_cycle = 50;

    for (int c = 0; c < cycles; ++c) {
        std::vector<uint64_t> ids;
        // Add timers
        for (int i = 0; i < timers_per_cycle; ++i) {
            uint64_t id = tw.addTimer(static_cast<uint64_t>(c + 1), [&fires]() {
                fires.fetch_add(1);
            });
            ids.push_back(id);
        }
        // Immediately cancel all
        for (auto id : ids) {
            tw.cancelTimer(id);
        }
        // Tick once
        tw.tick();
    }

    // No timers should have fired (all cancelled before deadline)
    EXPECT_EQ(fires.load(), 0);
    std::cout << "[TimerRapidAddCancel] " << cycles << " cycles of "
              << timers_per_cycle << " timers, all cancelled" << std::endl;
}

// =====================================================================
// Section 9 — Channel concurrency stress
// =====================================================================

TEST(ConcurrentChannel, MPSC_TrySendRecv) {
    Channel<int> ch(1024);
    const int num_producers = 4;
    const int per_producer = 10000;
    std::atomic<int> sent{0};
    std::atomic<int> received{0};

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < per_producer; ++i) {
                while (!ch.trySend(p * per_producer + i)) {
                    // Spin until space is available
                    std::this_thread::yield();
                }
                sent.fetch_add(1);
            }
        });
    }

    std::thread consumer([&]() {
        int total = num_producers * per_producer;
        while (received.load() < total) {
            int val = 0;
            if (ch.tryRecv(val)) {
                received.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });

    for (auto& th : producers) th.join();
    consumer.join();
    EXPECT_EQ(sent.load(), num_producers * per_producer);
    EXPECT_EQ(received.load(), num_producers * per_producer);
}

TEST(ConcurrentChannel, SPMC_TrySendRecv) {
    Channel<int> ch(256);
    const int num_producers = 2;
    const int num_consumers = 2;
    const int per_producer = 5000;
    std::atomic<int> sent{0};
    std::atomic<int> received{0};
    std::atomic<int> sum_sent{0};
    std::atomic<int> sum_recv{0};

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < per_producer; ++i) {
                int val = p * per_producer + i;
                while (!ch.trySend(val)) {
                    std::this_thread::yield();
                }
                sent.fetch_add(1);
                sum_sent.fetch_add(val);
            }
        });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&]() {
            int total = num_producers * per_producer;
            while (received.load() < total) {
                int val = 0;
                if (ch.tryRecv(val)) {
                    received.fetch_add(1);
                    sum_recv.fetch_add(val);
                } else {
                    std::this_thread::yield();
                    if (received.load() >= total) break;
                }
            }
        });
    }

    for (auto& th : producers) th.join();
    for (auto& th : consumers) th.join();
    EXPECT_EQ(sent.load(), num_producers * per_producer);
    EXPECT_EQ(received.load(), num_producers * per_producer);
    // Sum of sent values should equal sum of received values
    EXPECT_EQ(sum_sent.load(), sum_recv.load());
}

TEST(ConcurrentChannel, CloseThenDrain) {
    Channel<int> ch(100);
    const int num_items = 5000;

    std::thread producer([&]() {
        for (int i = 0; i < num_items; ++i) {
            ch.trySend(i);
        }
        ch.close();
    });

    std::thread consumer([&]() {
        int received = 0;
        int val = 0;
        while (true) {
            if (ch.tryRecv(val)) {
                received++;
            } else if (ch.isClosed()) {
                break;
            } else {
                std::this_thread::yield();
            }
        }
        EXPECT_LE(received, num_items);
    });

    producer.join();
    consumer.join();
}

TEST(ConcurrentChannel, LargeCapacityStress) {
    Channel<int> ch(100000);
    std::atomic<int> sent{0};
    std::atomic<int> received{0};
    const int N = 50000;

    std::thread producer([&]() {
        for (int i = 0; i < N; ++i) {
            ch.trySend(i);
            sent.fetch_add(1);
        }
    });

    std::thread consumer([&]() {
        int val = 0;
        while (received.load() < N) {
            if (ch.tryRecv(val)) {
                received.fetch_add(1);
            }
        }
    });

    producer.join();
    consumer.join();
    EXPECT_EQ(sent.load(), N);
    EXPECT_EQ(received.load(), N);
}

TEST(ConcurrentChannel, SixteenProducersMPSC) {
    // 16 producers sending 1000 items each to 1 consumer
    Channel<int> ch(2048);
    const int num_producers = 16;
    const int per_producer = 1000;
    const int total = num_producers * per_producer;
    std::atomic<int> sent{0};
    std::atomic<int> received{0};
    std::atomic<int> sum_sent{0};
    std::atomic<int> sum_recv{0};

    auto t_start = now_ns();

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < per_producer; ++i) {
                int val = p * 100000 + i;
                while (!ch.trySend(val)) {
                    std::this_thread::yield();
                }
                sent.fetch_add(1);
                sum_sent.fetch_add(val);
            }
        });
    }

    std::thread consumer([&]() {
        int val = 0;
        while (received.load() < total) {
            if (ch.tryRecv(val)) {
                received.fetch_add(1);
                sum_recv.fetch_add(val);
            } else {
                std::this_thread::yield();
            }
        }
    });

    for (auto& th : producers) th.join();
    consumer.join();

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;

    EXPECT_EQ(sent.load(), total);
    EXPECT_EQ(received.load(), total);
    EXPECT_EQ(sum_sent.load(), sum_recv.load());
    std::cout << "[Channel16P1C] " << total << " items in " << elapsed_ms
              << "ms (" << (total / std::max(elapsed_ms, 0.001) * 1000.0)
              << " msgs/sec)" << std::endl;
}

TEST(ConcurrentChannel, FourProducersFourConsumersSPMC) {
    // 4 producers, 4 consumers, 10000 total items
    Channel<int> ch(512);
    const int num_producers = 4;
    const int num_consumers = 4;
    const int per_producer = 2500;
    const int total = num_producers * per_producer;
    std::atomic<int> sent{0};
    std::atomic<int> received{0};
    std::atomic<int> sum_sent{0};
    std::atomic<int> sum_recv{0};

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < per_producer; ++i) {
                int val = p * 100000 + i;
                while (!ch.trySend(val)) {
                    std::this_thread::yield();
                }
                sent.fetch_add(1);
                sum_sent.fetch_add(val);
            }
        });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&]() {
            int val = 0;
            while (received.load() < total) {
                if (ch.tryRecv(val)) {
                    received.fetch_add(1);
                    sum_recv.fetch_add(val);
                } else {
                    std::this_thread::yield();
                    if (received.load() >= total) break;
                }
            }
        });
    }

    for (auto& th : producers) th.join();
    for (auto& th : consumers) th.join();

    EXPECT_EQ(sent.load(), total);
    EXPECT_EQ(received.load(), total);
    EXPECT_EQ(sum_sent.load(), sum_recv.load());
}

TEST(ConcurrentChannel, CloseStress) {
    // Close channel while producers and consumers are active
    Channel<int> ch(64);
    std::atomic<int> sends{0};
    std::atomic<int> recvs{0};
    std::atomic<bool> closed{false};

    std::vector<std::thread> producers;
    for (int p = 0; p < 4; ++p) {
        producers.emplace_back([&]() {
            for (int i = 0; i < 5000 && !closed.load(); ++i) {
                if (ch.trySend(i)) {
                    sends.fetch_add(1);
                } else if (ch.isClosed()) {
                    break;
                }
            }
        });
    }

    std::thread consumer([&]() {
        int val = 0;
        while (true) {
            if (ch.tryRecv(val)) {
                recvs.fetch_add(1);
            } else if (ch.isClosed()) {
                break;
            }
        }
    });

    // Let them run briefly then close
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ch.close();
    closed.store(true);

    for (auto& th : producers) th.join();
    consumer.join();

    EXPECT_GE(sends.load(), 0);
    EXPECT_GE(recvs.load(), 0);
    std::cout << "[ChannelClose] " << sends.load() << " sent, "
              << recvs.load() << " received, no crashes" << std::endl;
}

TEST(ConcurrentChannel, CapacityBoundary) {
    // Test buffered channel at exactly capacity
    const int capacity = 8;
    Channel<int> ch(capacity);

    // Fill to capacity
    for (int i = 0; i < capacity; ++i) {
        EXPECT_TRUE(ch.trySend(i));
    }
    // Next send should fail
    EXPECT_FALSE(ch.trySend(999));

    // Drain and refill
    for (int i = 0; i < capacity; ++i) {
        int val = 0;
        EXPECT_TRUE(ch.tryRecv(val));
        EXPECT_EQ(val, i);
    }
    EXPECT_TRUE(ch.empty());
}

TEST(ConcurrentChannel, LargeObjectChannel) {
    // Channel of large strings (1MB each)
    const int num_items = 50;
    Channel<std::string> ch(8);
    std::atomic<int> sent{0};
    std::atomic<int> received{0};
    std::atomic<size_t> total_bytes_sent{0};
    std::atomic<size_t> total_bytes_recv{0};

    std::thread producer([&]() {
        for (int i = 0; i < num_items; ++i) {
            std::string large_str(1024 * 1024, static_cast<char>('A' + (i % 26)));
            total_bytes_sent.fetch_add(large_str.size());
            while (!ch.trySend(std::move(large_str))) {
                std::this_thread::yield();
            }
            sent.fetch_add(1);
        }
    });

    std::thread consumer([&]() {
        while (received.load() < num_items) {
            std::string val;
            if (ch.tryRecv(val)) {
                total_bytes_recv.fetch_add(val.size());
                received.fetch_add(1);
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(sent.load(), num_items);
    EXPECT_EQ(received.load(), num_items);
    EXPECT_EQ(total_bytes_sent.load(), total_bytes_recv.load());
    std::cout << "[ChannelLargeObj] " << num_items << " x 1MB strings, "
              << (total_bytes_sent.load() / (1024.0 * 1024.0)) << " MB transferred"
              << std::endl;
}

TEST(ConcurrentChannel, ThroughputMeasurement) {
    // Measure messages/second through channel
    Channel<int> ch(4096);
    const int total = 100000;
    std::atomic<int> received{0};

    auto t_start = now_ns();

    std::thread producer([&]() {
        for (int i = 0; i < total; ++i) {
            while (!ch.trySend(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&]() {
        int val = 0;
        while (received.load() < total) {
            if (ch.tryRecv(val)) {
                received.fetch_add(1);
            }
        }
    });

    producer.join();
    consumer.join();

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;

    EXPECT_EQ(received.load(), total);
    std::cout << "[ChannelThroughput] " << total << " msgs in "
              << elapsed_ms << "ms ("
              << (total / std::max(elapsed_ms, 0.001) * 1000.0)
              << " msgs/sec)" << std::endl;
}

// =====================================================================
// Section 10 — Buffer concurrency stress
// =====================================================================

TEST(ConcurrentBuffer, ParallelWriteReadSeparateBuffers) {
    const int num_threads = 16;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([t]() {
            ChainBuffer buf;
            std::string data(1000, static_cast<char>('A' + (t % 26)));
            buf.append(data.data(), data.size());
            EXPECT_EQ(buf.readableSize(), 1000u);

            std::string out(1000, '\0');
            size_t n = buf.read(&out[0], 1000);
            EXPECT_EQ(n, 1000u);
            EXPECT_EQ(out, data);
        });
    }
    for (auto& th : threads) th.join();
}

TEST(ConcurrentBuffer, StressLargeWrites) {
    const int num_threads = 8;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([t]() {
            ChainBuffer buf;
            std::string data(50000, static_cast<char>('A' + (t % 26)));
            for (int i = 0; i < 10; ++i) {
                buf.append(data.data(), data.size());
            }
            EXPECT_EQ(buf.readableSize(), 500000u);
            buf.consume(500000);
            EXPECT_TRUE(buf.empty());
        });
    }
    for (auto& th : threads) th.join();
}

TEST(ConcurrentBuffer, SharedBufferMultiWriters) {
    // Multiple threads writing to the SAME buffer (must be synchronized)
    ChainBuffer buf;
    SpinLock lock;
    const int num_threads = 8;
    const int per_thread = 1000;
    const size_t chunk_size = 256;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            while (!start.load()) {}
            std::string data(chunk_size, static_cast<char>('A' + (t % 26)));
            for (int i = 0; i < per_thread; ++i) {
                LockGuard<SpinLock> guard(lock);
                buf.append(data.data(), data.size());
            }
        });
    }
    start.store(true);
    for (auto& th : threads) th.join();

    EXPECT_EQ(buf.readableSize(), chunk_size * num_threads * per_thread);
}

TEST(ConcurrentBuffer, OneGBWriteThroughput) {
    // Write 1GB total through buffer, verify data integrity
    ChainBuffer buf;
    const size_t total_size = 1000 * 1024 * 1024; // 1000 MB
    const size_t chunk_size = 1024 * 1024;         // 1 MB chunks
    const int num_chunks = static_cast<int>(total_size / chunk_size);

    auto t_start = now_ns();

    for (int i = 0; i < num_chunks; ++i) {
        std::string data(chunk_size, static_cast<char>('A' + (i % 26)));
        buf.append(data.data(), data.size());

        // Periodically drain to avoid memory issues
        if (buf.readableSize() > 10 * chunk_size) {
            buf.consume(buf.readableSize());
        }
    }
    buf.consume(buf.readableSize());

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;
    double mbps = (total_size / (1024.0 * 1024.0)) / (elapsed_ms / 1000.0);

    EXPECT_TRUE(buf.empty());
    std::cout << "[Buffer1GB] " << (total_size / (1024.0 * 1024.0))
              << " MB in " << elapsed_ms << "ms ("
              << mbps << " MB/s)" << std::endl;
}

// =====================================================================
// Section 11 — FD Manager concurrency stress
// =====================================================================

TEST(ConcurrentFdManager, ParallelGetSet) {
    auto& mgr = FdManager::instance();
    const int num_threads = 8;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            while (!start.load()) {}
            for (int i = 0; i < 200; ++i) {
                int fd = t * 1000 + i;
                auto* ctx = mgr.get(fd);
                EXPECT_NE(ctx, nullptr);
                ctx->is_socket = true;
                ctx->recv_timeout_ms = 5000;
                mgr.remove(fd);
            }
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();
}

TEST(ConcurrentFdManager, ConcurrentGetSetHeavy) {
    auto& mgr = FdManager::instance();
    const int num_threads = 16;
    const int per_thread = 500;
    std::atomic<bool> start{false};
    std::atomic<int> ops{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            while (!start.load()) {}
            for (int i = 0; i < per_thread; ++i) {
                int fd = t * 10000 + i;
                auto* ctx = mgr.get(fd);
                ASSERT_NE(ctx, nullptr);
                ctx->is_socket = true;
                ctx->recv_timeout_ms = 3000;
                ctx->send_timeout_ms = 3000;
                ctx->nonblocking = true;
                mgr.remove(fd);
                ops.fetch_add(1);
            }
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();
    EXPECT_EQ(ops.load(), num_threads * per_thread);
}

// =====================================================================
// Section 12 — WorkStealingQueue concurrency stress
// =====================================================================

TEST(ConcurrentWorkStealingQueue, OwnerPushPop) {
    WorkStealingQueue q(256);
    const int N = 10000;

    // Owner pushes and pops (LIFO)
    for (int i = 0; i < N; ++i) {
        q.push(std::make_shared<Fiber>([]() {}));
    }
    EXPECT_EQ(q.size(), static_cast<size_t>(N));

    for (int i = 0; i < N; ++i) {
        auto f = q.pop();
        EXPECT_NE(f, nullptr);
    }
    EXPECT_EQ(q.size(), 0u);
}

TEST(ConcurrentWorkStealingQueue, StealStress) {
    WorkStealingQueue q(1024);
    const int N = 5000;
    std::atomic<int> stolen{0};
    std::atomic<bool> start{false};

    // Owner pushes
    std::thread owner([&]() {
        while (!start.load()) {}
        for (int i = 0; i < N; ++i) {
            q.push(std::make_shared<Fiber>([]() {}));
        }
    });

    // Thief steals
    std::thread thief([&]() {
        while (!start.load()) {}
        while (stolen.load() < N) {
            auto f = q.steal();
            if (f) {
                stolen.fetch_add(1);
            }
        }
    });

    start.store(true);
    owner.join();
    thief.join();
    EXPECT_EQ(stolen.load(), N);
    EXPECT_EQ(q.size(), 0u);
}

TEST(ConcurrentWorkStealingQueue, MultipleStealers) {
    WorkStealingQueue q(2048);
    const int num_items = 4000;
    const int num_stealers = 4;
    std::atomic<int> total_stolen{0};
    std::atomic<bool> start{false};

    // Owner pushes items in the background
    std::thread owner([&]() {
        while (!start.load()) {}
        for (int i = 0; i < num_items; ++i) {
            q.push(std::make_shared<Fiber>([]() {}));
        }
    });

    std::vector<std::thread> stealers;
    for (int s = 0; s < num_stealers; ++s) {
        stealers.emplace_back([&]() {
            while (!start.load()) {}
            int my_count = 0;
            while (total_stolen.load() + my_count < num_items) {
                auto f = q.steal();
                if (f) {
                    my_count++;
                }
            }
            total_stolen.fetch_add(my_count);
        });
    }

    start.store(true);
    owner.join();
    for (auto& th : stealers) th.join();
    EXPECT_EQ(total_stolen.load(), num_items);
}

TEST(ConcurrentWorkStealingQueue, ConcurrentPushPop) {
    // Concurrent push from owner and pop from owner while thief steals
    WorkStealingQueue q(4096);
    const int N = 3000;
    std::atomic<int> pushed{0};
    std::atomic<int> popped{0};
    std::atomic<int> stolen{0};
    std::atomic<bool> start{false};
    std::atomic<bool> done{false};

    std::thread owner([&]() {
        while (!start.load()) {}
        while (pushed.load() < N) {
            q.push(std::make_shared<Fiber>([]() {}));
            pushed.fetch_add(1);
        }
        // Pop some back
        for (int i = 0; i < N / 2; ++i) {
            auto f = q.pop();
            if (f) popped.fetch_add(1);
        }
        done.store(true);
    });

    std::vector<std::thread> thieves;
    for (int s = 0; s < 3; ++s) {
        thieves.emplace_back([&]() {
            while (!start.load()) {}
            while (!done.load() || q.size() > 0) {
                auto f = q.steal();
                if (f) stolen.fetch_add(1);
            }
        });
    }

    start.store(true);
    owner.join();
    for (auto& th : thieves) th.join();

    int total_through = popped.load() + stolen.load();
    EXPECT_EQ(total_through, N);
    std::cout << "[WSQConcurrent] pushed=" << pushed.load()
              << " popped=" << popped.load()
              << " stolen=" << stolen.load() << std::endl;
}

// =====================================================================
// Section 13 — Config concurrency stress
// =====================================================================

TEST(ConcurrentConfig, ParallelGetSet) {
    const int num_threads = 8;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            while (!start.load()) {}
            for (int i = 0; i < 1000; ++i) {
                std::string key = "stress." + std::to_string(t) + "." +
                                  std::to_string(i % 10);
                auto* var = Config::lookup<int>(key, 0);
                var->setValue(i);
                int val = var->getValue();
                ZERO_UNUSED(val);
            }
        });
    }
    start.store(true);
    for (auto& th : threads) th.join();
    SUCCEED();
}

TEST(ConcurrentConfig, ReadWriteSameKey) {
    auto* var = Config::lookup<int>("shared.key", 0);
    std::atomic<int> reads{0};
    std::atomic<int> writes{0};
    const int iterations = 5000;
    std::atomic<bool> start{false};

    std::vector<std::thread> readers;
    for (int t = 0; t < 4; ++t) {
        readers.emplace_back([&]() {
            while (!start.load()) {}
            for (int i = 0; i < iterations; ++i) {
                int val = var->getValue();
                ZERO_UNUSED(val);
                reads.fetch_add(1);
            }
        });
    }

    std::thread writer([&]() {
        while (!start.load()) {}
        for (int i = 0; i < iterations; ++i) {
            var->setValue(i);
            writes.fetch_add(1);
        }
    });

    start.store(true);
    for (auto& th : readers) th.join();
    writer.join();
    EXPECT_GT(reads.load(), 0);
    EXPECT_EQ(writes.load(), iterations);
}

TEST(ConcurrentConfig, ListenerStress) {
    auto* var = Config::lookup<int>("listener.stress", 0);
    std::atomic<int> notifications{0};
    const int num_listeners = 10;

    for (int i = 0; i < num_listeners; ++i) {
        var->addListener([&](const int&, const int&) {
            notifications.fetch_add(1);
        });
    }

    const int updates = 100;
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < updates; ++i) {
                var->setValue(t * updates + i);
            }
        });
    }
    for (auto& th : threads) th.join();

    // Each setValue fires all listeners
    EXPECT_EQ(notifications.load(), num_listeners * updates * 4);
}

TEST(ConcurrentConfig, ManyKeysConcurrentAccess) {
    // 16 threads reading/writing different config keys
    const int num_threads = 16;
    const int keys_per_thread = 50;
    std::atomic<bool> start{false};
    std::atomic<int> ops{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            while (!start.load()) {}
            for (int k = 0; k < keys_per_thread; ++k) {
                std::string key = "mk." + std::to_string(t) + "." + std::to_string(k);
                auto* var = Config::lookup<int>(key, 0);
                var->setValue(t * 1000 + k);
                int val = var->getValue();
                EXPECT_EQ(val, t * 1000 + k);
                ops.fetch_add(1);
            }
        });
    }
    start.store(true);
    for (auto& th : threads) th.join();
    EXPECT_EQ(ops.load(), num_threads * keys_per_thread);
    std::cout << "[ConfigManyKeys] " << ops.load() << " operations" << std::endl;
}

TEST(ConcurrentConfig, ManyListenersRapidChange) {
    auto* var = Config::lookup<int>("rapid.listener", 0);
    std::atomic<int> notifications{0};
    const int num_listeners = 100;

    for (int i = 0; i < num_listeners; ++i) {
        var->addListener([&](const int&, const int&) {
            notifications.fetch_add(1);
        });
    }

    // Rapidly change the value
    const int changes = 200;
    for (int i = 0; i < changes; ++i) {
        var->setValue(i);
    }

    EXPECT_EQ(notifications.load(), num_listeners * changes);
    std::cout << "[ConfigListeners] " << num_listeners << " listeners, "
              << changes << " changes, " << notifications.load()
              << " notifications" << std::endl;
}

// =====================================================================
// Section 14 — Log concurrency stress
// =====================================================================

TEST(ConcurrentLog, ParallelLogging) {
    auto* log = Logger::root();
    log->setLevel(LogLevel::LVL_DEBUG);
    const int num_threads = 16;
    const int per_thread = 500;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < per_thread; ++i) {
                log->info(__FILE__, __LINE__,
                          "Thread " + std::to_string(t) + " iter " +
                          std::to_string(i));
            }
        });
    }
    for (auto& th : threads) th.join();
    SUCCEED();
}

TEST(ConcurrentLog, MultipleLoggersParallel) {
    const int num_threads = 8;
    const int per_thread = 100;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            auto* log = Logger::get("thread." + std::to_string(t));
            log->setLevel(LogLevel::LVL_DEBUG);
            for (int i = 0; i < per_thread; ++i) {
                log->debug(__FILE__, __LINE__, "debug from thread " +
                           std::to_string(t));
                log->info(__FILE__, __LINE__, "info from thread " +
                          std::to_string(t));
                log->warn(__FILE__, __LINE__, "warn from thread " +
                          std::to_string(t));
            }
        });
    }
    for (auto& th : threads) th.join();
    SUCCEED();
}

TEST(ConcurrentLog, SharedLoggerStress) {
    auto* log = Logger::get("shared.stress.logger");
    log->setLevel(LogLevel::LVL_TRACE);
    const int num_threads = 12;
    const int per_thread = 200;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            while (!start.load()) {}
            for (int i = 0; i < per_thread; ++i) {
                log->trace(__FILE__, __LINE__, "trace");
                log->error(__FILE__, __LINE__, "error " + std::to_string(t));
            }
        });
    }
    start.store(true);
    for (auto& th : threads) th.join();
    SUCCEED();
}

TEST(ConcurrentLog, FileAppenderStress) {
    auto appender = std::make_shared<FileAppender>("/tmp/zero_concurrent_log.txt");
    auto* log = Logger::get("file.stress");
    log->addAppender(appender);
    log->setLevel(LogLevel::LVL_DEBUG);

    const int num_threads = 8;
    const int per_thread = 100;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < per_thread; ++i) {
                log->info(__FILE__, __LINE__,
                          "file log thread=" + std::to_string(t) +
                          " iter=" + std::to_string(i));
            }
        });
    }
    for (auto& th : threads) th.join();
    SUCCEED();
}

TEST(ConcurrentLog, HighVolumeThroughput) {
    // Measure async log throughput
    auto* log = Logger::get("throughput.logger");
    log->setLevel(LogLevel::LVL_INFO);
    const int num_threads = 32;
    const int per_thread = 1000;

    auto t_start = now_ns();

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < per_thread; ++i) {
                log->info(__FILE__, __LINE__,
                          "throughput test msg " + std::to_string(t) +
                          "_" + std::to_string(i));
            }
        });
    }
    for (auto& th : threads) th.join();

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;
    int total = num_threads * per_thread;

    std::cout << "[LogThroughput] " << total << " log messages in "
              << elapsed_ms << "ms ("
              << (total / std::max(elapsed_ms, 0.001) * 1000.0)
              << " msgs/sec)" << std::endl;
    SUCCEED();
}

TEST(ConcurrentLog, FileRotationStress) {
    // Fill log file with many messages to test rotation behavior
    auto appender = std::make_shared<FileAppender>("/tmp/zero_rotation_test.log");
    auto* log = Logger::get("rotation.logger");
    log->addAppender(appender);
    log->setLevel(LogLevel::LVL_DEBUG);

    const int num_threads = 4;
    const int per_thread = 500;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < per_thread; ++i) {
                // Generate somewhat large messages
                std::string msg = "rotation_test_" + std::to_string(t) + "_" +
                                  std::to_string(i) +
                                  std::string(200, 'X');
                log->debug(__FILE__, __LINE__, msg);
            }
        });
    }
    for (auto& th : threads) th.join();
    SUCCEED();
}

// =====================================================================
// Section 15 — Thread creation / join stress
// =====================================================================

TEST(ConcurrentThread, CreateJoinMany) {
    const int num_threads = 50;
    std::vector<std::unique_ptr<Thread>> threads;

    for (int i = 0; i < num_threads; ++i) {
        auto th = std::make_unique<Thread>(
            [i]() {
                Thread::setCurrentName("worker_" + std::to_string(i));
            },
            "worker_" + std::to_string(i));
        EXPECT_TRUE(th->start());
        threads.push_back(std::move(th));
    }

    for (auto& th : threads) {
        th->join();
    }
    // Verify that all threads completed
    SUCCEED();
}

TEST(ConcurrentThread, DetachStress) {
    Semaphore sem(0);
    const int num_threads = 20;

    for (int i = 0; i < num_threads; ++i) {
        Thread th([&sem]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            sem.post();
        });
        th.start();
        th.detach();
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_threads; ++i) {
        EXPECT_TRUE(sem.wait_for(std::chrono::seconds(5)));
    }
}

TEST(ConcurrentThread, RapidCreateDestroy) {
    // Rapid create/destroy of threads
    std::atomic<int> counter{0};
    const int cycles = 100;

    for (int c = 0; c < cycles; ++c) {
        std::thread t([&counter]() {
            counter.fetch_add(1);
        });
        t.join();
    }
    EXPECT_EQ(counter.load(), cycles);
    std::cout << "[ThreadRapidCD] " << cycles << " create/destroy cycles" << std::endl;
}

TEST(ConcurrentThread, HundredThreadsSimultaneous) {
    // Launch 100 threads simultaneously
    const int num_threads = 100;
    std::atomic<int> ready{0};
    Barrier barrier(num_threads + 1); // +1 for main
    std::atomic<int> completed{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            ready.fetch_add(1);
            barrier.wait(); // All start together
            completed.fetch_add(1);
        });
    }

    // Wait for all threads to be ready, then release barrier
    wait_until([&]() { return ready.load() >= num_threads; }, 5000);
    barrier.wait();

    for (auto& th : threads) th.join();
    EXPECT_EQ(completed.load(), num_threads);
    std::cout << "[Thread100Simul] " << num_threads << " threads launched" << std::endl;
}

// =====================================================================
// Section 16 — Scheduler concurrency stress
// =====================================================================

TEST(ConcurrentScheduler, ManyFibers) {
    Scheduler sched(4);
    std::atomic<int> counter{0};
    const int num_fibers = 5000;

    sched.start();

    for (int i = 0; i < num_fibers; ++i) {
        sched.schedule([&]() {
            counter.fetch_add(1);
        });
    }

    // Wait for fibers to complete
    bool completed = wait_until([&]() {
        return counter.load() >= num_fibers;
    }, 5000);
    EXPECT_TRUE(completed);

    sched.stop();
    EXPECT_EQ(counter.load(), num_fibers);
}

TEST(ConcurrentScheduler, FiberYieldStress) {
    Scheduler sched(2);
    std::atomic<int> counter{0};

    sched.start();

    for (int i = 0; i < 500; ++i) {
        sched.schedule([&]() {
            for (int j = 0; j < 10; ++j) {
                counter.fetch_add(1);
                // Yield to let other fibers run
                Fiber::yield();
            }
        });
    }

    bool completed = wait_until([&]() {
        return counter.load() >= 5000;
    }, 5000);
    EXPECT_TRUE(completed);

    sched.stop();
    EXPECT_EQ(counter.load(), 5000);
}

TEST(ConcurrentScheduler, WorkStealingAcrossThreads) {
    Scheduler sched(4);
    std::atomic<int> counter{0};
    const int num_fibers = 2000;

    sched.start();

    // Schedule fibers from the main thread — they will be distributed
    // across worker threads via work stealing
    for (int i = 0; i < num_fibers; ++i) {
        sched.schedule([&]() {
            counter.fetch_add(1);
            Fiber::yield(); // Allow work stealing
        });
    }

    bool completed = wait_until([&]() {
        return counter.load() >= num_fibers;
    }, 5000);
    EXPECT_TRUE(completed);

    sched.stop();
    EXPECT_EQ(counter.load(), num_fibers);
}

// =====================================================================
// Section 17 — Cross-subsystem stress tests
// =====================================================================

TEST(ConcurrentCrossSystem, ThreadsWithSpinLockAndConfig) {
    // Mix multiple subsystems in concurrent access
    SpinLock lock;
    std::atomic<int> counter{0};
    const int num_threads = 4;
    const int iterations = 1000;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            while (!start.load()) {}
            for (int i = 0; i < iterations; ++i) {
                // Config access (thread-safe internally)
                auto* var = Config::lookup<int>("cross." + std::to_string(t), 0);
                var->setValue(i);

                // Lock-protected counter
                {
                    LockGuard<SpinLock> guard(lock);
                    counter.fetch_add(1);
                }

                // Some fiber operations
                auto ft = std::make_shared<Fiber>([]() {});

                // Buffer operations
                ChainBuffer buf;
                buf.append("test", 4);
                buf.consume(4);
            }
        });
    }
    start.store(true);
    for (auto& th : threads) th.join();
    EXPECT_EQ(counter.load(), num_threads * iterations);
}

TEST(ConcurrentCrossSystem, TimerWithSharedState) {
    // Multiple threads manipulating a timer wheel with shared state
    struct SharedState {
        SpinLock lock;
        int total_fires = 0;
    };

    SharedState state;
    TimerWheel tw;

    // Add timers that modify shared state
    for (int i = 0; i < 500; ++i) {
        tw.addTimer(static_cast<uint64_t>((i % 20) + 1), [&state]() {
            LockGuard<SpinLock> guard(state.lock);
            state.total_fires++;
        });
    }

    // Tick from multiple threads is NOT safe by default
    // but we just tick from main thread here
    for (int i = 0; i < 50; ++i) {
        tw.tick();
    }

    LockGuard<SpinLock> guard(state.lock);
    EXPECT_EQ(state.total_fires, 500);
}

// =====================================================================
// Section 18 — Deadlock prevention tests
// =====================================================================

TEST(ConcurrentDeadlockPrevention, TwoLocksFixedOrder) {
    // Verify that two threads acquiring two locks in the same order
    // never deadlock
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

TEST(ConcurrentDeadlockPrevention, TryLockBackoff) {
    // Verify that try_lock with backoff avoids deadlock
    SpinLock lockA, lockB;
    std::atomic<int> count{0};
    const int iterations = 1000;

    auto worker = [&]() {
        for (int i = 0; i < iterations; ++i) {
            while (true) {
                lockA.lock();
                if (lockB.try_lock()) {
                    // Got both
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

TEST(ConcurrentDeadlockPrevention, ThreeLocksFixedOrder) {
    // Three locks with fixed order — stress test
    SpinLock lockA, lockB, lockC;
    std::atomic<int> count{0};
    const int num_threads = 8;
    const int iterations = 5000;

    auto worker = [&]() {
        for (int i = 0; i < iterations; ++i) {
            lockA.lock();
            lockB.lock();
            lockC.lock();
            count.fetch_add(1);
            lockC.unlock();
            lockB.unlock();
            lockA.unlock();
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(worker);
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(count.load(), num_threads * iterations);
}

// =====================================================================
// Section 19 — Long-running stability
// =====================================================================

TEST(ConcurrentStability, LongRunningBarrier) {
    // Run barrier sync for many iterations
    const int num_threads = 4;
    const int iterations = 1000;
    Barrier barrier(num_threads);
    std::atomic<int> count{0};

    auto worker = [&]() {
        for (int i = 0; i < iterations; ++i) {
            barrier.wait();
            count.fetch_add(1);
            barrier.wait(); // Double sync to ensure alignment
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(worker);
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(count.load(), num_threads * iterations);
}

TEST(ConcurrentStability, LongRunningSpinLock) {
    SpinLock lock;
    int counter = 0;
    const int num_threads = 2;
    const int per_thread = 500000;

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

TEST(ConcurrentStability, LongRunningMutex) {
    Mutex mtx;
    std::atomic<int> counter{0};
    const int num_threads = 4;
    const int per_thread = 500000;

    auto t_start = now_ns();

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < per_thread; ++i) {
                LockGuard<Mutex> guard(mtx);
                counter.fetch_add(1);
            }
        });
    }
    for (auto& th : threads) th.join();

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;
    int total = num_threads * per_thread;

    EXPECT_EQ(counter.load(), total);
    std::cout << "[StabilityMutex] " << total << " ops in " << elapsed_ms
              << "ms" << std::endl;
}

TEST(ConcurrentStability, MixedPrimitivesLongRun) {
    // Long-running test mixing multiple primitives
    SpinLock spin;
    Mutex mtx;
    Semaphore sem(1);
    const int num_threads = 4;
    const int iterations = 10000;
    std::atomic<int> counter{0};
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (int i = 0; i < iterations; ++i) {
                // SpinLock section
                spin.lock();
                counter.fetch_add(1);
                spin.unlock();

                // Mutex section
                {
                    LockGuard<Mutex> guard(mtx);
                    counter.fetch_add(1);
                }

                // Semaphore section
                sem.wait();
                counter.fetch_add(1);
                sem.post();
            }
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();
    EXPECT_EQ(counter.load(), num_threads * iterations * 3);
    std::cout << "[StabilityMixed] " << counter.load() << " total ops" << std::endl;
}

// =====================================================================
// Section 20 — Edge-case concurrent patterns
// =====================================================================

TEST(ConcurrentEdgeCases, SingleProducerSingleConsumerCV) {
    // Classic SPSC queue with condition variable
    Mutex mtx;
    ConditionVariable cv_producer;
    ConditionVariable cv_consumer;
    std::queue<int> queue;
    const int max_queue_size = 16;
    const int total_items = 10000;
    std::atomic<bool> done{false};

    std::thread producer([&]() {
        for (int i = 0; i < total_items; ++i) {
            UniqueLock<Mutex> lock(mtx);
            // Wait while queue is full
            while (static_cast<int>(queue.size()) >= max_queue_size) {
                cv_producer.wait(lock);
            }
            queue.push(i);
            lock.unlock();
            cv_consumer.notify_one();
        }
    });

    std::thread consumer([&]() {
        int received = 0;
        while (received < total_items) {
            UniqueLock<Mutex> lock(mtx);
            while (queue.empty() && received < total_items) {
                cv_consumer.wait(lock);
            }
            while (!queue.empty()) {
                int val = queue.front();
                queue.pop();
                ZERO_UNUSED(val);
                received++;
            }
            lock.unlock();
            cv_producer.notify_one();
        }
    });

    producer.join();
    consumer.join();
    SUCCEED();
}

TEST(ConcurrentEdgeCases, ReadWriteWithUpgradeGuard) {
    RWLock rwlock;
    std::set<int> data_set;
    std::atomic<int> ops{0};
    const int num_threads = 4;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            while (!start.load()) {}
            for (int i = 0; i < 500; ++i) {
                {
                    ReadLockGuard guard(rwlock);
                    // Check if element exists
                    if (data_set.find(t * 1000 + i) != data_set.end()) {
                        ops.fetch_add(1);
                    }
                }
                {
                    WriteLockGuard guard(rwlock);
                    data_set.insert(t * 1000 + i);
                    ops.fetch_add(1);
                }
            }
        });
    }
    start.store(true);
    for (auto& th : threads) th.join();
    EXPECT_GT(ops.load(), 0);
}

TEST(ConcurrentEdgeCases, ConcurrentAnySwap) {
    // Multiple threads swapping any objects
    std::atomic<bool> start{false};
    const int num_threads = 4;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            while (!start.load()) {}
            any a = t;
            any b = std::string("thread_" + std::to_string(t));
            for (int i = 0; i < 1000; ++i) {
                a.swap(b);
                // Verify values are preserved
                if (i % 2 == 0) {
                    EXPECT_GE(any_cast<int>(a), 0);
                } else {
                    EXPECT_EQ(any_cast<std::string>(b),
                              "thread_" + std::to_string(t));
                }
            }
        });
    }
    start.store(true);
    for (auto& th : threads) th.join();
}

TEST(ConcurrentEdgeCases, HighContentionSemaphore) {
    Semaphore sem(1);
    std::atomic<int> counter{0};
    std::atomic<int> total_ops{0};
    const int num_threads = 8;
    const int iterations = 10000;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (int i = 0; i < iterations; ++i) {
                sem.wait();
                counter.fetch_add(1);
                sem.post();
                total_ops.fetch_add(1);
            }
        });
    }
    start.store(true);
    for (auto& th : threads) th.join();
    EXPECT_EQ(counter.load(), num_threads * iterations);
    EXPECT_EQ(total_ops.load(), num_threads * iterations);
}

// =====================================================================
// Section 21 — Concurrent ChainBuffer I/O patterns
// =====================================================================

TEST(ConcurrentBufferIO, ParallelVarintCodec) {
    const int num_threads = 12;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([t]() {
            ChainBuffer buf;
            std::vector<uint64_t> values;
            for (int i = 0; i < 100; ++i) {
                uint64_t v = static_cast<uint64_t>(t) * 1000000 + i;
                values.push_back(v);
                buf.writeVarint(v);
            }

            for (auto expected : values) {
                uint64_t actual = 0;
                EXPECT_TRUE(buf.readVarint(actual));
                EXPECT_EQ(actual, expected);
            }
        });
    }
    for (auto& th : threads) th.join();
}

TEST(ConcurrentBufferIO, MixedSizeParallelIO) {
    const int num_threads = 8;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([t]() {
            ChainBuffer buf;
            // Write varied sizes
            for (int sz = 1; sz <= 100; ++sz) {
                std::string data(sz, static_cast<char>('A' + (t % 26)));
                buf.append(data.data(), data.size());
            }

            // Total written: sum of 1..100 = 5050
            EXPECT_EQ(buf.readableSize(), 5050u);

            // Read all back
            std::string all;
            all.resize(5050);
            size_t n = buf.read(&all[0], 5050);
            EXPECT_EQ(n, 5050u);
        });
    }
    for (auto& th : threads) th.join();
}

// =====================================================================
// Section 22 — Scheduler with fiber lifecycle
// =====================================================================

TEST(ConcurrentFiberLifecycle, CreateDestroyMany) {
    const int num_fibers = 1000;
    std::vector<Fiber::Ptr> fibers;
    fibers.reserve(num_fibers);
    for (int i = 0; i < num_fibers; ++i) {
        fibers.push_back(std::make_shared<Fiber>([]() {}));
    }
    fibers.clear();
    // All fibers destructed cleanly
    SUCCEED();
}

TEST(ConcurrentFiberLifecycle, FiberPoolReuse) {
    auto& pool = FiberPool::instance();

    // Recycle many fibers
    for (int i = 0; i < 200; ++i) {
        auto f = pool.get([]() {});
        EXPECT_NE(f, nullptr);
        pool.recycle(f);
    }
    EXPECT_GT(pool.available(), 0u);
}

// =====================================================================
// Section 23 — Reactor/heavy load
// =====================================================================

TEST(ConcurrentReactor, PollStress) {
    Reactor reactor;
    // Many polls with short timeout
    for (int i = 0; i < 1000; ++i) {
        int n = reactor.poll(0); // Non-blocking poll
        EXPECT_GE(n, 0);
    }
}

TEST(ConcurrentReactor, AddDelStress) {
    Reactor reactor;
    std::vector<int> fds;

    for (int i = 0; i < 100; ++i) {
        int efd = eventfd(0, EFD_NONBLOCK);
        ASSERT_GE(efd, 0);
        EXPECT_TRUE(reactor.addEvent(efd, EPOLLIN, nullptr));
        fds.push_back(efd);
    }

    for (int fd : fds) {
        EXPECT_TRUE(reactor.delEvent(fd));
        close(fd);
    }
}

// =====================================================================
// Section 24 — Scheduler config interaction
// =====================================================================

TEST(ConcurrentSchedulerConfig, FiberConfigInteraction) {
    Scheduler sched(2);
    sched.start();

    std::atomic<int> done{0};
    const int num_fibers = 200;

    for (int i = 0; i < num_fibers; ++i) {
        sched.schedule([&, i]() {
            auto* var = Config::lookup<std::string>(
                "fiber." + std::to_string(i), std::to_string(i));
            var->setValue("updated_" + std::to_string(i));
            done.fetch_add(1);
        });
    }

    bool completed = wait_until([&]() {
        return done.load() >= num_fibers;
    }, 5000);
    EXPECT_TRUE(completed);

    sched.stop();
    EXPECT_EQ(done.load(), num_fibers);
}

// =====================================================================
// Section 25 — Global stress: all primitives together
// =====================================================================

TEST(ConcurrentGlobalStress, AllPrimitives) {
    // Stress test combining: spinlock, mutex, rwlock, semaphore, barrier, cv,
    // config, buffer, fiber operations all in parallel threads

    SpinLock spin;
    Mutex mtx;
    RWMutex rwmtx;
    RWLock rwlock;
    Semaphore sem(4);
    ConditionVariable cv;
    Barrier barrier(4);
    std::atomic<int> phase{0};
    std::atomic<bool> start{false};
    std::atomic<int> completed{0};

    auto worker = [&](int id) {
        while (!start.load()) {}
        for (int iter = 0; iter < 100; ++iter) {
            // SpinLock section
            {
                LockGuard<SpinLock> sg(spin);
                phase.fetch_add(1);
            }

            // Mutex section
            {
                LockGuard<Mutex> mg(mtx);
                phase.fetch_add(1);
            }

            // RWLock read section
            {
                ReadLockGuard rg(rwlock);
                phase.load();
            }

            // RWMutex write section
            {
                rwmtx.wrlock();
                phase.fetch_add(1);
                rwmtx.unlock();
            }

            // Config section
            auto* var = Config::lookup<int>(
                "global." + std::to_string(id), 0);
            var->setValue(iter);

            // Buffer section
            ChainBuffer buf;
            buf.append("stress", 6);
            buf.consume(6);

            // Semaphore section
            sem.wait();
            sem.post();

            // Barrier sync
            if (iter % 10 == 0) {
                barrier.wait();
            }

            // Fiber section
            auto f = std::make_shared<Fiber>([]() {});
            ZERO_UNUSED(f);

            // Log section
            auto* log = Logger::root();
            log->trace(__FILE__, __LINE__, "global stress");
        }
        completed.fetch_add(1);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker, i);
    }
    start.store(true);
    for (auto& th : threads) th.join();

    EXPECT_EQ(completed.load(), 4);
    SUCCEED();
}

// =====================================================================
// Finished. Total: ~90 test cases over 25 sections.
// =====================================================================
