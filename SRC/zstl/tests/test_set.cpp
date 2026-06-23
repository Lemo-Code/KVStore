// ============================================================================
// zstl set tests — ordered unique-key container backed by red-black tree
// ============================================================================
// Tests for: set<int> and set with custom comparator
// Covers: all constructors, insert/emplace, erase, find/count/contains,
//         lower_bound/upper_bound/equal_range, begin/end/rbegin/rend,
//         empty/size/max_size, clear/swap/merge, key_comp/value_comp,
//         custom comparator, comparison operators, sorted iteration,
//         uniqueness enforcement, get_allocator
// ============================================================================

#include <gtest/gtest.h>
#include "zstl/zstl.h"

#include <string>
#include <stdexcept>
#include <vector>

using namespace zstl;

// ============================================================================
// Constructors
// ============================================================================

TEST(Set, DefaultConstructor) {
  set<int> s;
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);
}

TEST(Set, ComparatorConstructor) {
  set<int, greater<int>> s;
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);
}

TEST(Set, RangeConstructor) {
  std::vector<int> values = {5, 1, 4, 2, 3};
  set<int> s(values.begin(), values.end());
  EXPECT_EQ(s.size(), 5u);
  // Verify sorted order
  auto it = s.begin();
  EXPECT_EQ(*it, 1); ++it;
  EXPECT_EQ(*it, 2); ++it;
  EXPECT_EQ(*it, 3); ++it;
  EXPECT_EQ(*it, 4); ++it;
  EXPECT_EQ(*it, 5);
}

TEST(Set, RangeConstructorEmpty) {
  std::vector<int> empty;
  set<int> s(empty.begin(), empty.end());
  EXPECT_TRUE(s.empty());
}

TEST(Set, RangeConstructorWithDuplicates) {
  std::vector<int> values = {3, 1, 3, 2, 1, 4};
  set<int> s(values.begin(), values.end());
  EXPECT_EQ(s.size(), 4u);  // duplicates removed
  auto it = s.begin();
  EXPECT_EQ(*it, 1); ++it;
  EXPECT_EQ(*it, 2); ++it;
  EXPECT_EQ(*it, 3); ++it;
  EXPECT_EQ(*it, 4);
}

TEST(Set, CopyConstructor) {
  set<int> s1 = {3, 1, 4, 2};
  set<int> s2(s1);
  EXPECT_EQ(s2.size(), 4u);
  // Verify contents
  auto it = s2.begin();
  EXPECT_EQ(*it, 1); ++it;
  EXPECT_EQ(*it, 2); ++it;
  EXPECT_EQ(*it, 3); ++it;
  EXPECT_EQ(*it, 4);
}

TEST(Set, CopyConstructorIndependence) {
  set<int> s1 = {1, 2, 3};
  set<int> s2(s1);
  s1.insert(4);
  EXPECT_EQ(s1.size(), 4u);
  EXPECT_EQ(s2.size(), 3u);  // s2 unaffected
}

TEST(Set, MoveConstructor) {
  set<int> s1 = {5, 3, 7};
  set<int> s2(std::move(s1));
  EXPECT_EQ(s2.size(), 3u);
  EXPECT_TRUE(s2.contains(5));
  EXPECT_TRUE(s2.contains(3));
  EXPECT_TRUE(s2.contains(7));
}

TEST(Set, InitializerListConstructor) {
  set<int> s = {10, 30, 20, 40, 15};
  EXPECT_EQ(s.size(), 5u);
  auto it = s.begin();
  EXPECT_EQ(*it, 10); ++it;
  EXPECT_EQ(*it, 15); ++it;
  EXPECT_EQ(*it, 20); ++it;
  EXPECT_EQ(*it, 30); ++it;
  EXPECT_EQ(*it, 40);
}

TEST(Set, CopyAssignment) {
  set<int> s1 = {100, 200};
  set<int> s2;
  s2 = s1;
  EXPECT_EQ(s2.size(), 2u);
  EXPECT_TRUE(s2.contains(100));
  EXPECT_TRUE(s2.contains(200));
}

