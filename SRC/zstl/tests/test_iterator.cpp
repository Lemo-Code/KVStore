// ============================================================================
// zstl Iterator Unit Tests
// Tests: iterator_traits, iterator tags, advance (all categories), distance,
// next, prev, begin/end/cbegin/cend/rbegin/rend for arrays and containers,
// size/empty/data free functions, iter_swap.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <vector>
#include <list>
#include <string>
#include <set>

// ============================================================
// Iterator tags
// ============================================================

TEST(IteratorTest, IteratorTags) {
    EXPECT_TRUE((std::is_same_v<zstl::input_iterator_tag, std::input_iterator_tag>));
    EXPECT_TRUE((std::is_same_v<zstl::output_iterator_tag, std::output_iterator_tag>));
    EXPECT_TRUE((std::is_same_v<zstl::forward_iterator_tag, std::forward_iterator_tag>));
    EXPECT_TRUE((std::is_same_v<zstl::bidirectional_iterator_tag, std::bidirectional_iterator_tag>));
    EXPECT_TRUE((std::is_same_v<zstl::random_access_iterator_tag, std::random_access_iterator_tag>));
}

// ============================================================
// iterator_traits for pointers
// ============================================================

TEST(IteratorTest, IteratorTraitsPointer) {
    using traits = zstl::iterator_traits<int*>;
    EXPECT_TRUE((std::is_same_v<traits::iterator_category, zstl::contiguous_iterator_tag>));
    EXPECT_TRUE((std::is_same_v<traits::value_type, int>));
    EXPECT_TRUE((std::is_same_v<traits::difference_type, ptrdiff_t>));
    EXPECT_TRUE((std::is_same_v<traits::pointer, int*>));
    EXPECT_TRUE((std::is_same_v<traits::reference, int&>));
}

TEST(IteratorTest, IteratorTraitsConstPointer) {
    using traits = zstl::iterator_traits<const int*>;
    EXPECT_TRUE((std::is_same_v<traits::value_type, int>));
    EXPECT_TRUE((std::is_same_v<traits::reference, const int&>));
    EXPECT_TRUE((std::is_same_v<traits::pointer, const int*>));
}

TEST(IteratorTest, IteratorTraitsStdContainer) {
    using traits = zstl::iterator_traits<std::vector<int>::iterator>;
    EXPECT_TRUE((std::is_same_v<traits::iterator_category,
                  std::random_access_iterator_tag>));
    EXPECT_TRUE((std::is_same_v<traits::value_type, int>));
}

TEST(IteratorTest, IteratorTraitsListContainer) {
    using traits = zstl::iterator_traits<std::list<int>::iterator>;
    EXPECT_TRUE((std::is_same_v<traits::iterator_category,
                  std::bidirectional_iterator_tag>));
}

// ============================================================
// Convenience aliases
// ============================================================

TEST(IteratorTest, ConvenienceAliases) {
    using traits = zstl::iterator_traits<int*>;
    EXPECT_TRUE((std::is_same_v<zstl::iterator_category_t<int*>, traits::iterator_category>));
    EXPECT_TRUE((std::is_same_v<zstl::iterator_value_t<int*>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::iterator_difference_t<int*>, ptrdiff_t>));
    EXPECT_TRUE((std::is_same_v<zstl::iterator_pointer_t<int*>, int*>));
    EXPECT_TRUE((std::is_same_v<zstl::iterator_reference_t<int*>, int&>));
}

// ============================================================
// Category check helpers
// ============================================================

TEST(IteratorTest, CategoryChecks) {
    EXPECT_TRUE((zstl::detail::is_random_access_iterator_v<int*>));
    EXPECT_TRUE((zstl::detail::is_contiguous_iterator_v<int*>));
    EXPECT_TRUE((zstl::detail::is_forward_iterator_v<int*>));
    EXPECT_TRUE((zstl::detail::is_bidirectional_iterator_v<int*>));
    EXPECT_TRUE((zstl::detail::is_input_iterator_v<int*>));

    using list_iter = std::list<int>::iterator;
    EXPECT_FALSE((zstl::detail::is_random_access_iterator_v<list_iter>));
    EXPECT_TRUE((zstl::detail::is_bidirectional_iterator_v<list_iter>));
    EXPECT_TRUE((zstl::detail::is_forward_iterator_v<list_iter>));
    EXPECT_TRUE((zstl::detail::is_input_iterator_v<list_iter>));
}

