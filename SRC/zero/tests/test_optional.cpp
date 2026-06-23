// test_optional.cpp — Comprehensive optional<T> unit tests
// Tests nullopt/default state, value construction, copy/move, assignment,
// observers (has_value, value, value_or, operator*, operator->),
// modifiers (emplace, reset, swap), make_optional, and all comparisons.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <string>
#include <type_traits>
#include <vector>

using namespace zero;

// ============================================================
// Custom types for testing
// ============================================================

struct TrackedOpt {
    inline static int alive = 0;
    TrackedOpt() { alive++; }
    TrackedOpt(const TrackedOpt&) { alive++; }
    TrackedOpt(TrackedOpt&&) noexcept { alive++; }
    ~TrackedOpt() { alive--; }
};

struct NonTrivialOpt {
    std::string name;
    int value = 0;
    NonTrivialOpt() = default;
    NonTrivialOpt(std::string n, int v) : name(std::move(n)), value(v) {}
    bool operator==(const NonTrivialOpt& o) const {
        return name == o.name && value == o.value;
    }
};

// ============================================================
// Default constructor (empty)
// ============================================================

TEST(Optional, DefaultEmpty) {
    optional<int> o;
    EXPECT_FALSE(o.has_value());
    EXPECT_FALSE(static_cast<bool>(o));
}

TEST(Optional, NulloptEmpty) {
    optional<int> o = nullopt;
    EXPECT_FALSE(o.has_value());
    optional<int> o2(nullopt);
    EXPECT_FALSE(o2.has_value());
}

// ============================================================
// Value constructor
// ============================================================

TEST(Optional, ValueConstructor) {
    optional<int> o = 42;
    EXPECT_TRUE(o.has_value());
    EXPECT_EQ(o.value(), 42);
    EXPECT_EQ(*o, 42);
}

TEST(Optional, MoveValueConstructor) {
    std::string s = "hello";
    optional<std::string> o = std::move(s);
    EXPECT_TRUE(o.has_value());
    EXPECT_EQ(*o, "hello");
}

TEST(Optional, StringValueConstructor) {
    optional<std::string> o = std::string("world");
    EXPECT_EQ(*o, "world");
}

// ============================================================
// Copy constructor
// ============================================================

TEST(Optional, CopyConstructor) {
    optional<int> a = 10;
    optional<int> b = a;
    EXPECT_EQ(*a, 10);
    EXPECT_EQ(*b, 10);
}

TEST(Optional, CopyConstructorEmpty) {
    optional<int> a;
    optional<int> b = a;
    EXPECT_FALSE(b.has_value());
}

TEST(Optional, CopyConstructorIndependent) {
    optional<std::string> a = std::string("original");
    optional<std::string> b = a;
    *b = "modified";
    EXPECT_EQ(*a, "original");
    EXPECT_EQ(*b, "modified");
}

// ============================================================
// Move constructor
// ============================================================