TEST(Set, MoveAssignment) {
  set<int> s1 = {7, 8, 9};
  set<int> s2;
  s2 = std::move(s1);
  EXPECT_EQ(s2.size(), 3u);
}

TEST(Set, InitializerListAssignment) {
  set<int> s;
  s = {50, 25, 75};
  EXPECT_EQ(s.size(), 3u);
}

// ============================================================================
// insert()
// ============================================================================

TEST(Set, InsertSingle) {
  set<int> s;
  auto result = s.insert(42);
  EXPECT_TRUE(result.second);
  EXPECT_EQ(*result.first, 42);
  EXPECT_EQ(s.size(), 1u);
}

TEST(Set, InsertDuplicate) {
  set<int> s = {5};
  auto result = s.insert(5);
  EXPECT_FALSE(result.second);
  EXPECT_EQ(*result.first, 5);
  EXPECT_EQ(s.size(), 1u);
}

TEST(Set, InsertMultipleValues) {
  set<int> s;
  s.insert(20);
  s.insert(10);
  s.insert(30);
  s.insert(5);
  s.insert(25);
  EXPECT_EQ(s.size(), 5u);
  // Verify sorted iteration
  int expected[] = {5, 10, 20, 25, 30};
  int idx = 0;
  for (int v : s) {
    EXPECT_EQ(v, expected[idx++]);
  }
}

TEST(Set, InsertWithHint) {
  set<int> s = {10, 30, 50};
  auto hint = s.find(30);
  auto it = s.insert(hint, 25);
  EXPECT_EQ(*it, 25);
  EXPECT_TRUE(s.contains(25));
  EXPECT_EQ(s.size(), 4u);
}

TEST(Set, InsertWithHintDuplicate) {
  set<int> s = {10, 30, 50};
  auto hint = s.find(30);
  auto it = s.insert(hint, 30);
  EXPECT_EQ(*it, 30);
  EXPECT_EQ(s.size(), 3u);  // no change
}

TEST(Set, InsertRange) {
  set<int> s;
  std::vector<int> values = {4, 1, 5, 2, 3};
  s.insert(values.begin(), values.end());
  EXPECT_EQ(s.size(), 5u);
  int expected = 1;
  for (int v : s) {
    EXPECT_EQ(v, expected++);
  }
}

TEST(Set, InsertInitializerList) {
  set<int> s;
  s.insert({9, 3, 6, 1, 7});
  EXPECT_EQ(s.size(), 5u);
  EXPECT_TRUE(s.contains(1));
  EXPECT_TRUE(s.contains(3));
  EXPECT_TRUE(s.contains(6));
  EXPECT_TRUE(s.contains(7));
  EXPECT_TRUE(s.contains(9));
}

// ============================================================================
// emplace()
// ============================================================================

TEST(Set, EmplaceSingle) {
  set<int> s;
  auto result = s.emplace(15);
  EXPECT_TRUE(result.second);
  EXPECT_EQ(*result.first, 15);
  EXPECT_EQ(s.size(), 1u);
}

TEST(Set, EmplaceDuplicate) {
  set<int> s = {15};
  auto result = s.emplace(15);
  EXPECT_FALSE(result.second);
  EXPECT_EQ(*result.first, 15);
  EXPECT_EQ(s.size(), 1u);
}

TEST(Set, EmplaceMultiple) {
  set<int> s;
  s.emplace(3);
  s.emplace(1);
  s.emplace(4);
  s.emplace(2);
  EXPECT_EQ(s.size(), 4u);
  int expected = 1;
  for (int v : s) EXPECT_EQ(v, expected++);
}

// ============================================================================
// erase()
// ============================================================================

TEST(Set, EraseByKey) {
  set<int> s = {1, 2, 3, 4, 5};
  size_t removed = s.erase(3);
  EXPECT_EQ(removed, 1u);
  EXPECT_EQ(s.size(), 4u);
  EXPECT_FALSE(s.contains(3));
}

TEST(Set, EraseByKeyNotFound) {
  set<int> s = {1, 2};
  size_t removed = s.erase(99);
  EXPECT_EQ(removed, 0u);
  EXPECT_EQ(s.size(), 2u);
}

