// test_cpu_affinity.cpp — Comprehensive CPU affinity unit tests
// Tests get_cpu_count, set_cpu_affinity, pin_to_core, get_current_cpu,
// multi-thread pinning, affinity mask operations, clear_cpu_affinity,
// and topology queries.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <thread>
#include <vector>
#include <algorithm>

using namespace zero;

// ============================================================
// get_cpu_count()
// ============================================================

TEST(CpuAffinity, GetCpuCount) {
    int count = get_cpu_count();
    EXPECT_GE(count, 1);
    // On a modern system, at least 1 logical CPU
}

TEST(CpuAffinity, GetPhysicalCoreCount) {
    int count = get_physical_core_count();
    EXPECT_GE(count, 1);
    EXPECT_LE(count, get_cpu_count());
}

// ============================================================
// set_cpu_affinity for a thread
// ============================================================

TEST(CpuAffinity, SetAffinityForCurrentThread) {
    // Pin current thread to core 0
    bool result = set_cpu_affinity(pthread_self(), 0);
    EXPECT_TRUE(result);
}

TEST(CpuAffinity, SetAffinityForValidCores) {
    int cpu_count = get_cpu_count();
    // Pin to each available core one by one
    for (int core = 0; core < cpu_count && core < 4; ++core) {
        EXPECT_TRUE(set_cpu_affinity(pthread_self(), core));
    }
}

TEST(CpuAffinity, SetAffinityOutOfRangeFails) {
    int cpu_count = get_cpu_count();
    // Try to set to a core that doesn't exist
    bool result = set_cpu_affinity(pthread_self(), cpu_count + 100);
    EXPECT_FALSE(result);
}

// ============================================================
// pin_to_core convenience
// ============================================================

TEST(CpuAffinity, PinToCore) {
    EXPECT_TRUE(pin_to_core(0));
}

TEST(CpuAffinity, PinToCoreMultiple) {
    int cpu_count = get_cpu_count();
    for (int core = 0; core < cpu_count && core < 8; ++core) {
        EXPECT_TRUE(pin_to_core(core));
    }
}

// ============================================================
// get_current_cpu()
// ============================================================

TEST(CpuAffinity, GetCurrentCpu) {
    int cpu = get_current_cpu();
    EXPECT_GE(cpu, 0);
    EXPECT_LT(cpu, get_cpu_count());
}

TEST(CpuAffinity, GetCurrentCpuAfterPin) {
    pin_to_core(0);
    int cpu = get_current_cpu();
    EXPECT_GE(cpu, 0);
    EXPECT_LT(cpu, get_cpu_count());
}

// ============================================================
// Pinning multiple threads to different cores
// ============================================================

TEST(CpuAffinity, PinMultipleThreadsToCore0) {
    const int kNumThreads = 4;
    std::atomic<int> successes{0};

    auto worker = [&successes]() {
        if (pin_to_core(0)) {
            successes.fetch_add(1);
        }
        // Verify it ran on a valid CPU
        int cpu = get_current_cpu();
        EXPECT_GE(cpu, 0);
        EXPECT_LT(cpu, get_cpu_count());
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < kNumThreads; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(successes.load(), kNumThreads);
}

TEST(CpuAffinity, PinThreadsToDifferentCores) {
    int cpu_count = get_cpu_count();
    int num_threads = std::min(cpu_count, 4);
    std::atomic<int> pinned{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([i, &pinned]() {
            if (set_cpu_affinity(pthread_self(), i)) {
                pinned.fetch_add(1);
            }
            int cpu = get_current_cpu();
            EXPECT_GE(cpu, 0);
            EXPECT_LT(cpu, get_cpu_count());
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(pinned.load(), num_threads);
}

// ============================================================
// get_cpu_affinity (read back the mask)
// ============================================================

TEST(CpuAffinity, GetAffinityReturnsCores) {
    // Pin to core 0
    set_cpu_affinity(pthread_self(), 0);

    auto cores = get_cpu_affinity(pthread_self());
    EXPECT_GE(cores.size(), 1u);
    // Core 0 should be in the list
    EXPECT_NE(std::find(cores.begin(), cores.end(), 0), cores.end());
}

TEST(CpuAffinity, GetAffinityAfterClearReturnsAll) {
    clear_cpu_affinity();

    auto cores = get_cpu_affinity(pthread_self());
    int cpu_count = get_cpu_count();
    EXPECT_EQ(cores.size(), static_cast<size_t>(cpu_count));
}

// ============================================================
// set_cpu_affinity_mask
// ============================================================

TEST(CpuAffinity, SetAffinityMask) {
    int cpu_count = get_cpu_count();
    if (cpu_count < 2) {
        GTEST_SKIP() << "Need at least 2 CPUs for mask test";
    }

    std::vector<int> mask = {0, 1};
    EXPECT_TRUE(set_cpu_affinity_mask(pthread_self(), mask));

    auto cores = get_cpu_affinity(pthread_self());
    EXPECT_GE(cores.size(), 1u);
}

TEST(CpuAffinity, SetAffinityMaskEmpty) {
    // Empty mask should fail
    std::vector<int> empty_mask;
    bool result = set_cpu_affinity_mask(pthread_self(), empty_mask);
    EXPECT_FALSE(result);
}

// ============================================================
// clear_cpu_affinity
// ============================================================

TEST(CpuAffinity, ClearAffinity) {
    pin_to_core(0);
    // Verify pinning worked
    auto cores_before = get_cpu_affinity(pthread_self());
    EXPECT_GE(cores_before.size(), 1u);

    EXPECT_TRUE(clear_cpu_affinity());

    auto cores_after = get_cpu_affinity(pthread_self());
    EXPECT_EQ(cores_after.size(), static_cast<size_t>(get_cpu_count()));
}

// ============================================================
// Single-core system handling (don't crash)
// ============================================================

TEST(CpuAffinity, PinOnSingleCoreSystem) {
    // pin_to_core(0) should work on any system (at least one core)
    bool result = pin_to_core(0);
    EXPECT_TRUE(result);
}

TEST(CpuAffinity, GetCurrentCpuAlwaysValid) {
    int cpu = get_current_cpu();
    EXPECT_GE(cpu, 0);
}

// ============================================================
// Pin thread from another thread
// ============================================================

TEST(CpuAffinity, PinRemoteThread) {
    std::thread t([]() {
        // This thread just exists
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });

    bool result = set_cpu_affinity(t.native_handle(), 0);
    // Pinning a running thread should work
    EXPECT_TRUE(result);

    t.join();
}

// ============================================================
// get_cpu_topology
// ============================================================

TEST(CpuAffinity, CpuTopology) {
    auto topo = get_cpu_topology();
    EXPECT_GE(topo.logical_count, 1);
    EXPECT_GE(topo.physical_cores, 1);
    EXPECT_GE(topo.sockets, 1);
    EXPECT_GE(topo.threads_per_core, 1);
}

// ============================================================
// Restore affinity after tests (cleanup)
// ============================================================

TEST(CpuAffinity, RestoreDefaultAffinity) {
    // Pin, verify, then restore
    pin_to_core(0);

    auto cores_pinned = get_cpu_affinity(pthread_self());
    EXPECT_GE(cores_pinned.size(), 1u);

    clear_cpu_affinity();

    auto cores_all = get_cpu_affinity(pthread_self());
    EXPECT_EQ(cores_all.size(), static_cast<size_t>(get_cpu_count()));
}
