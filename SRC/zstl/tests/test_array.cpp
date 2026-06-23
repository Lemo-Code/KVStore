// ============================================================================
// zstl array Unit Tests
// Tests array<T, N> for N=0, N=1, N=5 (and N=7 for broader coverage).
// Covers element access, iterators, capacity, modifiers, comparison operators,
// tuple-like get<I>, and zero-size array special behavior.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <stdexcept>
#include <type_traits>

using namespace zstl;

// ============================================================
// Element access — N=5
// ============================================================

TEST(ArrayTest, AtValid) {
    array<int, 5> arr = {10, 20, 30, 40, 50};
    EXPECT_EQ(arr.at(0), 10);
    EXPECT_EQ(arr.at(2), 30);
    EXPECT_EQ(arr.at(4), 50);
}

TEST(ArrayTest, AtOutOfRange) {
    array<int, 5> arr = {};
    EXPECT_THROW(arr.at(5), std::out_of_range);
    EXPECT_THROW(arr.at(100), std::out_of_range);
    const auto& carr = arr;
    EXPECT_THROW(carr.at(5), std::out_of_range);
}

TEST(ArrayTest, OperatorBracket) {
    array<int, 5> arr = {1, 2, 3, 4, 5};
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[4], 5);
    arr[2] = 99;
    EXPECT_EQ(arr[2], 99);
    const auto& carr = arr;
    EXPECT_EQ(carr[0], 1);
    EXPECT_EQ(carr[4], 5);
}

TEST(ArrayTest, FrontBack) {
    array<int, 5> arr = {10, 20, 30, 40, 50};
    EXPECT_EQ(arr.front(), 10);
    EXPECT_EQ(arr.back(), 50);
    arr.front() = 100;
    arr.back() = 500;
    EXPECT_EQ(arr.front(), 100);
    EXPECT_EQ(arr.back(), 500);
    const auto& carr = arr;
    EXPECT_EQ(carr.front(), 100);
    EXPECT_EQ(carr.back(), 500);
}

TEST(ArrayTest, Data) {
    array<int, 5> arr = {1, 2, 3, 4, 5};
    int* p = arr.data();
    EXPECT_EQ(p[0], 1);
    EXPECT_EQ(p[4], 5);
    p[0] = 99;
    EXPECT_EQ(arr[0], 99);
    const auto& carr = arr;
    const int* cp = carr.data();
    EXPECT_EQ(cp[0], 99);
    // data() returns pointer to underlying array
    EXPECT_EQ(p, &arr[0]);
}

// ============================================================
// Iterators — N=5
// ============================================================

TEST(ArrayTest, BeginEnd) {
    array<int, 5> arr = {1, 2, 3, 4, 5};
    auto it = arr.begin();
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_EQ(*it, 2);
    it += 3;
    EXPECT_EQ(*it, 5);
    ++it;
    EXPECT_EQ(it, arr.end());
    EXPECT_EQ(arr.end() - arr.begin(), 5);
}

TEST(ArrayTest, ConstBeginEnd) {
    const array<int, 5> arr = {10, 20, 30, 40, 50};
    EXPECT_EQ(*arr.begin(), 10);
    EXPECT_EQ(*arr.cbegin(), 10);
    EXPECT_EQ(*(arr.cend() - 1), 50);
}

TEST(ArrayTest, ReverseIterators) {
    array<int, 5> arr = {1, 2, 3, 4, 5};
    auto rit = arr.rbegin();
    EXPECT_EQ(*rit, 5);
    ++rit;
    EXPECT_EQ(*rit, 4);
    ++rit;
    EXPECT_EQ(*rit, 3);
    ++rit;
    EXPECT_EQ(*rit, 2);
    ++rit;
    EXPECT_EQ(*rit, 1);
    ++rit;
    EXPECT_EQ(rit, arr.rend());
}