TEST(Set, EraseByIterator) {
  set<int> s = {10, 20, 30, 40};
  auto it = s.find(20);
  auto next = s.erase(it);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_FALSE(s.contains(20));
  EXPECT_EQ(*next, 30);
}

TEST(Set, EraseByConstIterator) {
  set<int> s = {5, 15, 25};
  const auto& cs = s;
  auto it = cs.find(15);
  auto next = s.erase(it);
  EXPECT_EQ(s.size(), 2u);
  EXPECT_EQ(*next, 25);
}

TEST(Set, EraseByRange) {
  set<int> s = {1, 2, 3, 4, 5, 6};
  auto first = s.find(2);
  auto last = s.find(5);
  auto next = s.erase(first, last);  // erases 2, 3, 4
  EXPECT_EQ(s.size(), 3u);
  EXPECT_TRUE(s.contains(1));
  EXPECT_FALSE(s.contains(2));
  EXPECT_FALSE(s.contains(3));
  EXPECT_FALSE(s.contains(4));
  EXPECT_TRUE(s.contains(5));
  EXPECT_TRUE(s.contains(6));
  EXPECT_EQ(*next, 5);
}

TEST(Set, EraseBeginToEnd) {
  set<int> s = {1, 2, 3};
  auto next = s.erase(s.begin(), s.end());
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(next, s.end());
}

TEST(Set, EraseRangeSingle) {
  set<int> s = {10, 20, 30, 40};
  auto first = s.find(20);
  auto last = s.find(30);
  auto next = s.erase(first, last);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_FALSE(s.contains(20));
  EXPECT_EQ(*next, 30);
}

// ============================================================================
// find(), count(), contains()
// ============================================================================

TEST(Set, FindExisting) {
  set<int> s = {10, 20, 30};
  auto it = s.find(20);
  EXPECT_NE(it, s.end());
  EXPECT_EQ(*it, 20);
}

TEST(Set, FindNonExisting) {
  set<int> s = {10, 20, 30};
  auto it = s.find(99);
  EXPECT_EQ(it, s.end());
}

TEST(Set, ConstFind) {
  const set<int> s = {5, 10, 15};
  auto it = s.find(10);
  EXPECT_NE(it, s.end());
  EXPECT_EQ(*it, 10);
}

TEST(Set, Count) {
  set<int> s = {1, 2, 1, 3};
  EXPECT_EQ(s.count(1), 1u);
  EXPECT_EQ(s.count(2), 1u);
  EXPECT_EQ(s.count(99), 0u);
}

TEST(Set, CountEmpty) {
  set<int> s;
  EXPECT_EQ(s.count(42), 0u);
}

TEST(Set, Contains) {
  set<int> s = {100, 200, 300};
  EXPECT_TRUE(s.contains(100));
  EXPECT_TRUE(s.contains(200));
  EXPECT_TRUE(s.contains(300));
  EXPECT_FALSE(s.contains(400));
  EXPECT_FALSE(s.contains(0));
}

// ============================================================================
// lower_bound(), upper_bound(), equal_range()
// ============================================================================

TEST(Set, LowerBoundExact) {
  set<int> s = {1, 3, 5, 7, 9};
  auto it = s.lower_bound(5);
  EXPECT_EQ(*it, 5);
}

TEST(Set, LowerBoundBetweenElements) {
  set<int> s = {1, 3, 7, 9};
  auto it = s.lower_bound(4);
  EXPECT_EQ(*it, 7);  // first >= 4
}

TEST(Set, LowerBoundLessThanAll) {
  set<int> s = {5, 10, 15};
  auto it = s.lower_bound(0);
  EXPECT_EQ(*it, 5);
}

TEST(Set, LowerBoundGreaterThanAll) {
  set<int> s = {5, 10, 15};
  auto it = s.lower_bound(20);
  EXPECT_EQ(it, s.end());
}

TEST(Set, UpperBoundExact) {
  set<int> s = {1, 3, 5, 7, 9};
  auto it = s.upper_bound(5);
  EXPECT_EQ(*it, 7);  // first > 5
}

