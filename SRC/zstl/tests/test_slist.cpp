// ============================================================================
// zstl::slist Comprehensive Unit Tests
// ============================================================================
// Covers: constructors, assignment, before_begin/cbefore_begin, iterators,
// element access, insert_after, emplace_after, erase_after, push_front,
// pop_front, splice_after, remove/remove_if, reverse, unique, sort, merge,
// capacity, clear, swap, empty list edge cases, and comparison operators.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>
#include <algorithm>

using namespace zstl;

// ============================================================================
// slist<int> tests
// ============================================================================

// ---- Constructors ----

TEST(SlistInt, DefaultConstructor) {
    slist<int> s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
}

TEST(SlistInt, FillConstructor) {
    slist<int> s(5, 42);
    EXPECT_EQ(s.size(), 5u);
    for (auto v : s) {
        EXPECT_EQ(v, 42);
    }
}

TEST(SlistInt, FillConstructorDefaultValue) {
    slist<int> s(3);
    EXPECT_EQ(s.size(), 3u);
    for (auto v : s) {
        EXPECT_EQ(v, 0);
    }
}

TEST(SlistInt, FillConstructorZeroCount) {
    slist<int> s(0, 99);
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
}

TEST(SlistInt, RangeConstructor) {
    int arr[] = {1, 2, 3, 4, 5};
    slist<int> s(arr, arr + 5);
    EXPECT_EQ(s.size(), 5u);
    int expected = 1;
    for (auto v : s) {
        EXPECT_EQ(v, expected++);
    }
}

TEST(SlistInt, RangeConstructorEmpty) {
    int arr[] = {1, 2, 3};
    slist<int> s(arr, arr);
    EXPECT_TRUE(s.empty());
}

TEST(SlistInt, CopyConstructor) {
    slist<int> original = {10, 20, 30, 40, 50};
    slist<int> copy(original);
    EXPECT_EQ(copy.size(), 5u);
    int expected = 10;
    for (auto v : copy) {
        EXPECT_EQ(v, expected);
        expected += 10;
    }
    // Verify deep copy
    original.front() = 999;
    EXPECT_EQ(copy.front(), 10);
}

TEST(SlistInt, CopyConstructorEmpty) {
    slist<int> original;
    slist<int> copy(original);
    EXPECT_TRUE(copy.empty());
}

TEST(SlistInt, MoveConstructor) {
    slist<int> original = {1, 2, 3, 4, 5};
    slist<int> moved(std::move(original));
    EXPECT_EQ(moved.size(), 5u);
    EXPECT_EQ(moved.front(), 1);
    EXPECT_TRUE(original.empty());
    EXPECT_EQ(original.size(), 0u);
}

TEST(SlistInt, InitializerListConstructor) {
    slist<int> s = {100, 200, 300};
    EXPECT_EQ(s.size(), 3u);
    auto it = s.begin();
    EXPECT_EQ(*it++, 100);
    EXPECT_EQ(*it++, 200);
    EXPECT_EQ(*it++, 300);
}

TEST(SlistInt, InitializerListConstructorEmpty) {
    slist<int> s = {};
    EXPECT_TRUE(s.empty());
}

// ---- operator= ----

TEST(SlistInt, CopyAssignment) {
    slist<int> s1 = {1, 2, 3};
    slist<int> s2 = {10, 20};
    s2 = s1;
    EXPECT_EQ(s2.size(), 3u);
    EXPECT_EQ(s2.front(), 1);
    EXPECT_EQ(s1.size(), 3u);  // unchanged
}

TEST(SlistInt, MoveAssignment) {
    slist<int> s1 = {1, 2, 3};
    slist<int> s2 = {10, 20};
    s2 = std::move(s1);
    EXPECT_EQ(s2.size(), 3u);
    EXPECT_EQ(s2.front(), 1);
    EXPECT_TRUE(s1.empty());
}

TEST(SlistInt, InitializerListAssignment) {
    slist<int> s = {1, 2, 3};
    s = {100, 200, 300, 400};
    EXPECT_EQ(s.size(), 4u);
    EXPECT_EQ(s.front(), 100);
}

// ---- assign ----

TEST(SlistInt, AssignFill) {
    slist<int> s = {1, 2, 3};
    s.assign(4, 99);
    EXPECT_EQ(s.size(), 4u);
    for (auto v : s) {
        EXPECT_EQ(v, 99);
    }
}