TEST(ArrayTest, ConstReverseIterators) {
    const array<int, 5> arr = {1, 2, 3, 4, 5};
    auto rit = arr.crbegin();
    EXPECT_EQ(*rit, 5);
    ++rit;
    EXPECT_EQ(*rit, 4);
}

TEST(ArrayTest, RangeForLoop) {
    array<int, 5> arr = {10, 20, 30, 40, 50};
    int sum = 0;
    for (int v : arr) {
        sum += v;
    }
    EXPECT_EQ(sum, 150);
}

// ============================================================
// Capacity — N=5
// ============================================================

TEST(ArrayTest, Empty) {
    array<int, 5> arr;
    EXPECT_FALSE(arr.empty());
}

TEST(ArrayTest, Size) {
    array<int, 5> arr;
    EXPECT_EQ(arr.size(), 5u);
}

TEST(ArrayTest, MaxSize) {
    array<int, 5> arr;
    EXPECT_EQ(arr.max_size(), 5u);
    EXPECT_EQ(arr.size(), arr.max_size());
}

// ============================================================
// Modifiers — N=5
// ============================================================

TEST(ArrayTest, Fill) {
    array<int, 5> arr = {1, 2, 3, 4, 5};
    arr.fill(42);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(arr[i], 42);
    }
}

TEST(ArrayTest, FillPartial) {
    array<int, 5> arr = {};
    arr.fill(7);
    EXPECT_EQ(arr[0], 7);
    EXPECT_EQ(arr[4], 7);
}

TEST(ArrayTest, Swap) {
    array<int, 5> a = {1, 2, 3, 4, 5};
    array<int, 5> b = {10, 20, 30, 40, 50};
    a.swap(b);
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[4], 50);
    EXPECT_EQ(b[0], 1);
    EXPECT_EQ(b[4], 5);
}

TEST(ArrayTest, SwapNonMember) {
    array<int, 5> a = {1, 2, 3, 4, 5};
    array<int, 5> b = {6, 7, 8, 9, 10};
    swap(a, b);
    EXPECT_EQ(a[0], 6);
    EXPECT_EQ(b[0], 1);
}

TEST(ArrayTest, SwapSame) {
    array<int, 5> arr = {1, 2, 3, 4, 5};
    arr.swap(arr);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[4], 5);
}

// ============================================================
// Tuple-like access (get<I>)
// ============================================================

TEST(ArrayTest, Get) {
    array<int, 5> arr = {10, 20, 30, 40, 50};
    EXPECT_EQ(get<0>(arr), 10);
    EXPECT_EQ(get<2>(arr), 30);
    EXPECT_EQ(get<4>(arr), 50);
}

TEST(ArrayTest, GetConst) {
    const array<int, 5> arr = {10, 20, 30, 40, 50};
    EXPECT_EQ(get<0>(arr), 10);
    EXPECT_EQ(get<3>(arr), 40);
}

TEST(ArrayTest, GetRvalue) {
    array<int, 5> arr = {10, 20, 30, 40, 50};
    int&& v = get<2>(std::move(arr));
    EXPECT_EQ(v, 30);
}

TEST(ArrayTest, GetModify) {
    array<int, 5> arr = {1, 2, 3, 4, 5};
    get<0>(arr) = 99;
    get<4>(arr) = 88;
    EXPECT_EQ(arr[0], 99);
    EXPECT_EQ(arr[4], 88);
}

TEST(ArrayTest, TupleSize) {
    EXPECT_EQ((tuple_size<array<int, 5>>::value), 5u);
}

TEST(ArrayTest, TupleElement) {
    bool is_int = std::is_same_v<tuple_element<0, array<int, 5>>::type, int>;
    EXPECT_TRUE(is_int);
    bool is_int2 = std::is_same_v<tuple_element<3, array<int, 5>>::type, int>;
    EXPECT_TRUE(is_int2);
}

// ============================================================
// Comparison operators — N=5
// ============================================================