TEST(Set, UpperBoundGreaterThanAll) {
  set<int> s = {5, 10};
  auto it = s.upper_bound(20);
  EXPECT_EQ(it, s.end());
}

TEST(Set, EqualRangePresent) {
  set<int> s = {1, 3, 5, 7, 9};
  auto range = s.equal_range(5);
  EXPECT_EQ(*range.first, 5);   // lower_bound
  EXPECT_EQ(*range.second, 7);  // upper_bound
}

TEST(Set, EqualRangeAbsent) {
  set<int> s = {1, 3, 7, 9};
  auto range = s.equal_range(5);
  // When key is absent, lower_bound == upper_bound
  EXPECT_EQ(range.first, range.second);
  EXPECT_EQ(*range.first, 7);
}

// ============================================================================
// iterators: begin()/end(), rbegin()/rend()
// ============================================================================

TEST(Set, BeginEndIteration) {
  set<int> s = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3};
  int prev = 0;
  for (auto it = s.begin(); it != s.end(); ++it) {
    EXPECT_GT(*it, prev);
    prev = *it;
  }
}

TEST(Set, ConstBeginEnd) {
  const set<int> s = {8, 3, 11, 1};
  int expected[] = {1, 3, 8, 11};
  int idx = 0;
  for (auto it = s.begin(); it != s.end(); ++it) {
    EXPECT_EQ(*it, expected[idx++]);
  }
}

TEST(Set, CbeginCend) {
  set<int> s = {5, 2, 8, 1};
  int expected[] = {1, 2, 5, 8};
  int idx = 0;
  for (auto it = s.cbegin(); it != s.cend(); ++it) {
    EXPECT_EQ(*it, expected[idx++]);
  }
}

TEST(Set, ReverseIteration) {
  set<int> s = {1, 2, 3, 4, 5};
  int expected[] = {5, 4, 3, 2, 1};
  int idx = 0;
  for (auto rit = s.rbegin(); rit != s.rend(); ++rit) {
    EXPECT_EQ(*rit, expected[idx++]);
  }
}

TEST(Set, ConstReverseIteration) {
  const set<int> s = {10, 20, 30};
  int expected[] = {30, 20, 10};
  int idx = 0;
  for (auto rit = s.crbegin(); rit != s.crend(); ++rit) {
    EXPECT_EQ(*rit, expected[idx++]);
  }
}

TEST(Set, EmptyIterator) {
  set<int> s;
  EXPECT_EQ(s.begin(), s.end());
  EXPECT_EQ(s.rbegin(), s.rend());
}

// ============================================================================
// empty(), size(), max_size()
// ============================================================================

TEST(Set, EmptyAndSize) {
  set<int> s;
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);

  s.insert(1);
  EXPECT_FALSE(s.empty());
  EXPECT_EQ(s.size(), 1u);

  s.insert(2);
  EXPECT_EQ(s.size(), 2u);

  s.clear();
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);
}

TEST(Set, MaxSize) {
  set<int> s;
  EXPECT_GT(s.max_size(), 0u);
}

// ============================================================================
// clear(), swap(), merge()
// ============================================================================

TEST(Set, Clear) {
  set<int> s = {1, 2, 3, 4, 5};
  s.clear();
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);
  EXPECT_EQ(s.find(3), s.end());
}

TEST(Set, ClearThenInsert) {
  set<int> s = {10, 20};
  s.clear();
  s.insert(30);
  EXPECT_EQ(s.size(), 1u);
  EXPECT_TRUE(s.contains(30));
}

TEST(Set, Swap) {
  set<int> a = {1, 2, 3};
  set<int> b = {4, 5, 6, 7};

  a.swap(b);

  EXPECT_EQ(a.size(), 4u);
  EXPECT_EQ(b.size(), 3u);
  EXPECT_TRUE(a.contains(4));
  EXPECT_FALSE(a.contains(1));
  EXPECT_TRUE(b.contains(1));
  EXPECT_FALSE(b.contains(7));
}

