// test_expected.cpp — Comprehensive expected<T, E> unit tests
// Tests value/unexpected construction, copy/move, observers,
// monadic operations (and_then, or_else, transform, transform_error),
// equality, and swap for expected<int, std::string>.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <string>
#include <type_traits>
#include <stdexcept>

using namespace zero;

// ============================================================
// Value constructor
// ============================================================

TEST(Expected, ValueConstructor) {
    expected<int, std::string> e = 42;
    EXPECT_TRUE(e.has_value());
    EXPECT_TRUE(static_cast<bool>(e));
    EXPECT_EQ(e.value(), 42);
    EXPECT_EQ(*e, 42);
}

TEST(Expected, ValueMoveConstructor) {
    expected<std::string, std::string> e = std::string("hello");
    EXPECT_TRUE(e.has_value());
    EXPECT_EQ(e.value(), "hello");
}

// ============================================================
// Unexpected constructor
// ============================================================

TEST(Expected, UnexpectedConstructor) {
    expected<int, std::string> e = unexpected<std::string>("error msg");
    EXPECT_FALSE(e.has_value());
    EXPECT_FALSE(static_cast<bool>(e));
    EXPECT_EQ(e.error(), "error msg");
}

TEST(Expected, UnexpectedMoveConstructor) {
    expected<int, std::string> e = unexpected<std::string>(std::string("move error"));
    EXPECT_FALSE(e.has_value());
    EXPECT_EQ(e.error(), "move error");
}

TEST(Expected, UnexpectedRvalueConstructor) {
    unexpected<std::string> u("test error");
    expected<int, std::string> e = std::move(u);
    EXPECT_FALSE(e.has_value());
    EXPECT_EQ(e.error(), "test error");
}

// ============================================================
// In-place construction
// ============================================================

TEST(Expected, InPlaceValue) {
    expected<std::string, int> e(std::in_place, "hello world");
    EXPECT_TRUE(e.has_value());
    EXPECT_EQ(e.value(), "hello world");
}

// ============================================================
// Copy constructor
// ============================================================

TEST(Expected, CopyValueExpected) {
    expected<int, std::string> a = 42;
    expected<int, std::string> b = a;
    EXPECT_EQ(b.value(), 42);
}

TEST(Expected, CopyErrorExpected) {
    expected<int, std::string> a = unexpected<std::string>("err");
    expected<int, std::string> b = a;
    EXPECT_FALSE(b.has_value());
    EXPECT_EQ(b.error(), "err");
}

// ============================================================
// Move constructor
// ============================================================

TEST(Expected, MoveValueExpected) {
    expected<std::string, std::string> a = std::string("move it");
    expected<std::string, std::string> b = std::move(a);
    EXPECT_EQ(b.value(), "move it");
}

TEST(Expected, MoveErrorExpected) {
    expected<int, std::string> a = unexpected<std::string>("move error");
    expected<int, std::string> b = std::move(a);
    EXPECT_EQ(b.error(), "move error");
}

// ============================================================
// Copy assignment
// ============================================================

TEST(Expected, CopyAssignValueToValue) {
    expected<int, std::string> a = 1;
    expected<int, std::string> b = 2;
    a = b;
    EXPECT_EQ(a.value(), 2);
}

TEST(Expected, CopyAssignErrorToError) {
    expected<int, std::string> a = unexpected<std::string>("a");
    expected<int, std::string> b = unexpected<std::string>("b");
    a = b;
    EXPECT_EQ(a.error(), "b");
}

TEST(Expected, CopyAssignErrorToValue) {
    expected<int, std::string> a = unexpected<std::string>("was error");
    expected<int, std::string> b = 42;
    a = b;
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(a.value(), 42);
}

TEST(Expected, CopyAssignValueToError) {
    expected<int, std::string> a = 42;
    expected<int, std::string> b = unexpected<std::string>("now error");
    a = b;
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(a.error(), "now error");
}

