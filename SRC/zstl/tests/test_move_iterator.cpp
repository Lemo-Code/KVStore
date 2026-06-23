// ============================================================================
// zstl Move Iterator Unit Tests
// Tests: move_iterator, make_move_iterator, all operators (++/--/+/+=/-/-=),
// comparison operators (==, !=, <, >, <=, >=), base(), operator*, operator->,
// operator[], difference operator.
// Verifies move semantics (source elements moved-from).
// Test with vector of move-only types.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <vector>
#include <list>
#include <string>
#include <memory>

// ============================================================
// Move-only type for testing
// ============================================================

struct MoveOnlyInt {
    int value;
    bool moved_from = false;

    MoveOnlyInt() : value(0) {}
    explicit MoveOnlyInt(int v) : value(v) {}
    MoveOnlyInt(const MoveOnlyInt&) = delete;
    MoveOnlyInt& operator=(const MoveOnlyInt&) = delete;
    MoveOnlyInt(MoveOnlyInt&& o) noexcept : value(o.value) {
        o.value = -1;
        o.moved_from = true;
    }
    MoveOnlyInt& operator=(MoveOnlyInt&& o) noexcept {
        if (this != &o) {
            value = o.value;
            o.value = -1;
            o.moved_from = true;
        }
        return *this;
    }
    bool operator==(const MoveOnlyInt& o) const { return value == o.value; }
};

// ============================================================
// Construction and base()
// ============================================================

TEST(MoveIteratorTest, DefaultConstructor) {
    zstl::move_iterator<int*> mit;
    // Default constructed is valid
    SUCCEED();
}

TEST(MoveIteratorTest, ConstructFromIterator) {
    int arr[] = {1, 2, 3};
    zstl::move_iterator<int*> mit(arr);
    EXPECT_EQ(mit.base(), arr);
}

TEST(MoveIteratorTest, MakeMoveIterator) {
    int arr[] = {1, 2, 3};
    auto mit = zstl::make_move_iterator(arr);
    EXPECT_EQ(mit.base(), arr);
}

TEST(MoveIteratorTest, CopyConstruct) {
    int arr[] = {10, 20};
    auto mit1 = zstl::make_move_iterator(arr);
    auto mit2(mit1);
    EXPECT_EQ(mit1, mit2);
}

TEST(MoveIteratorTest, ConvertingConstructor) {
    int arr[] = {1, 2};
    zstl::move_iterator<int*> mit(arr);
    zstl::move_iterator<const int*> cmit(mit);
    EXPECT_EQ(cmit.base(), arr);
}

// ============================================================
// Dereference (returns rvalue)
// ============================================================

TEST(MoveIteratorTest, Dereference) {
    int arr[] = {42, 100};
    auto mit = zstl::make_move_iterator(arr);

    auto&& ref = *mit;
    // ref should be an rvalue reference to int
    EXPECT_TRUE((std::is_same_v<decltype(ref), int&&>));
    EXPECT_EQ(ref, 42);
}

TEST(MoveIteratorTest, ArrowOperator) {
    std::vector<std::string> v{"hello", "world"};
    auto mit = zstl::make_move_iterator(v.begin());
    // operator-> returns the underlying iterator for chaining
    EXPECT_EQ(mit->size(), 5u);
}

TEST(MoveIteratorTest, MoveSemantics) {
    std::vector<std::string> v{"aaa", "bbb", "ccc"};
    auto mit = zstl::make_move_iterator(v.begin());

    std::string moved = *mit;
    EXPECT_EQ(moved, "aaa");
    // The source element may or may not be moved-from
    // (move_iterator doesn't modify the source; it just casts to rvalue ref)
    // The actual move happens on the consumer side
}

TEST(MoveIteratorTest, MoveSemanticsWithMoveOnly) {
    MoveOnlyInt arr[3];
    arr[0] = MoveOnlyInt(10);
    arr[1] = MoveOnlyInt(20);

    auto mit = zstl::make_move_iterator(arr);
    MoveOnlyInt moved = *mit; // move-constructs
    EXPECT_EQ(moved.value, 10);
    EXPECT_EQ(arr[0].value, -1);
    EXPECT_TRUE(arr[0].moved_from);
}

// ============================================================
// Increment / Decrement
// ============================================================

TEST(MoveIteratorTest, PreIncrement) {
    int arr[] = {1, 2, 3};
    auto mit = zstl::make_move_iterator(arr);
    ++mit;
    EXPECT_EQ(mit.base(), arr + 1);
}