TEST(SlistInt, AssignFillZeroCount) {
    slist<int> s = {1, 2, 3};
    s.assign(0, 99);
    EXPECT_TRUE(s.empty());
}

TEST(SlistInt, AssignRange) {
    slist<int> s = {1, 2, 3};
    int arr[] = {10, 20, 30, 40};
    s.assign(arr, arr + 4);
    EXPECT_EQ(s.size(), 4u);
    int expected = 10;
    for (auto v : s) {
        EXPECT_EQ(v, expected);
        expected += 10;
    }
}

TEST(SlistInt, AssignRangeEmpty) {
    slist<int> s = {1, 2, 3};
    int arr[] = {10, 20};
    s.assign(arr, arr);
    EXPECT_TRUE(s.empty());
}

TEST(SlistInt, AssignInitializerList) {
    slist<int> s = {1, 2, 3};
    s.assign({100, 200});
    EXPECT_EQ(s.size(), 2u);
    EXPECT_EQ(s.front(), 100);
    auto it = s.begin();
    EXPECT_EQ(*it++, 100);
    EXPECT_EQ(*it++, 200);
}

// ---- Iterators ----

TEST(SlistInt, BeforeBegin) {
    slist<int> s = {1, 2, 3};
    auto bb = s.before_begin();
    EXPECT_EQ(++bb, s.begin());  // incrementing before_begin yields begin
}

TEST(SlistInt, CBeforeBegin) {
    const slist<int> s = {1, 2, 3};
    auto bb = s.cbefore_begin();
    EXPECT_EQ(++bb, s.cbegin());
}

TEST(SlistInt, BeginEnd) {
    slist<int> s = {1, 2, 3, 4, 5};
    int expected = 1;
    for (auto it = s.begin(); it != s.end(); ++it) {
        EXPECT_EQ(*it, expected++);
    }
    EXPECT_EQ(expected, 6);
}

