// ============================================================================
// zstl find/search algorithm tests
// ============================================================================
// Tests for: find/find_if/find_if_not, find_end, find_first_of, adjacent_find,
//           search (BMH for byte types), search_n, count/count_if, mismatch,
//           equal, all_of/any_of/none_of
// Covers: empty ranges, not-found cases, custom predicates, edge cases
// ============================================================================

#include <gtest/gtest.h>
#include "zstl/zstl.h"

#include <string>
#include <vector>
#include <list>
#include <chrono>

using namespace zstl;

// ============================================================================
// find tests
// ============================================================================

TEST(Find, EmptyRange) {
  vector<int> v;
  auto it = zstl::find(v.begin(), v.end(), 42);
  EXPECT_EQ(it, v.end());
}

TEST(Find, FoundFirst) {
  vector<int> v = {3, 1, 4, 1, 5, 9};
  auto it = zstl::find(v.begin(), v.end(), 3);
  EXPECT_EQ(it, v.begin());
  EXPECT_EQ(*it, 3);
}

TEST(Find, FoundMiddle) {
  vector<int> v = {1, 2, 3, 4, 5};
  auto it = zstl::find(v.begin(), v.end(), 3);
  EXPECT_EQ(it, v.begin() + 2);
  EXPECT_EQ(*it, 3);
}

TEST(Find, FoundLast) {
  vector<int> v = {1, 2, 3, 4, 5};
  auto it = zstl::find(v.begin(), v.end(), 5);
  EXPECT_EQ(it, v.end() - 1);
}

TEST(Find, NotFound) {
  vector<int> v = {1, 2, 3, 4, 5};
  auto it = zstl::find(v.begin(), v.end(), 99);
  EXPECT_EQ(it, v.end());
}

TEST(Find, SingleElementFound) {
  vector<int> v = {42};
  auto it = zstl::find(v.begin(), v.end(), 42);
  EXPECT_EQ(it, v.begin());
}

TEST(Find, SingleElementNotFound) {
  vector<int> v = {42};
  auto it = zstl::find(v.begin(), v.end(), 99);
  EXPECT_EQ(it, v.end());
}

TEST(Find, DuplicatesReturnsFirst) {
  vector<int> v = {1, 2, 2, 2, 3};
  auto it = zstl::find(v.begin(), v.end(), 2);
  EXPECT_EQ(it, v.begin() + 1);
}

TEST(Find, StringType) {
  vector<std::string> v = {"apple", "banana", "cherry"};
  auto it = zstl::find(v.begin(), v.end(), std::string("banana"));
  EXPECT_EQ(it, v.begin() + 1);
  EXPECT_EQ(*it, "banana");
}

// ============================================================================
// find_if tests
// ============================================================================

TEST(FindIf, EmptyRange) {
  vector<int> v;
  auto it = zstl::find_if(v.begin(), v.end(), [](int x) { return x > 0; });
  EXPECT_EQ(it, v.end());
}

TEST(FindIf, Found) {
  vector<int> v = {1, 3, 5, 6, 7};
  auto it = zstl::find_if(v.begin(), v.end(), [](int x) { return x % 2 == 0; });
  EXPECT_EQ(it, v.begin() + 3);
  EXPECT_EQ(*it, 6);
}

TEST(FindIf, NotFound) {
  vector<int> v = {1, 3, 5, 7, 9};
  auto it = zstl::find_if(v.begin(), v.end(), [](int x) { return x % 2 == 0; });
  EXPECT_EQ(it, v.end());
}

TEST(FindIf, Single) {
  vector<int> v = {42};
  auto it1 = zstl::find_if(v.begin(), v.end(), [](int x) { return x > 40; });
  EXPECT_EQ(it1, v.begin());
  auto it2 = zstl::find_if(v.begin(), v.end(), [](int x) { return x < 0; });
  EXPECT_EQ(it2, v.end());
}

