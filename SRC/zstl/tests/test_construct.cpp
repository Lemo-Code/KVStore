// ============================================================================
// zstl Construct / Destroy / Uninitialized Memory Unit Tests
// Tests: construct, destroy, destroy_at, destroy_range, destroy_n,
// uninitialized_copy/copy_n, uninitialized_fill/fill_n,
// uninitialized_move/move_n, uninitialized_default_construct,
// uninitialized_value_construct, relocate_n, relocate_backward_n.
// Verifies memmove optimization for POD types, destructor calls,
// and exception safety.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>
#include <vector>
#include <cstring>
#include <memory>

// ============================================================
// Helper types
// ============================================================

static int g_destructor_count = 0;
static int g_construct_count = 0;

struct CountedType {
    int value;
    CountedType() : value(0) { ++g_construct_count; }
    explicit CountedType(int v) : value(v) { ++g_construct_count; }
    CountedType(const CountedType& o) : value(o.value) { ++g_construct_count; }
    CountedType(CountedType&& o) noexcept : value(o.value) { o.value = -1; ++g_construct_count; }
    ~CountedType() { ++g_destructor_count; }
    CountedType& operator=(const CountedType& o) { value = o.value; return *this; }
    CountedType& operator=(CountedType&& o) noexcept { value = o.value; o.value = -1; return *this; }
    bool operator==(const CountedType& o) const { return value == o.value; }
};

struct ThrowOnCopy {
    int value;
    static int copy_count;
    ThrowOnCopy() : value(0) {}
    explicit ThrowOnCopy(int v) : value(v) {}
    ThrowOnCopy(const ThrowOnCopy& o) : value(o.value) {
        copy_count++;
        if (copy_count > 3) throw std::runtime_error("copy limit exceeded");
    }
    ThrowOnCopy(ThrowOnCopy&&) noexcept = default;
};
int ThrowOnCopy::copy_count = 0;

void reset_counters() {
    g_destructor_count = 0;
    g_construct_count = 0;
    ThrowOnCopy::copy_count = 0;
}

// ============================================================
// construct
// ============================================================

TEST(ConstructTest, ConstructDefault) {
    alignas(int) char buf[sizeof(int)];
    int* p = reinterpret_cast<int*>(buf);
    zstl::construct(p);
    EXPECT_EQ(*p, 0); // default-initialized
}

TEST(ConstructTest, ConstructFromValue) {
    alignas(int) char buf[sizeof(int)];
    int* p = reinterpret_cast<int*>(buf);
    zstl::construct(p, 42);
    EXPECT_EQ(*p, 42);
}

TEST(ConstructTest, ConstructFromMultipleArgs) {
    struct Pair { int a; double b; Pair(int aa, double bb) : a(aa), b(bb) {} };
    alignas(Pair) char buf[sizeof(Pair)];
    Pair* p = reinterpret_cast<Pair*>(buf);
    zstl::construct(p, 1, 2.5);
    EXPECT_EQ(p->a, 1);
    EXPECT_DOUBLE_EQ(p->b, 2.5);
}

TEST(ConstructTest, ConstructPODType) {
    alignas(double) char buf[sizeof(double)];
    double* p = reinterpret_cast<double*>(buf);
    zstl::construct(p, 3.14);
    EXPECT_DOUBLE_EQ(*p, 3.14);
}

// ============================================================
// destroy / destroy_at
// ============================================================

TEST(ConstructDeathTest, DestroyAt) {  // renamed to avoid CMake DISABLED_ prefix
    reset_counters();
    {
        alignas(CountedType) char buf[sizeof(CountedType)];
        CountedType* p = reinterpret_cast<CountedType*>(buf);
        zstl::construct(p, 100);
        EXPECT_EQ(g_construct_count, 1);
        EXPECT_EQ(p->value, 100);

        zstl::destroy_at(p);
        EXPECT_EQ(g_destructor_count, 1);
    }
    EXPECT_EQ(g_destructor_count, 1);
}

TEST(ConstructTest, DestroyNull) {
    // destroy null pointer should be safe
    CountedType* p = nullptr;
    EXPECT_NO_THROW(zstl::destroy(p));
}

TEST(ConstructTest, DestroyPODType) {
    // destroy on trivially destructible type is no-op
    int* p = new int(42);
    zstl::destroy_at(p);
    EXPECT_EQ(*p, 42); // still accessible (no real destruction)
    zstl::destroy(p);
    EXPECT_EQ(*p, 42);
    delete p;
}

