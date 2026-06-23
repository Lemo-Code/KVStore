// ============================================================================
// zstl::list Comprehensive Unit Tests
// ============================================================================
// Covers: constructors, assignment, element access, iterators, capacity,
// modifiers (insert/emplace/erase/push/pop/resize), swap, merge, splice,
// remove/remove_if, reverse, unique, sort, iterator stability,
// comparison operators, and self-assignment.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>
#include <algorithm>

using namespace zstl;

// ============================================================================
// Custom struct for testing non-trivial types
// ============================================================================
namespace {
struct Point {
    int x, y;
    Point(int x_ = 0, int y_ = 0) : x(x_), y(y_) {}
    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Point& other) const { return !(*this == other); }
    bool operator<(const Point& other) const {
        if (x != other.x) return x < other.x;
        return y < other.y;
    }
};
}

// ============================================================================
// list<int> tests — fundamental operations
// ============================================================================

// ---- Constructors ----

TEST(ListInt, DefaultConstructor) {
    list<int> l;
    EXPECT_TRUE(l.empty());
    EXPECT_EQ(l.size(), 0u);
}

TEST(ListInt, FillConstructor) {
    list<int> l(5, 42);
    EXPECT_EQ(l.size(), 5u);
    for (auto v : l) {
        EXPECT_EQ(v, 42);
    }
}

TEST(ListInt, FillConstructorDefaultValue) {
    list<int> l(3);
    EXPECT_EQ(l.size(), 3u);
    for (auto v : l) {
        EXPECT_EQ(v, 0);
    }
}

TEST(ListInt, FillConstructorZeroCount) {
    list<int> l(0, 99);
    EXPECT_TRUE(l.empty());
    EXPECT_EQ(l.size(), 0u);
}

TEST(ListInt, RangeConstructor) {
    int arr[] = {1, 2, 3, 4, 5};
    list<int> l(arr, arr + 5);
    EXPECT_EQ(l.size(), 5u);
    int expected = 1;
    for (auto v : l) {
        EXPECT_EQ(v, expected++);
    }
}

TEST(ListInt, RangeConstructorEmpty) {
    int arr[] = {1, 2, 3};
    list<int> l(arr, arr);
    EXPECT_TRUE(l.empty());
}

TEST(ListInt, CopyConstructor) {
    list<int> original = {10, 20, 30, 40, 50};
    list<int> copy(original);
    EXPECT_EQ(copy.size(), 5u);
    auto it = copy.begin();
    EXPECT_EQ(*it++, 10);
    EXPECT_EQ(*it++, 20);
    // Original unchanged
    original.front() = 999;
    EXPECT_EQ(copy.front(), 10);
}

TEST(ListInt, CopyConstructorEmpty) {
    list<int> original;
    list<int> copy(original);
    EXPECT_TRUE(copy.empty());
}

TEST(ListInt, MoveConstructor) {
    list<int> original = {1, 2, 3, 4, 5};
    list<int> moved(std::move(original));
    EXPECT_EQ(moved.size(), 5u);
    EXPECT_EQ(moved.front(), 1);
    EXPECT_EQ(moved.back(), 5);
    EXPECT_TRUE(original.empty());
    EXPECT_EQ(original.size(), 0u);
}

TEST(ListInt, InitializerListConstructor) {
    list<int> l = {100, 200, 300};
    EXPECT_EQ(l.size(), 3u);
    auto it = l.begin();
    EXPECT_EQ(*it++, 100);
    EXPECT_EQ(*it++, 200);
    EXPECT_EQ(*it++, 300);
}

TEST(ListInt, InitializerListConstructorEmpty) {
    list<int> l = {};
    EXPECT_TRUE(l.empty());
}

// ---- operator= ----

TEST(ListInt, CopyAssignment) {
    list<int> l1 = {1, 2, 3};
    list<int> l2 = {10, 20};
    l2 = l1;
    EXPECT_EQ(l2.size(), 3u);
    EXPECT_EQ(l2.front(), 1);
    EXPECT_EQ(l2.back(), 3);
    EXPECT_EQ(l1.size(), 3u);  // unchanged
}

TEST(ListInt, MoveAssignment) {
    list<int> l1 = {1, 2, 3};
    list<int> l2 = {10, 20};
    l2 = std::move(l1);
    EXPECT_EQ(l2.size(), 3u);
    EXPECT_EQ(l2.front(), 1);
    EXPECT_TRUE(l1.empty());
}

TEST(ListInt, InitializerListAssignment) {
    list<int> l = {1, 2, 3};
    l = {100, 200, 300, 400};
    EXPECT_EQ(l.size(), 4u);
    EXPECT_EQ(l.front(), 100);
    EXPECT_EQ(l.back(), 400);
}

// ---- assign ----

TEST(ListInt, AssignFill) {
    list<int> l = {1, 2, 3};
    l.assign(4, 99);
    EXPECT_EQ(l.size(), 4u);
    for (auto v : l) {
        EXPECT_EQ(v, 99);
    }
}

TEST(ListInt, AssignFillZeroCount) {
    list<int> l = {1, 2, 3};
    l.assign(0, 99);
    EXPECT_TRUE(l.empty());
}