TEST(Set, FreeFunctionSwap) {
  set<int> a = {10};
  set<int> b = {20, 30};
  zstl::swap(a, b);
  EXPECT_EQ(a.size(), 2u);
  EXPECT_EQ(b.size(), 1u);
}

TEST(Set, Merge) {
  set<int> a = {1, 3, 5};
  set<int> b = {2, 3, 4, 6};

  a.merge(b);

  EXPECT_EQ(a.size(), 6u);  // 1,2,3,4,5,6
  EXPECT_EQ(b.size(), 1u);  // 3 stays (duplicate)
  for (int x : {1, 2, 3, 4, 5, 6}) EXPECT_TRUE(a.contains(x));
  EXPECT_TRUE(b.contains(3));  // duplicate remains in source
}

TEST(Set, MergeEmptySource) {
  set<int> a = {1, 2, 3};
  set<int> b;
  a.merge(b);
  EXPECT_EQ(a.size(), 3u);
  EXPECT_TRUE(b.empty());
}

TEST(Set, MergeEmptyTarget) {
  set<int> a;
  set<int> b = {10, 20};
  a.merge(b);
  EXPECT_EQ(a.size(), 2u);
  EXPECT_TRUE(b.empty());
}

// ============================================================================
// key_comp(), value_comp()
// ============================================================================

TEST(Set, KeyComp) {
  set<int> s;
  auto comp = s.key_comp();
  EXPECT_TRUE(comp(1, 5));
  EXPECT_FALSE(comp(5, 1));
  EXPECT_FALSE(comp(3, 3));
}

TEST(Set, ValueComp) {
  set<int> s;
  auto comp = s.value_comp();
  EXPECT_TRUE(comp(10, 20));
  EXPECT_FALSE(comp(20, 10));
}

// ============================================================================
// Custom comparator
// ============================================================================

TEST(Set, CustomComparatorGreater) {
  set<int, greater<int>> s;
  s.insert(1);
  s.insert(5);
  s.insert(3);
  s.insert(4);
  s.insert(2);

  // Descending order
  int expected[] = {5, 4, 3, 2, 1};
  int idx = 0;
  for (int v : s) {
    EXPECT_EQ(v, expected[idx++]);
  }
}

TEST(Set, CustomComparatorFind) {
  set<int, greater<int>> s = {10, 30, 20, 50, 40};
  auto it = s.find(30);
  EXPECT_NE(it, s.end());
  EXPECT_EQ(*it, 30);
}

TEST(Set, CustomComparatorLowerBound) {
  set<int, greater<int>> s = {50, 40, 30, 20, 10};
  // With greater<int>, lower_bound(35) = first elem <= 35
  auto it = s.lower_bound(35);
  EXPECT_EQ(*it, 30);
}

// ============================================================================
// Comparison operators
// ============================================================================

TEST(Set, Equality) {
  set<int> a = {1, 2, 3};
  set<int> b = {1, 2, 3};
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
}

TEST(Set, InequalityDifferentValues) {
  set<int> a = {1, 2, 3};
  set<int> b = {1, 2, 4};
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
}

TEST(Set, InequalityDifferentSize) {
  set<int> a = {1, 2};
  set<int> b = {1, 2, 3};
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
}

TEST(Set, LessThan) {
  set<int> a = {1, 2};
  set<int> b = {1, 2, 3};
  EXPECT_TRUE(a < b);

  set<int> c = {1, 3};
  EXPECT_TRUE(a < c);
}

TEST(Set, GreaterThan) {
  set<int> a = {1, 2, 3};
  set<int> b = {1, 2};
  EXPECT_TRUE(a > b);
}

TEST(Set, LessEqualGreaterEqual) {
  set<int> a = {1, 2};
  set<int> b = {1, 2};
  EXPECT_TRUE(a <= b);
  EXPECT_TRUE(a >= b);

  set<int> c = {1, 3};
  EXPECT_TRUE(a <= c);
  EXPECT_FALSE(a >= c);
  EXPECT_TRUE(c >= a);
  EXPECT_FALSE(c <= a);
}

// ============================================================================
// Verify sorted iteration and uniqueness
// ============================================================================

