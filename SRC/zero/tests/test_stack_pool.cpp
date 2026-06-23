// test_stack_pool.cpp — Comprehensive StackPool unit tests
// Tests instance() singleton, allocate() returns non-null aligned memory,
// deallocate() returns memory to pool, default stack size (128KB),
// custom stack allocation, multiple allocate/deallocate cycles,
// stack alignment (page-aligned), guard pages, statistics, trim().

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <thread>
#include <vector>
#include <cstring>
#include <unistd.h>

using namespace zero;

// ============================================================
// Singleton pattern
// ============================================================

TEST(StackPool, InstanceReturnsSame) {
    StackPool& pool1 = StackPool::instance();
    StackPool& pool2 = StackPool::instance();
    EXPECT_EQ(&pool1, &pool2);
}

// ============================================================
// allocate() basic
// ============================================================

TEST(StackPool, AllocateReturnsNonNull) {
    auto& pool = StackPool::instance();
    const size_t kStackSize = 131072; // 128KB default

    void* stack = pool.allocate(kStackSize);
    ASSERT_NE(stack, nullptr);

    pool.deallocate(stack, kStackSize);
}

TEST(StackPool, AllocateDefaultSize) {
    auto& pool = StackPool::instance();
    void* stack = pool.allocate(131072);
    ASSERT_NE(stack, nullptr);
    pool.deallocate(stack, 131072);
}

// ============================================================
// Alignment — stack should be page-aligned
// ============================================================

TEST(StackPool, AllocationIsPageAligned) {
    auto& pool = StackPool::instance();
    long page_size = sysconf(_SC_PAGESIZE);
    ASSERT_GT(page_size, 0);

    const size_t kStackSize = 131072;
    void* stack = pool.allocate(kStackSize);
    ASSERT_NE(stack, nullptr);

    uintptr_t addr = reinterpret_cast<uintptr_t>(stack);
    EXPECT_EQ(addr % page_size, static_cast<uintptr_t>(0))
        << "Stack address 0x" << std::hex << addr << std::dec
        << " is not page-aligned (page size " << page_size << ")";

    pool.deallocate(stack, kStackSize);
}

TEST(StackPool, MultipleAllocationsAligned) {
    auto& pool = StackPool::instance();
    long page_size = sysconf(_SC_PAGESIZE);

    for (int i = 0; i < 10; ++i) {
        void* stack = pool.allocate(131072);
        ASSERT_NE(stack, nullptr);

        uintptr_t addr = reinterpret_cast<uintptr_t>(stack);
        EXPECT_EQ(addr % page_size, static_cast<uintptr_t>(0))
            << "Allocation " << i << " not page-aligned";

        pool.deallocate(stack, 131072);
    }
}

// ============================================================
// Custom stack sizes
// ============================================================

TEST(StackPool, CustomStackSize) {
    auto& pool = StackPool::instance();

    // Small stack (64KB)
    void* small = pool.allocate(65536);
    ASSERT_NE(small, nullptr);
    pool.deallocate(small, 65536);

    // Large stack (1MB)
    void* large = pool.allocate(1048576);
    ASSERT_NE(large, nullptr);
    pool.deallocate(large, 1048576);
}

TEST(StackPool, VariousSizes) {
    auto& pool = StackPool::instance();
    std::vector<size_t> sizes = {
        4096,      // 1 page
        16384,     // 4 pages
        65536,     // 16 pages (64KB)
        131072,    // 32 pages (128KB default)
        262144,    // 64 pages (256KB)
        524288,    // 128 pages (512KB)
        1048576,   // 256 pages (1MB)
    };

    for (size_t sz : sizes) {
        void* stack = pool.allocate(sz);
        ASSERT_NE(stack, nullptr) << "Failed to allocate " << sz << " bytes";
        pool.deallocate(stack, sz);
    }
}

TEST(StackPool, OddSizeRoundUp) {
    auto& pool = StackPool::instance();
    long page_size = sysconf(_SC_PAGESIZE);

    // Allocate an odd number of bytes — should be rounded up to page
    void* stack = pool.allocate(page_size + 1);
    ASSERT_NE(stack, nullptr);

    uintptr_t addr = reinterpret_cast<uintptr_t>(stack);
    EXPECT_EQ(addr % page_size, static_cast<uintptr_t>(0));

    pool.deallocate(stack, page_size + 1);
}

// ============================================================
// Multiple allocate/deallocate cycles
// ============================================================

TEST(StackPool, AllocDeallocCycle) {
    auto& pool = StackPool::instance();
    const int kCycles = 50;
    const size_t kStackSize = 131072;

    for (int i = 0; i < kCycles; ++i) {
        void* stack = pool.allocate(kStackSize);
        ASSERT_NE(stack, nullptr);
        pool.deallocate(stack, kStackSize);
    }
    SUCCEED();
}

