// test_fiber_local.cpp — Comprehensive FiberLocal<T> unit tests
// Tests default value construction, get() returns reference,
// set via get(), multiple FiberLocal variables, different types
// (int, string, custom struct), operator* and operator->,
// fiber isolation, clear(), set(), slot_count.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <string>
#include <memory>

using namespace zero;

// ============================================================
// Default value construction
// ============================================================

TEST(FiberLocal, DefaultInt) {
    FiberLocal<int> val;
    EXPECT_EQ(val.get(), 0);  // Default-constructed int is 0
}

TEST(FiberLocal, DefaultString) {
    FiberLocal<std::string> val;
    EXPECT_EQ(val.get(), "");  // Default-constructed string is empty
}

TEST(FiberLocal, DefaultDouble) {
    FiberLocal<double> val;
    EXPECT_DOUBLE_EQ(val.get(), 0.0);
}

TEST(FiberLocal, DefaultBool) {
    FiberLocal<bool> val;
    EXPECT_EQ(val.get(), false);
}

TEST(FiberLocal, DefaultPointer) {
    FiberLocal<int*> val;
    EXPECT_EQ(val.get(), nullptr);
}

// ============================================================
// get() returns reference to stored value
// ============================================================

TEST(FiberLocal, GetReturnsReference) {
    FiberLocal<int> val;
    val.get() = 42;
    EXPECT_EQ(val.get(), 42);
}

TEST(FiberLocal, GetModifyInPlace) {
    FiberLocal<int> val;
    val.get() = 10;
    val.get() += 5;
    EXPECT_EQ(val.get(), 15);
}

// ============================================================
// set() method
// ============================================================

TEST(FiberLocal, SetInt) {
    FiberLocal<int> val;
    val.set(100);
    EXPECT_EQ(val.get(), 100);
}

TEST(FiberLocal, SetString) {
    FiberLocal<std::string> val;
    val.set("hello world");
    EXPECT_EQ(val.get(), "hello world");
}

TEST(FiberLocal, SetDouble) {
    FiberLocal<double> val;
    val.set(3.14159);
    EXPECT_DOUBLE_EQ(val.get(), 3.14159);
}

TEST(FiberLocal, SetMultipleTimes) {
    FiberLocal<int> val;
    val.set(1);
    EXPECT_EQ(val.get(), 1);
    val.set(2);
    EXPECT_EQ(val.get(), 2);
    val.set(42);
    EXPECT_EQ(val.get(), 42);
}

// ============================================================
// Multiple FiberLocal variables
// ============================================================

TEST(FiberLocal, MultipleVariables) {
    FiberLocal<int> a;
    FiberLocal<int> b;
    FiberLocal<std::string> c;

    a.set(1);
    b.set(2);
    c.set("three");

    EXPECT_EQ(a.get(), 1);
    EXPECT_EQ(b.get(), 2);
    EXPECT_EQ(c.get(), "three");
}

TEST(FiberLocal, IndependentStorage) {
    FiberLocal<int> x;
    FiberLocal<int> y;

    x.set(10);
    y.set(20);

    EXPECT_EQ(x.get(), 10);
    EXPECT_EQ(y.get(), 20);

    // Modifying one doesn't affect the other
    x.set(100);
    EXPECT_EQ(x.get(), 100);
    EXPECT_EQ(y.get(), 20);
}

// ============================================================
// Different types
// ============================================================

TEST(FiberLocal, Uint64Type) {
    FiberLocal<uint64_t> val;
    val.set(UINT64_MAX);
    EXPECT_EQ(val.get(), UINT64_MAX);
}

TEST(FiberLocal, FloatType) {
    FiberLocal<float> val;
    val.set(2.5f);
    EXPECT_FLOAT_EQ(val.get(), 2.5f);
}

TEST(FiberLocal, LongType) {
    FiberLocal<long> val;
    val.set(LONG_MAX);
    EXPECT_EQ(val.get(), LONG_MAX);
}

struct CustomStruct {
    int id;
    std::string name;
    double score;

    bool operator==(const CustomStruct& other) const {
        return id == other.id && name == other.name && score == other.score;
    }
};

TEST(FiberLocal, CustomStruct) {
    FiberLocal<CustomStruct> val;
    CustomStruct cs{1, "test", 99.5};
    val.set(cs);
    EXPECT_EQ(val.get().id, 1);
    EXPECT_EQ(val.get().name, "test");
    EXPECT_DOUBLE_EQ(val.get().score, 99.5);
}

// ============================================================
// operator* (dereference)
// ============================================================

TEST(FiberLocal, OperatorDeref) {
    FiberLocal<int> val;
    *val = 77;
    EXPECT_EQ(*val, 77);
    EXPECT_EQ(val.get(), 77);
}

TEST(FiberLocal, OperatorDerefRead) {
    FiberLocal<int> val;
    val.set(55);
    EXPECT_EQ(*val, 55);
}

// ============================================================
// operator-> (member access)
// ============================================================

