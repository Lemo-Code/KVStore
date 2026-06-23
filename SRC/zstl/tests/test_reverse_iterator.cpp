// ============================================================================
// zstl Reverse Iterator Unit Tests
// Tests: reverse_iterator from vector/list, all operators (++/--/+/+=/-/-=),
// comparison operators (==, !=, <, >, <=, >=), base(), make_reverse_iterator,
// operator[], operator->, difference operator.
// Test with bidirectional and random_access iterators.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <vector>
#include <list>
#include <string>
#include <set>

// ============================================================
// Construction and base()
// ============================================================

TEST(ReverseIteratorTest, DefaultConstructor) {
    zstl::reverse_iterator<int*> rit;
    // Default constructed is valid (though may not point to anything useful)
    SUCCEED();
}

TEST(ReverseIteratorTest, ConstructFromIterator) {
    std::vector<int> v{1, 2, 3, 4, 5};
    auto rit = zstl::reverse_iterator<std::vector<int>::iterator>(v.end());
    // base() should return end()
    EXPECT_EQ(rit.base(), v.end());
}

TEST(ReverseIteratorTest, BaseAccessor) {
    std::vector<int> v{10, 20, 30};
    auto rit = zstl::reverse_iterator<std::vector<int>::iterator>(v.end());
    EXPECT_EQ(rit.base(), v.end());
}

TEST(ReverseIteratorTest, MakeReverseIterator) {
    std::vector<int> v{1, 2, 3};
    auto rit = zstl::make_reverse_iterator(v.end());
    EXPECT_EQ(rit.base(), v.end());
}

TEST(ReverseIteratorTest, CopyConstruct) {
    std::vector<int> v{1, 2, 3};
    auto rit1 = zstl::make_reverse_iterator(v.end());
    auto rit2(rit1);
    EXPECT_EQ(rit1, rit2);
}

TEST(ReverseIteratorTest, ConvertingConstructor) {
    std::vector<int> v{1, 2, 3};
    // reverse_iterator<iterator> to reverse_iterator<const_iterator>
    zstl::reverse_iterator<std::vector<int>::iterator> rit(v.end());
    zstl::reverse_iterator<std::vector<int>::const_iterator> crit(rit);
    EXPECT_EQ(crit.base(), v.end());
}

// ============================================================
// Dereference and arrow
// ============================================================

TEST(ReverseIteratorTest, Dereference) {
    std::vector<int> v{10, 20, 30, 40, 50};
    // rbegin = reverse_iterator(end) — dereferences to last element
    auto rbegin = zstl::make_reverse_iterator(v.end());
    EXPECT_EQ(*rbegin, 50);

    // rend = reverse_iterator(begin) — one past the first element (reverse direction)
    auto rend = zstl::make_reverse_iterator(v.begin());

    // Iterate through all reversed elements
    std::vector<int> reversed;
    for (auto it = rbegin; it != rend; ++it) {
        reversed.push_back(*it);
    }
    EXPECT_EQ(reversed, (std::vector<int>{50, 40, 30, 20, 10}));
}

TEST(ReverseIteratorTest, ArrowOperator) {
    struct Pair { int a; int b; };
    std::vector<Pair> v{{1, 2}, {3, 4}, {5, 6}};
    auto rit = zstl::make_reverse_iterator(v.end());
    EXPECT_EQ(rit->a, 5);
    EXPECT_EQ(rit->b, 6);
}

TEST(ReverseIteratorTest, DereferenceEmpty) {
    // reverse_iterator(end) when end==begin would dereference *(--end)
    // which is undefined, so don't do that. Just test that construction is fine.
    std::vector<int> v;
    auto rit = zstl::make_reverse_iterator(v.end());
    // Don't dereference — it would be UB
    SUCCEED();
}

// ============================================================
// Increment / Decrement
// ============================================================

TEST(ReverseIteratorTest, PreIncrement) {
    std::vector<int> v{1, 2, 3};
    auto rit = zstl::make_reverse_iterator(v.end());
    EXPECT_EQ(*rit, 3);

    ++rit;
    EXPECT_EQ(*rit, 2);

    ++rit;
    EXPECT_EQ(*rit, 1);

    ++rit;
    EXPECT_EQ(rit, zstl::make_reverse_iterator(v.begin()));
}

TEST(ReverseIteratorTest, PostIncrement) {
    std::vector<int> v{1, 2, 3};
    auto rit = zstl::make_reverse_iterator(v.end());
    auto old = rit++;
    EXPECT_EQ(*old, 3);
    EXPECT_EQ(*rit, 2);
}

TEST(ReverseIteratorTest, PreDecrement) {
    std::vector<int> v{1, 2, 3};
    auto rit = zstl::make_reverse_iterator(v.begin()); // rend
    --rit;
    EXPECT_EQ(*rit, 1); // first element in forward direction
}