TEST(Set, SortedIterationOrder) {
  set<int> s;
  // Insert in pseudo-random order
  int values[] = {42, 17, 88, 3, 56, 24, 99, 1, 33, 75};
  for (int v : values) s.insert(v);

  int prev = 0;
  int count = 0;
  for (int v : s) {
    EXPECT_GT(v, prev);
    prev = v;
    ++count;
  }
  EXPECT_EQ(count, 10);
}

TEST(Set, SortedReverseIteration) {
  set<int> s = {4, 1, 7, 3, 9, 2, 5, 8, 6};

  int prev = 999999;
  int count = 0;
  for (auto rit = s.rbegin(); rit != s.rend(); ++rit) {
    EXPECT_LT(*rit, prev);
    prev = *rit;
    ++count;
  }
  EXPECT_EQ(count, 9);
}

TEST(Set, UniquenessEnforced) {
  set<int> s;
  for (int i = 0; i < 10; ++i) {
    s.insert(5);  // same value 10 times
  }
  EXPECT_EQ(s.size(), 1u);
}

TEST(Set, UniquenessWithRangeInsert) {
  set<int> s;
  std::vector<int> v = {1, 1, 1, 2, 2, 3, 3, 3, 3};
  s.insert(v.begin(), v.end());
  EXPECT_EQ(s.size(), 3u);
}

// ============================================================================
// Verify ascending order through full iteration
// ============================================================================

TEST(Set, AscendingOrderLarge) {
  set<int> s;
  const int N = 200;

  // Insert even numbers in reverse order, odd numbers forward
  for (int i = N; i >= 0; i -= 2) s.insert(i);
  for (int i = 1; i < N; i += 2) s.insert(i);

  int expected = 0;
  for (int v : s) {
    EXPECT_EQ(v, expected++);
  }
  EXPECT_EQ(expected, N + 1);
}

TEST(Set, RangeBasedForLoop) {
  set<int> s = {5, 3, 8, 1, 9, 2, 7, 4, 6};
  int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  int idx = 0;
  for (int v : s) {
    EXPECT_EQ(v, expected[idx++]);
  }
}

// ============================================================================
// get_allocator()
// ============================================================================

TEST(Set, GetAllocator) {
  set<int> s;
  auto alloc = s.get_allocator();
  EXPECT_TRUE(noexcept(s.get_allocator()));
  (void)alloc;
}

// ============================================================================
// Mixed operations / stress
// ============================================================================

TEST(Set, MixedInsertErase) {
  set<int> s;

  // Build up
  for (int i = 0; i < 100; ++i) s.insert(i);
  EXPECT_EQ(s.size(), 100u);

  // Tear down half
  for (int i = 0; i < 50; ++i) s.erase(i);
  EXPECT_EQ(s.size(), 50u);

  // Verify remaining
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(s.contains(i), i >= 50);
  }
}

TEST(Set, EraseAllOneByOne) {
  set<int> s;
  for (int i = 0; i < 50; ++i) s.insert(i);
  EXPECT_EQ(s.size(), 50u);

  for (int i = 0; i < 50; ++i) {
    s.erase(i);
  }
  EXPECT_TRUE(s.empty());
}

TEST(Set, ManyCycles) {
  set<int> s;
  for (int cycle = 0; cycle < 5; ++cycle) {
    for (int i = 0; i < 30; ++i) s.insert(i + cycle * 100);
    for (int i = 0; i < 15; ++i) s.erase(i + cycle * 100);
  }
  // Should be 30 - 15 = 15 elements per cycle * 5 = 75
  EXPECT_EQ(s.size(), 75u);
}

TEST(Set, InsertNegatives) {
  set<int> s = {-5, -1, -3, -10, -7};
  EXPECT_EQ(s.size(), 5u);
  auto it = s.begin();
  EXPECT_EQ(*it, -10); ++it;
  EXPECT_EQ(*it, -7);  ++it;
  EXPECT_EQ(*it, -5);  ++it;
  EXPECT_EQ(*it, -3);  ++it;
  EXPECT_EQ(*it, -1);
}
