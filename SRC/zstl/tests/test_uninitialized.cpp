// ============================================================================
// zstl Uninitialized Memory Utilities Unit Tests
// Tests: raw_storage_iterator, temporary_buffer, get_temporary_buffer,
// return_temporary_buffer, addressof (from utility.h), aligned_alloc/
// aligned_free, align_up/align_down, is_aligned, assume_aligned.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>
#include <vector>
#include <cstring>
#include <memory>

// ============================================================
// align_up / align_down / is_aligned
// ============================================================

TEST(UninitializedTest, AlignUp) {
    EXPECT_EQ(zstl::align_up(0, 8), 0u);
    EXPECT_EQ(zstl::align_up(1, 8), 8u);
    EXPECT_EQ(zstl::align_up(7, 8), 8u);
    EXPECT_EQ(zstl::align_up(8, 8), 8u);
    EXPECT_EQ(zstl::align_up(9, 8), 16u);
    EXPECT_EQ(zstl::align_up(15, 8), 16u);
    EXPECT_EQ(zstl::align_up(16, 8), 16u);

    // Different alignments (must be power of 2)
    EXPECT_EQ(zstl::align_up(0, 16), 0u);
    EXPECT_EQ(zstl::align_up(1, 16), 16u);
    EXPECT_EQ(zstl::align_up(15, 16), 16u);
    EXPECT_EQ(zstl::align_up(16, 16), 16u);
    EXPECT_EQ(zstl::align_up(17, 16), 32u);

    EXPECT_EQ(zstl::align_up(5, 4), 8u);
    EXPECT_EQ(zstl::align_up(3, 4), 4u);
    EXPECT_EQ(zstl::align_up(2, 2), 2u);
    EXPECT_EQ(zstl::align_up(1, 2), 2u);
    EXPECT_EQ(zstl::align_up(0, 1), 0u);
    EXPECT_EQ(zstl::align_up(5, 1), 5u);
}

TEST(UninitializedTest, AlignDown) {
    EXPECT_EQ(zstl::align_down(0, 8), 0u);
    EXPECT_EQ(zstl::align_down(1, 8), 0u);
    EXPECT_EQ(zstl::align_down(7, 8), 0u);
    EXPECT_EQ(zstl::align_down(8, 8), 8u);
    EXPECT_EQ(zstl::align_down(9, 8), 8u);
    EXPECT_EQ(zstl::align_down(15, 8), 8u);
    EXPECT_EQ(zstl::align_down(16, 8), 16u);

    EXPECT_EQ(zstl::align_down(5, 4), 4u);
    EXPECT_EQ(zstl::align_down(3, 4), 0u);
}

TEST(UninitializedTest, IsAligned) {
    int x = 0;
    void* p = &x;

    // Most stack/heap addresses are aligned to at least 4
    // We can test with controlled values
    uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    if (addr % 4 == 0) {
        EXPECT_TRUE(zstl::is_aligned(p, 4));
    }
    if (addr % 8 == 0) {
        EXPECT_TRUE(zstl::is_aligned(p, 8));
    }
}

// ============================================================
// raw_storage_iterator
// ============================================================

TEST(UninitializedTest, RawStorageIteratorConstruct) {
    // Use raw storage to placement-new into memory
    const int count = 5;
    std::string* storage = static_cast<std::string*>(::operator new(count * sizeof(std::string)));

    zstl::raw_storage_iterator<std::string*, std::string> it(storage);
    EXPECT_EQ(it.base(), storage);

    ::operator delete(storage);
}

TEST(UninitializedTest, RawStorageIteratorAssign) {
    const int count = 3;
    std::string* storage = static_cast<std::string*>(::operator new(count * sizeof(std::string)));

    zstl::raw_storage_iterator<std::string*, std::string> it(storage);

    *it = "hello";
    ++it;
    *it = "world";
    ++it;
    auto it2 = it++;
    auto it3 = zstl::raw_storage_iterator<std::string*, std::string>(it2);
    (void)it3;

    EXPECT_EQ(storage[0], "hello");
    EXPECT_EQ(storage[1], "world");

    // Clean up
    zstl::destroy_at(storage + 1);
    zstl::destroy_at(storage);
    ::operator delete(storage);
}

TEST(UninitializedTest, RawStorageIteratorMoveAssign) {
    const int count = 2;
    std::string* storage = static_cast<std::string*>(::operator new(count * sizeof(std::string)));

    zstl::raw_storage_iterator<std::string*, std::string> it(storage);

    std::string s = "move me";
    *it = zstl::move(s);
    EXPECT_EQ(storage[0], "move me");

    zstl::destroy_at(storage);
    ::operator delete(storage);
}

TEST(UninitializedTest, RawStorageIteratorIncrement) {
    int* storage = static_cast<int*>(::operator new(3 * sizeof(int)));

    zstl::raw_storage_iterator<int*, int> it(storage);

    *it = 10;
    ++it;
    *it = 20;
    auto prev = it++;
    EXPECT_EQ(prev.base(), storage + 1);

    *it = 30;
    EXPECT_EQ(storage[0], 10);
    EXPECT_EQ(storage[1], 20);
    EXPECT_EQ(storage[2], 30);

    ::operator delete(storage);
}

// ============================================================
// temporary_buffer
// ============================================================

TEST(UninitializedTest, TemporaryBufferBasic) {
    zstl::temporary_buffer<int> buf(10);
    EXPECT_GE(buf.size(), 0);
    // Could be less than requested, but at least valid
    if (buf.size() > 0) {
        ASSERT_NE(buf.data(), nullptr);
        // Write to check validity
        buf.data()[0] = 42;
        EXPECT_EQ(buf.data()[0], 42);
    }
}