// ============================================================
// Move assignment
// ============================================================

TEST(Expected, MoveAssignValue) {
    expected<std::string, std::string> a = std::string("old");
    expected<std::string, std::string> b = std::string("new");
    a = std::move(b);
    EXPECT_EQ(a.value(), "new");
}

// ============================================================
// Value assignment
// ============================================================

TEST(Expected, AssignValue) {
    expected<int, std::string> e = 1;
    e = 42;
    EXPECT_EQ(e.value(), 42);
}

TEST(Expected, AssignValueOverError) {
    expected<int, std::string> e = unexpected<std::string>("was error");
    e = 100;
    EXPECT_TRUE(e.has_value());
    EXPECT_EQ(e.value(), 100);
}

// ============================================================
// Unexpected assignment
// ============================================================

TEST(Expected, AssignUnexpected) {
    expected<int, std::string> e = 42;
    e = unexpected<std::string>("now error");
    EXPECT_FALSE(e.has_value());
    EXPECT_EQ(e.error(), "now error");
}

TEST(Expected, AssignUnexpectedOverError) {
    expected<int, std::string> e = unexpected<std::string>("old");
    e = unexpected<std::string>("new");
    EXPECT_EQ(e.error(), "new");
}

// ============================================================
// value() — throws on error
// ============================================================

TEST(Expected, ValueThrowsOnError) {
    expected<int, std::string> e = unexpected<std::string>("failure");
    try {
        e.value();
        FAIL() << "Expected bad_expected_access";
    } catch (const bad_expected_access<std::string>& ex) {
        EXPECT_EQ(ex.error(), "failure");
    }
}

TEST(Expected, ValueReturnsRef) {
    expected<int, std::string> e = 42;
    e.value() = 100;
    EXPECT_EQ(e.value(), 100);
}

// ============================================================
// value_or
// ============================================================

TEST(Expected, ValueOrWithValue) {
    expected<int, std::string> e = 42;
    EXPECT_EQ(e.value_or(0), 42);
}

TEST(Expected, ValueOrWithError) {
    expected<int, std::string> e = unexpected<std::string>("error");
    EXPECT_EQ(e.value_or(42), 42);
}

// ============================================================
// operator* / operator->
// ============================================================

TEST(Expected, Dereference) {
    expected<int, std::string> e = 42;
    EXPECT_EQ(*e, 42);
    *e = 100;
    EXPECT_EQ(*e, 100);
}

TEST(Expected, ArrowOperator) {
    expected<std::string, std::string> e = std::string("hello");
    EXPECT_EQ(e->size(), 5u);
}

// ============================================================
// error()
// ============================================================

TEST(Expected, Error) {
    expected<int, std::string> e = unexpected<std::string>("my error");
    EXPECT_EQ(e.error(), "my error");
}

TEST(Expected, ErrorMutable) {
    expected<int, std::string> e = unexpected<std::string>("mutable");
    e.error() = "changed";
    EXPECT_EQ(e.error(), "changed");
}

// ============================================================
// and_then (monadic bind)
// ============================================================

TEST(Expected, AndThenSuccess) {
    expected<int, std::string> e = 21;
    auto e2 = e.and_then([](int x) -> expected<int, std::string> {
        return x * 2;
    });
    EXPECT_TRUE(e2.has_value());
    EXPECT_EQ(e2.value(), 42);
}

TEST(Expected, AndThenChain) {
    expected<int, std::string> e = 1;
    auto result = e
        .and_then([](int x) -> expected<int, std::string> {
            return x + 1;
        })
        .and_then([](int x) -> expected<int, std::string> {
            return x * 10;
        });
    EXPECT_EQ(result.value(), 20);
}

TEST(Expected, AndThenErrorShortCircuit) {
    expected<int, std::string> e = unexpected<std::string>("failure");
    bool called = false;
    auto e2 = e.and_then([&](int x) -> expected<int, std::string> {
        called = true;
        return x * 2;
    });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), "failure");
    EXPECT_FALSE(called); // Lambda should not be called for error
}