TEST(Optional, MoveConstructor) {
    optional<std::string> a = std::string("movable");
    optional<std::string> b = std::move(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_TRUE(b.has_value());
    EXPECT_EQ(*b, "movable");
}

TEST(Optional, MoveConstructorInt) {
    optional<int> a = 42;
    optional<int> b = std::move(a);
    EXPECT_EQ(*b, 42);
}

// ============================================================
// Copy assignment
// ============================================================

TEST(Optional, CopyAssignment) {
    optional<int> a = 10;
    optional<int> b;
    b = a;
    EXPECT_EQ(*b, 10);
}

TEST(Optional, CopyAssignmentOverwrite) {
    optional<int> a = 10;
    optional<int> b = 20;
    b = a;
    EXPECT_EQ(*a, 10);
    EXPECT_EQ(*b, 10);
}

// ============================================================
// Move assignment
// ============================================================

TEST(Optional, MoveAssignment) {
    optional<std::string> a = std::string("move it");
    optional<std::string> b;
    b = std::move(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(*b, "move it");
}

// ============================================================
// Value assignment
// ============================================================

TEST(Optional, ValueAssignmentEmpty) {
    optional<int> o;
    o = 42;
    EXPECT_EQ(*o, 42);
}

TEST(Optional, ValueAssignmentOverwrite) {
    optional<int> o = 10;
    o = 100;
    EXPECT_EQ(*o, 100);
}

TEST(Optional, MoveValueAssignment) {
    optional<std::string> o;
    o = std::string("assigned");
    EXPECT_EQ(*o, "assigned");
    o = std::string("overwritten");
    EXPECT_EQ(*o, "overwritten");
}

// ============================================================
// Assign nullopt
// ============================================================

TEST(Optional, AssignNullopt) {
    optional<int> o = 42;
    o = nullopt;
    EXPECT_FALSE(o.has_value());
}

// ============================================================
// has_value / operator bool
// ============================================================

TEST(Optional, HasValueTrue) {
    optional<int> o = 0; // 0 is a valid value
    EXPECT_TRUE(o.has_value());
    EXPECT_TRUE(static_cast<bool>(o));
}

TEST(Optional, HasValueFalse) {
    optional<int> o;
    EXPECT_FALSE(o.has_value());
    EXPECT_FALSE(static_cast<bool>(o));
}

// ============================================================
// value() — access with exception on empty
// ============================================================

TEST(Optional, Value) {
    optional<int> o = 42;
    EXPECT_EQ(o.value(), 42);
}

TEST(Optional, ValueThrowsOnEmpty) {
    optional<int> o;
    EXPECT_THROW(o.value(), bad_optional_access);
}

TEST(Optional, ValueLvalueRef) {
    optional<int> o = 10;
    o.value() = 20;
    EXPECT_EQ(o.value(), 20);
}

TEST(Optional, ValueConstRef) {
    const optional<int> o = 42;
    EXPECT_EQ(o.value(), 42);
}

TEST(Optional, ValueRvalue) {
    optional<std::string> o = std::string("move me");
    std::string s = std::move(o).value();
    EXPECT_EQ(s, "move me");
}

// ============================================================
// value_or
// ============================================================

TEST(Optional, ValueOrWithValue) {
    optional<int> o = 42;
    EXPECT_EQ(o.value_or(0), 42);
}

TEST(Optional, ValueOrWithEmpty) {
    optional<int> o;
    EXPECT_EQ(o.value_or(99), 99);
}

TEST(Optional, ValueOrString) {
    optional<std::string> o;
    EXPECT_EQ(o.value_or("default"), "default");

    o = "actual";
    EXPECT_EQ(o.value_or("default"), "actual");
}

// ============================================================
// operator* / operator-> / pointer access
// ============================================================

TEST(Optional, Dereference) {
    optional<int> o = 42;
    EXPECT_EQ(*o, 42);
    *o = 100;
    EXPECT_EQ(*o, 100);
}

TEST(Optional, ConstDereference) {
    const optional<int> o = 42;
    EXPECT_EQ(*o, 42);
}

TEST(Optional, ArrowOperator) {
    optional<std::string> o = std::string("hello");
    EXPECT_EQ(o->size(), 5u);
}

TEST(Optional, ConstArrowOperator) {
    const optional<std::string> o = std::string("world");
    EXPECT_EQ(o->size(), 5u);
}

// ============================================================
// emplace
// ============================================================

TEST(Optional, EmplaceOnEmpty) {
    optional<std::string> o;
    o.emplace("hello world");
    EXPECT_EQ(*o, "hello world");
}

TEST(Optional, EmplaceOverwrite) {
    optional<std::string> o = std::string("old");
    o.emplace("new value");
    EXPECT_EQ(*o, "new value");
}

TEST(Optional, EmplaceReturnsReference) {
    optional<int> o;
    int& ref = o.emplace(42);
    EXPECT_EQ(ref, 42);
    ref = 100;
    EXPECT_EQ(*o, 100);
}

TEST(Optional, EmplaceNonTrivial) {
    optional<NonTrivialOpt> o;
    o.emplace("test", 42);
    EXPECT_EQ(o->name, "test");
    EXPECT_EQ(o->value, 42);
}

// ============================================================
// reset
// ============================================================

TEST(Optional, Reset) {
    optional<int> o = 42;
    EXPECT_TRUE(o.has_value());
    o.reset();
    EXPECT_FALSE(o.has_value());
}

TEST(Optional, DoubleReset) {
    optional<int> o = 42;
    o.reset();
    o.reset();
    EXPECT_FALSE(o.has_value());
}

TEST(Optional, ResetOnEmpty) {
    optional<int> o;
    o.reset();
    EXPECT_FALSE(o.has_value());
}

// ============================================================
// swap
// ============================================================

TEST(Optional, SwapBothFull) {
    optional<int> a = 1;
    optional<int> b = 2;
    a.swap(b);
    EXPECT_EQ(*a, 2);
    EXPECT_EQ(*b, 1);
}

TEST(Optional, SwapOneEmpty) {
    optional<int> a = 42;
    optional<int> b;
    a.swap(b);
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(*b, 42);
}

TEST(Optional, SwapBothEmpty) {
    optional<int> a;
    optional<int> b;
    a.swap(b);
    EXPECT_FALSE(a.has_value());
    EXPECT_FALSE(b.has_value());
}

TEST(Optional, StdSwap) {
    optional<int> a = 1;
    optional<int> b = 2;
    std::swap(a, b);
    EXPECT_EQ(*a, 2);
    EXPECT_EQ(*b, 1);
}

// ============================================================
// make_optional
// ============================================================

TEST(Optional, MakeOptionalValue) {
    auto o = make_optional(42);
    static_assert(std::is_same_v<decltype(o), optional<int>>);
    EXPECT_EQ(*o, 42);
}

TEST(Optional, MakeOptionalString) {
    auto o = make_optional<std::string>("hello");
    EXPECT_EQ(*o, "hello");
}

TEST(Optional, MakeOptionalInPlace) {
    auto o = make_optional<std::string>(5, 'x');
    EXPECT_EQ(*o, "xxxxx");
}

// ============================================================
// Comparisons between optionals
// ============================================================

TEST(Optional, EqualityBothFull) {
    optional<int> a = 1, b = 1, c = 2;
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(Optional, EqualityBothEmpty) {
    optional<int> a, b;
    EXPECT_TRUE(a == b);
}

TEST(Optional, EqualityOneEmpty) {
    optional<int> a = 1, b;
    EXPECT_FALSE(a == b);
}

TEST(Optional, NotEqual) {
    optional<int> a = 1, b = 2;
    EXPECT_TRUE(a != b);
    optional<int> e1, e2;
    EXPECT_FALSE(e1 != e2);
}

TEST(Optional, LessThan) {
    optional<int> a = 1, b = 2;
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
    optional<int> empty;
    EXPECT_TRUE(empty < b);
    EXPECT_FALSE(b < empty);
    EXPECT_FALSE(empty < empty);
}

TEST(Optional, GreaterThan) {
    optional<int> a = 1, b = 2;
    EXPECT_FALSE(a > b);
    EXPECT_TRUE(b > a);
}

TEST(Optional, LessEqual) {
    optional<int> a = 1, b = 1, c = 2;
    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(a <= c);
}

TEST(Optional, GreaterEqual) {
    optional<int> a = 1, b = 1, c = 2;
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(c >= a);
}

// ============================================================
// Comparisons with nullopt
// ============================================================

TEST(Optional, EqNullopt) {
    optional<int> empty;
    optional<int> full = 42;
    EXPECT_TRUE(empty == nullopt);
    EXPECT_TRUE(nullopt == empty);
    EXPECT_FALSE(full == nullopt);
    EXPECT_FALSE(nullopt == full);
}

TEST(Optional, NeqNullopt) {
    optional<int> empty;
    optional<int> full = 42;
    EXPECT_FALSE(empty != nullopt);
    EXPECT_FALSE(nullopt != empty);
    EXPECT_TRUE(full != nullopt);
    EXPECT_TRUE(nullopt != full);
}

TEST(Optional, LessThanNullopt) {
    optional<int> empty;
    optional<int> full = 42;
    EXPECT_FALSE(empty < nullopt);
    EXPECT_FALSE(nullopt < empty);
    EXPECT_TRUE(nullopt < full);
}

// ============================================================
// Comparisons with value T
// ============================================================

TEST(Optional, EqValue) {
    optional<int> o = 42;
    EXPECT_TRUE(o == 42);
    EXPECT_TRUE(42 == o);
    EXPECT_FALSE(o == 0);
    EXPECT_FALSE(0 == o);
}

TEST(Optional, NeqValue) {
    optional<int> o = 42;
    EXPECT_FALSE(o != 42);
    EXPECT_FALSE(42 != o);
    EXPECT_TRUE(o != 0);
    EXPECT_TRUE(0 != o);
}

TEST(Optional, LtValue) {
    optional<int> o = 42;
    EXPECT_TRUE(o < 100);
    EXPECT_TRUE(0 < o);
    EXPECT_FALSE(o < 0);
}

// ============================================================
// Tracked destruction
// ============================================================

TEST(Optional, TrackedDestruction) {
    TrackedOpt::alive = 0;
    {
        optional<TrackedOpt> o;
        o.emplace();
        EXPECT_EQ(TrackedOpt::alive, 1);
        o.reset();
        EXPECT_EQ(TrackedOpt::alive, 0);
    }
    EXPECT_EQ(TrackedOpt::alive, 0);
}

TEST(Optional, TrackedCopyDestruction) {
    TrackedOpt::alive = 0;
    {
        optional<TrackedOpt> a;
        a.emplace();
        optional<TrackedOpt> b = a;
        EXPECT_GE(TrackedOpt::alive, 2);
        a.reset();
        b.reset();
        EXPECT_EQ(TrackedOpt::alive, 0);
    }
}

// ============================================================
// Zero-valued optional (0 is not empty)
// ============================================================

TEST(Optional, ZeroIsNotEmpty) {
    optional<int> o = 0;
    EXPECT_TRUE(o.has_value());
    EXPECT_EQ(o.value(), 0);
    EXPECT_EQ(o.value_or(42), 0); // Not 42, because we have a value
}

// ============================================================
// Rvalue member access
// ============================================================

TEST(Optional, RvalueDereference) {
    optional<std::string> o = std::string("rvalue");
    std::string s = *std::move(o);
    EXPECT_EQ(s, "rvalue");
}

TEST(Optional, RvalueValueOr) {
    optional<std::string> o = std::string("hello");
    std::string s = std::move(o).value_or("default");
    EXPECT_EQ(s, "hello");
}
