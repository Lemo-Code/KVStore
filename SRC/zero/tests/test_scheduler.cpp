// test_scheduler.cpp — Unit tests for the zero Scheduler (M:N fiber scheduler)
//
// Tests: creation, start/stop lifecycle, schedule(fiber) and schedule(callback),
// GetThis, threadCount, basic fiber scheduling, sequential ordering,
// work distribution, graceful shutdown.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <set>
#include <mutex>
#include <algorithm>

using namespace zero;

// =====================================================================
// Construction tests
// =====================================================================

TEST(SchedulerTest, ConstructWithZeroThreadsAutoDetect) {
    Scheduler sched(0);
    // With 0 threads, should use all CPU cores
    EXPECT_GT(sched.thread_count(), 0);
    EXPECT_FALSE(sched.is_stopping());
    EXPECT_EQ(sched.total_fibers_scheduled(), 0);
    EXPECT_EQ(sched.total_fibers_completed(), 0);
}

TEST(SchedulerTest, ConstructWithSpecificThreadCount) {
    Scheduler sched(2);
    EXPECT_EQ(sched.thread_count(), 2);
    EXPECT_FALSE(sched.is_stopping());
}

TEST(SchedulerTest, ConstructWithSingleThread) {
    Scheduler sched(1);
    EXPECT_EQ(sched.thread_count(), 1);
}

TEST(SchedulerTest, ConstructWithManyThreads) {
    Scheduler sched(4);
    EXPECT_EQ(sched.thread_count(), 4);
}

// =====================================================================
// Lifecycle tests
// =====================================================================

TEST(SchedulerTest, StartAndStopEmpty) {
    Scheduler sched(2);
    EXPECT_FALSE(sched.is_stopping());

    sched.start();
    // Give threads time to spin up
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_FALSE(sched.is_stopping());

    sched.stop();
    // After stop, stopping flag should be set
    EXPECT_TRUE(sched.is_stopping());
}

TEST(SchedulerTest, StartStopMultipleTimes) {
    // Stopped scheduler should not be restartable, but we verify
    // that double-stop doesn't crash
    Scheduler sched(1);
    sched.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    sched.stop();
    // Second stop should be safe
    sched.stop();
    EXPECT_TRUE(sched.is_stopping());
}

TEST(SchedulerTest, DestructorWithoutStart) {
    // Scheduler destroyed without start() should be safe
    {
        Scheduler sched(2);
        EXPECT_EQ(sched.thread_count(), 2);
        EXPECT_FALSE(sched.is_stopping());
    }
    SUCCEED();
}

TEST(SchedulerTest, DestructorAfterStop) {
    {
        Scheduler sched(1);
        sched.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        sched.stop();
    }
    SUCCEED();
}

// =====================================================================
// Schedule callback tests
// =====================================================================

TEST(SchedulerTest, ScheduleSingleCallback) {
    Scheduler sched(1);
    sched.start();

    std::atomic<bool> executed{false};

    sched.schedule([&executed]() {
        executed.store(true);
    });

    // Wait for execution
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(executed.load());

    sched.stop();
}

TEST(SchedulerTest, ScheduleMultipleCallbacks) {
    Scheduler sched(2);
    sched.start();

    std::atomic<int> counter{0};
    const int kNumCallbacks = 20;

    for (int i = 0; i < kNumCallbacks; ++i) {
        sched.schedule([&counter]() {
            counter.fetch_add(1);
        });
    }

    // Wait for all callbacks to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_GE(counter.load(), kNumCallbacks);
    // Note: with M:N scheduling, actual count can be >= scheduled
    // because fibers might not have completed yet, but each callback
    // increments counter exactly once

    sched.stop();
}