TEST(Expected, AndThenChangeType) {
    expected<int, std::string> e = 42;
    auto e2 = e.and_then([](int x) -> expected<std::string, std::string> {
        return std::to_string(x);
    });
    EXPECT_TRUE(e2.has_value());
    EXPECT_EQ(e2.value(), "42");
}

// ============================================================
// or_else (monadic error handler)
// ============================================================

TEST(Expected, OrElseRecover) {
    expected<int, std::string> e = unexpected<std::string>("oops");
    auto e2 = e.or_else([](const std::string& err) -> expected<int, std::string> {
        if (err == "oops") return 0;
        return unexpected<std::string>(err);
    });
    EXPECT_TRUE(e2.has_value());
    EXPECT_EQ(e2.value(), 0);
}

TEST(Expected, OrElsePassThrough) {
    expected<int, std::string> e = 42;
    bool called = false;
    auto e2 = e.or_else([&](const std::string&) -> expected<int, std::string> {
        called = true;
        return -1;
    });
    EXPECT_EQ(e2.value(), 42);
    EXPECT_FALSE(called); // Lambda should not be called for value
}

TEST(Expected, OrElsePropagateError) {
    expected<int, std::string> e = unexpected<std::string>("unknown");
    auto e2 = e.or_else([](const std::string& err) -> expected<int, std::string> {
        return unexpected<std::string>("wrapped: " + err);
    });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), "wrapped: unknown");
}

// ============================================================
// transform (map)
// ============================================================

TEST(Expected, TransformSuccess) {
    expected<int, std::string> e = 21;
    auto e2 = e.transform([](int x) { return x * 2; });
    static_assert(std::is_same_v<decltype(e2), expected<int, std::string>>);
    EXPECT_EQ(e2.value(), 42);
}

TEST(Expected, TransformErrorPropagate) {
    expected<int, std::string> e = unexpected<std::string>("bad");
    auto e2 = e.transform([](int x) { return x + 1; });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), "bad");
}

TEST(Expected, TransformTypeChange) {
    expected<int, std::string> e = 42;
    auto e2 = e.transform([](int x) { return std::to_string(x); });
    static_assert(std::is_same_v<decltype(e2), expected<std::string, std::string>>);
    EXPECT_EQ(e2.value(), "42");
}

// ============================================================
// transform_error
// ============================================================

TEST(Expected, TransformError) {
    expected<int, std::string> e = unexpected<std::string>("old error");
    auto e2 = e.transform_error([](const std::string& err) -> std::string {
        return "new: " + err;
    });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), "new: old error");
}

TEST(Expected, TransformErrorPassThrough) {
    expected<int, std::string> e = 42;
    auto e2 = e.transform_error([](const std::string&) -> int {
        return 0;
    });
    EXPECT_EQ(e2.value(), 42);
}

// ============================================================
// Monadic chain test
// ============================================================

TEST(Expected, MonadicChain) {
    // Simulate a pipeline: parse -> validate -> transform
    auto parse = [](const std::string& s) -> expected<int, std::string> {
        if (s.empty()) return unexpected<std::string>("empty input");
        return 42; // Simulated parse
    };

    auto validate = [](int x) -> expected<int, std::string> {
        if (x < 0) return unexpected<std::string>("negative");
        return x;
    };

    auto transform_val = [](int x) -> expected<int, std::string> {
        return x * 2;
    };

    auto result = parse("input")
        .and_then(validate)
        .and_then(transform_val);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 84);
}

TEST(Expected, MonadicChainErrorPropagation) {
    auto parse = [](const std::string& s) -> expected<int, std::string> {
        return unexpected<std::string>("parse error");
    };

    auto validate = [](int x) -> expected<int, std::string> {
        return x;
    };

    auto result = parse("bad")
        .and_then(validate);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "parse error");
}

// ============================================================
// Swap
// ============================================================