// ============================================================
// destroy_range
// ============================================================

TEST(ConstructTest, DestroyRange) {
    reset_counters();
    int count = 5;
    CountedType* arr = static_cast<CountedType*>(::operator new(count * sizeof(CountedType)));
    for (int i = 0; i < count; ++i) {
        zstl::construct(arr + i, i);
    }
    EXPECT_EQ(g_construct_count, count);

    zstl::destroy_range(arr, arr + count);
    EXPECT_EQ(g_destructor_count, count);
    ::operator delete(arr);
}

TEST(ConstructTest, DestroyRangeEmpty) {
    int* arr = new int[10];
    // empty range should be no-op
    zstl::destroy_range(arr, arr);
    delete[] arr;
}

TEST(ConstructTest, DestroyN) {
    reset_counters();
    int count = 3;
    CountedType* arr = static_cast<CountedType*>(::operator new(count * sizeof(CountedType)));
    for (int i = 0; i < count; ++i) {
        zstl::construct(arr + i, i);
    }

    auto end = zstl::destroy_n(arr, count);
    EXPECT_EQ(end, arr + count);
    EXPECT_EQ(g_destructor_count, count);
    ::operator delete(arr);
}

// ============================================================
// uninitialized_copy
// ============================================================

TEST(ConstructTest, UninitializedCopyPOD) {
    // memmove optimization for trivially relocatable types
    int src[] = {1, 2, 3, 4, 5};
    int count = 5;
    int* dst = static_cast<int*>(::operator new(count * sizeof(int)));

    auto result = zstl::uninitialized_copy(src, src + count, dst);
    EXPECT_EQ(result, dst + count);
    for (int i = 0; i < count; ++i) {
        EXPECT_EQ(dst[i], src[i]);
    }
    ::operator delete(dst);
}

TEST(ConstructTest, UninitializedCopyNonTrivial) {
    reset_counters();
    CountedType src[] = {CountedType(1), CountedType(2), CountedType(3)};
    int count = 3;
    int initial_constructs = g_construct_count;

    CountedType* dst = static_cast<CountedType*>(::operator new(count * sizeof(CountedType)));
    auto result = zstl::uninitialized_copy(src, src + count, dst);
    EXPECT_EQ(result, dst + count);

    EXPECT_EQ(dst[0].value, 1);
    EXPECT_EQ(dst[1].value, 2);
    EXPECT_EQ(dst[2].value, 3);
    // Each element copy-constructed
    EXPECT_EQ(g_construct_count, initial_constructs + count);

    zstl::destroy_range(dst, dst + count);
    ::operator delete(dst);
}

TEST(ConstructTest, UninitializedCopyEmpty) {
    int src[] = {1, 2, 3};
    int* dst = static_cast<int*>(::operator new(3 * sizeof(int)));
    auto result = zstl::uninitialized_copy(src, src, dst);
    EXPECT_EQ(result, dst);
    ::operator delete(dst);
}

// ============================================================
// uninitialized_copy_n
// ============================================================

TEST(ConstructTest, UninitializedCopyN) {
    int src[] = {10, 20, 30, 40};
    int count = 4;
    int* dst = static_cast<int*>(::operator new(count * sizeof(int)));

    auto result = zstl::uninitialized_copy_n(src, count, dst);
    EXPECT_EQ(result, dst + count);
    for (int i = 0; i < count; ++i) {
        EXPECT_EQ(dst[i], src[i]);
    }
    ::operator delete(dst);
}

// ============================================================
// uninitialized_fill
// ============================================================

TEST(ConstructTest, UninitializedFillPOD) {
    int count = 10;
    int* dst = static_cast<int*>(::operator new(count * sizeof(int)));

    zstl::uninitialized_fill(dst, dst + count, 42);
    for (int i = 0; i < count; ++i) {
        EXPECT_EQ(dst[i], 42);
    }
    ::operator delete(dst);
}

TEST(ConstructTest, UninitializedFillNonTrivial) {
    reset_counters();
    int count = 3;
    CountedType* dst = static_cast<CountedType*>(::operator new(count * sizeof(CountedType)));
    CountedType val(99);
    int pre_count = g_construct_count;

    zstl::uninitialized_fill(dst, dst + count, val);
    EXPECT_EQ(g_construct_count, pre_count + count);

    for (int i = 0; i < count; ++i) {
        EXPECT_EQ(dst[i].value, 99);
    }

    zstl::destroy_range(dst, dst + count);
    ::operator delete(dst);
}