TEST(SlistInt, CBeginCEnd) {
    const slist<int> s = {10, 20, 30};
    int sum = 0;
    for (auto it = s.cbegin(); it != s.cend(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 60);
}

TEST(SlistInt, EmptyListBeginEqualsEnd) {
    slist<int> s;
    EXPECT_EQ(s.begin(), s.end());
}

// ---- Element access ----

TEST(SlistInt, Front) {
    slist<int> s = {1, 2, 3};
    EXPECT_EQ(s.front(), 1);
    s.front() = 99;
    EXPECT_EQ(s.front(), 99);
}

TEST(SlistInt, ConstFront) {
    const slist<int> s = {10, 20, 30};
    EXPECT_EQ(s.front(), 10);
}

// ---- insert_after ----

TEST(SlistInt, InsertAfterBeforeBegin) {
    slist<int> s = {2, 3, 4};
    auto it = s.insert_after(s.before_begin(), 1);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(s.size(), 4u);
    EXPECT_EQ(s.front(), 1);
}

TEST(SlistInt, InsertAfterAtBegin) {
    slist<int> s = {10, 30, 40};
    auto it = s.insert_after(s.begin(), 20);
    EXPECT_EQ(*it, 20);
    EXPECT_EQ(s.size(), 4u);
    int expected[] = {10, 20, 30, 40};
    int i = 0;
    for (auto v : s) EXPECT_EQ(v, expected[i++]);
}

TEST(SlistInt, InsertAfterAtEnd) {
    slist<int> s = {1, 2, 3};
    auto it_end = s.begin();
    ++it_end; ++it_end;  // points to 3
    auto it = s.insert_after(it_end, 4);
    EXPECT_EQ(*it, 4);
    EXPECT_EQ(s.size(), 4u);
    int expected[] = {1, 2, 3, 4};
    int i = 0;
    for (auto v : s) EXPECT_EQ(v, expected[i++]);
}

TEST(SlistInt, InsertAfterIntoEmpty) {
    slist<int> s;
    auto it = s.insert_after(s.before_begin(), 42);
    EXPECT_EQ(*it, 42);
    EXPECT_EQ(s.size(), 1u);
    EXPECT_EQ(s.front(), 42);
}

TEST(SlistInt, InsertAfterByMove) {
    slist<int> s = {1, 3};
    int val = 2;
    auto it = s.insert_after(s.begin(), std::move(val));
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(s.size(), 3u);
}

// ---- insert_after: fill ----

TEST(SlistInt, InsertAfterFill) {
    slist<int> s = {1, 5};
    auto it = s.insert_after(s.begin(), 3, 9);
    EXPECT_EQ(s.size(), 5u);
    int expected[] = {1, 9, 9, 9, 5};
    int i = 0;
    for (auto v : s) EXPECT_EQ(v, expected[i++]);
}

TEST(SlistInt, InsertAfterFillBeforeBegin) {
    slist<int> s = {4, 5};
    s.insert_after(s.before_begin(), 3, 9);
    EXPECT_EQ(s.size(), 5u);
    int expected[] = {9, 9, 9, 4, 5};
    int i = 0;
    for (auto v : s) EXPECT_EQ(v, expected[i++]);
}

TEST(SlistInt, InsertAfterFillZeroCount) {
    slist<int> s = {1, 2, 3};
    auto it = s.insert_after(s.begin(), 0, 99);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(s.size(), 3u);
}

// ---- insert_after: range ----

TEST(SlistInt, InsertAfterRange) {
    slist<int> s = {1, 4};
    int arr[] = {2, 3};
    s.insert_after(s.begin(), arr, arr + 2);
    EXPECT_EQ(s.size(), 4u);
    int expected[] = {1, 2, 3, 4};
    int i = 0;
    for (auto v : s) EXPECT_EQ(v, expected[i++]);
}

TEST(SlistInt, InsertAfterRangeEmpty) {
    slist<int> s = {1, 2, 3};
    int arr[] = {10, 20};
    auto it = s.insert_after(s.begin(), arr, arr);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(s.size(), 3u);
}

// ---- insert_after: initializer_list ----

TEST(SlistInt, InsertAfterInitializerList) {
    slist<int> s = {1, 5};
    s.insert_after(s.begin(), {2, 3, 4});
    EXPECT_EQ(s.size(), 5u);
    int expected[] = {1, 2, 3, 4, 5};
    int i = 0;
    for (auto v : s) EXPECT_EQ(v, expected[i++]);
}

// ---- emplace_after ----

TEST(SlistInt, EmplaceAfter) {
    slist<int> s = {1, 3, 4};
    auto it = s.emplace_after(s.begin(), 2);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(s.size(), 4u);
    int expected[] = {1, 2, 3, 4};
    int i = 0;
    for (auto v : s) EXPECT_EQ(v, expected[i++]);
}

TEST(SlistInt, EmplaceAfterBeforeBegin) {
    slist<int> s;
    auto it = s.emplace_after(s.before_begin(), 99);
    EXPECT_EQ(*it, 99);
    EXPECT_EQ(s.size(), 1u);
    EXPECT_EQ(s.front(), 99);
}

TEST(SlistInt, EmplaceAfterAtEnd) {
    slist<int> s = {1, 2};
    auto it_end = s.begin(); ++it_end;  // points to 2
    auto it = s.emplace_after(it_end, 3);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(s.size(), 3u);
}

// ---- erase_after ----

TEST(SlistInt, EraseAfterSingle) {
    slist<int> s = {1, 2, 3, 4, 5};
    auto it = s.erase_after(s.begin());  // erase 2
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(s.size(), 4u);
    int expected[] = {1, 3, 4, 5};
    int i = 0;
    for (auto v : s) EXPECT_EQ(v, expected[i++]);
}

TEST(SlistInt, EraseAfterBeforeBegin) {
    slist<int> s = {1, 2, 3};
    auto it = s.erase_after(s.before_begin());  // erase 1
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(s.size(), 2u);
    EXPECT_EQ(s.front(), 2);
}

TEST(SlistInt, EraseAfterLastElement) {
    slist<int> s = {1, 2, 3};
    auto it_end = s.begin();
    ++it_end; ++it_end;  // points to 3
    auto it = s.erase_after(it_end);  // nothing after 3
    EXPECT_EQ(it, s.end());
    EXPECT_EQ(s.size(), 3u);
}

TEST(SlistInt, EraseAfterFromEmpty) {
    slist<int> s;
    auto it = s.erase_after(s.before_begin());
    EXPECT_EQ(it, s.end());
    EXPECT_TRUE(s.empty());
}

// ---- erase_after: range ----

TEST(SlistInt, EraseAfterRange) {
    slist<int> s = {1, 2, 3, 4, 5};
    // Erase elements after 1 up to (not including) 5: i.e., erase 2,3,4
    auto first = s.begin();
    auto last = s.begin();
    ++last; ++last; ++last; ++last;  // points to 5
    auto it = s.erase_after(first, last);
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(s.size(), 2u);
    int expected[] = {1, 5};
    int i = 0;
    for (auto v : s) EXPECT_EQ(v, expected[i++]);
}

TEST(SlistInt, EraseAfterRangeFromBeforeBegin) {
    slist<int> s = {1, 2, 3, 4, 5};
    auto last = s.begin();
    ++last; ++last; ++last;  // points to 3
    auto it = s.erase_after(s.before_begin(), last);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(s.size(), 3u);
    int expected[] = {3, 4, 5};
    int i = 0;
    for (auto v : s) EXPECT_EQ(v, expected[i++]);
}

TEST(SlistInt, EraseAfterRangeEmptyRange) {
    slist<int> s = {1, 2, 3};
    auto it = s.erase_after(s.begin(), s.begin());
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(s.size(), 3u);
}

// ---- push_front / emplace_front ----

TEST(SlistInt, PushFrontCopy) {
    slist<int> s;
    int val = 42;
    s.push_front(val);
    EXPECT_EQ(val, 42);
    EXPECT_EQ(s.front(), 42);
}

TEST(SlistInt, PushFrontMove) {
    slist<int> s;
    int val = 42;
    s.push_front(std::move(val));
    EXPECT_EQ(s.front(), 42);
}

TEST(SlistInt, EmplaceFront) {
    slist<int> s;
    auto& ref = s.emplace_front(10);
    EXPECT_EQ(ref, 10);
    s.emplace_front(5);
    EXPECT_EQ(s.front(), 5);
    EXPECT_EQ(s.size(), 2u);
}

TEST(SlistInt, EmplaceFrontReturnsReference) {
    slist<int> s;
    auto& ref = s.emplace_front(100);
    EXPECT_EQ(ref, 100);
    ref = 200;
    EXPECT_EQ(s.front(), 200);
}

TEST(SlistInt, BuildListByPushFront) {
    slist<int> s;
    s.push_front(1);
    s.push_front(2);
    s.push_front(3);
    EXPECT_EQ(s.size(), 3u);
    // Order should be 3,2,1 (reverse of insertion)
    int expected[] = {3, 2, 1};
    int i = 0;
    for (auto v : s) EXPECT_EQ(v, expected[i++]);
}

// ---- pop_front ----

TEST(SlistInt, PopFront) {
    slist<int> s = {1, 2, 3};
    s.pop_front();
    EXPECT_EQ(s.size(), 2u);
    EXPECT_EQ(s.front(), 2);
    s.pop_front();
    EXPECT_EQ(s.size(), 1u);
    EXPECT_EQ(s.front(), 3);
    s.pop_front();
    EXPECT_TRUE(s.empty());
}

// ---- splice_after ----

TEST(SlistInt, SpliceAfterEntireList) {
    slist<int> s1 = {1, 2, 3};
    slist<int> s2 = {10, 20, 30};

    // Splice all from s2 after the last element of s1
    auto pos = s1.begin();
    ++pos; ++pos;  // points to 3
    s1.splice_after(pos, s2);

    EXPECT_EQ(s1.size(), 6u);
    EXPECT_TRUE(s2.empty());
    int expected[] = {1, 2, 3, 10, 20, 30};
    int i = 0;
    for (auto v : s1) EXPECT_EQ(v, expected[i++]);
}

TEST(SlistInt, SpliceAfterEntireListBeforeBegin) {
    slist<int> s1 = {4, 5, 6};
    slist<int> s2 = {1, 2, 3};
    s1.splice_after(s1.before_begin(), s2);
    EXPECT_EQ(s1.size(), 6u);
    int expected = 1;
    for (auto v : s1) EXPECT_EQ(v, expected++);
}

TEST(SlistInt, SpliceAfterSingleElement) {
    slist<int> s1 = {1, 2, 5};
    slist<int> s2 = {3, 4};

    // Splice the element after s2.begin() (which is 4) after s1.begin()+1 (which is 2)
    auto pos = ++s1.begin();  // points to 2
    auto it_src = s2.begin();  // points to 3; after this is 4
    s1.splice_after(pos, s2, it_src);

    EXPECT_EQ(s1.size(), 4u);
    EXPECT_EQ(s2.size(), 1u);
    int expected1[] = {1, 2, 4, 5};
    int i = 0;
    for (auto v : s1) EXPECT_EQ(v, expected1[i++]);
    EXPECT_EQ(s2.front(), 3);
}

TEST(SlistInt, SpliceAfterRange) {
    slist<int> s1 = {1, 6};
    slist<int> s2 = {2, 3, 4, 5};

    // before_first is s2.begin() -> 2, before_last points to 5 (end)
    auto before_first = s2.begin();
    auto before_last = s2.begin();
    ++before_last; ++before_last; ++before_last;  // points to 5

    s1.splice_after(s1.begin(), s2, before_first, before_last);

    EXPECT_EQ(s1.size(), 6u);
    EXPECT_EQ(s2.size(), 1u);
    int expected1[] = {1, 3, 4, 5, 6};
    int i = 0;
    for (auto v : s1) EXPECT_EQ(v, expected1[i++]);
}

TEST(SlistInt, SpliceAfterEmptySource) {
    slist<int> s1 = {1, 2, 3};
    slist<int> s2;
    s1.splice_after(s1.begin(), s2);
    EXPECT_EQ(s1.size(), 3u);
    EXPECT_TRUE(s2.empty());
}

// ---- remove / remove_if ----

TEST(SlistInt, Remove) {
    slist<int> s = {1, 2, 3, 2, 4, 2, 5};
    s.remove(2);
    EXPECT_EQ(s.size(), 4u);
    int expected[] = {1, 3, 4, 5};
    int i = 0;
    for (auto v : s) EXPECT_EQ(v, expected[i++]);
}

TEST(SlistInt, RemoveFromFront) {
    slist<int> s = {1, 1, 2, 3};
    s.remove(1);
    EXPECT_EQ(s.size(), 2u);
    EXPECT_EQ(s.front(), 2);
}

TEST(SlistInt, RemoveNonExistent) {
    slist<int> s = {1, 2, 3};
    s.remove(99);
    EXPECT_EQ(s.size(), 3u);
}

TEST(SlistInt, RemoveAll) {
    slist<int> s = {7, 7, 7};
    s.remove(7);
    EXPECT_TRUE(s.empty());
}

TEST(SlistInt, RemoveFromEmpty) {
    slist<int> s;
    s.remove(1);
    EXPECT_TRUE(s.empty());
}

TEST(SlistInt, RemoveIf) {
    slist<int> s = {1, 2, 3, 4, 5, 6};
    s.remove_if([](int v) { return v % 2 == 0; });
    EXPECT_EQ(s.size(), 3u);
    for (auto v : s) EXPECT_EQ(v % 2, 1);
}

TEST(SlistInt, RemoveIfNoneMatch) {
    slist<int> s = {1, 3, 5};
    s.remove_if([](int v) { return v > 100; });
    EXPECT_EQ(s.size(), 3u);
}

TEST(SlistInt, RemoveIfAllMatch) {
    slist<int> s = {2, 4, 6};
    s.remove_if([](int v) { return true; });
    EXPECT_TRUE(s.empty());
}

// ---- reverse ----

TEST(SlistInt, Reverse) {
    slist<int> s = {1, 2, 3, 4, 5};
    s.reverse();
    EXPECT_EQ(s.size(), 5u);
    int expected = 5;
    for (auto v : s) EXPECT_EQ(v, expected--);
}

TEST(SlistInt, ReverseEmpty) {
    slist<int> s;
    s.reverse();
    EXPECT_TRUE(s.empty());
}

TEST(SlistInt, ReverseSingleElement) {
    slist<int> s = {42};
    s.reverse();
    EXPECT_EQ(s.size(), 1u);
    EXPECT_EQ(s.front(), 42);
}

TEST(SlistInt, ReverseTwoElements) {
    slist<int> s = {1, 2};
    s.reverse();
    EXPECT_EQ(s.front(), 2);
    auto it = s.begin(); ++it;
    EXPECT_EQ(*it, 1);
}

TEST(SlistInt, ReverseLarge) {
    slist<int> s;
    for (int i = 0; i < 100; ++i) s.push_front(i);
    // Current order: 99, 98, ..., 0
    s.reverse();
    // Expected: 0, 1, ..., 99
    int expected = 0;
    for (auto v : s) EXPECT_EQ(v, expected++);
}

// ---- unique ----

TEST(SlistInt, UniqueConsecutiveDuplicates) {
    slist<int> s = {1, 2, 2, 3, 3, 3, 4, 1};
    s.unique();
    EXPECT_EQ(s.size(), 5u);
    int expected[] = {1, 2, 3, 4, 1};
    int i = 0;
    for (auto v : s) EXPECT_EQ(v, expected[i++]);
}

TEST(SlistInt, UniqueNoDuplicates) {
    slist<int> s = {1, 2, 3, 4, 5};
    s.unique();
    EXPECT_EQ(s.size(), 5u);
}

TEST(SlistInt, UniqueAllSame) {
    slist<int> s = {9, 9, 9, 9};
    s.unique();
    EXPECT_EQ(s.size(), 1u);
    EXPECT_EQ(s.front(), 9);
}

TEST(SlistInt, UniqueSingleElement) {
    slist<int> s = {42};
    s.unique();
    EXPECT_EQ(s.size(), 1u);
}

TEST(SlistInt, UniqueEmpty) {
    slist<int> s;
    s.unique();
    EXPECT_TRUE(s.empty());
}

TEST(SlistInt, UniqueCustomPredicate) {
    slist<int> s = {1, 2, 2, 3, 4, 4, 5};
    s.unique(equal_to<int>());
    EXPECT_EQ(s.size(), 5u);
}

// ---- sort ----

TEST(SlistInt, SortBasic) {
    slist<int> s = {5, 3, 1, 4, 2};
    s.sort();
    EXPECT_EQ(s.size(), 5u);
    int expected = 1;
    for (auto v : s) EXPECT_EQ(v, expected++);
}

TEST(SlistInt, SortAlreadySorted) {
    slist<int> s = {1, 2, 3, 4, 5};
    s.sort();
    int expected = 1;
    for (auto v : s) EXPECT_EQ(v, expected++);
}

TEST(SlistInt, SortReversed) {
    slist<int> s = {5, 4, 3, 2, 1};
    s.sort();
    int expected = 1;
    for (auto v : s) EXPECT_EQ(v, expected++);
}

TEST(SlistInt, SortEmpty) {
    slist<int> s;
    s.sort();
    EXPECT_TRUE(s.empty());
}

TEST(SlistInt, SortSingleElement) {
    slist<int> s = {42};
    s.sort();
    EXPECT_EQ(s.size(), 1u);
    EXPECT_EQ(s.front(), 42);
}

TEST(SlistInt, SortDescendingComparator) {
    slist<int> s = {1, 4, 2, 5, 3};
    s.sort(greater<int>());
    EXPECT_EQ(s.size(), 5u);
    int prev = 999;
    for (auto v : s) {
        EXPECT_LE(v, prev);
        prev = v;
    }
}

TEST(SlistInt, SortLargeDataset) {
    slist<int> s;
    const int N = 500;
    for (int i = 0; i < N; ++i) s.push_front((i * 7 + 3) % N);
    s.sort();
    EXPECT_EQ(s.size(), static_cast<size_t>(N));
    int prev = -1;
    for (auto v : s) {
        EXPECT_GE(v, prev);
        prev = v;
    }
}

// ---- merge ----

TEST(SlistInt, MergeTwoSortedLists) {
    slist<int> s1 = {1, 3, 5, 7};
    slist<int> s2 = {2, 4, 6, 8};
    s1.merge(s2);
    EXPECT_EQ(s1.size(), 8u);
    EXPECT_TRUE(s2.empty());
    int expected = 1;
    for (auto v : s1) EXPECT_EQ(v, expected++);
}

TEST(SlistInt, MergeWithEmpty) {
    slist<int> s1 = {1, 2, 3};
    slist<int> s2;
    s1.merge(s2);
    EXPECT_EQ(s1.size(), 3u);
    EXPECT_TRUE(s2.empty());
}

TEST(SlistInt, MergeEmptyWithNonEmpty) {
    slist<int> s1;
    slist<int> s2 = {1, 2, 3};
    s1.merge(s2);
    EXPECT_EQ(s1.size(), 3u);
    EXPECT_TRUE(s2.empty());
    EXPECT_EQ(s1.front(), 1);
}

TEST(SlistInt, MergeBothEmpty) {
    slist<int> s1, s2;
    s1.merge(s2);
    EXPECT_TRUE(s1.empty());
    EXPECT_TRUE(s2.empty());
}

TEST(SlistInt, MergeInterleaving) {
    slist<int> s1 = {1, 3, 5};
    slist<int> s2 = {1, 2, 4};
    s1.merge(s2);
    EXPECT_EQ(s1.size(), 6u);
    int expected[] = {1, 1, 2, 3, 4, 5};
    int i = 0;
    for (auto v : s1) EXPECT_EQ(v, expected[i++]);
}

TEST(SlistInt, MergeCustomComparator) {
    slist<int> s1 = {5, 3, 1};
    slist<int> s2 = {8, 6, 4, 2};
    s1.merge(s2, greater<int>());
    EXPECT_EQ(s1.size(), 7u);
    EXPECT_TRUE(s2.empty());
    int prev = 999;
    for (auto v : s1) {
        EXPECT_LE(v, prev);
        prev = v;
    }
}

TEST(SlistInt, MergeSelf) {
    slist<int> s = {1, 2, 3};
    s.merge(s);
    EXPECT_EQ(s.size(), 3u);
}

// ---- resize ----

TEST(SlistInt, ResizeGrow) {
    slist<int> s = {1, 2};
    s.resize(5);
    EXPECT_EQ(s.size(), 5u);
    int expected[] = {1, 2, 0, 0, 0};
    int i = 0;
    for (auto v : s) EXPECT_EQ(v, expected[i++]);
}

TEST(SlistInt, ResizeGrowWithValue) {
    slist<int> s = {1, 2};
    s.resize(5, 99);
    EXPECT_EQ(s.size(), 5u);
    int expected[] = {1, 2, 99, 99, 99};
    int i = 0;
    for (auto v : s) EXPECT_EQ(v, expected[i++]);
}

TEST(SlistInt, ResizeShrink) {
    slist<int> s = {1, 2, 3, 4, 5};
    s.resize(2);
    EXPECT_EQ(s.size(), 2u);
    EXPECT_EQ(s.front(), 1);
    auto it = s.begin(); ++it;
    EXPECT_EQ(*it, 2);
}

TEST(SlistInt, ResizeSameSize) {
    slist<int> s = {1, 2, 3};
    s.resize(3);
    EXPECT_EQ(s.size(), 3u);
}

TEST(SlistInt, ResizeEmpty) {
    slist<int> s;
    s.resize(3, 42);
    EXPECT_EQ(s.size(), 3u);
    for (auto v : s) EXPECT_EQ(v, 42);
}

// ---- Capacity ----

TEST(SlistInt, Empty) {
    slist<int> s;
    EXPECT_TRUE(s.empty());
    s.push_front(1);
    EXPECT_FALSE(s.empty());
    s.clear();
    EXPECT_TRUE(s.empty());
}

TEST(SlistInt, Size) {
    slist<int> s;
    EXPECT_EQ(s.size(), 0u);
    s.push_front(1);
    EXPECT_EQ(s.size(), 1u);
    s.push_front(2);
    EXPECT_EQ(s.size(), 2u);
    s.pop_front();
    EXPECT_EQ(s.size(), 1u);
}

TEST(SlistInt, MaxSize) {
    slist<int> s;
    EXPECT_GT(s.max_size(), 0u);
    EXPECT_GE(s.max_size(), 1024u * 1024u);
}

// ---- clear ----

TEST(SlistInt, Clear) {
    slist<int> s = {1, 2, 3, 4, 5};
    s.clear();
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
}

TEST(SlistInt, ClearEmpty) {
    slist<int> s;
    s.clear();
    EXPECT_TRUE(s.empty());
}

// ---- swap ----

TEST(SlistInt, SwapMember) {
    slist<int> s1 = {1, 2, 3};
    slist<int> s2 = {10, 20};
    s1.swap(s2);
    EXPECT_EQ(s1.size(), 2u);
    EXPECT_EQ(s1.front(), 10);
    EXPECT_EQ(s2.size(), 3u);
    EXPECT_EQ(s2.front(), 1);
}

TEST(SlistInt, SwapNonMember) {
    slist<int> s1 = {1, 2, 3};
    slist<int> s2 = {10, 20};
    zstl::swap(s1, s2);
    EXPECT_EQ(s1.size(), 2u);
    EXPECT_EQ(s2.size(), 3u);
}

TEST(SlistInt, SwapEmptyWithNonEmpty) {
    slist<int> s1;
    slist<int> s2 = {1, 2, 3};
    s1.swap(s2);
    EXPECT_EQ(s1.size(), 3u);
    EXPECT_TRUE(s2.empty());
}

TEST(SlistInt, SwapBothEmpty) {
    slist<int> s1, s2;
    s1.swap(s2);
    EXPECT_TRUE(s1.empty());
    EXPECT_TRUE(s2.empty());
}

TEST(SlistInt, SwapSelf) {
    slist<int> s = {1, 2, 3};
    s.swap(s);
    EXPECT_EQ(s.size(), 3u);
    EXPECT_EQ(s.front(), 1);
}

// ---- Comparison operators ----

TEST(SlistInt, OperatorEquals) {
    slist<int> s1 = {1, 2, 3};
    slist<int> s2 = {1, 2, 3};
    slist<int> s3 = {1, 2, 4};
    slist<int> s4 = {1, 2};
    EXPECT_TRUE(s1 == s2);
    EXPECT_FALSE(s1 == s3);
    EXPECT_FALSE(s1 == s4);
}

TEST(SlistInt, OperatorNotEquals) {
    slist<int> s1 = {1, 2, 3};
    slist<int> s2 = {1, 2, 4};
    EXPECT_TRUE(s1 != s2);
}

TEST(SlistInt, OperatorLess) {
    slist<int> s1 = {1, 2, 3};
    slist<int> s2 = {1, 2, 4};
    slist<int> s3 = {1, 2};
    EXPECT_TRUE(s1 < s2);
    EXPECT_TRUE(s3 < s1);
}

TEST(SlistInt, OperatorGreater) {
    slist<int> s1 = {5, 6};
    slist<int> s2 = {1, 2, 3};
    EXPECT_TRUE(s1 > s2);
}

TEST(SlistInt, OperatorLessEqualGreaterEqual) {
    slist<int> s1 = {1, 2, 3};
    slist<int> s2 = {1, 2, 3};
    slist<int> s3 = {1, 2, 4};
    EXPECT_TRUE(s1 <= s2);
    EXPECT_TRUE(s1 >= s2);
    EXPECT_TRUE(s1 <= s3);
    EXPECT_TRUE(s3 >= s1);
}

TEST(SlistInt, EmptyComparison) {
    slist<int> e1, e2;
    EXPECT_TRUE(e1 == e2);
    EXPECT_FALSE(e1 < e2);
    EXPECT_TRUE(e1 <= e2);
    slist<int> n = {1};
    EXPECT_TRUE(e1 < n);
}

// ---- Self-assignment ----

TEST(SlistInt, SelfCopyAssignment) {
    slist<int> s = {1, 2, 3, 4, 5};
    s = s;
    EXPECT_EQ(s.size(), 5u);
    EXPECT_EQ(s.front(), 1);
}

TEST(SlistInt, SelfMoveAssignment) {
    slist<int> s = {10, 20, 30};
    s = std::move(s);
    EXPECT_EQ(s.size(), 3u);
    EXPECT_EQ(s.front(), 10);
}

// ---- Empty list edge cases ----

TEST(SlistInt, OperationsOnEmptyList) {
    slist<int> s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_EQ(s.begin(), s.end());

    // These operations should be no-ops or safe
    s.clear();
    EXPECT_TRUE(s.empty());

    s.reverse();
    EXPECT_TRUE(s.empty());

    s.sort();
    EXPECT_TRUE(s.empty());

    s.unique();
    EXPECT_TRUE(s.empty());

    s.remove(5);
    EXPECT_TRUE(s.empty());

    s.remove_if([](int) { return true; });
    EXPECT_TRUE(s.empty());

    s.resize(0);
    EXPECT_TRUE(s.empty());
}

TEST(SlistInt, InsertAfterOnEmptyList) {
    slist<int> s;
    auto it = s.insert_after(s.before_begin(), 42);
    EXPECT_EQ(*it, 42);
    EXPECT_EQ(s.size(), 1u);
    EXPECT_EQ(s.front(), 42);
}

TEST(SlistInt, EraseAfterOnEmptyList) {
    slist<int> s;
    auto it = s.erase_after(s.before_begin());
    EXPECT_EQ(it, s.end());
}

// ---- Iteration after modifications ----

TEST(SlistInt, IterationAfterMultiplePushFront) {
    slist<int> s;
    for (int i = 0; i < 100; ++i) {
        s.push_front(i);
    }
    EXPECT_EQ(s.size(), 100u);
    int expected = 99;
    for (auto v : s) {
        EXPECT_EQ(v, expected--);
    }
}

TEST(SlistInt, IterationAfterSort) {
    slist<int> s;
    const int N = 200;
    for (int i = 0; i < N; ++i) {
        s.push_front(i);
    }
    s.sort();
    EXPECT_EQ(s.size(), static_cast<size_t>(N));
    int expected = 0;
    for (auto v : s) EXPECT_EQ(v, expected++);
}

TEST(SlistInt, IterationAfterReverse) {
    slist<int> s;
    for (int i = 0; i < 50; ++i) {
        s.push_front(i);
    }
    // Current order: 49, 48, ..., 0
    s.reverse();
    int expected = 0;
    for (auto v : s) EXPECT_EQ(v, expected++);
}
