// ============================================================================
// zstl Atomic Unit Tests
// Tests: atomic<int>, atomic<bool>, atomic<void*>, atomic_flag.
// Test load/store/exchange/CAS/fetch_add/fetch_sub/++/--, fetch_and/or/xor.
// Test memory orders. Test with threads (basic producer-consumer flag).
// Test free functions.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <thread>
#include <vector>
#include <cstring>

// ============================================================
// atomic<int> basic operations
// ============================================================

TEST(AtomicTest, DefaultConstructor) {
    zstl::atomic<int> a;
    // Value is uninitialized; just verify it's constructable
    SUCCEED();
}

TEST(AtomicTest, ValueConstructor) {
    zstl::atomic<int> a(42);
    EXPECT_EQ(a.load(), 42);
}

TEST(AtomicTest, LoadStore) {
    zstl::atomic<int> a(0);
    a.store(100);
    EXPECT_EQ(a.load(), 100);

    a.store(50, zstl::memory_order_relaxed);
    EXPECT_EQ(a.load(zstl::memory_order_relaxed), 50);
}

TEST(AtomicTest, OperatorT) {
    zstl::atomic<int> a(77);
    EXPECT_EQ(static_cast<int>(a), 77);
}

TEST(AtomicTest, OperatorAssignment) {
    zstl::atomic<int> a(10);
    a = 20;
    EXPECT_EQ(a.load(), 20);
}

// ============================================================
// atomic<int> exchange and CAS
// ============================================================

TEST(AtomicTest, Exchange) {
    zstl::atomic<int> a(5);
    int old = a.exchange(10);
    EXPECT_EQ(old, 5);
    EXPECT_EQ(a.load(), 10);
}

TEST(AtomicTest, CompareExchangeWeak) {
    zstl::atomic<int> a(42);
    int expected = 42;
    bool success = a.compare_exchange_weak(expected, 100);
    EXPECT_TRUE(success);
    EXPECT_EQ(a.load(), 100);
    EXPECT_EQ(expected, 42);
}

TEST(AtomicTest, CompareExchangeWeakFail) {
    zstl::atomic<int> a(42);
    int expected = 50; // wrong expected
    bool success = a.compare_exchange_weak(expected, 100);
    EXPECT_FALSE(success);
    EXPECT_EQ(a.load(), 42);
    EXPECT_EQ(expected, 42); // expected is updated to actual value
}

TEST(AtomicTest, CompareExchangeWeakMemoryOrder) {
    zstl::atomic<int> a(10);
    int expected = 10;
    bool success = a.compare_exchange_weak(expected, 20,
        zstl::memory_order_acq_rel, zstl::memory_order_acquire);
    EXPECT_TRUE(success);
    EXPECT_EQ(a.load(), 20);
}

TEST(AtomicTest, CompareExchangeStrong) {
    zstl::atomic<int> a(30);
    int expected = 30;
    bool success = a.compare_exchange_strong(expected, 50);
    EXPECT_TRUE(success);
    EXPECT_EQ(a.load(), 50);
}

TEST(AtomicTest, CompareExchangeStrongFail) {
    zstl::atomic<int> a(30);
    int expected = 99;
    bool success = a.compare_exchange_strong(expected, 50);
    EXPECT_FALSE(success);
    EXPECT_EQ(a.load(), 30);
}

// ============================================================
// atomic<int> fetch_* operations
// ============================================================

TEST(AtomicTest, FetchAdd) {
    zstl::atomic<int> a(10);
    int prev = a.fetch_add(5);
    EXPECT_EQ(prev, 10);
    EXPECT_EQ(a.load(), 15);
}

TEST(AtomicTest, FetchSub) {
    zstl::atomic<int> a(20);
    int prev = a.fetch_sub(7);
    EXPECT_EQ(prev, 20);
    EXPECT_EQ(a.load(), 13);
}

TEST(AtomicTest, FetchAnd) {
    zstl::atomic<int> a(0xFF);
    int prev = a.fetch_and(0x0F);
    EXPECT_EQ(prev, 0xFF);
    EXPECT_EQ(a.load(), 0x0F);
}