TEST(FindIf, AllElementsSatisfy) {
  vector<int> v = {2, 4, 6, 8};
  auto it = zstl::find_if(v.begin(), v.end(), [](int x) { return x % 2 == 0; });
  EXPECT_EQ(it, v.begin());
}

TEST(FindIf, CustomPredicateString) {
  vector<std::string> v = {"cat", "elephant", "dog", "hippopotamus"};
  auto it = zstl::find_if(v.begin(), v.end(), [](const std::string& s) {
    return s.length() > 5;
  });
  EXPECT_EQ(it, v.begin() + 1);
  EXPECT_EQ(*it, "elephant");
}

// ============================================================================
// find_if_not tests
// ============================================================================

TEST(FindIfNot, EmptyRange) {
  vector<int> v;
  auto it = zstl::find_if_not(v.begin(), v.end(), [](int x) { return x > 0; });
  EXPECT_EQ(it, v.end());
}

TEST(FindIfNot, Found) {
  vector<int> v = {2, 4, 5, 6, 8};
  auto it = zstl::find_if_not(v.begin(), v.end(), [](int x) { return x % 2 == 0; });
  EXPECT_EQ(it, v.begin() + 2);
  EXPECT_EQ(*it, 5);
}

TEST(FindIfNot, NotFound) {
  vector<int> v = {2, 4, 6, 8};
  auto it = zstl::find_if_not(v.begin(), v.end(), [](int x) { return x % 2 == 0; });
  EXPECT_EQ(it, v.end());
}

TEST(FindIfNot, AllTrue) {
  vector<int> v = {1, 3, 5, 7};
  auto it = zstl::find_if_not(v.begin(), v.end(), [](int x) { return x % 2 == 1; });
  EXPECT_EQ(it, v.end());
}

// ============================================================================
// find_end tests
// ============================================================================

TEST(FindEnd, EmptyNeedle) {
  vector<int> v = {1, 2, 3};
  vector<int> needle;
  auto it = zstl::find_end(v.begin(), v.end(), needle.begin(), needle.end());
  // Empty subsequence matches at the end
  EXPECT_EQ(it, v.end());
}

TEST(FindEnd, EmptyHaystack) {
  vector<int> v;
  vector<int> needle = {1, 2};
  auto it = zstl::find_end(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.end());
}

TEST(FindEnd, FoundSingle) {
  vector<int> v = {1, 2, 3, 1, 2, 3};
  vector<int> needle = {1, 2};
  auto it = zstl::find_end(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.begin() + 3);  // last occurrence starts at index 3
}

TEST(FindEnd, NotFound) {
  vector<int> v = {1, 2, 3, 4, 5};
  vector<int> needle = {2, 4};
  auto it = zstl::find_end(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.end());
}

TEST(FindEnd, SingleElement) {
  vector<int> v = {5, 1, 2, 5, 3};
  vector<int> needle = {5};
  auto it = zstl::find_end(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.begin() + 3);
}

TEST(FindEnd, EntireRange) {
  vector<int> v = {1, 2, 3};
  vector<int> needle = {1, 2, 3};
  auto it = zstl::find_end(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.begin());
}

// ============================================================================
// find_first_of tests
// ============================================================================

TEST(FindFirstOf, EmptyNeedle) {
  vector<int> v = {1, 2, 3};
  vector<int> needle;
  auto it = zstl::find_first_of(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.end());
}

TEST(FindFirstOf, EmptyHaystack) {
  vector<int> v;
  vector<int> needle = {1, 2};
  auto it = zstl::find_first_of(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.end());
}

TEST(FindFirstOf, Found) {
  vector<int> v = {1, 2, 3, 4, 5};
  vector<int> needle = {5, 3, 0};
  auto it = zstl::find_first_of(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.begin() + 2);  // 3 is the first match
  EXPECT_EQ(*it, 3);
}

TEST(FindFirstOf, NotFound) {
  vector<int> v = {1, 2, 3};
  vector<int> needle = {4, 5, 6};
  auto it = zstl::find_first_of(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.end());
}