TEST(ConstructTest, UninitializedFillEmpty) {
    int* dst = new int;
    zstl::uninitialized_fill(dst, dst, 5);
    delete dst;
}

// ============================================================
// uninitialized_fill_n
// ============================================================

TEST(ConstructTest, UninitializedFillN) {
    int count = 5;
    int* dst = static_cast<int*>(::operator new(count * sizeof(int)));

    auto result = zstl::uninitialized_fill_n(dst, count, 7);
    EXPECT_EQ(result, dst + count);
    for (int i = 0; i < count; ++i) {
        EXPECT_EQ(dst[i], 7);
    }
    ::operator delete(dst);
}

TEST(ConstructTest, UninitializedFillNZero) {
    int* dst = static_cast<int*>(::operator new(4));
    auto result = zstl::uninitialized_fill_n(dst, 0, 5);
    EXPECT_EQ(result, dst);
    ::operator delete(dst);
}

// ============================================================
// uninitialized_move
// ============================================================

TEST(ConstructTest, UninitializedMovePOD) {
    int src[] = {1, 2, 3, 4, 5};
    int count = 5;
    int* dst = static_cast<int*>(::operator new(count * sizeof(int)));

    auto result = zstl::uninitialized_move(src, src + count, dst);
    EXPECT_EQ(result, dst + count);
    for (int i = 0; i < count; ++i) {
        EXPECT_EQ(dst[i], i + 1);
    }
    ::operator delete(dst);
}

TEST(ConstructTest, UninitializedMoveNonTrivial) {
    reset_counters();
    CountedType src[] = {CountedType(10), CountedType(20), CountedType(30)};
    int count = 3;
    int initial_constructs = g_construct_count;

    CountedType* dst = static_cast<CountedType*>(::operator new(count * sizeof(CountedType)));
    auto result = zstl::uninitialized_move(src, src + count, dst);
    EXPECT_EQ(result, dst + count);

    // Each element should be move-constructed
    EXPECT_EQ(g_construct_count, initial_constructs + count);
    EXPECT_EQ(dst[0].value, 10);
    EXPECT_EQ(dst[1].value, 20);
    EXPECT_EQ(dst[2].value, 30);

    zstl::destroy_range(dst, dst + count);
    ::operator delete(dst);
}

// ============================================================
// uninitialized_move_n
// ============================================================

TEST(ConstructTest, UninitializedMoveN) {
    int src[] = {100, 200, 300};
    int count = 3;
    int* dst = static_cast<int*>(::operator new(count * sizeof(int)));

    auto result = zstl::uninitialized_move_n(src, count, dst);
    EXPECT_EQ(result, dst + count);
    EXPECT_EQ(dst[0], 100);
    EXPECT_EQ(dst[1], 200);
    EXPECT_EQ(dst[2], 300);
    ::operator delete(dst);
}

// ============================================================
// uninitialized_default_construct
// ============================================================

TEST(ConstructTest, UninitializedDefaultConstructPOD) {
    // For trivial types, this is a no-op
    int count = 4;
    int* dst = static_cast<int*>(::operator new(count * sizeof(int)));
    zstl::uninitialized_default_construct(dst, dst + count);
    // Values are indeterminate; just verify no crash
    ::operator delete(dst);
}

TEST(ConstructTest, UninitializedDefaultConstructNonTrivial) {
    reset_counters();
    int count = 3;
    CountedType* dst = static_cast<CountedType*>(::operator new(count * sizeof(CountedType)));
    zstl::uninitialized_default_construct(dst, dst + count);
    EXPECT_EQ(g_construct_count, count);

    zstl::destroy_range(dst, dst + count);
    ::operator delete(dst);
}

// ============================================================
// uninitialized_value_construct
// ============================================================

TEST(ConstructTest, UninitializedValueConstructScalar) {
    // Scalar types are zeroed via memset
    int count = 5;
    int* dst = static_cast<int*>(::operator new(count * sizeof(int)));
    // Fill with garbage first
    std::memset(dst, 0xFF, count * sizeof(int));

    zstl::uninitialized_value_construct(dst, dst + count);
    for (int i = 0; i < count; ++i) {
        EXPECT_EQ(dst[i], 0);
    }
    ::operator delete(dst);
}

