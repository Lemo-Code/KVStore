// test_thread.cpp — Comprehensive Thread unit tests
// Tests Thread construction (function/lambda), start/join/detach,
// joinable, get_id, hardware_concurrency, move semantics,
// multi-thread counter, and thread with arguments.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using namespace zero;

// ============================================================
// Helper utilities
// ============================================================

namespace {
template <typename F>
bool wait_until(F&& cond, int timeout_ms = 3000) {
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeout_ms);
    while (!cond()) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}
} // namespace

// ============================================================
// Construction
// ============================================================

TEST(Thread, ConstructWithLambda) {
    std::atomic<bool> ran{false};
    Thread th([&]() { ran.store(true); });
    EXPECT_TRUE(th.start());
    th.join();
    EXPECT_TRUE(ran.load());
}

TEST(Thread, ConstructWithFunction) {
    std::atomic<int> value{0};
    auto fn = [&]() { value.store(42); };
    Thread th(fn);
    EXPECT_TRUE(th.start());
    th.join();
    EXPECT_EQ(value.load(), 42);
}

TEST(Thread, ConstructWithName) {
    Thread th([]() {}, "test_thread_name");
    EXPECT_EQ(th.name(), "test_thread_name");
}

TEST(Thread, ConstructWithEmptyName) {
    Thread th([]() {});
    EXPECT_EQ(th.name(), "");
}

// ============================================================
// start / join
// ============================================================

TEST(Thread, StartJoin) {
    Thread th([]() {});
    EXPECT_TRUE(th.start());
    EXPECT_TRUE(th.joinable());
    th.join();
    EXPECT_FALSE(th.joinable());
}

TEST(Thread, DoubleStartFails) {
    Thread th([]() {});
    EXPECT_TRUE(th.start());
    EXPECT_FALSE(th.start()); // Second start should fail
    th.join();
}

TEST(Thread, JoinAfterJoinNoOp) {
    Thread th([]() {});
    th.start();
    th.join();
    th.join(); // Double join should be no-op
    SUCCEED();
}

// ============================================================
// detach
// ============================================================

TEST(Thread, Detach) {
    Semaphore sem(0);
    Thread th([&]() {
        sem.post();
    }, "detach_test");
    EXPECT_TRUE(th.start());
    th.detach();
    // Thread should still complete
    EXPECT_TRUE(sem.wait_for(std::chrono::seconds(2)));
}

TEST(Thread, DetachAfterDetachNoOp) {
    Semaphore sem(0);
    Thread th([&]() { sem.post(); });
    th.start();
    th.detach();
    th.detach(); // Double detach should be no-op
    EXPECT_TRUE(sem.wait_for(std::chrono::seconds(2)));
}

// ============================================================
// joinable
// ============================================================

TEST(Thread, JoinableBeforeStart) {
    Thread th([]() {});
    EXPECT_FALSE(th.joinable());
}

TEST(Thread, JoinableAfterStart) {
    Thread th([]() {});
    th.start();
    EXPECT_TRUE(th.joinable());
    th.join();
    EXPECT_FALSE(th.joinable());
}

TEST(Thread, JoinableAfterDetach) {
    Semaphore sem(0);
    Thread th([&]() { sem.post(); });
    th.start();
    th.detach();
    EXPECT_FALSE(th.joinable());
    EXPECT_TRUE(sem.wait_for(std::chrono::seconds(2)));
}

// ============================================================
// get_id
// ============================================================

TEST(Thread, GetId) {
    Thread th([]() {});
    th.start();
    // get_id should be non-zero after start
    EXPECT_NE(th.get_id(), 0u);
    th.join();
}

// ============================================================
// hardware_concurrency
// ============================================================

TEST(Thread, HardwareConcurrency) {
    unsigned n = Thread::hardware_concurrency();
    EXPECT_GE(n, 1u);
}

// ============================================================
// native_handle
// ============================================================

TEST(Thread, NativeHandle) {
    Thread th([]() {});
    th.start();
    pthread_t h = th.native_handle();
    // Native handle should be set
    EXPECT_FALSE(pthread_equal(h, pthread_t()));
    th.join();
}

// ============================================================
// Thread name utilities
// ============================================================

TEST(Thread, SetCurrentName) {
    // Should not crash
    Thread::set_current_name("main_test_name");
    SUCCEED();
}