TEST(FindFirstOf, MultipleMatches) {
  vector<int> v = {1, 2, 3, 4, 5};
  vector<int> needle = {5, 1};
  auto it = zstl::find_first_of(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.begin());  // 1 is found first
  EXPECT_EQ(*it, 1);
}

// ============================================================================
// adjacent_find tests
// ============================================================================

TEST(AdjacentFind, Empty) {
  vector<int> v;
  auto it = zstl::adjacent_find(v.begin(), v.end());
  EXPECT_EQ(it, v.end());
}

TEST(AdjacentFind, Single) {
  vector<int> v = {1};
  auto it = zstl::adjacent_find(v.begin(), v.end());
  EXPECT_EQ(it, v.end());
}

TEST(AdjacentFind, Found) {
  vector<int> v = {1, 2, 3, 3, 4, 5};
  auto it = zstl::adjacent_find(v.begin(), v.end());
  EXPECT_EQ(it, v.begin() + 2);
  EXPECT_EQ(*it, 3);
}

TEST(AdjacentFind, NotFound) {
  vector<int> v = {1, 2, 3, 4, 5};
  auto it = zstl::adjacent_find(v.begin(), v.end());
  EXPECT_EQ(it, v.end());
}

TEST(AdjacentFind, FirstPair) {
  vector<int> v = {1, 1, 2, 2, 3, 3};
  auto it = zstl::adjacent_find(v.begin(), v.end());
  EXPECT_EQ(it, v.begin());
}

TEST(AdjacentFind, CustomPredicate) {
  vector<int> v = {1, 3, 5, 7, 8, 10};
  // Find first adjacent pair where second is exactly first + 1
  auto it = zstl::adjacent_find(v.begin(), v.end(), [](int a, int b) {
    return b == a + 1;
  });
  EXPECT_EQ(it, v.begin() + 3);  // 7,8
}

// ============================================================================
// search tests
// ============================================================================

TEST(Search, EmptyNeedle) {
  vector<int> v = {1, 2, 3, 4};
  vector<int> needle;
  auto it = zstl::search(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.begin());  // empty needle matches at start
}

TEST(Search, EmptyHaystack) {
  vector<int> v;
  vector<int> needle = {1, 2};
  auto it = zstl::search(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.end());
}

TEST(Search, Found) {
  vector<int> v = {1, 2, 3, 4, 5, 6};
  vector<int> needle = {3, 4, 5};
  auto it = zstl::search(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.begin() + 2);
}

TEST(Search, FoundAtStart) {
  vector<int> v = {1, 2, 3, 4};
  vector<int> needle = {1, 2};
  auto it = zstl::search(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.begin());
}

TEST(Search, FoundAtEnd) {
  vector<int> v = {1, 2, 3, 4};
  vector<int> needle = {3, 4};
  auto it = zstl::search(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.begin() + 2);
}

TEST(Search, NotFound) {
  vector<int> v = {1, 2, 3, 4};
  vector<int> needle = {2, 4};
  auto it = zstl::search(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.end());
}

TEST(Search, NeedleLargerThanHaystack) {
  vector<int> v = {1, 2};
  vector<int> needle = {1, 2, 3};
  auto it = zstl::search(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.end());
}

TEST(Search, BMHByteType) {
  // Test Boyer-Moore-Horspool path for byte-sized types
  vector<int8_t> v = {1, 2, 3, 4, 5, 1, 2, 3};
  vector<int8_t> needle = {1, 2, 3};
  auto it = zstl::search(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.begin());  // first occurrence at index 0
}

TEST(Search, BMHByteTypeMultipleOccurrences) {
  // BMH should find first occurrence
  vector<uint8_t> v = {0, 0, 1, 2, 1, 2, 3, 4};
  vector<uint8_t> needle = {1, 2};
  auto it = zstl::search(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.begin() + 2);
}

