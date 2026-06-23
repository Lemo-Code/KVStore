// ============================================================================
// zstl Allocator Unit Tests
// Tests: default_alloc allocate/deallocate, std_alloc allocate/deallocate,
// allocator_traits (rebind, construct, destroy, propagate traits), max_size.
// Test with different types. Verify alignment.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>
#include <vector>
#include <cstddef>

// ============================================================
// default_alloc basic allocation
// ============================================================

TEST(AllocatorTest, DefaultAllocAllocateInt) {
    zstl::default_alloc<int> alloc;
    int* p = alloc.allocate(10);
    ASSERT_NE(p, nullptr);
    // Should be properly aligned for int
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % alignof(int), 0u);
    alloc.deallocate(p, 10);
}

TEST(AllocatorTest, DefaultAllocAllocateZero) {
    zstl::default_alloc<int> alloc;
    int* p = alloc.allocate(0);
    EXPECT_EQ(p, nullptr);
    alloc.deallocate(p, 0); // should be safe
}

TEST(AllocatorTest, DefaultAllocAllocateOne) {
    zstl::default_alloc<double> alloc;
    double* p = alloc.allocate(1);
    ASSERT_NE(p, nullptr);
    // Verify alignment
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % alignof(double), 0u);

    // Write and read
    zstl::construct(p, 3.14);
    EXPECT_DOUBLE_EQ(*p, 3.14);
    zstl::destroy_at(p);

    alloc.deallocate(p, 1);
}

TEST(AllocatorTest, DefaultAllocLargeAllocation) {
    // Allocate more than kMaxPoolSize (8192 bytes) to test fallback path
    zstl::default_alloc<char> alloc;
    constexpr size_t large_n = 10000; // 10000 > 8192
    char* p = alloc.allocate(large_n);
    ASSERT_NE(p, nullptr);
    // Write to verify it's usable memory
    p[0] = 'a';
    p[large_n - 1] = 'z';
    EXPECT_EQ(p[0], 'a');
    EXPECT_EQ(p[large_n - 1], 'z');
    alloc.deallocate(p, large_n);
}

TEST(AllocatorTest, DefaultAllocDifferentTypes) {
    struct BigStruct {
        char data[256];
    };

    zstl::default_alloc<BigStruct> alloc;
    BigStruct* p = alloc.allocate(5);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % alignof(BigStruct), 0u);
    alloc.deallocate(p, 5);
}

// ============================================================
// default_alloc deallocation
// ============================================================

TEST(AllocatorTest, DefaultAllocAllocateDeallocateMultiple) {
    zstl::default_alloc<int> alloc;
    std::vector<int*> ptrs;

    for (int i = 0; i < 10; ++i) {
        int* p = alloc.allocate(4);
        ASSERT_NE(p, nullptr);
        ptrs.push_back(p);
    }

    for (auto* p : ptrs) {
        alloc.deallocate(p, 4);
    }
}

TEST(AllocatorTest, DefaultAllocDeallocateNull) {
    zstl::default_alloc<int> alloc;
    EXPECT_NO_THROW(alloc.deallocate(nullptr, 10));
}

// ============================================================
// std_alloc basic allocation
// ============================================================

TEST(AllocatorTest, StdAllocAllocateInt) {
    zstl::std_alloc<int> alloc;
    int* p = alloc.allocate(5);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % alignof(int), 0u);
    alloc.deallocate(p, 5);
}

TEST(AllocatorTest, StdAllocAllocateZero) {
    zstl::std_alloc<int> alloc;
    int* p = alloc.allocate(0);
    EXPECT_EQ(p, nullptr);
}

TEST(AllocatorTest, StdAllocAllocateString) {
    zstl::std_alloc<std::string> alloc;
    std::string* p = alloc.allocate(3);
    ASSERT_NE(p, nullptr);

    zstl::construct(p, "hello");
    zstl::construct(p + 1, "world");
    zstl::construct(p + 2, "!");

    EXPECT_EQ(*p, "hello");
    EXPECT_EQ(*(p + 1), "world");
    EXPECT_EQ(*(p + 2), "!");

    zstl::destroy_at(p + 2);
    zstl::destroy_at(p + 1);
    zstl::destroy_at(p);

    alloc.deallocate(p, 3);
}

TEST(AllocatorTest, StdAllocBadAlloc) {
    zstl::std_alloc<int> alloc;
    // Request impossibly large allocation
    EXPECT_THROW(alloc.allocate(static_cast<size_t>(-1) / sizeof(int) + 1), std::bad_alloc);
}

// ============================================================
// allocator equality
// ============================================================

TEST(AllocatorTest, AllocatorEquality) {
    zstl::default_alloc<int> a1, a2;
    EXPECT_TRUE(a1 == a2);
    EXPECT_FALSE(a1 != a2);

    zstl::default_alloc<double> a3;
    EXPECT_TRUE(a1 == a3); // cross-type equality

    zstl::std_alloc<int> s1, s2;
    EXPECT_TRUE(s1 == s2);
}

// ============================================================
// allocator rebind
// ============================================================