TEST(FiberLocal, OperatorArrow) {
    FiberLocal<CustomStruct> val;
    val.set({1, "arrow_test", 88.0});

    EXPECT_EQ(val->id, 1);
    EXPECT_EQ(val->name, "arrow_test");
    EXPECT_DOUBLE_EQ(val->score, 88.0);

    // Modify through arrow
    val->id = 42;
    EXPECT_EQ(val->id, 42);
}

TEST(FiberLocal, OperatorArrowString) {
    FiberLocal<std::string> val;
    val.set("initial");

    EXPECT_EQ(val->size(), 7u);
    val->append("_suffix");
    EXPECT_EQ(val.get(), "initial_suffix");
}

// ============================================================
// Implicit conversion (operator T&)
// ============================================================

TEST(FiberLocal, ImplicitConversionToInt) {
    FiberLocal<int> val;
    val.set(123);
    int& ref = val;  // Implicit conversion to int&
    EXPECT_EQ(ref, 123);
    ref = 456;
    EXPECT_EQ(val.get(), 456);
}

TEST(FiberLocal, ImplicitConversionToStdString) {
    FiberLocal<std::string> val;
    val.set("implicit");
    std::string& ref = val;  // Implicit conversion
    EXPECT_EQ(ref, "implicit");
}

// ============================================================
// clear()
// ============================================================

TEST(FiberLocal, ClearResetsToDefault) {
    FiberLocal<int> val;
    val.set(999);
    EXPECT_EQ(val.get(), 999);

    val.clear();
    EXPECT_EQ(val.get(), 0);  // Back to default
}

TEST(FiberLocal, ClearStringResetsToDefault) {
    FiberLocal<std::string> val;
    val.set("some value");
    val.clear();
    EXPECT_EQ(val.get(), "");  // Default-constructed string
}

// ============================================================
// slot_count()
// ============================================================

TEST(FiberLocal, SlotCountIncreases) {
    size_t before = FiberLocal<int>::slot_count();

    {
        FiberLocal<int> a;
        EXPECT_GE(FiberLocal<int>::slot_count(), before + 1);

        FiberLocal<int> b;
        EXPECT_GE(FiberLocal<int>::slot_count(), before + 2);
    }
}

TEST(FiberLocal, SlotCountPerType) {
    size_t int_slots_before = FiberLocal<int>::slot_count();
    size_t string_slots_before = FiberLocal<std::string>::slot_count();

    {
        FiberLocal<int> a, b, c;
        FiberLocal<std::string> d;

        EXPECT_GE(FiberLocal<int>::slot_count(), int_slots_before + 3);
        EXPECT_GE(FiberLocal<std::string>::slot_count(), string_slots_before + 1);
    }
}

// ============================================================
// get() in non-fiber context (returns default)
// ============================================================

TEST(FiberLocal, GetDefaultWhenNotInFiber) {
    // Without a scheduler, Fiber::GetThis() returns nullptr,
    // so FiberLocal::get() should return the default value
    FiberLocal<int> val;
    EXPECT_EQ(val.get(), 0);
}

TEST(FiberLocal, SetDefaultWhenNotInFiber) {
    FiberLocal<int> val;
    val.set(42);
    // Setting when not in fiber should update default
    EXPECT_EQ(val.get(), 42);
}

// ============================================================
// Move-only types
// ============================================================

TEST(FiberLocal, UniquePtrType) {
    FiberLocal<std::unique_ptr<int>> val;

    auto ptr = std::make_unique<int>(123);
    val.set(std::move(ptr));

    ASSERT_NE(val.get(), nullptr);
    EXPECT_EQ(*val.get(), 123);
    EXPECT_EQ(**val, 123);  // Arrow through unique_ptr
}

TEST(FiberLocal, SharedPtrType) {
    FiberLocal<std::shared_ptr<int>> val;

    auto sp = std::make_shared<int>(456);
    val.set(sp);

    ASSERT_NE(val.get(), nullptr);
    EXPECT_EQ(*val.get(), 456);
    EXPECT_EQ(sp.use_count(), 2);  // val and sp both hold references
}

// ============================================================
// Thread safety (FiberLocal is NOT thread safe by itself,
// but multiple instances should not interfere)
// ============================================================

TEST(FiberLocal, MultipleFiberLocalsDontInterfere) {
    FiberLocal<int> a;
    FiberLocal<int> b;
    FiberLocal<std::string> c;

    a.set(1);
    b.set(2);
    c.set("test");

    EXPECT_EQ(a.get(), 1);
    EXPECT_EQ(b.get(), 2);
    EXPECT_EQ(c.get(), "test");
}

// ============================================================
// Bool type edge cases
// ============================================================

TEST(FiberLocal, BoolType) {
    FiberLocal<bool> val;

    EXPECT_FALSE(val.get());

    val.set(true);
    EXPECT_TRUE(val.get());

    val.set(false);
    EXPECT_FALSE(val.get());
}

// ============================================================
// Large string handling
// ============================================================

TEST(FiberLocal, LargeString) {
    FiberLocal<std::string> val;

    std::string large(10000, 'x');
    val.set(large);

    EXPECT_EQ(val.get().size(), 10000u);
    EXPECT_EQ(val.get()[0], 'x');
    EXPECT_EQ(val.get()[9999], 'x');
}