// ============================================================
// advance
// ============================================================

TEST(IteratorTest, AdvanceRandomAccess) {
    std::vector<int> v{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    auto it = v.begin();
    zstl::advance(it, 5);
    EXPECT_EQ(*it, 5);
}

TEST(IteratorTest, AdvanceRandomAccessNegative) {
    std::vector<int> v{0, 1, 2, 3, 4};
    auto it = v.begin() + 4;
    zstl::advance(it, -2);
    EXPECT_EQ(*it, 2);
}

TEST(IteratorTest, AdvanceBidirectional) {
    std::list<int> l{10, 20, 30, 40, 50};
    auto it = l.begin();
    zstl::advance(it, 3);
    EXPECT_EQ(*it, 40);

    zstl::advance(it, -1);
    EXPECT_EQ(*it, 30);
}

TEST(IteratorTest, AdvanceForward) {
    std::set<int> s{5, 3, 8, 1};
    auto it = s.begin();
    zstl::advance(it, 2);
    // Move 2 steps forward in ordered set
    // Typical order: 1, 3, 5, 8
    EXPECT_GT(*it, 1);
}

TEST(IteratorTest, AdvanceZero) {
    std::vector<int> v{1, 2, 3};
    auto it = v.begin();
    zstl::advance(it, 0);
    EXPECT_EQ(*it, 1);
}

// ============================================================
// distance
// ============================================================

TEST(IteratorTest, DistanceRandomAccess) {
    std::vector<int> v{0, 1, 2, 3, 4};
    auto d = zstl::distance(v.begin(), v.end());
    EXPECT_EQ(d, 5);
}

TEST(IteratorTest, DistanceBidirectional) {
    std::list<int> l{1, 2, 3};
    auto d = zstl::distance(l.begin(), l.end());
    EXPECT_EQ(d, 3);
}

TEST(IteratorTest, DistanceSameIterator) {
    std::vector<int> v{1, 2, 3};
    EXPECT_EQ(zstl::distance(v.begin(), v.begin()), 0);
}

// ============================================================
// next / prev
// ============================================================

TEST(IteratorTest, Next) {
    std::vector<int> v{10, 20, 30, 40, 50};
    auto it = zstl::next(v.begin());
    EXPECT_EQ(*it, 20);

    auto it3 = zstl::next(v.begin(), 3);
    EXPECT_EQ(*it3, 40);
}

TEST(IteratorTest, Prev) {
    std::vector<int> v{10, 20, 30, 40, 50};
    auto it = zstl::prev(v.end());
    EXPECT_EQ(*it, 50);

    auto it2 = zstl::prev(v.end(), 2);
    EXPECT_EQ(*it2, 40);
}

// ============================================================
// begin / end for arrays
// ============================================================

TEST(IteratorTest, BeginEndArray) {
    int arr[] = {10, 20, 30, 40, 50};
    auto b = zstl::begin(arr);
    auto e = zstl::end(arr);
    EXPECT_EQ(*b, 10);
    EXPECT_EQ(*(e - 1), 50);
    EXPECT_EQ(e - b, 5);
}

TEST(IteratorTest, BeginEndConstArray) {
    const int arr[] = {1, 2, 3};
    auto b = zstl::begin(arr);
    auto e = zstl::end(arr);
    EXPECT_TRUE((std::is_same_v<decltype(b), const int*>));
    EXPECT_EQ(*b, 1);
    EXPECT_EQ(e - b, 3);
}

// ============================================================
// begin / end for containers
// ============================================================

TEST(IteratorTest, BeginEndVector) {
    std::vector<int> v{1, 2, 3};
    EXPECT_EQ(*zstl::begin(v), 1);
    EXPECT_EQ(zstl::end(v) - zstl::begin(v), 3);
}

TEST(IteratorTest, BeginEndConstVector) {
    const std::vector<int> v{10, 20, 30};
    EXPECT_EQ(*zstl::begin(v), 10);
    EXPECT_EQ(zstl::end(v) - zstl::begin(v), 3);
}

// ============================================================
// cbegin / cend
// ============================================================

TEST(IteratorTest, CBeginCEnd) {
    const std::vector<int> v{5, 10, 15};
    auto b = zstl::cbegin(v);
    auto e = zstl::cend(v);
    EXPECT_EQ(*b, 5);
    EXPECT_EQ(e - b, 3);
}

// ============================================================
// rbegin / rend for arrays
// ============================================================

TEST(IteratorTest, RBeginREndArray) {
    int arr[] = {10, 20, 30};
    auto rb = zstl::rbegin(arr);
    auto re = zstl::rend(arr);
    EXPECT_EQ(*rb, 30);
    // *re would be before the first element (arr[-1] logically), not dereferenceable
}

// ============================================================
// rbegin / rend for containers
// ============================================================

TEST(IteratorTest, RBeginREndVector) {
    std::vector<int> v{1, 2, 3, 4, 5};
    auto rb = zstl::rbegin(v);
    EXPECT_EQ(*rb, 5);
}

// ============================================================
// crbegin / crend
// ============================================================

TEST(IteratorTest, CRBeginCREnd) {
    const std::vector<int> v{1, 2, 3};
    auto rb = zstl::crbegin(v);
    EXPECT_EQ(*rb, 3);
}

// ============================================================
// size / empty / data free functions
// ============================================================

TEST(IteratorTest, SizeArray) {
    int arr[] = {1, 2, 3, 4};
    EXPECT_EQ(zstl::size(arr), 4u);
}

TEST(IteratorTest, SizeContainer) {
    std::vector<int> v{1, 2, 3};
    EXPECT_EQ(zstl::size(v), 3u);
}

TEST(IteratorTest, EmptyArray) {
    int arr[] = {1, 2};
    EXPECT_FALSE(zstl::empty(arr)); // C-style array is never empty (N > 0)
}

TEST(IteratorTest, EmptyContainer) {
    std::vector<int> v;
    EXPECT_TRUE(zstl::empty(v));

    v.push_back(1);
    EXPECT_FALSE(zstl::empty(v));
}

TEST(IteratorTest, DataArray) {
    int arr[] = {10, 20, 30};
    EXPECT_EQ(zstl::data(arr), arr);
    EXPECT_EQ(zstl::data(arr), &arr[0]);
}

TEST(IteratorTest, DataContainer) {
    std::vector<int> v{1, 2, 3};
    EXPECT_EQ(zstl::data(v), v.data());
}

// ============================================================
// iter_swap
// ============================================================

TEST(IteratorTest, IterSwap) {
    int a = 10, b = 20;
    zstl::iter_swap(&a, &b);
    EXPECT_EQ(a, 20);
    EXPECT_EQ(b, 10);
}

TEST(IteratorTest, IterSwapVector) {
    std::vector<int> v{1, 2, 3, 4};
    zstl::iter_swap(v.begin(), v.begin() + 2);
    EXPECT_EQ(v[0], 3);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 1);
    EXPECT_EQ(v[3], 4);
}

// ============================================================
// iterator base class
// ============================================================

struct MyCountingIterator
    : zstl::iterator<zstl::forward_iterator_tag, int, ptrdiff_t, int*, int&> {
    int* ptr;
    explicit MyCountingIterator(int* p) : ptr(p) {}
    int& operator*() const { return *ptr; }
    MyCountingIterator& operator++() { ++ptr; return *this; }
    MyCountingIterator operator++(int) { auto t = *this; ++ptr; return t; }
    bool operator==(const MyCountingIterator& o) const { return ptr == o.ptr; }
    bool operator!=(const MyCountingIterator& o) const { return ptr != o.ptr; }
};

TEST(IteratorTest, CustomIteratorWithBase) {
    int arr[] = {10, 20, 30};
    MyCountingIterator it(arr);
    EXPECT_EQ(*it, 10);

    ++it;
    EXPECT_EQ(*it, 20);
}