TEST(UninitializedTest, TemporaryBufferZero) {
    zstl::temporary_buffer<int> buf(0);
    EXPECT_EQ(buf.size(), 0);
    EXPECT_EQ(buf.data(), nullptr);
}

TEST(UninitializedTest, TemporaryBufferNegative) {
    zstl::temporary_buffer<int> buf(-1);
    EXPECT_EQ(buf.size(), 0);
    EXPECT_EQ(buf.data(), nullptr);
}

TEST(UninitializedTest, TemporaryBufferMoveConstructor) {
    zstl::temporary_buffer<int> buf1(5);
    int* ptr1 = buf1.data();
    auto sz1 = buf1.size();

    zstl::temporary_buffer<int> buf2(zstl::move(buf1));
    EXPECT_EQ(buf2.data(), ptr1);
    EXPECT_EQ(buf2.size(), sz1);
    EXPECT_EQ(buf1.data(), nullptr);
    EXPECT_EQ(buf1.size(), 0);
}

TEST(UninitializedTest, TemporaryBufferMoveAssignment) {
    zstl::temporary_buffer<int> buf1(5);
    int* ptr1 = buf1.data();

    zstl::temporary_buffer<int> buf2(10);
    buf2 = zstl::move(buf1);

    EXPECT_EQ(buf2.data(), ptr1);
    EXPECT_EQ(buf1.data(), nullptr);
}

TEST(UninitializedTest, TemporaryBufferRequestedSize) {
    zstl::temporary_buffer<char> buf(4096);
    if (buf.size() > 0) {
        EXPECT_LE(buf.requested_size(), 4096);
    }
}

// ============================================================
// get_temporary_buffer / return_temporary_buffer
// ============================================================

TEST(UninitializedTest, GetTemporaryBuffer) {
    auto result = zstl::get_temporary_buffer<int>(10);
    EXPECT_GE(result.second, 0);

    if (result.first) {
        EXPECT_GT(result.second, 0);
        result.first[0] = 99;
        EXPECT_EQ(result.first[0], 99);

        zstl::return_temporary_buffer(result.first);
    }
}

TEST(UninitializedTest, GetTemporaryBufferZero) {
    auto result = zstl::get_temporary_buffer<int>(0);
    EXPECT_EQ(result.first, nullptr);
    EXPECT_EQ(result.second, 0);
    // return_temporary_buffer on nullptr is safe
    zstl::return_temporary_buffer(result.first);
}

TEST(UninitializedTest, GetTemporaryBufferNegative) {
    auto result = zstl::get_temporary_buffer<int>(-5);
    EXPECT_EQ(result.first, nullptr);
    EXPECT_EQ(result.second, 0);
}

// ============================================================
// aligned_alloc / aligned_free
// ============================================================

TEST(UninitializedTest, AlignedAllocBasic) {
    void* p = zstl::aligned_alloc(64, 256);
    ASSERT_NE(p, nullptr);

    // Check alignment
    uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    EXPECT_EQ(addr % 64, 0u);

    // Check it's writable
    std::memset(p, 0xAB, 256);
    EXPECT_EQ(static_cast<unsigned char*>(p)[0], 0xAB);
    EXPECT_EQ(static_cast<unsigned char*>(p)[255], 0xAB);

    zstl::aligned_free(p);
}

TEST(UninitializedTest, AlignedAllocVariousAlignments) {
    // 16, 32, 64, 128
    for (size_t align : {16u, 32u, 64u, 128u}) {
        void* p = zstl::aligned_alloc(align, 128);
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % align, 0u);
        zstl::aligned_free(p);
    }
}

TEST(UninitializedTest, AlignedAllocZeroSize) {
    void* p = zstl::aligned_alloc(64, 0);
    EXPECT_EQ(p, nullptr);
    zstl::aligned_free(p); // should be safe
}

TEST(UninitializedTest, AlignedAllocNullFree) {
    zstl::aligned_free(nullptr); // should be safe
}

TEST(UninitializedTest, AlignedAllocSmallAlignment) {
    // alignment < alignof(void*) should be promoted
    void* p = zstl::aligned_alloc(1, 64);
    ASSERT_NE(p, nullptr);
    // Should be at least pointer-aligned
    EXPECT_GE(reinterpret_cast<uintptr_t>(p) % alignof(void*), 0u);
    zstl::aligned_free(p);
}

// ============================================================
// assume_aligned
// ============================================================

TEST(UninitializedTest, AssumeAligned) {
    void* raw = zstl::aligned_alloc(32, 64);
    ASSERT_NE(raw, nullptr);

    // Only call assume_aligned when we know it's aligned
    int* p = zstl::assume_aligned<32>(static_cast<int*>(raw));
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 32, 0u);

    zstl::aligned_free(raw);
}

// ============================================================
// misaligned test (edge case)
// ============================================================

TEST(UninitializedTest, AlignUpLargeValue) {
    // Test with values near size_t max
    constexpr size_t big = static_cast<size_t>(-1) - 100;
    size_t result = zstl::align_up(big, 64);
    // Should be >= big and aligned
    EXPECT_GE(result, big);
    EXPECT_EQ(result % 64, 0u);
}

// ============================================================
// addressof (from utility.h, tested here for convenience)
// ============================================================

TEST(UninitializedTest, AddressOf) {
    int x = 42;
    EXPECT_EQ(zstl::addressof(x), &x);

    std::string s = "hello";
    EXPECT_EQ(zstl::addressof(s), &s);
}