TEST(ReverseIteratorTest, PostDecrement) {
    std::vector<int> v{1, 2, 3};
    auto rit = zstl::make_reverse_iterator(v.begin());
    auto old = rit--;
    // old points to rend, new points to first element
    EXPECT_EQ(*rit, 1);
}

// ============================================================
// Arithmetic (random-access only)
// ============================================================

TEST(ReverseIteratorTest, Addition) {
    std::vector<int> v{10, 20, 30, 40, 50};
    auto rit = zstl::make_reverse_iterator(v.end()); // points to 50
    auto rit2 = rit + 2;
    EXPECT_EQ(*rit2, 30);

    auto rit3 = 2 + rit;
    EXPECT_EQ(*rit3, 30);
}

TEST(ReverseIteratorTest, Subtraction) {
    std::vector<int> v{10, 20, 30, 40, 50};
    auto rit = zstl::make_reverse_iterator(v.begin()); // rend
    auto rit2 = rit - 2;
    EXPECT_EQ(*rit2, 20);
}

TEST(ReverseIteratorTest, PlusEquals) {
    std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8};
    auto rit = zstl::make_reverse_iterator(v.end());
    EXPECT_EQ(*rit, 8);

    rit += 3;
    EXPECT_EQ(*rit, 5);
}

TEST(ReverseIteratorTest, MinusEquals) {
    std::vector<int> v{1, 2, 3, 4, 5};
    auto rit = zstl::make_reverse_iterator(v.begin()); // rend
    rit -= 1;
    EXPECT_EQ(*rit, 1);
}

TEST(ReverseIteratorTest, Subscript) {
    std::vector<int> v{10, 20, 30, 40, 50};
    auto rit = zstl::make_reverse_iterator(v.end());
    EXPECT_EQ(rit[0], 50);
    EXPECT_EQ(rit[1], 40);
    EXPECT_EQ(rit[2], 30);
    EXPECT_EQ(rit[3], 20);
    EXPECT_EQ(rit[4], 10);
}

// ============================================================
// Comparison
// ============================================================

TEST(ReverseIteratorTest, Equality) {
    std::vector<int> v{1, 2, 3};
    auto rit1 = zstl::make_reverse_iterator(v.end());
    auto rit2 = zstl::make_reverse_iterator(v.end());
    EXPECT_TRUE(rit1 == rit2);
    EXPECT_FALSE(rit1 != rit2);

    ++rit1;
    EXPECT_FALSE(rit1 == rit2);
    EXPECT_TRUE(rit1 != rit2);
}

TEST(ReverseIteratorTest, Relational) {
    std::vector<int> v{1, 2, 3, 4, 5};
    auto rb = zstl::make_reverse_iterator(v.end());   // rbegin: points to 5
    auto re = zstl::make_reverse_iterator(v.begin());  // rend

    EXPECT_TRUE(rb < re);
    EXPECT_FALSE(re < rb);
    EXPECT_TRUE(rb <= re);
    EXPECT_TRUE(rb <= rb);
    EXPECT_FALSE(re <= rb);
    EXPECT_TRUE(re > rb);
}

// ============================================================
// Difference
// ============================================================

TEST(ReverseIteratorTest, DifferenceOperator) {
    std::vector<int> v{1, 2, 3, 4, 5};
    auto rb = zstl::make_reverse_iterator(v.end());
    auto re = zstl::make_reverse_iterator(v.begin());

    auto diff = re - rb;
    EXPECT_EQ(diff, 5);
}

// ============================================================
// Bidirectional iterator test
// ============================================================

TEST(ReverseIteratorTest, BidirectionalIterator) {
    std::list<int> l{100, 200, 300, 400, 500};
    auto rb = zstl::make_reverse_iterator(l.end());
    auto re = zstl::make_reverse_iterator(l.begin());

    EXPECT_EQ(*rb, 500);
    ++rb;
    EXPECT_EQ(*rb, 400);
    ++rb;
    EXPECT_EQ(*rb, 300);

    std::vector<int> reversed;
    for (auto it = zstl::make_reverse_iterator(l.end());
         it != zstl::make_reverse_iterator(l.begin()); ++it) {
        reversed.push_back(*it);
    }
    EXPECT_EQ(reversed, (std::vector<int>{500, 400, 300, 200, 100}));
}

// ============================================================
// is_reverse_iterator type trait
// ============================================================

TEST(ReverseIteratorTest, IsReverseIterator) {
    EXPECT_TRUE((zstl::is_reverse_iterator_v<zstl::reverse_iterator<int*>>));
    EXPECT_FALSE((zstl::is_reverse_iterator_v<int*>));
    EXPECT_FALSE((zstl::is_reverse_iterator_v<std::vector<int>::iterator>));
}

// ============================================================
// reverse_iterator_underlying
// ============================================================

TEST(ReverseIteratorTest, ReverseIteratorUnderlying) {
    using Riv = zstl::reverse_iterator<int*>;
    EXPECT_TRUE((std::is_same_v<zstl::reverse_iterator_underlying_t<Riv>, int*>));
}