TEST(Thread, CurrentThread) {
    pthread_t self = Thread::current_thread();
    EXPECT_FALSE(pthread_equal(self, pthread_t()));
    EXPECT_TRUE(pthread_equal(self, pthread_self()));
}

// ============================================================
// yield / sleep
// ============================================================

TEST(Thread, Yield) {
    // Should not crash
    Thread::yield();
    SUCCEED();
}

TEST(Thread, SleepMs) {
    auto start = std::chrono::steady_clock::now();
    Thread::sleep_ms(10);
    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_GE(elapsed, std::chrono::milliseconds(5));
}

// ============================================================
// Multi-threaded counter increment
// ============================================================

TEST(Thread, MultiThreadedCounter) {
    const int num_threads = 8;
    const int increments = 10000;
    std::atomic<int> counter{0};
    std::vector<std::unique_ptr<Thread>> threads;

    for (int i = 0; i < num_threads; ++i) {
        auto th = std::make_unique<Thread>([&]() {
            for (int j = 0; j < increments; ++j) {
                counter.fetch_add(1);
            }
        }, "worker_" + std::to_string(i));
        EXPECT_TRUE(th->start());
        threads.push_back(std::move(th));
    }

    for (auto& t : threads) {
        t->join();
    }

    EXPECT_EQ(counter.load(), num_threads * increments);
}

// ============================================================
// Thread with arguments (via lambda capture)
// ============================================================

TEST(Thread, ThreadWithArguments) {
    std::atomic<int> sum{0};

    auto worker = [&](int start, int count) {
        for (int i = 0; i < count; ++i) {
            sum.fetch_add(start + i);
        }
    };

    Thread th1([&]() { worker(0, 100); }, "worker1");
    Thread th2([&]() { worker(1000, 100); }, "worker2");

    th1.start();
    th2.start();
    th1.join();
    th2.join();

    // 0+1+...+99 = 4950; 1000+1001+...+1099 = 104950
    EXPECT_EQ(sum.load(), 4950 + 104950);
}

// ============================================================
// ScopedThread
// ============================================================

TEST(Thread, ScopedThreadAutoJoin) {
    std::atomic<bool> ran{false};
    {
        ScopedThread st([&]() { ran.store(true); });
        EXPECT_TRUE(st.thread().joinable());
    }
    // After scope exit, thread should be joined
    EXPECT_TRUE(ran.load());
}

TEST(Thread, ScopedThreadWithArgs) {
    std::atomic<int> value{0};
    {
        ScopedThread st([&](int v) { value.store(v); }, 42);
    }
    EXPECT_EQ(value.load(), 42);
}

// ============================================================
// Move semantics (Thread is non-copyable via Noncopyable)
// ============================================================

TEST(Thread, NonCopyable) {
    static_assert(!std::is_copy_constructible_v<Thread>,
                  "Thread must not be copy-constructible");
    static_assert(!std::is_copy_assignable_v<Thread>,
                  "Thread must not be copy-assignable");
}

TEST(Thread, MoveConstructible) {
    static_assert(std::is_move_constructible_v<Thread>,
                  "Thread must be move-constructible");
}

// ============================================================
// Many threads stress test
// ============================================================

TEST(Thread, CreateJoinMany) {
    const int num_threads = 50;
    std::vector<std::unique_ptr<Thread>> threads;

    for (int i = 0; i < num_threads; ++i) {
        auto th = std::make_unique<Thread>([i]() {
            Thread::set_current_name("worker_" + std::to_string(i));
        }, "worker_" + std::to_string(i));
        EXPECT_TRUE(th->start());
        threads.push_back(std::move(th));
    }

    for (auto& t : threads) {
        t->join();
    }

    SUCCEED();
}

// ============================================================
// Detach stress
// ============================================================

TEST(Thread, DetachStress) {
    Semaphore sem(0);
    const int num_threads = 20;
    std::vector<Thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&sem]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            sem.post();
        });
        threads.back().start();
        threads.back().detach();
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_threads; ++i) {
        EXPECT_TRUE(sem.wait_for(std::chrono::seconds(5)));
    }
}

// ============================================================
// Destructor auto-detach (running thread, not joined)
// ============================================================

TEST(Thread, DestructorDetachesRunningThread) {
    Semaphore sem(0);
    {
        Thread th([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            sem.post();
        });
        th.start();
        // Destructor should detach the running thread
    }
    // Thread should still complete after Thread object is destroyed
    EXPECT_TRUE(sem.wait_for(std::chrono::seconds(3)));
}