TEST(ArrayTest, OperatorEquals) {
    array<int, 5> a = {1, 2, 3, 4, 5};
    array<int, 5> b = {1, 2, 3, 4, 5};
    array<int, 5> c = {1, 2, 3, 4, 6};
    array<int, 5> d = {1, 2, 0, 4, 5};

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_FALSE(a == d);
}

TEST(ArrayTest, OperatorNotEquals) {
    array<int, 5> a = {1, 2, 3, 4, 5};
    array<int, 5> b = {1, 2, 3, 4, 6};
    EXPECT_TRUE(a != b);
    EXPECT_FALSE(a != array<int, 5>{1, 2, 3, 4, 5});
}

TEST(ArrayTest, OperatorLess) {
    array<int, 5> a = {1, 2, 3, 4, 5};
    array<int, 5> b = {1, 2, 3, 4, 6};
    array<int, 5> c = {1, 2, 3, 4, 4};
    array<int, 5> d = {1, 2, 3, 4, 5};

    EXPECT_TRUE(a < b);   // 5 < 6 at last position
    EXPECT_FALSE(b < a);
    EXPECT_TRUE(c < a);   // 4 < 5 at last position
    EXPECT_FALSE(a < c);
    EXPECT_FALSE(a < d);  // equal
    EXPECT_FALSE(d < a);  // equal
}

TEST(ArrayTest, OperatorGreater) {
    array<int, 5> a = {5, 4, 3, 2, 1};
    array<int, 5> b = {1, 2, 3, 4, 5};
    EXPECT_TRUE(a > b);
    EXPECT_FALSE(b > a);
}

TEST(ArrayTest, OperatorLessEqual) {
    array<int, 5> a = {1, 2, 3, 4, 5};
    array<int, 5> b = {1, 2, 3, 4, 5};
    array<int, 5> c = {1, 2, 3, 4, 6};
    EXPECT_TRUE(a <= b);  // equal
    EXPECT_TRUE(a <= c);  // less
    EXPECT_FALSE(c <= a); // greater
}

TEST(ArrayTest, OperatorGreaterEqual) {
    array<int, 5> a = {5, 5, 5, 5, 5};
    array<int, 5> b = {1, 2, 3, 4, 5};
    array<int, 5> c = {5, 5, 5, 5, 5};
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(a >= c);  // equal
    EXPECT_FALSE(b >= a);
}

// ============================================================
// Non-member comparison operators
// ============================================================

TEST(ArrayTest, NonMemberOperators) {
    array<int, 5> a = {1, 2, 3, 4, 5};
    array<int, 5> b = {1, 2, 3, 4, 5};

    // operator== non-member
    bool eq = operator==(a, b);
    EXPECT_TRUE(eq);

    // operator< non-member
    array<int, 5> c = {0, 2, 3, 4, 5};
    bool lt = operator<(c, a);
    EXPECT_TRUE(lt);
}

// ============================================================
// N=1
// ============================================================

TEST(ArraySize1Test, BasicAccess) {
    array<int, 1> arr = {42};
    EXPECT_EQ(arr.size(), 1u);
    EXPECT_FALSE(arr.empty());
    EXPECT_EQ(arr.max_size(), 1u);
    EXPECT_EQ(arr[0], 42);
    EXPECT_EQ(arr.at(0), 42);
    EXPECT_EQ(arr.front(), 42);
    EXPECT_EQ(arr.back(), 42);
    EXPECT_EQ(&arr.front(), &arr.back());
}

TEST(ArraySize1Test, Iterators) {
    array<int, 1> arr = {99};
    EXPECT_EQ(*arr.begin(), 99);
    EXPECT_EQ(arr.begin() + 1, arr.end());
    EXPECT_EQ(arr.end() - arr.begin(), 1);
    // rbegin/rend
    EXPECT_EQ(*arr.rbegin(), 99);
    EXPECT_EQ(arr.rbegin() + 1, arr.rend());
}

TEST(ArraySize1Test, Data) {
    array<int, 1> arr = {77};
    EXPECT_EQ(arr.data()[0], 77);
    EXPECT_EQ(arr.data(), &arr[0]);
}