TEST(ListInt, AssignRange) {
    list<int> l = {1, 2, 3};
    int arr[] = {10, 20, 30, 40};
    l.assign(arr, arr + 4);
    EXPECT_EQ(l.size(), 4u);
    int expected = 10;
    for (auto v : l) {
        EXPECT_EQ(v, expected);
        expected += 10;
    }
}

TEST(ListInt, AssignRangeEmpty) {
    list<int> l = {1, 2, 3};
    int arr[] = {10, 20};
    l.assign(arr, arr);
    EXPECT_TRUE(l.empty());
}

TEST(ListInt, AssignInitializerList) {
    list<int> l = {1, 2, 3};
    l.assign({100, 200});
    EXPECT_EQ(l.size(), 2u);
    EXPECT_EQ(l.front(), 100);
    EXPECT_EQ(l.back(), 200);
}

// ---- Element access ----

TEST(ListInt, Front) {
    list<int> l = {1, 2, 3};
    EXPECT_EQ(l.front(), 1);
    l.front() = 99;
    EXPECT_EQ(l.front(), 99);
}

TEST(ListInt, ConstFront) {
    const list<int> l = {10, 20, 30};
    EXPECT_EQ(l.front(), 10);
}

TEST(ListInt, Back) {
    list<int> l = {1, 2, 3};
    EXPECT_EQ(l.back(), 3);
    l.back() = 99;
    EXPECT_EQ(l.back(), 99);
}

TEST(ListInt, ConstBack) {
    const list<int> l = {10, 20, 30};
    EXPECT_EQ(l.back(), 30);
}

// ---- Iterators ----

TEST(ListInt, BeginEnd) {
    list<int> l = {1, 2, 3, 4, 5};
    int expected = 1;
    for (auto it = l.begin(); it != l.end(); ++it) {
        EXPECT_EQ(*it, expected++);
    }
    EXPECT_EQ(expected, 6);
}

TEST(ListInt, ReverseIterators) {
    list<int> l = {1, 2, 3, 4, 5};
    int expected = 5;
    for (auto it = l.rbegin(); it != l.rend(); ++it) {
        EXPECT_EQ(*it, expected--);
    }
    EXPECT_EQ(expected, 0);
}

TEST(ListInt, CRBeginCREnd) {
    const list<int> l = {10, 20, 30};
    int expected = 30;
    for (auto it = l.crbegin(); it != l.crend(); ++it) {
        EXPECT_EQ(*it, expected);
        expected -= 10;
    }
    EXPECT_EQ(expected, 0);
}