TEST(Expected, SwapBothValue) {
    expected<int, std::string> a = 1;
    expected<int, std::string> b = 2;
    std::swap(a, b);
    EXPECT_EQ(a.value(), 2);
    EXPECT_EQ(b.value(), 1);
}

TEST(Expected, SwapValueAndError) {
    expected<int, std::string> a = 42;
    expected<int, std::string> b = unexpected<std::string>("err");
    std::swap(a, b);
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(a.error(), "err");
    EXPECT_TRUE(b.has_value());
    EXPECT_EQ(b.value(), 42);
}

TEST(Expected, SwapBothError) {
    expected<int, std::string> a = unexpected<std::string>("x");
    expected<int, std::string> b = unexpected<std::string>("y");
    std::swap(a, b);
    EXPECT_EQ(a.error(), "y");
    EXPECT_EQ(b.error(), "x");
}

// ============================================================
// Equality
// ============================================================

TEST(Expected, EqualityBothValue) {
    expected<int, std::string> a = 42, b = 42, c = 99;
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(Expected, EqualityBothError) {
    expected<int, std::string> a = unexpected<std::string>("err");
    expected<int, std::string> b = unexpected<std::string>("err");
    expected<int, std::string> c = unexpected<std::string>("other");
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(Expected, EqualityMixed) {
    expected<int, std::string> a = 42;
    expected<int, std::string> b = unexpected<std::string>("err");
    EXPECT_FALSE(a == b);
}

TEST(Expected, EqualityWithValue) {
    expected<int, std::string> e = 42;
    EXPECT_TRUE(e == 42);
    EXPECT_TRUE(42 == e);
    EXPECT_FALSE(e == 99);
    EXPECT_FALSE(99 == e);
    EXPECT_FALSE(e != 42);
    EXPECT_TRUE(e != 99);
}

TEST(Expected, EqualityWithUnexpected) {
    expected<int, std::string> e = unexpected<std::string>("err");
    EXPECT_TRUE(e == unexpected<std::string>("err"));
    EXPECT_TRUE(unexpected<std::string>("err") == e);
    EXPECT_FALSE(e == unexpected<std::string>("other"));
    EXPECT_FALSE(unexpected<std::string>("other") == e);
    EXPECT_FALSE(e != unexpected<std::string>("err"));
    EXPECT_TRUE(e != unexpected<std::string>("other"));
}

// ============================================================
// Unexpected constructors
// ============================================================

TEST(Expected, UnexpectedCopyConstructor) {
    unexpected<std::string> u1("hello");
    unexpected<std::string> u2 = u1;
    EXPECT_EQ(u2.error(), "hello");
}

TEST(Expected, UnexpectedMoveConstructor) {
    unexpected<std::string> u1("move");
    unexpected<std::string> u2 = std::move(u1);
    EXPECT_EQ(u2.error(), "move");
}

TEST(Expected, UnexpectedInPlace) {
    unexpected<std::string> u(std::in_place, "constructed");
    EXPECT_EQ(u.error(), "constructed");
}

// ============================================================
// bad_expected_access exception
// ============================================================

TEST(Expected, BadExpectedAccessError) {
    expected<int, std::string> e = unexpected<std::string>("my fault");
    try {
        e.value();
        FAIL() << "Expected bad_expected_access";
    } catch (const bad_expected_access<std::string>& ex) {
        EXPECT_EQ(ex.error(), "my fault");
        // Verify what() message
        std::string msg(ex.what());
        EXPECT_NE(msg.find("bad_expected_access"), std::string::npos);
    }
}

// ============================================================
// Self-assignment
// ============================================================

TEST(Expected, SelfAssignValue) {
    expected<int, std::string> e = 42;
    e = e;
    EXPECT_EQ(e.value(), 42);
}

TEST(Expected, SelfAssignError) {
    expected<int, std::string> e = unexpected<std::string>("err");
    e = e;
    EXPECT_EQ(e.error(), "err");
}