TEST(MoveIteratorTest, PostIncrement) {
    int arr[] = {1, 2, 3};
    auto mit = zstl::make_move_iterator(arr);
    auto old = mit++;
    EXPECT_EQ(old.base(), arr);
    EXPECT_EQ(mit.base(), arr + 1);
}

TEST(MoveIteratorTest, PreDecrement) {
    int arr[] = {1, 2, 3};
    auto mit = zstl::make_move_iterator(arr + 2);
    --mit;
    EXPECT_EQ(mit.base(), arr + 1);
}

TEST(MoveIteratorTest, PostDecrement) {
    int arr[] = {1, 2, 3};
    auto mit = zstl::make_move_iterator(arr + 2);
    auto old = mit--;
    EXPECT_EQ(old.base(), arr + 2);
    EXPECT_EQ(mit.base(), arr + 1);
}

// ============================================================
// Arithmetic (random access)
// ============================================================

TEST(MoveIteratorTest, Addition) {
    int arr[] = {1, 2, 3, 4, 5};
    auto mit = zstl::make_move_iterator(arr);
    auto mit2 = mit + 3;
    EXPECT_EQ(mit2.base(), arr + 3);
    EXPECT_EQ(*mit2, 4);

    auto mit3 = 2 + mit;
    EXPECT_EQ(mit3.base(), arr + 2);
}

TEST(MoveIteratorTest, Subtraction) {
    int arr[] = {1, 2, 3, 4, 5};
    auto mit = zstl::make_move_iterator(arr + 4);
    auto mit2 = mit - 2;
    EXPECT_EQ(mit2.base(), arr + 2);
}

TEST(MoveIteratorTest, PlusEquals) {
    int arr[] = {1, 2, 3, 4, 5};
    auto mit = zstl::make_move_iterator(arr);
    mit += 3;
    EXPECT_EQ(mit.base(), arr + 3);
}

TEST(MoveIteratorTest, MinusEquals) {
    int arr[] = {1, 2, 3, 4, 5};
    auto mit = zstl::make_move_iterator(arr + 3);
    mit -= 2;
    EXPECT_EQ(mit.base(), arr + 1);
}

TEST(MoveIteratorTest, Subscript) {
    int arr[] = {10, 20, 30, 40, 50};
    auto mit = zstl::make_move_iterator(arr);
    EXPECT_EQ(mit[0], 10);
    EXPECT_EQ(mit[2], 30);
    EXPECT_EQ(mit[4], 50);
}

// ============================================================
// Comparison
// ============================================================

TEST(MoveIteratorTest, Equality) {
    int arr[] = {1, 2, 3};
    auto mit1 = zstl::make_move_iterator(arr);
    auto mit2 = zstl::make_move_iterator(arr);
    EXPECT_TRUE(mit1 == mit2);
    EXPECT_FALSE(mit1 != mit2);

    ++mit1;
    EXPECT_FALSE(mit1 == mit2);
    EXPECT_TRUE(mit1 != mit2);
}

TEST(MoveIteratorTest, Relational) {
    int arr[] = {1, 2, 3, 4, 5};
    auto mit1 = zstl::make_move_iterator(arr);
    auto mit2 = zstl::make_move_iterator(arr + 3);

    EXPECT_TRUE(mit1 < mit2);
    EXPECT_FALSE(mit2 < mit1);
    EXPECT_TRUE(mit1 <= mit2);
    EXPECT_FALSE(mit2 <= mit1);
    EXPECT_TRUE(mit2 > mit1);
}

TEST(MoveIteratorTest, DifferenceOperator) {
    int arr[] = {1, 2, 3, 4, 5};
    auto mit1 = zstl::make_move_iterator(arr);
    auto mit2 = zstl::make_move_iterator(arr + 3);

    EXPECT_EQ(mit2 - mit1, 3);
}

// ============================================================
// Use case: move elements from vector
// ============================================================

TEST(MoveIteratorTest, MoveFromVector) {
    std::vector<std::string> src{"a", "bb", "ccc"};
    auto first = zstl::make_move_iterator(src.begin());
    auto last = zstl::make_move_iterator(src.end());

    std::vector<std::string> dst(first, last);

    EXPECT_EQ(dst.size(), 3u);
    EXPECT_EQ(dst[0], "a");
    EXPECT_EQ(dst[1], "bb");
    EXPECT_EQ(dst[2], "ccc");

    // Source strings may be moved-from (valid but unspecified state)
    // We can verify they are still valid objects
    EXPECT_EQ(src.size(), 3u);
}