TEST(ListInt, CBeginCEnd) {
    const list<int> l = {10, 20, 30};
    int sum = 0;
    for (auto it = l.cbegin(); it != l.cend(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 60);
}

// ---- Capacity ----

TEST(ListInt, Empty) {
    list<int> l;
    EXPECT_TRUE(l.empty());
    l.push_back(1);
    EXPECT_FALSE(l.empty());
    l.clear();
    EXPECT_TRUE(l.empty());
}

TEST(ListInt, Size) {
    list<int> l;
    EXPECT_EQ(l.size(), 0u);
    l.push_back(1);
    EXPECT_EQ(l.size(), 1u);
    l.push_front(0);
    EXPECT_EQ(l.size(), 2u);
    l.pop_back();
    EXPECT_EQ(l.size(), 1u);
    l.pop_front();
    EXPECT_EQ(l.size(), 0u);
}

TEST(ListInt, MaxSize) {
    list<int> l;
    EXPECT_GT(l.max_size(), 0u);
    EXPECT_GE(l.max_size(), 1024u * 1024u);
}

// ---- clear ----

TEST(ListInt, Clear) {
    list<int> l = {1, 2, 3, 4, 5};
    l.clear();
    EXPECT_TRUE(l.empty());
    EXPECT_EQ(l.size(), 0u);
}

TEST(ListInt, ClearEmpty) {
    list<int> l;
    l.clear();
    EXPECT_TRUE(l.empty());
}

// ---- insert: single element ----

TEST(ListInt, InsertSingleAtBegin) {
    list<int> l = {2, 3, 4};
    auto it = l.insert(l.begin(), 1);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(l.size(), 4u);
    EXPECT_EQ(l.front(), 1);
}

TEST(ListInt, InsertSingleAtEnd) {
    list<int> l = {1, 2, 3};
    auto it = l.insert(l.end(), 4);
    EXPECT_EQ(*it, 4);
    EXPECT_EQ(l.size(), 4u);
    EXPECT_EQ(l.back(), 4);
}

TEST(ListInt, InsertSingleInMiddle) {
    list<int> l = {1, 3, 4};
    auto it = l.insert(++++l.begin(), 2);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(l.size(), 4u);
    auto fit = l.begin();
    EXPECT_EQ(*fit++, 1);
    EXPECT_EQ(*fit++, 2);
    EXPECT_EQ(*fit++, 3);
    EXPECT_EQ(*fit++, 4);
}

TEST(ListInt, InsertSingleIntoEmpty) {
    list<int> l;
    auto it = l.insert(l.begin(), 42);
    EXPECT_EQ(*it, 42);
    EXPECT_EQ(l.size(), 1u);
    EXPECT_EQ(l.front(), 42);
}

TEST(ListInt, InsertSingleByMove) {
    list<int> l = {1, 3};
    int val = 2;
    auto it = l.insert(++l.begin(), std::move(val));
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(l.size(), 3u);
}

// ---- insert: fill ----

TEST(ListInt, InsertFillAtBegin) {
    list<int> l = {4, 5};
    auto it = l.insert(l.begin(), 3, 9);
    EXPECT_EQ(l.size(), 5u);
    auto fit = l.begin();
    EXPECT_EQ(*fit++, 9);
    EXPECT_EQ(*fit++, 9);
    EXPECT_EQ(*fit++, 9);
    EXPECT_EQ(*fit++, 4);
    EXPECT_EQ(*fit++, 5);
}

TEST(ListInt, InsertFillInMiddle) {
    list<int> l = {1, 5};
    auto it = l.insert(++l.begin(), 3, 9);
    EXPECT_EQ(l.size(), 5u);
    auto fit = l.begin();
    EXPECT_EQ(*fit++, 1);
    EXPECT_EQ(*fit++, 9);
    EXPECT_EQ(*fit++, 9);
    EXPECT_EQ(*fit++, 9);
    EXPECT_EQ(*fit++, 5);
}

TEST(ListInt, InsertFillZeroCount) {
    list<int> l = {1, 2, 3};
    auto it = l.insert(l.begin(), 0, 99);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(l.size(), 3u);
}

// ---- insert: range ----

TEST(ListInt, InsertRangeAtBegin) {
    list<int> l = {4, 5};
    int arr[] = {1, 2, 3};
    l.insert(l.begin(), arr, arr + 3);
    EXPECT_EQ(l.size(), 5u);
    int expected = 1;
    for (auto v : l) {
        EXPECT_EQ(v, expected++);
    }
}

TEST(ListInt, InsertRangeAtEnd) {
    list<int> l = {1, 2};
    int arr[] = {3, 4, 5};
    l.insert(l.end(), arr, arr + 3);
    EXPECT_EQ(l.size(), 5u);
    int expected = 1;
    for (auto v : l) {
        EXPECT_EQ(v, expected++);
    }
}

TEST(ListInt, InsertRangeEmpty) {
    list<int> l = {1, 2, 3};
    int arr[] = {10, 20};
    auto it = l.insert(l.begin(), arr, arr);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(l.size(), 3u);
}

// ---- insert: initializer_list ----

TEST(ListInt, InsertInitializerList) {
    list<int> l = {1, 5};
    l.insert(++l.begin(), {2, 3, 4});
    EXPECT_EQ(l.size(), 5u);
    int expected = 1;
    for (auto v : l) {
        EXPECT_EQ(v, expected++);
    }
}

TEST(ListInt, InsertInitializerListEmpty) {
    list<int> l = {1, 2, 3};
    l.insert(l.begin(), {});
    EXPECT_EQ(l.size(), 3u);
}

// ---- emplace ----

TEST(ListInt, EmplaceInMiddle) {
    list<int> l = {1, 2, 4, 5};
    auto it = l.emplace(++++++l.begin(), 3);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(l.size(), 5u);
    int expected = 1;
    for (auto v : l) EXPECT_EQ(v, expected++);
}

TEST(ListInt, EmplaceAtBegin) {
    list<int> l = {2, 3};
    auto it = l.emplace(l.begin(), 1);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(l.front(), 1);
    EXPECT_EQ(l.size(), 3u);
}

TEST(ListInt, EmplaceAtEnd) {
    list<int> l = {1, 2};
    auto it = l.emplace(l.end(), 3);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(l.back(), 3);
    EXPECT_EQ(l.size(), 3u);
}

// ---- emplace_front / emplace_back ----

TEST(ListInt, EmplaceFront) {
    list<int> l;
    l.emplace_front(3);
    l.emplace_front(2);
    l.emplace_front(1);
    EXPECT_EQ(l.size(), 3u);
    EXPECT_EQ(l.front(), 1);
    EXPECT_EQ(l.back(), 3);
}

TEST(ListInt, EmplaceBack) {
    list<int> l;
    auto& ref = l.emplace_back(42);
    EXPECT_EQ(ref, 42);
    l.emplace_back(43);
    EXPECT_EQ(l.size(), 2u);
    EXPECT_EQ(l.front(), 42);
    EXPECT_EQ(l.back(), 43);
}

TEST(ListInt, EmplaceFrontReturnsReference) {
    list<int> l;
    auto& ref = l.emplace_front(100);
    EXPECT_EQ(ref, 100);
    ref = 200;
    EXPECT_EQ(l.front(), 200);
}

TEST(ListInt, EmplaceBackReturnsReference) {
    list<int> l;
    auto& ref = l.emplace_back(100);
    EXPECT_EQ(ref, 100);
    ref = 200;
    EXPECT_EQ(l.back(), 200);
}

// ---- push_front / push_back ----

TEST(ListInt, PushFrontCopy) {
    list<int> l;
    int val = 1;
    l.push_front(val);
    EXPECT_EQ(val, 1);
    EXPECT_EQ(l.front(), 1);
}

TEST(ListInt, PushFrontMove) {
    list<int> l;
    int val = 42;
    l.push_front(std::move(val));
    EXPECT_EQ(l.front(), 42);
}

TEST(ListInt, PushBackCopy) {
    list<int> l;
    int val = 1;
    l.push_back(val);
    EXPECT_EQ(val, 1);
    EXPECT_EQ(l.back(), 1);
}

TEST(ListInt, PushBackMove) {
    list<int> l;
    int val = 42;
    l.push_back(std::move(val));
    EXPECT_EQ(l.back(), 42);
}

// ---- pop_front / pop_back ----

TEST(ListInt, PopFront) {
    list<int> l = {1, 2, 3};
    l.pop_front();
    EXPECT_EQ(l.size(), 2u);
    EXPECT_EQ(l.front(), 2);
    l.pop_front();
    EXPECT_EQ(l.size(), 1u);
    EXPECT_EQ(l.front(), 3);
    l.pop_front();
    EXPECT_TRUE(l.empty());
}

TEST(ListInt, PopBack) {
    list<int> l = {1, 2, 3};
    l.pop_back();
    EXPECT_EQ(l.size(), 2u);
    EXPECT_EQ(l.back(), 2);
    l.pop_back();
    EXPECT_EQ(l.size(), 1u);
    EXPECT_EQ(l.back(), 1);
    l.pop_back();
    EXPECT_TRUE(l.empty());
}

// ---- erase: single ----

TEST(ListInt, EraseSingle) {
    list<int> l = {1, 2, 3, 4, 5};
    auto it = l.erase(++++++++l.begin());  // erase 3
    EXPECT_EQ(*it, 4);
    EXPECT_EQ(l.size(), 4u);
    int expected[] = {1, 2, 4, 5};
    int i = 0;
    for (auto v : l) EXPECT_EQ(v, expected[i++]);
}

TEST(ListInt, EraseFirst) {
    list<int> l = {1, 2, 3};
    auto it = l.erase(l.begin());
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(l.size(), 2u);
}

TEST(ListInt, EraseLast) {
    list<int> l = {1, 2, 3};
    auto it = l.erase(----l.end());
    EXPECT_EQ(it, l.end());
    EXPECT_EQ(l.size(), 2u);
}

TEST(ListInt, EraseOnlyElement) {
    list<int> l = {42};
    auto it = l.erase(l.begin());
    EXPECT_EQ(it, l.end());
    EXPECT_TRUE(l.empty());
}

// ---- erase: range ----

TEST(ListInt, EraseRange) {
    list<int> l = {1, 2, 3, 4, 5};
    auto first = ++l.begin();
    auto last = ----l.end();
    auto it = l.erase(first, last);
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(l.size(), 2u);
    EXPECT_EQ(l.front(), 1);
    EXPECT_EQ(l.back(), 5);
}

TEST(ListInt, EraseAll) {
    list<int> l = {1, 2, 3};
    auto it = l.erase(l.begin(), l.end());
    EXPECT_EQ(it, l.end());
    EXPECT_TRUE(l.empty());
}

TEST(ListInt, EraseRangeEmpty) {
    list<int> l = {1, 2, 3};
    auto it = l.erase(l.begin(), l.begin());
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(l.size(), 3u);
}

// ---- resize ----

TEST(ListInt, ResizeGrow) {
    list<int> l = {1, 2};
    l.resize(5);
    EXPECT_EQ(l.size(), 5u);
    auto it = l.begin();
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(*it++, 2);
    EXPECT_EQ(*it++, 0);
    EXPECT_EQ(*it++, 0);
    EXPECT_EQ(*it++, 0);
}

TEST(ListInt, ResizeGrowWithValue) {
    list<int> l = {1, 2};
    l.resize(5, 99);
    EXPECT_EQ(l.size(), 5u);
    auto it = l.begin();
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(*it++, 2);
    EXPECT_EQ(*it++, 99);
    EXPECT_EQ(*it++, 99);
    EXPECT_EQ(*it++, 99);
}

TEST(ListInt, ResizeShrink) {
    list<int> l = {1, 2, 3, 4, 5};
    l.resize(2);
    EXPECT_EQ(l.size(), 2u);
    EXPECT_EQ(l.front(), 1);
    EXPECT_EQ(l.back(), 2);
}

TEST(ListInt, ResizeSameSize) {
    list<int> l = {1, 2, 3};
    l.resize(3);
    EXPECT_EQ(l.size(), 3u);
}

// ---- swap ----

TEST(ListInt, SwapMember) {
    list<int> l1 = {1, 2, 3};
    list<int> l2 = {10, 20};
    l1.swap(l2);
    EXPECT_EQ(l1.size(), 2u);
    EXPECT_EQ(l1.front(), 10);
    EXPECT_EQ(l1.back(), 20);
    EXPECT_EQ(l2.size(), 3u);
    EXPECT_EQ(l2.front(), 1);
    EXPECT_EQ(l2.back(), 3);
}

TEST(ListInt, SwapNonMember) {
    list<int> l1 = {1, 2, 3};
    list<int> l2 = {10, 20};
    zstl::swap(l1, l2);
    EXPECT_EQ(l1.size(), 2u);
    EXPECT_EQ(l2.size(), 3u);
}

TEST(ListInt, SwapEmptyWithNonEmpty) {
    list<int> l1;
    list<int> l2 = {1, 2, 3};
    l1.swap(l2);
    EXPECT_EQ(l1.size(), 3u);
    EXPECT_TRUE(l2.empty());
    l2.swap(l1);
    EXPECT_EQ(l2.size(), 3u);
    EXPECT_TRUE(l1.empty());
}

TEST(ListInt, SwapBothEmpty) {
    list<int> l1, l2;
    l1.swap(l2);
    EXPECT_TRUE(l1.empty());
    EXPECT_TRUE(l2.empty());
}

TEST(ListInt, SwapSelf) {
    list<int> l = {1, 2, 3};
    l.swap(l);
    EXPECT_EQ(l.size(), 3u);
    EXPECT_EQ(l.front(), 1);
    EXPECT_EQ(l.back(), 3);
}

// ---- merge ----

TEST(ListInt, MergeTwoSortedLists) {
    list<int> l1 = {1, 3, 5, 7};
    list<int> l2 = {2, 4, 6, 8};
    l1.merge(l2);
    EXPECT_EQ(l1.size(), 8u);
    EXPECT_TRUE(l2.empty());
    int expected = 1;
    for (auto v : l1) EXPECT_EQ(v, expected++);
}

TEST(ListInt, MergeWithEmpty) {
    list<int> l1 = {1, 2, 3};
    list<int> l2;
    l1.merge(l2);
    EXPECT_EQ(l1.size(), 3u);
    EXPECT_TRUE(l2.empty());
}

TEST(ListInt, MergeEmptyWithNonEmpty) {
    list<int> l1;
    list<int> l2 = {1, 2, 3};
    l1.merge(l2);
    EXPECT_EQ(l1.size(), 3u);
    EXPECT_TRUE(l2.empty());
    EXPECT_EQ(l1.front(), 1);
    EXPECT_EQ(l1.back(), 3);
}

TEST(ListInt, MergeBothEmpty) {
    list<int> l1, l2;
    l1.merge(l2);
    EXPECT_TRUE(l1.empty());
    EXPECT_TRUE(l2.empty());
}

TEST(ListInt, MergeInterleaving) {
    list<int> l1 = {1, 3, 5};
    list<int> l2 = {1, 2, 4};
    l1.merge(l2);
    EXPECT_EQ(l1.size(), 6u);
    int expected[] = {1, 1, 2, 3, 4, 5};
    int i = 0;
    for (auto v : l1) EXPECT_EQ(v, expected[i++]);
}

TEST(ListInt, MergeCustomComparator) {
    list<int> l1 = {5, 3, 1};
    list<int> l2 = {8, 6, 4, 2};
    // sorted descending
    l1.merge(l2, greater<int>());
    EXPECT_EQ(l1.size(), 7u);
    EXPECT_TRUE(l2.empty());
    int prev = 999;
    for (auto v : l1) {
        EXPECT_LE(v, prev);
        prev = v;
    }
}

TEST(ListInt, MergeSelf) {
    list<int> l = {1, 2, 3};
    l.merge(l);
    EXPECT_EQ(l.size(), 3u);
}

// ---- splice ----

TEST(ListInt, SpliceEntireList) {
    list<int> l1 = {1, 2, 3};
    list<int> l2 = {10, 20, 30};
    l1.splice(l1.end(), l2);
    EXPECT_EQ(l1.size(), 6u);
    EXPECT_TRUE(l2.empty());
    int expected = 1;
    for (auto v : l1) {
        if (expected <= 3) EXPECT_EQ(v, expected);
        else               EXPECT_EQ(v, (expected - 3) * 10);
        ++expected;
    }
}

TEST(ListInt, SpliceEntireListAtBegin) {
    list<int> l1 = {4, 5, 6};
    list<int> l2 = {1, 2, 3};
    l1.splice(l1.begin(), l2);
    EXPECT_EQ(l1.size(), 6u);
    int expected = 1;
    for (auto v : l1) EXPECT_EQ(v, expected++);
}

TEST(ListInt, SpliceSingleElement) {
    list<int> l1 = {1, 2, 5};
    list<int> l2 = {3, 4};
    auto it = l2.begin();
    l1.splice(----l1.end(), l2, it);
    EXPECT_EQ(l1.size(), 4u);
    EXPECT_EQ(l2.size(), 1u);
    int expected[] = {1, 2, 3, 5};
    int i = 0;
    for (auto v : l1) EXPECT_EQ(v, expected[i++]);
}

TEST(ListInt, SpliceRange) {
    list<int> l1 = {1, 6};
    list<int> l2 = {2, 3, 4, 5};
    auto first = l2.begin();
    auto last = l2.end();
    l1.splice(++l1.begin(), l2, first, last);
    EXPECT_EQ(l1.size(), 6u);
    EXPECT_TRUE(l2.empty());
    int expected = 1;
    for (auto v : l1) EXPECT_EQ(v, expected++);
}

TEST(ListInt, SpliceRangePartial) {
    list<int> l1 = {1, 5};
    list<int> l2 = {2, 3, 4, 6, 7};
    auto first = l2.begin();
    auto last = --------l2.end();
    l1.splice(++l1.begin(), l2, first, last);
    EXPECT_EQ(l1.size(), 5u);
    EXPECT_EQ(l2.size(), 2u);
    int expected1[] = {1, 2, 3, 4, 5};
    int i = 0;
    for (auto v : l1) EXPECT_EQ(v, expected1[i++]);
    EXPECT_EQ(l2.front(), 6);
    EXPECT_EQ(l2.back(), 7);
}

TEST(ListInt, SpliceFromSelf) {
    list<int> l = {1, 2, 3, 4};
    // Splice element 1 before the end
    auto it = l.begin();
    l.splice(l.end(), l, it);
    EXPECT_EQ(l.size(), 4u);
    int expected[] = {2, 3, 4, 1};
    int i = 0;
    for (auto v : l) EXPECT_EQ(v, expected[i++]);
}

TEST(ListInt, SpliceEmptySource) {
    list<int> l1 = {1, 2, 3};
    list<int> l2;
    l1.splice(l1.end(), l2);
    EXPECT_EQ(l1.size(), 3u);
    EXPECT_TRUE(l2.empty());
}

// ---- remove / remove_if ----

TEST(ListInt, Remove) {
    list<int> l = {1, 2, 3, 2, 4, 2, 5};
    l.remove(2);
    EXPECT_EQ(l.size(), 4u);
    int expected[] = {1, 3, 4, 5};
    int i = 0;
    for (auto v : l) EXPECT_EQ(v, expected[i++]);
}

TEST(ListInt, RemoveNonExistent) {
    list<int> l = {1, 2, 3};
    l.remove(99);
    EXPECT_EQ(l.size(), 3u);
}

TEST(ListInt, RemoveAll) {
    list<int> l = {7, 7, 7};
    l.remove(7);
    EXPECT_TRUE(l.empty());
}

TEST(ListInt, RemoveFromEmpty) {
    list<int> l;
    l.remove(1);
    EXPECT_TRUE(l.empty());
}

TEST(ListInt, RemoveIf) {
    list<int> l = {1, 2, 3, 4, 5, 6, 7, 8};
    l.remove_if([](int v) { return v % 2 == 0; });
    EXPECT_EQ(l.size(), 4u);
    for (auto v : l) EXPECT_EQ(v % 2, 1);
}

TEST(ListInt, RemoveIfNoneMatch) {
    list<int> l = {1, 3, 5};
    l.remove_if([](int v) { return v > 100; });
    EXPECT_EQ(l.size(), 3u);
}

TEST(ListInt, RemoveIfAllMatch) {
    list<int> l = {2, 4, 6};
    l.remove_if([](int v) { return true; });
    EXPECT_TRUE(l.empty());
}

// ---- reverse ----

TEST(ListInt, Reverse) {
    list<int> l = {1, 2, 3, 4, 5};
    l.reverse();
    EXPECT_EQ(l.size(), 5u);
    int expected = 5;
    for (auto v : l) EXPECT_EQ(v, expected--);
}

TEST(ListInt, ReverseEmpty) {
    list<int> l;
    l.reverse();
    EXPECT_TRUE(l.empty());
}

TEST(ListInt, ReverseSingleElement) {
    list<int> l = {42};
    l.reverse();
    EXPECT_EQ(l.size(), 1u);
    EXPECT_EQ(l.front(), 42);
}

TEST(ListInt, ReverseTwoElements) {
    list<int> l = {1, 2};
    l.reverse();
    EXPECT_EQ(l.front(), 2);
    EXPECT_EQ(l.back(), 1);
}

// ---- unique ----

TEST(ListInt, UniqueConsecutiveDuplicates) {
    list<int> l = {1, 2, 2, 3, 3, 3, 4, 1};
    l.unique();
    EXPECT_EQ(l.size(), 5u);
    int expected[] = {1, 2, 3, 4, 1};
    int i = 0;
    for (auto v : l) EXPECT_EQ(v, expected[i++]);
}

TEST(ListInt, UniqueNoDuplicates) {
    list<int> l = {1, 2, 3, 4, 5};
    l.unique();
    EXPECT_EQ(l.size(), 5u);
}

TEST(ListInt, UniqueAllSame) {
    list<int> l = {9, 9, 9, 9};
    l.unique();
    EXPECT_EQ(l.size(), 1u);
    EXPECT_EQ(l.front(), 9);
}

TEST(ListInt, UniqueSingleElement) {
    list<int> l = {42};
    l.unique();
    EXPECT_EQ(l.size(), 1u);
}

TEST(ListInt, UniqueCustomPredicate) {
    list<int> l = {1, 2, 2, 3, 4, 4, 5};
    l.unique(equal_to<int>());
    EXPECT_EQ(l.size(), 5u);
    int expected[] = {1, 2, 3, 4, 5};
    int i = 0;
    for (auto v : l) EXPECT_EQ(v, expected[i++]);
}

// ---- sort ----

TEST(ListInt, SortBasic) {
    list<int> l = {5, 3, 1, 4, 2};
    l.sort();
    EXPECT_EQ(l.size(), 5u);
    int expected = 1;
    for (auto v : l) EXPECT_EQ(v, expected++);
}

TEST(ListInt, SortAlreadySorted) {
    list<int> l = {1, 2, 3, 4, 5};
    l.sort();
    int expected = 1;
    for (auto v : l) EXPECT_EQ(v, expected++);
}

TEST(ListInt, SortReversed) {
    list<int> l = {5, 4, 3, 2, 1};
    l.sort();
    int expected = 1;
    for (auto v : l) EXPECT_EQ(v, expected++);
}

TEST(ListInt, SortEmpty) {
    list<int> l;
    l.sort();
    EXPECT_TRUE(l.empty());
}

TEST(ListInt, SortSingleElement) {
    list<int> l = {42};
    l.sort();
    EXPECT_EQ(l.size(), 1u);
    EXPECT_EQ(l.front(), 42);
}

TEST(ListInt, SortDescendingComparator) {
    list<int> l = {1, 4, 2, 5, 3};
    l.sort(greater<int>());
    EXPECT_EQ(l.size(), 5u);
    int prev = 999;
    for (auto v : l) {
        EXPECT_LE(v, prev);
        prev = v;
    }
}

TEST(ListInt, SortLargeDataset) {
    list<int> l;
    const int N = 1000;
    for (int i = 0; i < N; ++i) l.push_back((i * 7 + 3) % N);
    l.sort();
    EXPECT_EQ(l.size(), static_cast<size_t>(N));
    int prev = -1;
    for (auto v : l) {
        EXPECT_GE(v, prev);
        prev = v;
    }
}

TEST(ListInt, SortStability) {
    // sort is stable; verify with equal elements tagged by original order
    struct TaggedValue {
        int value, tag;
        bool operator==(const TaggedValue& o) const { return value == o.value && tag == o.tag; }
        bool operator<(const TaggedValue& o) const { return value < o.value; }
    };
    list<TaggedValue> l;
    l.push_back({2, 0});
    l.push_back({1, 0});
    l.push_back({2, 1});
    l.push_back({1, 1});
    l.push_back({2, 2});
    l.sort();
    EXPECT_EQ(l.size(), 5u);
    auto it = l.begin();
    EXPECT_EQ(it->value, 1); EXPECT_EQ(it->tag, 0); ++it;
    EXPECT_EQ(it->value, 1); EXPECT_EQ(it->tag, 1); ++it;
    EXPECT_EQ(it->value, 2); EXPECT_EQ(it->tag, 0); ++it;
    EXPECT_EQ(it->value, 2); EXPECT_EQ(it->tag, 1); ++it;
    EXPECT_EQ(it->value, 2); EXPECT_EQ(it->tag, 2);
}

// ---- Iterator stability ----

TEST(ListInt, IteratorStabilityAcrossInsert) {
    list<int> l = {1, 3, 5};
    auto it = l.begin();  // points to 1
    auto it3 = ----l.end();  // points to 5
    l.insert(++l.begin(), 2);
    l.insert(----l.end(), 4);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(*it3, 5);
    EXPECT_EQ(l.size(), 5u);
    int expected = 1;
    for (auto v : l) EXPECT_EQ(v, expected++);
}

TEST(ListInt, IteratorStabilityAcrossSplice) {
    list<int> l1 = {1, 2, 3};
    list<int> l2 = {10, 20};

    auto it_l1_2 = ++l1.begin();  // points to 2
    auto it_l2_20 = ++l2.begin();  // points to 20

    EXPECT_EQ(*it_l1_2, 2);
    EXPECT_EQ(*it_l2_20, 20);

    l1.splice(l1.end(), l2);

    EXPECT_EQ(*it_l1_2, 2);   // still valid
    EXPECT_EQ(*it_l2_20, 20);  // still valid, now in l1

    EXPECT_EQ(l1.size(), 5u);
    int expected[] = {1, 2, 3, 10, 20};
    int i = 0;
    for (auto v : l1) EXPECT_EQ(v, expected[i++]);
}

// ---- Comparison operators ----

TEST(ListInt, OperatorEquals) {
    list<int> l1 = {1, 2, 3};
    list<int> l2 = {1, 2, 3};
    list<int> l3 = {1, 2, 4};
    list<int> l4 = {1, 2};
    EXPECT_TRUE(l1 == l2);
    EXPECT_FALSE(l1 == l3);
    EXPECT_FALSE(l1 == l4);
}

TEST(ListInt, OperatorNotEquals) {
    list<int> l1 = {1, 2, 3};
    list<int> l2 = {1, 2, 4};
    EXPECT_TRUE(l1 != l2);
}

TEST(ListInt, OperatorLess) {
    list<int> l1 = {1, 2, 3};
    list<int> l2 = {1, 2, 4};
    list<int> l3 = {1, 2};
    EXPECT_TRUE(l1 < l2);
    EXPECT_TRUE(l3 < l1);
}

TEST(ListInt, OperatorGreater) {
    list<int> l1 = {5, 6};
    list<int> l2 = {1, 2, 3};
    EXPECT_TRUE(l1 > l2);
}

TEST(ListInt, OperatorLessEqualGreaterEqual) {
    list<int> l1 = {1, 2, 3};
    list<int> l2 = {1, 2, 3};
    list<int> l3 = {1, 2, 4};
    EXPECT_TRUE(l1 <= l2);
    EXPECT_TRUE(l1 >= l2);
    EXPECT_TRUE(l1 <= l3);
    EXPECT_TRUE(l3 >= l1);
}

TEST(ListInt, EmptyComparison) {
    list<int> e1, e2;
    EXPECT_TRUE(e1 == e2);
    EXPECT_FALSE(e1 < e2);
    EXPECT_TRUE(e1 <= e2);
    list<int> n = {1};
    EXPECT_TRUE(e1 < n);
}

// ---- Self-assignment ----

TEST(ListInt, SelfCopyAssignment) {
    list<int> l = {1, 2, 3, 4, 5};
    l = l;
    EXPECT_EQ(l.size(), 5u);
    EXPECT_EQ(l.front(), 1);
    EXPECT_EQ(l.back(), 5);
}

TEST(ListInt, SelfMoveAssignment) {
    list<int> l = {10, 20, 30};
    l = std::move(l);
    EXPECT_EQ(l.size(), 3u);
    EXPECT_EQ(l.front(), 10);
}


// ============================================================================
// list<Point> tests — custom struct type
// ============================================================================

TEST(ListPoint, DefaultConstructor) {
    list<Point> l;
    EXPECT_TRUE(l.empty());
}

TEST(ListPoint, FillConstructor) {
    list<Point> l(3, Point(1, 2));
    EXPECT_EQ(l.size(), 3u);
    for (const auto& p : l) {
        EXPECT_EQ(p.x, 1);
        EXPECT_EQ(p.y, 2);
    }
}

TEST(ListPoint, RangeConstructor) {
    std::vector<Point> pts = {{0,0}, {1,1}, {2,2}};
    list<Point> l(pts.begin(), pts.end());
    EXPECT_EQ(l.size(), 3u);
    auto it = l.begin();
    EXPECT_EQ(it->x, 0); ++it;
    EXPECT_EQ(it->x, 1); ++it;
    EXPECT_EQ(it->x, 2);
}

TEST(ListPoint, CopyConstructor) {
    list<Point> original = {{1,2}, {3,4}, {5,6}};
    list<Point> copy(original);
    EXPECT_EQ(copy.size(), 3u);
    original.front().x = 999;
    EXPECT_EQ(copy.front().x, 1);
}

TEST(ListPoint, PushBackEmplaceFront) {
    list<Point> l;
    l.push_back(Point(1, 2));
    l.emplace_front(0, 0);
    l.emplace_back(3, 3);
    EXPECT_EQ(l.size(), 3u);
    EXPECT_EQ(l.front().x, 0);
    EXPECT_EQ(l.back().x, 3);
}

TEST(ListPoint, InsertEmplace) {
    list<Point> l = {{1,1}, {3,3}};
    l.insert(++l.begin(), Point(2,2));
    EXPECT_EQ(l.size(), 3u);
    auto it = l.begin();
    EXPECT_EQ(it->x, 1); ++it;
    EXPECT_EQ(it->x, 2); ++it;
    EXPECT_EQ(it->x, 3);
}

TEST(ListPoint, RemoveIf) {
    list<Point> l = {{1,1}, {2,0}, {3,1}, {4,0}};
    l.remove_if([](const Point& p) { return p.y == 0; });
    EXPECT_EQ(l.size(), 2u);
    for (const auto& p : l) {
        EXPECT_EQ(p.y, 1);
    }
}

TEST(ListPoint, SortByX) {
    list<Point> l = {{3,3}, {1,1}, {2,2}};
    l.sort();
    EXPECT_EQ(l.front().x, 1);
    EXPECT_EQ(l.back().x, 3);
}

TEST(ListPoint, SortByCustomComparator) {
    list<Point> l = {{1,3}, {1,2}, {1,1}};
    l.sort([](const Point& a, const Point& b) { return a.y < b.y; });
    auto it = l.begin();
    EXPECT_EQ(it->y, 1); ++it;
    EXPECT_EQ(it->y, 2); ++it;
    EXPECT_EQ(it->y, 3);
}

TEST(ListPoint, Merge) {
    list<Point> l1 = {{1,1}, {3,3}, {5,5}};
    list<Point> l2 = {{2,2}, {4,4}};
    l1.merge(l2);
    EXPECT_EQ(l1.size(), 5u);
    int expected_x = 1;
    for (const auto& p : l1) {
        EXPECT_EQ(p.x, expected_x);
        ++expected_x;
    }
}

TEST(ListPoint, UniqueByX) {
    list<Point> l = {{1,0}, {1,1}, {2,0}, {2,1}};
    l.unique([](const Point& a, const Point& b) { return a.x == b.x; });
    EXPECT_EQ(l.size(), 2u);
}
