// test_any.cpp — Comprehensive any unit tests
// Tests default/empty state, value construction, copy/move semantics,
// assignment, any_cast (pointer, reference, rvalue), SBO vs heap storage,
// make_any, and bad_any_cast exception handling.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <string>
#include <type_traits>
#include <cstring>
#include <cstddef>

using namespace zero;

// ============================================================
// Custom test types
// ============================================================

// Small type that should fit in SBO (<= 32 bytes)
struct SmallType {
    int x = 0;
    int y = 0;
    bool operator==(const SmallType& o) const { return x == o.x && y == o.y; }
};

// Large type that should NOT fit in SBO (> 32 bytes)
struct LargeType {
    char data[128] = {};
    bool operator==(const LargeType& o) const {
        return std::memcmp(data, o.data, sizeof(data)) == 0;
    }
};

// Non-copyable but movable type
struct MoveOnlyType {
    int value = 0;
    MoveOnlyType() = default;
    explicit MoveOnlyType(int v) : value(v) {}
    MoveOnlyType(const MoveOnlyType&) = delete;
    MoveOnlyType& operator=(const MoveOnlyType&) = delete;
    MoveOnlyType(MoveOnlyType&&) noexcept = default;
    MoveOnlyType& operator=(MoveOnlyType&&) noexcept = default;
};

// Track construction/destruction
struct TrackedType {
    inline static int alive = 0;
    int id = 0;
    TrackedType() { alive++; }
    explicit TrackedType(int i) : id(i) { alive++; }
    TrackedType(const TrackedType&) { alive++; }
    TrackedType(TrackedType&&) noexcept { alive++; }
    ~TrackedType() { alive--; }
};

// ============================================================
// Default constructor
// ============================================================

TEST(Any, DefaultConstructorEmpty) {
    any a;
    EXPECT_FALSE(a.has_value());
    EXPECT_FALSE(static_cast<bool>(a));
    EXPECT_EQ(a.type(), typeid(void));
}

TEST(Any, DefaultConstructorAnyCastThrows) {
    any a;
    EXPECT_THROW(any_cast<int>(a), bad_any_cast);
}

TEST(Any, DefaultConstructorAnyCastPointerNull) {
    any a;
    EXPECT_EQ(any_cast<int>(&a), nullptr);
    EXPECT_EQ(any_cast<int>(static_cast<const any*>(&a)), nullptr);
}

// ============================================================
// Value constructor
// ============================================================

TEST(Any, IntValueConstructor) {
    any a = 42;
    EXPECT_TRUE(a.has_value());
    EXPECT_TRUE(static_cast<bool>(a));
    EXPECT_EQ(a.type(), typeid(int));
    EXPECT_EQ(any_cast<int>(a), 42);
}

TEST(Any, StringValueConstructor) {
    any a = std::string("hello world");
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(any_cast<std::string>(a), "hello world");
}

TEST(Any, DoubleValueConstructor) {
    any a = 3.14;
    EXPECT_TRUE(a.has_value());
    EXPECT_DOUBLE_EQ(any_cast<double>(a), 3.14);
}

TEST(Any, BoolValueConstructor) {
    any a = true;
    EXPECT_TRUE(a.has_value());
    EXPECT_TRUE(any_cast<bool>(a));
}

TEST(Any, SmallTypeValueConstructor) {
    SmallType st{10, 20};
    any a = st;
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(any_cast<SmallType>(a).x, 10);
    EXPECT_EQ(any_cast<SmallType>(a).y, 20);
}

TEST(Any, LargeTypeValueConstructor) {
    LargeType lt;
    lt.data[42] = 'x';
    any a = lt;
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(any_cast<LargeType>(a).data[42], 'x');
}

// ============================================================
// Copy constructor
// ============================================================

TEST(Any, CopyConstructor) {
    any a = 42;
    any b = a;
    EXPECT_EQ(any_cast<int>(a), 42);
    EXPECT_EQ(any_cast<int>(b), 42);
}

TEST(Any, CopyConstructorIndependent) {
    any a = std::string("original");
    any b = a;
    any_cast<std::string&>(b) = "modified";
    EXPECT_EQ(any_cast<std::string>(a), "original");
    EXPECT_EQ(any_cast<std::string>(b), "modified");
}

TEST(Any, CopyConstructorEmpty) {
    any a;
    any b = a;
    EXPECT_FALSE(b.has_value());
}

// ============================================================
// Move constructor
// ============================================================