TEST(AllocatorTest, DefaultAllocRebind) {
    zstl::default_alloc<int>::rebind<double>::other alloc;
    double* p = alloc.allocate(1);
    ASSERT_NE(p, nullptr);
    *p = 3.14;
    EXPECT_DOUBLE_EQ(*p, 3.14);
    alloc.deallocate(p, 1);
}

TEST(AllocatorTest, StdAllocRebind) {
    zstl::std_alloc<int>::rebind<char>::other alloc;
    char* p = alloc.allocate(10);
    ASSERT_NE(p, nullptr);
    p[0] = 'x';
    EXPECT_EQ(p[0], 'x');
    alloc.deallocate(p, 10);
}

// ============================================================
// void specialization
// ============================================================

TEST(AllocatorTest, DefaultAllocVoid) {
    zstl::default_alloc<void> alloc;
    using rebind_int = zstl::default_alloc<void>::rebind<int>::other;
    rebind_int int_alloc;
    int* p = int_alloc.allocate(5);
    ASSERT_NE(p, nullptr);
    int_alloc.deallocate(p, 5);
}

TEST(AllocatorTest, StdAllocVoid) {
    zstl::std_alloc<void> alloc;
    using rebind_double = zstl::std_alloc<void>::rebind<double>::other;
    rebind_double d_alloc;
    double* p = d_alloc.allocate(3);
    ASSERT_NE(p, nullptr);
    d_alloc.deallocate(p, 3);
}

// ============================================================
// allocator_traits
// ============================================================

TEST(AllocatorTest, AllocatorTraitsAllocateDeallocate) {
    zstl::default_alloc<int> alloc;
    using traits = zstl::allocator_traits<zstl::default_alloc<int>>;

    auto* p = traits::allocate(alloc, 5);
    ASSERT_NE(p, nullptr);
    traits::deallocate(alloc, p, 5);

    // null
    traits::deallocate(alloc, nullptr, 0);
}

TEST(AllocatorTest, AllocatorTraitsConstruct) {
    zstl::std_alloc<std::string> alloc;
    using traits = zstl::allocator_traits<zstl::std_alloc<std::string>>;

    std::string* p = traits::allocate(alloc, 1);
    ASSERT_NE(p, nullptr);

    traits::construct(alloc, p, "hello world");
    EXPECT_EQ(*p, "hello world");

    traits::destroy(alloc, p);
    traits::deallocate(alloc, p, 1);
}

TEST(AllocatorTest, AllocatorTraitsConstructDefault) {
    zstl::default_alloc<int> alloc;
    using traits = zstl::allocator_traits<zstl::default_alloc<int>>;

    int* p = traits::allocate(alloc, 1);
    ASSERT_NE(p, nullptr);

    traits::construct(alloc, p, 42);
    EXPECT_EQ(*p, 42);

    traits::destroy(alloc, p);
    traits::deallocate(alloc, p, 1);
}

TEST(AllocatorTest, AllocatorTraitsPropagationTypes) {
    // default_alloc
    using da_traits = zstl::allocator_traits<zstl::default_alloc<int>>;
    EXPECT_FALSE((da_traits::propagate_on_container_copy_assignment::value));
    EXPECT_TRUE((da_traits::propagate_on_container_move_assignment::value));
    EXPECT_FALSE((da_traits::propagate_on_container_swap::value));
    EXPECT_TRUE((da_traits::is_always_equal::value));

    // std_alloc
    using sa_traits = zstl::allocator_traits<zstl::std_alloc<int>>;
    EXPECT_FALSE((sa_traits::propagate_on_container_copy_assignment::value));
    EXPECT_FALSE((sa_traits::propagate_on_container_move_assignment::value));
    EXPECT_FALSE((sa_traits::propagate_on_container_swap::value));
    EXPECT_TRUE((sa_traits::is_always_equal::value));
}

TEST(AllocatorTest, AllocatorTraitsRebind) {
    using traits = zstl::allocator_traits<zstl::default_alloc<int>>;
    using rebind_alloc = traits::template rebind_alloc<double>;
    EXPECT_TRUE((std::is_same_v<rebind_alloc, zstl::default_alloc<double>>));

    rebind_alloc alloc;
    double* p = alloc.allocate(3);
    ASSERT_NE(p, nullptr);
    alloc.deallocate(p, 3);
}

TEST(AllocatorTest, AllocatorTraitsMaxSize) {
    zstl::default_alloc<int> alloc;
    using traits = zstl::allocator_traits<zstl::default_alloc<int>>;
    // max_size should be callable — value is implementation-defined
    size_t ms = traits::max_size(alloc);
    EXPECT_GT(ms, 0u);
}

TEST(AllocatorTest, AllocatorTraitsSelectOnCopy) {
    zstl::default_alloc<int> alloc;
    using traits = zstl::allocator_traits<zstl::default_alloc<int>>;
    auto new_alloc = traits::select_on_container_copy_construction(alloc);
    EXPECT_TRUE(new_alloc == alloc);
}

// ============================================================
// allocator type aliases
// ============================================================

TEST(AllocatorTest, AllocatorConvenienceAlias) {
    EXPECT_TRUE((std::is_same_v<zstl::allocator<int>, zstl::default_alloc<int>>));
}