TEST(ArraySize1Test, Fill) {
    array<int, 1> arr = {1};
    arr.fill(100);
    EXPECT_EQ(arr[0], 100);
}

TEST(ArraySize1Test, Swap) {
    array<int, 1> a = {10};
    array<int, 1> b = {20};
    a.swap(b);
    EXPECT_EQ(a[0], 20);
    EXPECT_EQ(b[0], 10);
}

TEST(ArraySize1Test, Get) {
    array<int, 1> arr = {123};
    EXPECT_EQ(get<0>(arr), 123);
    get<0>(arr) = 456;
    EXPECT_EQ(arr[0], 456);
}

TEST(ArraySize1Test, Comparison) {
    array<int, 1> a = {1};
    array<int, 1> b = {1};
    array<int, 1> c = {2};
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a < c);
    EXPECT_FALSE(c < a);
    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(c >= a);
}

// ============================================================
// N=0 (zero-size array specialization)
// ============================================================

TEST(ArraySize0Test, Empty) {
    array<int, 0> arr;
    EXPECT_TRUE(arr.empty());
}

TEST(ArraySize0Test, Size) {
    array<int, 0> arr;
    EXPECT_EQ(arr.size(), 0u);
    EXPECT_EQ(arr.max_size(), 0u);
}

TEST(ArraySize0Test, AtAlwaysThrows) {
    array<int, 0> arr;
    EXPECT_THROW(arr.at(0), std::out_of_range);
    EXPECT_THROW(arr.at(1), std::out_of_range);
    EXPECT_THROW(arr.at(100), std::out_of_range);
    const auto& carr = arr;
    EXPECT_THROW(carr.at(0), std::out_of_range);
}

TEST(ArraySize0Test, DataReturnsNullptr) {
    array<int, 0> arr;
    EXPECT_EQ(arr.data(), nullptr);
    const auto& carr = arr;
    EXPECT_EQ(carr.data(), nullptr);
}

TEST(ArraySize0Test, Iterators) {
    array<int, 0> arr;
    EXPECT_EQ(arr.begin(), nullptr);
    EXPECT_EQ(arr.end(), nullptr);
    EXPECT_EQ(arr.cbegin(), nullptr);
    EXPECT_EQ(arr.cend(), nullptr);
    EXPECT_EQ(arr.begin(), arr.end());
}

TEST(ArraySize0Test, ReverseIterators) {
    array<int, 0> arr;
    // begin == end == nullptr, so rbegin == rend (wrapping nullptr)
    // Just ensure they are comparable
    (void)arr.rbegin();
    (void)arr.rend();
    SUCCEED();
}

TEST(ArraySize0Test, FillNoop) {
    array<int, 0> arr;
    arr.fill(42);  // should be a no-op, no crash
    SUCCEED();
}

TEST(ArraySize0Test, SwapNoop) {
    array<int, 0> a;
    array<int, 0> b;
    a.swap(b);  // should be a no-op, no crash
    SUCCEED();
}

TEST(ArraySize0Test, NonMemberSwapNoop) {
    array<int, 0> a;
    array<int, 0> b;
    swap(a, b);
    SUCCEED();
}

TEST(ArraySize0Test, ComparisonOperators) {
    array<int, 0> a;
    array<int, 0> b;
    // All zero-size arrays compare equal
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_FALSE(a < b);
    EXPECT_FALSE(a > b);
    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(a >= b);
}

// ============================================================
// N=7 (additional general tests for broader coverage)
// ============================================================

TEST(ArraySize7Test, ElementAccess) {
    array<int, 7> arr = {10, 20, 30, 40, 50, 60, 70};
    EXPECT_EQ(arr.size(), 7u);
    EXPECT_FALSE(arr.empty());
    EXPECT_EQ(arr.at(0), 10);
    EXPECT_EQ(arr.at(6), 70);
    EXPECT_EQ(arr.front(), 10);
    EXPECT_EQ(arr.back(), 70);
    EXPECT_THROW(arr.at(7), std::out_of_range);
}