TEST(Search, SingleElementNeedle) {
  vector<int> v = {5, 1, 3, 7, 3, 9};
  vector<int> needle = {3};
  auto it = zstl::search(v.begin(), v.end(), needle.begin(), needle.end());
  EXPECT_EQ(it, v.begin() + 2);
}

// ============================================================================
// search_n tests
// ============================================================================

TEST(SearchN, ZeroCount) {
  vector<int> v = {1, 2, 3};
  auto it = zstl::search_n(v.begin(), v.end(), 0, 1);
  EXPECT_EQ(it, v.begin());  // Zero count matches at start
}

TEST(SearchN, Empty) {
  vector<int> v;
  auto it = zstl::search_n(v.begin(), v.end(), 2, 1);
  EXPECT_EQ(it, v.end());
}

TEST(SearchN, Found) {
  vector<int> v = {1, 2, 2, 2, 3};
  auto it = zstl::search_n(v.begin(), v.end(), 3, 2);
  EXPECT_EQ(it, v.begin() + 1);
}

TEST(SearchN, NotFound) {
  vector<int> v = {1, 2, 2, 2, 3};
  auto it = zstl::search_n(v.begin(), v.end(), 4, 2);
  EXPECT_EQ(it, v.end());
}

TEST(SearchN, CountOne) {
  vector<int> v = {1, 2, 3, 4};
  auto it = zstl::search_n(v.begin(), v.end(), 1, 3);
  EXPECT_EQ(it, v.begin() + 2);
}

TEST(SearchN, EntireRange) {
  vector<int> v = {5, 5, 5, 5};
  auto it = zstl::search_n(v.begin(), v.end(), 4, 5);
  EXPECT_EQ(it, v.begin());
}

TEST(SearchN, NotEnoughElements) {
  vector<int> v = {1, 2, 3};
  auto it = zstl::search_n(v.begin(), v.end(), 5, 1);
  EXPECT_EQ(it, v.end());
}

// ============================================================================
// count / count_if tests
// ============================================================================

TEST(Count, Empty) {
  vector<int> v;
  EXPECT_EQ(zstl::count(v.begin(), v.end(), 42), 0);
}

TEST(Count, None) {
  vector<int> v = {1, 2, 3, 4, 5};
  EXPECT_EQ(zstl::count(v.begin(), v.end(), 99), 0);
}

TEST(Count, Some) {
  vector<int> v = {1, 2, 2, 2, 3, 2, 4};
  EXPECT_EQ(zstl::count(v.begin(), v.end(), 2), 4);
}

TEST(Count, All) {
  vector<int> v = {7, 7, 7, 7, 7};
  EXPECT_EQ(zstl::count(v.begin(), v.end(), 7), 5);
}

TEST(Count, Single) {
  vector<int> v = {42};
  EXPECT_EQ(zstl::count(v.begin(), v.end(), 42), 1);
  EXPECT_EQ(zstl::count(v.begin(), v.end(), 0), 0);
}

TEST(CountIf, Empty) {
  vector<int> v;
  EXPECT_EQ(zstl::count_if(v.begin(), v.end(), [](int x) { return x > 0; }), 0);
}

TEST(CountIf, Basic) {
  vector<int> v = {1, 2, 3, 4, 5, 6};
  EXPECT_EQ(zstl::count_if(v.begin(), v.end(), [](int x) { return x % 2 == 0; }), 3);
}

TEST(CountIf, All) {
  vector<int> v = {2, 4, 6, 8};
  EXPECT_EQ(zstl::count_if(v.begin(), v.end(), [](int x) { return x > 0; }), 4);
}

TEST(CountIf, None) {
  vector<int> v = {1, 3, 5, 7};
  EXPECT_EQ(zstl::count_if(v.begin(), v.end(), [](int x) { return x < 0; }), 0);
}