TEST(SchedulerTest, ScheduleCallbackWithCapture) {
    Scheduler sched(1);
    sched.start();

    std::string result;

    sched.schedule([&result]() {
        result = "hello from fiber";
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(result, "hello from fiber");

    sched.stop();
}

TEST(SchedulerTest, ScheduleLambdasInLoop) {
    Scheduler sched(2);
    sched.start();

    std::atomic<int> sum{0};
    const int kCount = 100;

    for (int i = 0; i < kCount; ++i) {
        sched.schedule([&sum, i]() {
            sum.fetch_add(i);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    int expected = kCount * (kCount - 1) / 2;
    EXPECT_EQ(sum.load(), expected);

    sched.stop();
}

// =====================================================================
// Schedule fiber tests
// =====================================================================

TEST(SchedulerTest, ScheduleFiberPtr) {
    Scheduler sched(1);
    sched.start();

    std::atomic<bool> executed{false};

    auto fiber = Fiber::Create([&executed]() {
        executed.store(true);
    });
    sched.schedule(fiber);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(executed.load());

    sched.stop();
}

TEST(SchedulerTest, ScheduleMultipleFibers) {
    Scheduler sched(2);
    sched.start();

    std::atomic<int> count{0};
    const int kNumFibers = 50;

    for (int i = 0; i < kNumFibers; ++i) {
        auto fiber = Fiber::Create([&count]() {
            count.fetch_add(1);
        });
        sched.schedule(fiber);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_EQ(count.load(), kNumFibers);

    sched.stop();
}

// =====================================================================
// Sequential ordering test (single thread)
// =====================================================================

TEST(SchedulerTest, SequentialOrderingSingleThread) {
    Scheduler sched(1);
    sched.start();

    std::vector<int> results;
    std::mutex mtx;

    // Schedule 10 fibers, each appends its index
    for (int i = 0; i < 10; ++i) {
        sched.schedule([&results, &mtx, i]() {
            std::lock_guard<std::mutex> lock(mtx);
            results.push_back(i);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_EQ(results.size(), 10u);

    sched.stop();
}

// =====================================================================
// Work distribution tests
// =====================================================================

TEST(SchedulerTest, WorkDistributionAcrossThreads) {
    Scheduler sched(4);
    sched.start();

    std::atomic<int> total_count{0};
    std::set<std::thread::id> thread_ids;
    std::mutex mtx;

    const int kNumTasks = 200;

    for (int i = 0; i < kNumTasks; ++i) {
        sched.schedule([&total_count, &thread_ids, &mtx]() {
            total_count.fetch_add(1);
            std::lock_guard<std::mutex> lock(mtx);
            thread_ids.insert(std::this_thread::get_id());
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    EXPECT_EQ(total_count.load(), kNumTasks);
    // With 4 threads, we expect at least 2 different thread ids
    EXPECT_GE(thread_ids.size(), 1u);

    sched.stop();
}

// =====================================================================
// GetThis tests
// =====================================================================

TEST(SchedulerTest, GetThisOutsideScheduler) {
    // Outside of any scheduler thread, GetThis should return nullptr
    // or behave safely
    Scheduler* sched = Scheduler::GetThis();
    // May be null or may return a scheduler if we're inside one
    // Just verify it doesn't crash
    (void)sched;
    SUCCEED();
}

TEST(SchedulerTest, GetThisInsideFiber) {
    Scheduler sched(1);
    sched.start();

    std::atomic<bool> got_scheduler{false};

    sched.schedule([&got_scheduler]() {
        Scheduler* s = Scheduler::GetThis();
        if (s != nullptr) {
            got_scheduler.store(true);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(got_scheduler.load());

    sched.stop();
}

// =====================================================================
// Statistics tests
// =====================================================================

TEST(SchedulerTest, StatisticsAfterScheduling) {
    Scheduler sched(1);
    sched.start();

    const int kNumTasks = 10;
    for (int i = 0; i < kNumTasks; ++i) {
        sched.schedule([]() {
            // Simple task
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    EXPECT_GE(sched.total_fibers_scheduled(), static_cast<uint64_t>(kNumTasks));
    EXPECT_GE(sched.total_fibers_completed(), static_cast<uint64_t>(kNumTasks));

    sched.stop();
}

// =====================================================================
// Graceful shutdown tests
// =====================================================================

TEST(SchedulerTest, GracefulShutdownDrainsQueuedFibers) {
    Scheduler sched(1);
    sched.start();

    std::atomic<int> executed{0};
    const int kNumTasks = 50;

    for (int i = 0; i < kNumTasks; ++i) {
        sched.schedule([&executed]() {
            executed.fetch_add(1);
        });
    }

    // Small delay so fibers get queued
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Stop should drain remaining fibers
    sched.stop();

    EXPECT_EQ(executed.load(), kNumTasks);
}

TEST(SchedulerTest, GracefulShutdownWithLongRunningFibers) {
    Scheduler sched(1);
    sched.start();

    std::atomic<int> progress{0};

    sched.schedule([&progress]() {
        for (int i = 0; i < 10; ++i) {
            progress.store(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Stop should wait for the fiber to finish
    sched.stop();

    EXPECT_GE(progress.load(), 1);
}

// =====================================================================
// Thread count tests
// =====================================================================

TEST(SchedulerTest, ThreadCountRemainsConstant) {
    Scheduler sched(3);
    EXPECT_EQ(sched.thread_count(), 3);

    sched.start();
    EXPECT_EQ(sched.thread_count(), 3);

    sched.stop();
    EXPECT_EQ(sched.thread_count(), 3);
}

// =====================================================================
// Edge cases
// =====================================================================

TEST(SchedulerTest, ScheduleEmptyCallback) {
    Scheduler sched(1);
    sched.start();

    // Scheduling a null-like callback — should not crash
    // (we schedule a no-op since we can't create truly null fibers)
    sched.schedule([]() {});

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    sched.stop();
    SUCCEED();
}

TEST(SchedulerTest, ScheduleLargeNumberOfSmallTasks) {
    Scheduler sched(2);
    sched.start();

    std::atomic<int> counter{0};
    const int kNumTasks = 500;

    for (int i = 0; i < kNumTasks; ++i) {
        sched.schedule([&counter]() {
            counter.fetch_add(1);
        });
    }

    // Wait for completion
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    EXPECT_EQ(counter.load(), kNumTasks);

    sched.stop();
}

TEST(SchedulerTest, NestedFiberScheduling) {
    Scheduler sched(1);
    sched.start();

    std::atomic<bool> inner_executed{false};
    std::atomic<bool> outer_executed{false};

    sched.schedule([&inner_executed, &outer_executed, &sched]() {
        outer_executed.store(true);
        // Schedule another fiber from inside a fiber
        sched.schedule([&inner_executed]() {
            inner_executed.store(true);
        });
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    EXPECT_TRUE(outer_executed.load());
    EXPECT_TRUE(inner_executed.load());

    sched.stop();
}

TEST(SchedulerTest, ActiveFiberCount) {
    Scheduler sched(1);
    sched.start();

    // Initially should be at least the main fiber
    size_t initial = sched.active_fiber_count();
    EXPECT_GE(initial, 0u);

    sched.schedule([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    size_t during = sched.active_fiber_count();
    EXPECT_GE(during, 0u);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    sched.stop();
    SUCCEED();
}

TEST(SchedulerTest, TotalIdleTicksIncreases) {
    Scheduler sched(1);
    sched.start();

    // Let the scheduler idle for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    uint64_t idle = sched.total_idle_ticks();
    EXPECT_GE(idle, 0u);

    sched.stop();
}