TEST(ArraySize7Test, Iterators) {
    array<int, 7> arr = {1, 2, 3, 4, 5, 6, 7};
    int count = 0;
    for (auto it = arr.begin(); it != arr.end(); ++it) {
        EXPECT_EQ(*it, count + 1);
        ++count;
    }
    EXPECT_EQ(count, 7);

    // Reverse
    count = 0;
    for (auto rit = arr.rbegin(); rit != arr.rend(); ++rit) {
        EXPECT_EQ(*rit, 7 - count);
        ++count;
    }
    EXPECT_EQ(count, 7);
}

TEST(ArraySize7Test, Fill) {
    array<int, 7> arr;
    arr.fill(77);
    for (size_t i = 0; i < 7; ++i) {
        EXPECT_EQ(arr[i], 77);
    }
}

TEST(ArraySize7Test, Swap) {
    array<int, 7> a;
    array<int, 7> b;
    for (size_t i = 0; i < 7; ++i) {
        a[i] = static_cast<int>(i);
        b[i] = static_cast<int>(i * 10);
    }
    a.swap(b);
    for (size_t i = 0; i < 7; ++i) {
        EXPECT_EQ(a[i], static_cast<int>(i * 10));
        EXPECT_EQ(b[i], static_cast<int>(i));
    }
}

TEST(ArraySize7Test, ComparisonLexicographic) {
    array<int, 7> a = {1, 2, 3, 4, 5, 6, 7};
    array<int, 7> b = {1, 2, 3, 4, 5, 6, 8};
    array<int, 7> c = {1, 2, 3, 0, 5, 6, 7};

    EXPECT_TRUE(a < b);   // differs at last element
    EXPECT_FALSE(b < a);
    EXPECT_TRUE(c < a);   // differs at index 3 (0 < 4)
    EXPECT_FALSE(a < c);
}

TEST(ArraySize7Test, GetAllIndices) {
    array<int, 7> arr = {100, 200, 300, 400, 500, 600, 700};
    EXPECT_EQ(get<0>(arr), 100);
    EXPECT_EQ(get<1>(arr), 200);
    EXPECT_EQ(get<2>(arr), 300);
    EXPECT_EQ(get<3>(arr), 400);
    EXPECT_EQ(get<4>(arr), 500);
    EXPECT_EQ(get<5>(arr), 600);
    EXPECT_EQ(get<6>(arr), 700);
}

// ============================================================
// constexpr usage (compile-time verification)
// ============================================================

TEST(ArrayTest, ConstexprLike) {
    array<int, 5> arr = {};
    arr.fill(1);
    for (size_t i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], 1);
    }
}

// ============================================================
// Aggregate initialization
// ============================================================

TEST(ArrayTest, AggregateInit) {
    array<int, 5> arr = {1, 2, 3};  // partial initialization
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
    EXPECT_EQ(arr[3], 0);  // zero-initialized
    EXPECT_EQ(arr[4], 0);  // zero-initialized
}

TEST(ArrayTest, AggregateInitWithZero) {
    array<int, 5> arr = {};  // all zeros
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(arr[i], 0);
    }
}

// ============================================================
// Different value types
// ============================================================

TEST(ArrayTest, DoubleType) {
    array<double, 3> arr = {1.1, 2.2, 3.3};
    EXPECT_DOUBLE_EQ(arr[0], 1.1);
    EXPECT_DOUBLE_EQ(arr[1], 2.2);
    EXPECT_DOUBLE_EQ(arr[2], 3.3);
    EXPECT_EQ(arr.size(), 3u);
}

TEST(ArrayTest, CharType) {
    array<char, 4> arr = {'a', 'b', 'c', 'd'};
    EXPECT_EQ(arr[0], 'a');
    EXPECT_EQ(arr[3], 'd');
    arr.fill('x');
    EXPECT_EQ(arr[0], 'x');
    EXPECT_EQ(arr[3], 'x');
}