TEST(Any, MoveConstructor) {
    any a = 42;
    any b = std::move(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_TRUE(b.has_value());
    EXPECT_EQ(any_cast<int>(b), 42);
}

TEST(Any, MoveConstructorLarge) {
    any a = std::string("move me");
    any b = std::move(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(any_cast<std::string>(b), "move me");
}

// ============================================================
// Copy assignment
// ============================================================

TEST(Any, CopyAssignment) {
    any a = 100;
    any b;
    b = a;
    EXPECT_EQ(any_cast<int>(a), 100);
    EXPECT_EQ(any_cast<int>(b), 100);
}

TEST(Any, CopyAssignmentOverwrite) {
    any a = 42;
    any b = 3.14;
    b = a;
    EXPECT_EQ(any_cast<int>(a), 42);
    EXPECT_EQ(any_cast<int>(b), 42);
}

// ============================================================
// Move assignment
// ============================================================

TEST(Any, MoveAssignment) {
    any a = 100;
    any b;
    b = std::move(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(any_cast<int>(b), 100);
}

TEST(Any, MoveAssignmentOverwrite) {
    any a = 42;
    any b = std::string("hello");
    b = std::move(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(any_cast<int>(b), 42);
}

// ============================================================
// Value assignment
// ============================================================

TEST(Any, ValueAssignment) {
    any a;
    a = 42;
    EXPECT_EQ(any_cast<int>(a), 42);
    a = 3.14;
    EXPECT_DOUBLE_EQ(any_cast<double>(a), 3.14);
    a = std::string("reassigned");
    EXPECT_EQ(any_cast<std::string>(a), "reassigned");
}

// ============================================================
// Self-assignment
// ============================================================

TEST(Any, SelfAssignment) {
    any a = 42;
    a = a; // Self-assignment should be a no-op
    EXPECT_EQ(any_cast<int>(a), 42);
}

TEST(Any, SelfMoveAssignment) {
    any a = 42;
    a = std::move(a); // Self-move should be safe
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(any_cast<int>(a), 42);
}

// ============================================================
// Reset
// ============================================================

TEST(Any, Reset) {
    any a = 42;
    EXPECT_TRUE(a.has_value());
    a.reset();
    EXPECT_FALSE(a.has_value());
}

TEST(Any, DoubleReset) {
    any a = 42;
    a.reset();
    a.reset(); // Double reset is safe
    EXPECT_FALSE(a.has_value());
}

TEST(Any, ResetEmpty) {
    any a;
    a.reset(); // Reset on empty is safe
    EXPECT_FALSE(a.has_value());
}

// ============================================================
// Swap
// ============================================================

TEST(Any, SwapTwoValues) {
    any a = 42;
    any b = std::string("hello");
    a.swap(b);
    EXPECT_EQ(any_cast<std::string>(a), "hello");
    EXPECT_EQ(any_cast<int>(b), 42);
}

TEST(Any, SwapWithEmpty) {
    any a = 42;
    any b;
    a.swap(b);
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(any_cast<int>(b), 42);
}

TEST(Any, SwapBothEmpty) {
    any a;
    any b;
    a.swap(b);
    EXPECT_FALSE(a.has_value());
    EXPECT_FALSE(b.has_value());
}

TEST(Any, SwapBack) {
    any a = 42;
    any b = std::string("world");
    a.swap(b);
    b.swap(a);
    EXPECT_EQ(any_cast<int>(a), 42);
    EXPECT_EQ(any_cast<std::string>(b), "world");
}

// ============================================================
// any_cast — pointer form
// ============================================================

TEST(Any, AnyCastPointerSuccess) {
    any a = 42;
    int* p = any_cast<int>(&a);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(*p, 42);
}

TEST(Any, AnyCastPointerTypeMismatch) {
    any a = 42;
    EXPECT_EQ(any_cast<double>(&a), nullptr);
    EXPECT_EQ(any_cast<std::string>(&a), nullptr);
}

TEST(Any, AnyCastPointerEmpty) {
    any a;
    EXPECT_EQ(any_cast<int>(&a), nullptr);
}

TEST(Any, AnyCastPointerNullAny) {
    EXPECT_EQ(any_cast<int>(static_cast<any*>(nullptr)), nullptr);
}

TEST(Any, AnyCastConstPointer) {
    const any a = 42;
    const int* p = any_cast<int>(&a);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(*p, 42);
}

// ============================================================
// any_cast — reference form
// ============================================================

TEST(Any, AnyCastReference) {
    any a = 42;
    EXPECT_EQ(any_cast<int>(a), 42);
}

TEST(Any, AnyCastRefTypeMismatchThrows) {
    any a = 42;
    EXPECT_THROW(any_cast<double>(a), bad_any_cast);
}

TEST(Any, AnyCastConstReference) {
    const any a = 42;
    EXPECT_EQ(any_cast<int>(a), 42);
    // const any returns const T (for value types)
}

// ============================================================
// any_cast — rvalue form
// ============================================================

TEST(Any, AnyCastRvalue) {
    any a = std::string("move me via rvalue");
    std::string s = any_cast<std::string>(std::move(a));
    EXPECT_EQ(s, "move me via rvalue");
}

TEST(Any, AnyCastRvalueInt) {
    any a = 42;
    int v = any_cast<int>(std::move(a));
    EXPECT_EQ(v, 42);
}

// ============================================================
// any_cast — mutable reference modification
// ============================================================

TEST(Any, AnyCastMutableRefModify) {
    any a = 42;
    any_cast<int&>(a) = 100;
    EXPECT_EQ(any_cast<int>(a), 100);
}

// ============================================================
// make_any
// ============================================================

TEST(Any, MakeAnyInt) {
    auto a = make_any<int>(42);
    EXPECT_EQ(any_cast<int>(a), 42);
}

TEST(Any, MakeAnyString) {
    auto a = make_any<std::string>("hello");
    EXPECT_EQ(any_cast<std::string>(a), "hello");
}

TEST(Any, MakeAnyLarge) {
    LargeType lt;
    lt.data[0] = 'a';
    auto a = make_any<LargeType>(lt);
    EXPECT_EQ(any_cast<LargeType>(a).data[0], 'a');
}

// ============================================================
// In-place construction
// ============================================================

TEST(Any, InPlaceConstruction) {
    any a(std::in_place_type<std::string>, "in place");
    EXPECT_EQ(any_cast<std::string>(a), "in place");
}

// ============================================================
// SBO (small buffer optimization) — implicit test
// ============================================================

TEST(Any, SmallObjectFitsInInlineStorage) {
    // int (4 bytes) should fit in SBO
    any a = 42;
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(any_cast<int>(a), 42);
}

TEST(Any, StringMayExceedSBO) {
    // Long string may exceed SBO
    std::string long_str(200, 'x');
    any a = long_str;
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(any_cast<std::string>(a).size(), 200u);
}

TEST(Any, ManySmallValuesReuse) {
    for (int i = 0; i < 100; ++i) {
        any a = i;
        EXPECT_EQ(any_cast<int>(a), i);
    }
}

// ============================================================
// bad_any_cast exception
// ============================================================

TEST(Any, BadAnyCastException) {
    any a = 42;
    try {
        any_cast<double>(a);
        FAIL() << "Expected bad_any_cast";
    } catch (const bad_any_cast& e) {
        EXPECT_NE(e.what(), nullptr);
        std::string msg(e.what());
        EXPECT_NE(msg.find("bad_any_cast"), std::string::npos);
    }
}

TEST(Any, BadAnyCastFromEmpty) {
    any a;
    EXPECT_THROW(any_cast<int>(a), bad_any_cast);
}

// ============================================================
// Tracked destruction
// ============================================================

TEST(Any, TrackedDestruction) {
    TrackedType::alive = 0;
    {
        any a = TrackedType(42);
        EXPECT_EQ(TrackedType::alive, 1);
        a.reset();
        EXPECT_EQ(TrackedType::alive, 0);
    }
    EXPECT_EQ(TrackedType::alive, 0);
}

TEST(Any, TrackedDestructionOnReassign) {
    TrackedType::alive = 0;
    {
        any a = TrackedType(1);
        EXPECT_EQ(TrackedType::alive, 1);
        a = TrackedType(2); // Old destroyed, new created
        EXPECT_LE(TrackedType::alive, 2);
        a.reset();
        EXPECT_EQ(TrackedType::alive, 0);
    }
}

// ============================================================
// Multiple type reassignment stress
// ============================================================

TEST(Any, MultipleTypeReassignment) {
    any a;
    a = 42;
    EXPECT_EQ(any_cast<int>(a), 42);
    a = 3.14;
    EXPECT_DOUBLE_EQ(any_cast<double>(a), 3.14);
    a = std::string("test");
    EXPECT_EQ(any_cast<std::string>(a), "test");
    a = true;
    EXPECT_TRUE(any_cast<bool>(a));
    a = static_cast<uint64_t>(UINT64_MAX);
    EXPECT_EQ(any_cast<uint64_t>(a), UINT64_MAX);
    a = SmallType{5, 10};
    EXPECT_EQ(any_cast<SmallType>(a), SmallType{5, 10});
}

// ============================================================
// Exception safety
// ============================================================

TEST(Any, ExceptionSafetyOnBadCast) {
    any a = 42;
    try {
        (void)any_cast<double>(a);
    } catch (const bad_any_cast&) {
        // any should still be valid after failed cast
        EXPECT_TRUE(a.has_value());
        EXPECT_EQ(any_cast<int>(a), 42);
    }
}

TEST(Any, ExceptionSafetyResetAfterException) {
    any a = 42;
    try {
        (void)any_cast<double>(a);
    } catch (const bad_any_cast&) {
        a.reset();
        EXPECT_FALSE(a.has_value());
        a.reset(); // Double reset after exception
        EXPECT_FALSE(a.has_value());
    }
}