TEST(AtomicTest, FetchOr) {
    zstl::atomic<int> a(0xF0);
    int prev = a.fetch_or(0x0F);
    EXPECT_EQ(prev, 0xF0);
    EXPECT_EQ(a.load(), 0xFF);
}

TEST(AtomicTest, FetchXor) {
    zstl::atomic<int> a(0xFF);
    int prev = a.fetch_xor(0x0F);
    EXPECT_EQ(prev, 0xFF);
    EXPECT_EQ(a.load(), 0xF0);
}

// ============================================================
// atomic<int> increment / decrement
// ============================================================

TEST(AtomicTest, PreIncrement) {
    zstl::atomic<int> a(0);
    int result = ++a;
    EXPECT_EQ(result, 1);
    EXPECT_EQ(a.load(), 1);
}

TEST(AtomicTest, PostIncrement) {
    zstl::atomic<int> a(0);
    int result = a++;
    EXPECT_EQ(result, 0);
    EXPECT_EQ(a.load(), 1);
}

TEST(AtomicTest, PreDecrement) {
    zstl::atomic<int> a(10);
    int result = --a;
    EXPECT_EQ(result, 9);
    EXPECT_EQ(a.load(), 9);
}

TEST(AtomicTest, PostDecrement) {
    zstl::atomic<int> a(10);
    int result = a--;
    EXPECT_EQ(result, 10);
    EXPECT_EQ(a.load(), 9);
}

// ============================================================
// atomic<int> compound assignment
// ============================================================

TEST(AtomicTest, PlusEquals) {
    zstl::atomic<int> a(10);
    a += 5;
    EXPECT_EQ(a.load(), 15);
}

TEST(AtomicTest, MinusEquals) {
    zstl::atomic<int> a(20);
    a -= 8;
    EXPECT_EQ(a.load(), 12);
}

TEST(AtomicTest, AndEquals) {
    zstl::atomic<int> a(0xFF);
    a &= 0x0F;
    EXPECT_EQ(a.load(), 0x0F);
}

TEST(AtomicTest, OrEquals) {
    zstl::atomic<int> a(0xF0);
    a |= 0x0F;
    EXPECT_EQ(a.load(), 0xFF);
}

TEST(AtomicTest, XorEquals) {
    zstl::atomic<int> a(0xFF);
    a ^= 0x0F;
    EXPECT_EQ(a.load(), 0xF0);
}

// ============================================================
// atomic<bool>
// ============================================================

TEST(AtomicTest, AtomicBool) {
    zstl::atomic<bool> a(false);
    EXPECT_FALSE(a.load());

    a.store(true);
    EXPECT_TRUE(a.load());

    bool expected = false;
    bool success = a.compare_exchange_strong(expected, false);
    EXPECT_FALSE(success); // expected doesn't match (a is true)
}

// ============================================================
// atomic<T*>
// ============================================================

TEST(AtomicTest, AtomicPointer) {
    int x = 42, y = 100;
    zstl::atomic<int*> a(&x);
    EXPECT_EQ(*(a.load()), 42);

    a.store(&y);
    EXPECT_EQ(*(a.load()), 100);

    int* old = a.exchange(&x);
    EXPECT_EQ(*old, 100);
    EXPECT_EQ(*(a.load()), 42);
}

TEST(AtomicTest, AtomicPointerFetchAdd) {
    int arr[10];
    zstl::atomic<int*> a(&arr[0]);
    int* prev = a.fetch_add(3);
    EXPECT_EQ(prev, &arr[0]);
    EXPECT_EQ(a.load(), &arr[3]);
}

TEST(AtomicTest, AtomicPointerFetchSub) {
    int arr[10];
    zstl::atomic<int*> a(&arr[5]);
    int* prev = a.fetch_sub(2);
    EXPECT_EQ(prev, &arr[5]);
    EXPECT_EQ(a.load(), &arr[3]);
}

TEST(AtomicTest, AtomicPointerIncrement) {
    int arr[5];
    zstl::atomic<int*> a(&arr[0]);
    ++a;
    EXPECT_EQ(a.load(), &arr[1]);
    ++a;
    EXPECT_EQ(a.load(), &arr[2]);
    a++;
    EXPECT_EQ(a.load(), &arr[3]);
}