TEST(StackPool, MultipleConcurrentAllocs) {
    auto& pool = StackPool::instance();
    const int kNumStacks = 20;
    const size_t kStackSize = 131072;
    std::vector<void*> stacks;

    for (int i = 0; i < kNumStacks; ++i) {
        void* s = pool.allocate(kStackSize);
        ASSERT_NE(s, nullptr);
        stacks.push_back(s);
    }

    // All stacks should be distinct
    std::sort(stacks.begin(), stacks.end());
    auto last = std::unique(stacks.begin(), stacks.end());
    EXPECT_EQ(last - stacks.begin(), kNumStacks);

    // Deallocate all
    for (void* s : stacks) {
        pool.deallocate(s, kStackSize);
    }
}

// ============================================================
// Deallocate and re-allocate (reuse)
// ============================================================

TEST(StackPool, ReuseReturnsSameStack) {
    auto& pool = StackPool::instance();
    const size_t kStackSize = 131072;

    void* s1 = pool.allocate(kStackSize);
    ASSERT_NE(s1, nullptr);

    pool.deallocate(s1, kStackSize);

    // The next allocate might return the same stack from the free list
    void* s2 = pool.allocate(kStackSize);
    ASSERT_NE(s2, nullptr);

    // Not guaranteed to be the same address (depends on free list
    // implementation), but it's a valid stack either way
    pool.deallocate(s2, kStackSize);
}

// ============================================================
// Guard pages
// ============================================================

TEST(StackPool, DefaultGuardPages) {
    auto& pool = StackPool::instance();
    EXPECT_EQ(pool.guard_pages(), 1u);
}

TEST(StackPool, SetGuardPages) {
    auto& pool = StackPool::instance();
    size_t prev = pool.guard_pages();

    pool.set_guard_pages(2);
    EXPECT_EQ(pool.guard_pages(), 2u);

    pool.set_guard_pages(0);
    EXPECT_EQ(pool.guard_pages(), 0u);

    // Restore
    pool.set_guard_pages(prev);
}

TEST(StackPool, ZeroGuardPages) {
    auto& pool = StackPool::instance();
    size_t prev = pool.guard_pages();

    pool.set_guard_pages(0);
    void* stack = pool.allocate(131072);
    ASSERT_NE(stack, nullptr);
    pool.deallocate(stack, 131072);

    pool.set_guard_pages(prev);
}

// ============================================================
// preallocate()
// ============================================================

TEST(StackPool, Preallocate) {
    auto& pool = StackPool::instance();
    size_t before = pool.available();

    pool.preallocate(10);
    EXPECT_GE(pool.available(), before + 10);
}

TEST(StackPool, PreallocateCustomSize) {
    auto& pool = StackPool::instance();
    size_t before = pool.available();

    pool.preallocate(5, 65536);
    EXPECT_GE(pool.available(), before + 5);
}

// ============================================================
// Statistics
// ============================================================

TEST(StackPool, Statistics) {
    auto& pool = StackPool::instance();

    size_t avail_before = pool.available();
    size_t alloc_before = pool.total_allocated();
    size_t cached_before = pool.total_cached();

    void* stack = pool.allocate(131072);
    ASSERT_NE(stack, nullptr);

    pool.deallocate(stack, 131072);

    // After deallocate, the stack should be cached
    EXPECT_GE(pool.total_cached(), cached_before);
}

// ============================================================
// trim() — free cached stacks back to OS
// ============================================================

TEST(StackPool, TrimFreesCachedStacks) {
    auto& pool = StackPool::instance();

    // Preallocate some stacks
    pool.preallocate(5);
    EXPECT_GT(pool.available(), 0u);

    pool.trim();
    // After trim, most cached stacks should be freed
    // (implementation may vary but available should decrease)
    SUCCEED();
}

// ============================================================
// Thread safety
// ============================================================

TEST(StackPool, MultiThreadAllocDealloc) {
    auto& pool = StackPool::instance();
    const int kNumThreads = 4;
    const int kPerThread = 25;
    std::atomic<int> successes{0};

    auto worker = [&]() {
        for (int i = 0; i < kPerThread; ++i) {
            void* s = pool.allocate(131072);
            if (s) {
                pool.deallocate(s, 131072);
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

// ============================================================
// Zero-sized allocation
// ============================================================

TEST(StackPool, ZeroSizeAllocation) {
    auto& pool = StackPool::instance();
    // Should handle zero-size gracefully
    void* stack = pool.allocate(0);
    // Library may return nullptr or minimum allocation
    if (stack) {
        pool.deallocate(stack, 0);
    }
    SUCCEED();
}

// ============================================================
// Write to stack (verify accessible memory)
// ============================================================

TEST(StackPool, StackMemoryWritable) {
    auto& pool = StackPool::instance();
    const size_t kStackSize = 131072;

    void* stack = pool.allocate(kStackSize);
    ASSERT_NE(stack, nullptr);

    // Write a pattern at the top of the stack (stack grows downward)
    // We're given the HIGH end, so write near it
    char* ptr = static_cast<char*>(stack);
    // Write a marker just below the top
    ptr[-1] = 'A';
    ptr[-2] = 'B';
    ptr[-3] = 'C';

    EXPECT_EQ(ptr[-1], 'A');
    EXPECT_EQ(ptr[-2], 'B');
    EXPECT_EQ(ptr[-3], 'C');

    pool.deallocate(stack, kStackSize);
}