TEST(Count, Large) {
  const int N = 1000;
  vector<int> v(N);
  for (int i = 0; i < N; ++i) v[i] = i % 10;
  EXPECT_EQ(zstl::count(v.begin(), v.end(), 0), 100);
  EXPECT_EQ(zstl::count(v.begin(), v.end(), 5), 100);
  EXPECT_EQ(zstl::count(v.begin(), v.end(), 9), 100);
  EXPECT_EQ(zstl::count(v.begin(), v.end(), 100), 0);
}

// ============================================================================
// mismatch tests
// ============================================================================

TEST(Mismatch, EmptyFirst) {
  vector<int> v1, v2 = {1};
  auto [it1, it2] = zstl::mismatch(v1.begin(), v1.end(), v2.begin());
  EXPECT_EQ(it1, v1.end());
}

TEST(Mismatch, AllMatch) {
  vector<int> v1 = {1, 2, 3};
  vector<int> v2 = {1, 2, 3};
  auto [it1, it2] = zstl::mismatch(v1.begin(), v1.end(), v2.begin());
  EXPECT_EQ(it1, v1.end());
}

TEST(Mismatch, Found) {
  vector<int> v1 = {1, 2, 4, 5};
  vector<int> v2 = {1, 2, 3, 5};
  auto [it1, it2] = zstl::mismatch(v1.begin(), v1.end(), v2.begin());
  EXPECT_EQ(*it1, 4);
  EXPECT_EQ(*it2, 3);
}

TEST(Mismatch, Immediate) {
  vector<int> v1 = {1, 2, 3};
  vector<int> v2 = {2, 2, 3};
  auto [it1, it2] = zstl::mismatch(v1.begin(), v1.end(), v2.begin());
  EXPECT_EQ(it1, v1.begin());
  EXPECT_EQ(it2, v2.begin());
}

TEST(Mismatch, FourIteratorEmpty) {
  vector<int> v1;
  vector<int> v2;
  auto [it1, it2] = zstl::mismatch(v1.begin(), v1.end(), v2.begin(), v2.end());
  EXPECT_EQ(it1, v1.end());
  EXPECT_EQ(it2, v2.end());
}

TEST(Mismatch, FourIteratorDifferentLengths) {
  vector<int> v1 = {1, 2, 3};
  vector<int> v2 = {1, 2};
  auto [it1, it2] = zstl::mismatch(v1.begin(), v1.end(), v2.begin(), v2.end());
  EXPECT_EQ(it1, v1.begin() + 2);
  EXPECT_EQ(it2, v2.end());
}

// ============================================================================
// equal tests
// ============================================================================

TEST(Equal, Empty) {
  vector<int> v1, v2;
  EXPECT_TRUE(zstl::equal(v1.begin(), v1.end(), v2.begin()));
}

TEST(Equal, EqualRanges) {
  vector<int> v1 = {1, 2, 3, 4};
  vector<int> v2 = {1, 2, 3, 4};
  EXPECT_TRUE(zstl::equal(v1.begin(), v1.end(), v2.begin()));
}

TEST(Equal, NotEqual) {
  vector<int> v1 = {1, 2, 3};
  vector<int> v2 = {1, 2, 4};
  EXPECT_FALSE(zstl::equal(v1.begin(), v1.end(), v2.begin()));
}

TEST(Equal, DifferentLengthsFirstLonger) {
  vector<int> v1 = {1, 2, 3, 4};
  vector<int> v2 = {1, 2, 3};
  EXPECT_FALSE(zstl::equal(v1.begin(), v1.end(), v2.begin(), v2.end()));
}

TEST(Equal, FourIteratorEqual) {
  vector<int> v1 = {1, 2, 3};
  vector<int> v2 = {1, 2, 3};
  EXPECT_TRUE(zstl::equal(v1.begin(), v1.end(), v2.begin(), v2.end()));
}

TEST(Equal, FourIteratorNotEqual) {
  vector<int> v1 = {1, 2, 3};
  vector<int> v2 = {1, 2, 4};
  EXPECT_FALSE(zstl::equal(v1.begin(), v1.end(), v2.begin(), v2.end()));
}