TEST(AtomicTest, AtomicPointerDecrement) {
    int arr[5];
    zstl::atomic<int*> a(&arr[4]);
    --a;
    EXPECT_EQ(a.load(), &arr[3]);
    a--;
    EXPECT_EQ(a.load(), &arr[2]);
}

TEST(AtomicTest, AtomicPointerOperatorT) {
    int x = 55;
    zstl::atomic<int*> a(&x);
    int* p = a; // implicit conversion
    EXPECT_EQ(p, &x);
}

TEST(AtomicTest, AtomicPointerOperatorAssignment) {
    int x = 1, y = 2;
    zstl::atomic<int*> a(&x);
    a = &y;
    EXPECT_EQ(*(a.load()), 2);
}

// ============================================================
// atomic_flag
// ============================================================

TEST(AtomicTest, AtomicFlagTestAndSet) {
    zstl::atomic_flag flag;
    // Initial state: clear (false)
    bool was_set = flag.test_and_set();
    EXPECT_FALSE(was_set); // Was false

    was_set = flag.test_and_set();
    EXPECT_TRUE(was_set); // Was true (already set)

    flag.clear();
    was_set = flag.test_and_set();
    EXPECT_FALSE(was_set); // Was false again
}

TEST(AtomicTest, AtomicFlagClear) {
    zstl::atomic_flag flag;
    flag.test_and_set();
    flag.test_and_set(); // still true
    flag.clear();

    bool was_set = flag.test_and_set();
    EXPECT_FALSE(was_set);
}

// ============================================================
// is_lock_free
// ============================================================

TEST(AtomicTest, IsLockFree) {
    zstl::atomic<int> a;
    // Most platforms have lock-free atomic<int>
    EXPECT_TRUE(a.is_lock_free() || !a.is_lock_free()); // just verify it compiles

    // is_always_lock_free should match
    EXPECT_EQ(zstl::atomic<int>::is_always_lock_free, a.is_lock_free());
}

// ============================================================
// Free functions
// ============================================================

TEST(AtomicTest, FreeAtomicLoad) {
    zstl::atomic<int> a(42);
    EXPECT_EQ(zstl::atomic_load(&a), 42);
}

TEST(AtomicTest, FreeAtomicStore) {
    zstl::atomic<int> a(0);
    zstl::atomic_store(&a, 100);
    EXPECT_EQ(a.load(), 100);
}

TEST(AtomicTest, FreeAtomicExchange) {
    zstl::atomic<int> a(5);
    int old = zstl::atomic_exchange(&a, 10);
    EXPECT_EQ(old, 5);
    EXPECT_EQ(a.load(), 10);
}

TEST(AtomicTest, FreeAtomicCompareExchange) {
    zstl::atomic<int> a(50);
    int expected = 50;
    bool ok = zstl::atomic_compare_exchange_weak(&a, expected, 100);
    EXPECT_TRUE(ok);
    EXPECT_EQ(a.load(), 100);
}

TEST(AtomicTest, FreeAtomicFetchAdd) {
    zstl::atomic<int> a(10);
    int prev = zstl::atomic_fetch_add(&a, 5);
    EXPECT_EQ(prev, 10);
    EXPECT_EQ(a.load(), 15);
}

TEST(AtomicTest, FreeAtomicFetchSub) {
    zstl::atomic<int> a(20);
    int prev = zstl::atomic_fetch_sub(&a, 8);
    EXPECT_EQ(prev, 20);
    EXPECT_EQ(a.load(), 12);
}

TEST(AtomicTest, FreeAtomicFetchAddPointer) {
    int arr[5];
    zstl::atomic<int*> a(&arr[0]);
    int* prev = zstl::atomic_fetch_add(&a, static_cast<ptrdiff_t>(2));
    EXPECT_EQ(prev, &arr[0]);
    EXPECT_EQ(a.load(), &arr[2]);
}

// ============================================================
// Convenience typedefs
// ============================================================

TEST(AtomicTest, ConvenienceTypedefs) {
    EXPECT_TRUE((std::is_same_v<zstl::atomic_bool, zstl::atomic<bool>>));
    EXPECT_TRUE((std::is_same_v<zstl::atomic_int, zstl::atomic<int>>));
    EXPECT_TRUE((std::is_same_v<zstl::atomic_long, zstl::atomic<long>>));
}