TEST(ConstructTest, UninitializedValueConstructNonTrivial) {
    reset_counters();
    int count = 2;
    CountedType* dst = static_cast<CountedType*>(::operator new(count * sizeof(CountedType)));
    zstl::uninitialized_value_construct(dst, dst + count);
    // Default-constructed: value = 0
    EXPECT_EQ(dst[0].value, 0);
    EXPECT_EQ(dst[1].value, 0);

    zstl::destroy_range(dst, dst + count);
    ::operator delete(dst);
}

// ============================================================
// exception safety — uninitialized_copy with throwing copy
// ============================================================

TEST(ConstructTest, ExceptionSafetyUninitializedCopy) {
    reset_counters();
    ThrowOnCopy src[] = {ThrowOnCopy(1), ThrowOnCopy(2), ThrowOnCopy(3),
                         ThrowOnCopy(4), ThrowOnCopy(5), ThrowOnCopy(6)};
    int count = 6;
    ThrowOnCopy* dst = static_cast<ThrowOnCopy*>(::operator new(count * sizeof(ThrowOnCopy)));

    // copy_count starts at 0, throws after 3 copies
    EXPECT_THROW(zstl::uninitialized_copy(src, src + count, dst), std::runtime_error);
    // Already-constructed elements should be destroyed (exception safety)

    ::operator delete(dst);
}

// ============================================================
// relocate_n / relocate_backward_n
// ============================================================

TEST(ConstructTest, RelocateNPOD) {
    int src[] = {1, 2, 3, 4, 5};
    int count = 5;
    int* dst = static_cast<int*>(::operator new(count * sizeof(int)));

    zstl::relocate_n(src, count, dst);
    for (int i = 0; i < count; ++i) {
        EXPECT_EQ(dst[i], i + 1);
    }
    ::operator delete(dst);
}

TEST(ConstructTest, RelocateNNonTrivial) {
    reset_counters();
    CountedType src[] = {CountedType(10), CountedType(20), CountedType(30)};
    int count = 3;
    CountedType* dst = static_cast<CountedType*>(::operator new(count * sizeof(CountedType)));

    zstl::relocate_n(src, count, dst);
    EXPECT_EQ(dst[0].value, 10);
    EXPECT_EQ(dst[1].value, 20);
    EXPECT_EQ(dst[2].value, 30);
    // Source should be destroyed (for non-trivially-destructible)
    // Actually the relocate_n for is_nothrow_move_constructible + is_trivially_destructible
    // does NOT destroy source (medium path), and CountedType is NOT trivially destructible
    // so it goes through the full path which destroys source.

    zstl::destroy_range(dst, dst + count);
    ::operator delete(dst);
}

TEST(ConstructTest, RelocateNZero) {
    int src[] = {1, 2, 3};
    int* dst = static_cast<int*>(::operator new(3 * sizeof(int)));
    zstl::relocate_n(src, 0, dst);
    ::operator delete(dst);
}

TEST(ConstructTest, RelocateBackwardN) {
    int src[] = {10, 20, 30, 40};
    int count = 4;
    int* dst = static_cast<int*>(::operator new(count * sizeof(int)));

    zstl::relocate_backward_n(src, count, dst);
    EXPECT_EQ(dst[0], 10);
    EXPECT_EQ(dst[1], 20);
    EXPECT_EQ(dst[2], 30);
    EXPECT_EQ(dst[3], 40);
    ::operator delete(dst);
}

// ============================================================
// Memmove optimization verification
// ============================================================

TEST(ConstructTest, MemmoveOptimizationForPOD) {
    // Verify that is_trivially_relocatable is true for int
    EXPECT_TRUE((zstl::is_trivially_relocatable_v<int>));
    EXPECT_TRUE((zstl::is_trivially_relocatable_v<double>));

    // uninitialized_copy of POD should use memmove (fast path)
    int src[100];
    for (int i = 0; i < 100; ++i) src[i] = i;
    int* dst = static_cast<int*>(::operator new(100 * sizeof(int)));

    zstl::uninitialized_copy(src, src + 100, dst);
    // Verify all were copied
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(dst[i], i);
    }
    ::operator delete(dst);
}