TEST(Equal, SingleElement) {
  vector<int> v1 = {42};
  vector<int> v2 = {42};
  EXPECT_TRUE(zstl::equal(v1.begin(), v1.end(), v2.begin()));
}

TEST(Equal, Large) {
  vector<int> v1(1000), v2(1000);
  for (int i = 0; i < 1000; ++i) v1[i] = v2[i] = i;
  EXPECT_TRUE(zstl::equal(v1.begin(), v1.end(), v2.begin()));
  v2[500] = -1;
  EXPECT_FALSE(zstl::equal(v1.begin(), v1.end(), v2.begin()));
}

// ============================================================================
// all_of / any_of / none_of tests
// ============================================================================

TEST(AllOf, Empty) {
  vector<int> v;
  EXPECT_TRUE(zstl::all_of(v.begin(), v.end(), [](int x) { return false; }));
}

TEST(AllOf, AllSatisfy) {
  vector<int> v = {2, 4, 6, 8};
  EXPECT_TRUE(zstl::all_of(v.begin(), v.end(), [](int x) { return x % 2 == 0; }));
}

TEST(AllOf, NotAllSatisfy) {
  vector<int> v = {2, 4, 5, 8};
  EXPECT_FALSE(zstl::all_of(v.begin(), v.end(), [](int x) { return x % 2 == 0; }));
}

TEST(AnyOf, Empty) {
  vector<int> v;
  EXPECT_FALSE(zstl::any_of(v.begin(), v.end(), [](int x) { return true; }));
}

TEST(AnyOf, HasMatch) {
  vector<int> v = {1, 3, 4, 5};
  EXPECT_TRUE(zstl::any_of(v.begin(), v.end(), [](int x) { return x % 2 == 0; }));
}

TEST(AnyOf, NoMatch) {
  vector<int> v = {1, 3, 5, 7};
  EXPECT_FALSE(zstl::any_of(v.begin(), v.end(), [](int x) { return x % 2 == 0; }));
}

TEST(NoneOf, Empty) {
  vector<int> v;
  EXPECT_TRUE(zstl::none_of(v.begin(), v.end(), [](int x) { return true; }));
}

TEST(NoneOf, NoMatch) {
  vector<int> v = {1, 3, 5, 7};
  EXPECT_TRUE(zstl::none_of(v.begin(), v.end(), [](int x) { return x % 2 == 0; }));
}

TEST(NoneOf, HasMatch) {
  vector<int> v = {1, 3, 4, 7};
  EXPECT_FALSE(zstl::none_of(v.begin(), v.end(), [](int x) { return x % 2 == 0; }));
}

TEST(AllOf, Single) {
  vector<int> v = {42};
  EXPECT_TRUE(zstl::all_of(v.begin(), v.end(), [](int x) { return x == 42; }));
  EXPECT_FALSE(zstl::all_of(v.begin(), v.end(), [](int x) { return x < 0; }));
}

// ============================================================================
// Custom predicate tests
// ============================================================================

struct Point { int x, y; };

TEST(Find, CustomPredicateFindIfStruct) {
  vector<Point> v = {{1, 2}, {3, 4}, {5, 6}};
  auto it = zstl::find_if(v.begin(), v.end(), [](const Point& p) {
    return p.x + p.y == 7;
  });
  EXPECT_EQ(it, v.begin() + 1);
  EXPECT_EQ(it->x, 3);
  EXPECT_EQ(it->y, 4);
}

TEST(Search, CustomPredicate) {
  vector<int> v = {1, 2, 3, 4, 5};
  vector<int> needle = {2, 3};
  auto it = zstl::search(v.begin(), v.end(), needle.begin(), needle.end(),
                          [](int a, int b) { return a * 2 == b * 2; });
  EXPECT_EQ(it, v.begin() + 1);
}
