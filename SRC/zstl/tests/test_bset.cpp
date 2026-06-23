// ============================================================================
// zstl bset unit tests — B+ tree backed ordered set
// ============================================================================
// Tests for: constructors, insert, emplace, emplace_hint, erase,
//           find, count, contains, lower_bound, upper_bound, equal_range,
//           begin/end iteration, reverse iteration,
//           empty, size, clear, max_size, swap, get_allocator,
//           key_comp, value_comp, merge, comparison operators,
//           large dataset, uniqueness, sorted-order verification.
// Covers: empty, single, many elements, duplicates rejected, edge cases.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>
#include <vector>
#include <algorithm>
#include <random>

using namespace zstl;

// ============================================================================
// Helper: verify a bset is sorted
// ============================================================================
template<typename Set>
void verifySortedOrder(const Set& s) {
  if (s.empty()) return;
  auto it = s.begin();
  auto prev = *it;
  ++it;
  for (; it != s.end(); ++it) {
    EXPECT_LE(prev, *it) << "Elements not in sorted order: "
                         << prev << " > " << *it;
    prev = *it;
  }
}

// ============================================================================
// Constructors
// ============================================================================

TEST(Bset, DefaultConstructor) {
  bset<int> s;
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);
}

TEST(Bset, ComparatorConstructor) {
  bset<int, greater<int>> s;
  s.insert(5);
  s.insert(1);
  s.insert(3);
  // With greater<int>, elements should be in descending order
  auto it = s.begin();
  EXPECT_EQ(*it, 5);
  ++it;
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(*it, 1);
}

TEST(Bset, InitializerListConstructor) {
  bset<int> s = {5, 1, 4, 2, 3};
  EXPECT_EQ(s.size(), 5u);
  verifySortedOrder(s);
}

TEST(Bset, RangeConstructor) {
  std::vector<int> vec = {9, 2, 7, 1, 5, 3};
  bset<int> s(vec.begin(), vec.end());
  EXPECT_EQ(s.size(), 6u);
  verifySortedOrder(s);
}

TEST(Bset, CopyConstructor) {
  bset<int> s1 = {3, 1, 2};
  bset<int> s2(s1);
  EXPECT_EQ(s2.size(), 3u);
  EXPECT_TRUE(s1 == s2);
}

TEST(Bset, MoveConstructor) {
  bset<int> s1 = {1, 2, 3, 4, 5};
  bset<int> s2(zstl::move(s1));
  EXPECT_EQ(s2.size(), 5u);
  EXPECT_TRUE(s1.empty());
}

TEST(Bset, CopyAssignment) {
  bset<int> s1 = {10, 20, 30};
  bset<int> s2;
  s2 = s1;
  EXPECT_EQ(s2.size(), 3u);
  EXPECT_TRUE(s2.contains(10));
  EXPECT_TRUE(s2.contains(20));
  EXPECT_TRUE(s2.contains(30));
}

TEST(Bset, MoveAssignment) {
  bset<int> s1 = {7, 8, 9};
  bset<int> s2;
  s2 = zstl::move(s1);
  EXPECT_EQ(s2.size(), 3u);
  EXPECT_TRUE(s1.empty());
}

TEST(Bset, InitializerListAssignment) {
  bset<int> s;
  s = {100, 200, 300, 50, 150};
  EXPECT_EQ(s.size(), 5u);
  verifySortedOrder(s);
}

// ============================================================================
// insert
// ============================================================================

TEST(Bset, InsertSingle) {
  bset<int> s;
  auto result = s.insert(42);
  EXPECT_TRUE(result.second);
  EXPECT_EQ(*result.first, 42);
  EXPECT_EQ(s.size(), 1u);
}

TEST(Bset, InsertDuplicate) {
  bset<int> s;
  auto r1 = s.insert(10);
  EXPECT_TRUE(r1.second);

  auto r2 = s.insert(10);
  EXPECT_FALSE(r2.second);
  EXPECT_EQ(*r2.first, 10);
  EXPECT_EQ(s.size(), 1u);  // still 1
}

TEST(Bset, InsertMultiple) {
  bset<int> s;
  for (int i = 99; i >= 0; --i) {
    auto result = s.insert(i);
    EXPECT_TRUE(result.second);
  }
  EXPECT_EQ(s.size(), 100u);
  verifySortedOrder(s);
}

TEST(Bset, InsertMixedWithDuplicates) {
  bset<int> s;
  s.insert(5);
  s.insert(3);
  s.insert(5);  // duplicate — rejected
  s.insert(7);
  s.insert(3);  // duplicate — rejected
  s.insert(1);

  EXPECT_EQ(s.size(), 4u);  // only {1, 3, 5, 7}
  verifySortedOrder(s);
}

TEST(Bset, InsertHint) {
  bset<int> s = {1, 5, 10};
  auto hint = s.find(5);
  auto it = s.insert(hint, 7);
  EXPECT_EQ(*it, 7);
  EXPECT_EQ(s.size(), 4u);
  verifySortedOrder(s);
}

TEST(Bset, InsertRange) {
  std::vector<int> vec;
  for (int i = 0; i < 50; ++i) vec.push_back(i);
  std::mt19937 rng(42);
  std::shuffle(vec.begin(), vec.end(), rng);

  bset<int> s;
  s.insert(vec.begin(), vec.end());
  EXPECT_EQ(s.size(), 50u);
  verifySortedOrder(s);
}

// ============================================================================
// emplace and emplace_hint
// ============================================================================

TEST(Bset, EmplaceNewElement) {
  bset<int> s;
  auto result = s.emplace(88);
  EXPECT_TRUE(result.second);
  EXPECT_EQ(*result.first, 88);
  EXPECT_EQ(s.size(), 1u);
}

TEST(Bset, EmplaceDuplicate) {
  bset<int> s = {42};
  auto result = s.emplace(42);
  EXPECT_FALSE(result.second);
  EXPECT_EQ(*result.first, 42);
  EXPECT_EQ(s.size(), 1u);
}

TEST(Bset, EmplaceMultiple) {
  bset<int> s;
  for (int i = 0; i < 50; ++i) {
    s.emplace(i * 2);
  }
  EXPECT_EQ(s.size(), 50u);
  verifySortedOrder(s);
}

TEST(Bset, EmplaceHint) {
  bset<int> s = {1, 10};
  auto hint = s.find(1);
  auto it = s.emplace_hint(hint, 5);
  EXPECT_EQ(*it, 5);
  EXPECT_EQ(s.size(), 3u);
  verifySortedOrder(s);
}

// ============================================================================
// erase
// ============================================================================

TEST(Bset, EraseByKey) {
  bset<int> s = {1, 2, 3, 4, 5};
  size_t erased = s.erase(3);
  EXPECT_EQ(erased, 1u);
  EXPECT_EQ(s.size(), 4u);
  EXPECT_FALSE(s.contains(3));
}

TEST(Bset, EraseNonExistent) {
  bset<int> s = {1, 2, 3};
  size_t erased = s.erase(999);
  EXPECT_EQ(erased, 0u);
  EXPECT_EQ(s.size(), 3u);
}

TEST(Bset, EraseByIterator) {
  bset<int> s = {10, 20, 30};
  auto it = s.find(20);
  auto next = s.erase(it);
  EXPECT_EQ(*next, 30);
  EXPECT_EQ(s.size(), 2u);
  EXPECT_FALSE(s.contains(20));
}

TEST(Bset, EraseRange) {
  bset<int> s;
  for (int i = 0; i < 50; ++i) s.insert(i);
  // Erase [10, 30)
  auto first = s.lower_bound(10);
  auto last = s.lower_bound(30);
  auto it = s.erase(first, last);
  EXPECT_EQ(s.size(), 30u);  // 50 - 20 removed
  EXPECT_FALSE(s.contains(10));
  EXPECT_FALSE(s.contains(20));
  EXPECT_TRUE(s.contains(0));
  EXPECT_TRUE(s.contains(30));
  EXPECT_TRUE(s.contains(49));
  verifySortedOrder(s);
}

TEST(Bset, EraseLastElement) {
  bset<int> s = {1};
  s.erase(1);
  EXPECT_TRUE(s.empty());
}

// ============================================================================
// find, count, contains
// ============================================================================

TEST(Bset, FindExisting) {
  bset<int> s = {5, 10, 15, 20};
  auto it = s.find(10);
  EXPECT_NE(it, s.end());
  EXPECT_EQ(*it, 10);
}

TEST(Bset, FindNonExisting) {
  bset<int> s = {1, 2, 3};
  auto it = s.find(999);
  EXPECT_EQ(it, s.end());
}

TEST(Bset, FindConst) {
  const bset<int> s = {42, 84};
  auto it = s.find(42);
  EXPECT_NE(it, s.end());
  EXPECT_EQ(*it, 42);
}

TEST(Bset, Count) {
  bset<int> s = {1, 2, 3, 4};
  EXPECT_EQ(s.count(1), 1u);
  EXPECT_EQ(s.count(2), 1u);
  EXPECT_EQ(s.count(999), 0u);
}

TEST(Bset, Contains) {
  bset<int> s = {10, 20, 30};
  EXPECT_TRUE(s.contains(10));
  EXPECT_TRUE(s.contains(20));
  EXPECT_TRUE(s.contains(30));
  EXPECT_FALSE(s.contains(0));
  EXPECT_FALSE(s.contains(40));
}

// ============================================================================
// lower_bound, upper_bound, equal_range
// ============================================================================

TEST(Bset, LowerBoundExact) {
  bset<int> s = {1, 3, 5, 7, 9};
  auto it = s.lower_bound(5);
  EXPECT_EQ(*it, 5);
}

TEST(Bset, LowerBoundBetween) {
  bset<int> s = {10, 20, 30};
  auto it = s.lower_bound(15);
  EXPECT_EQ(*it, 20);  // first >= 15
}

TEST(Bset, LowerBoundPastEnd) {
  bset<int> s = {1, 2};
  auto it = s.lower_bound(100);
  EXPECT_EQ(it, s.end());
}

TEST(Bset, UpperBoundExact) {
  bset<int> s = {1, 3, 5, 7, 9};
  auto it = s.upper_bound(5);
  EXPECT_EQ(*it, 7);  // first > 5
}

TEST(Bset, UpperBoundPastEnd) {
  bset<int> s = {1, 2};
  auto it = s.upper_bound(100);
  EXPECT_EQ(it, s.end());
}

TEST(Bset, EqualRangeSingle) {
  bset<int> s = {1, 2, 3, 4, 5};
  auto range = s.equal_range(3);
  EXPECT_NE(range.first, s.end());
  EXPECT_EQ(*range.first, 3);
  // Since unique, range should be [find(3), next after 3)
  auto next = range.first;
  ++next;
  EXPECT_EQ(range.second, next);
}

TEST(Bset, EqualRangeMissing) {
  bset<int> s = {1, 3, 5};
  auto range = s.equal_range(2);
  EXPECT_EQ(range.first, range.second);
}

TEST(Bset, EqualRangeConst) {
  const bset<int> s = {10, 20, 30};
  auto range = s.equal_range(20);
  EXPECT_NE(range.first, s.end());
  EXPECT_EQ(*range.first, 20);
}

// ============================================================================
// Iteration: begin/end — verify sorted order
// ============================================================================

TEST(Bset, IterationSortedOrder) {
  bset<int> s;
  // Insert reverse order
  for (int i = 99; i >= 0; --i) {
    s.insert(i);
  }
  EXPECT_EQ(s.size(), 100u);

  int expected = 0;
  for (int val : s) {
    EXPECT_EQ(val, expected);
    ++expected;
  }
  EXPECT_EQ(expected, 100);
}

TEST(Bset, IteratorBeginEnd) {
  bset<int> s = {42};
  auto it = s.begin();
  EXPECT_NE(it, s.end());
  EXPECT_EQ(*it, 42);
  ++it;
  EXPECT_EQ(it, s.end());
}

TEST(Bset, IteratorEmpty) {
  bset<int> s;
  EXPECT_EQ(s.begin(), s.end());
}

TEST(Bset, CbeginCend) {
  const bset<int> s = {5, 3, 1, 4, 2};
  int count = 0;
  int prev = -1;
  for (auto it = s.cbegin(); it != s.cend(); ++it) {
    EXPECT_GT(*it, prev);  // sorted
    prev = *it;
    ++count;
  }
  EXPECT_EQ(count, 5);
}

// ============================================================================
// Reverse iteration
// ============================================================================

TEST(Bset, ReverseIteration) {
  bset<int> s;
  for (int i = 0; i < 10; ++i) s.insert(i);
  // Reverse: 9..0
  int expected = 9;
  for (auto it = s.rbegin(); it != s.rend(); ++it) {
    EXPECT_EQ(*it, expected);
    --expected;
  }
  EXPECT_EQ(expected, -1);
}

TEST(Bset, ConstReverseIteration) {
  const bset<int> s = {1, 4, 9};
  auto it = s.crbegin();
  EXPECT_EQ(*it, 9);
  ++it;
  EXPECT_EQ(*it, 4);
  ++it;
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(it, s.crend());
}

// ============================================================================
// empty, size, clear, max_size
// ============================================================================

TEST(Bset, EmptySizeClear) {
  bset<int> s;
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);

  s.insert(1);
  s.insert(2);
  s.insert(3);
  EXPECT_FALSE(s.empty());
  EXPECT_EQ(s.size(), 3u);

  s.clear();
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);
}

TEST(Bset, MaxSize) {
  bset<int> s;
  EXPECT_GT(s.max_size(), 0u);
}

// ============================================================================
// swap
// ============================================================================

TEST(Bset, SwapMember) {
  bset<int> s1 = {1, 2, 3};
  bset<int> s2 = {10, 20};

  s1.swap(s2);
  EXPECT_EQ(s1.size(), 2u);
  EXPECT_EQ(s2.size(), 3u);
  EXPECT_TRUE(s1.contains(10));
  EXPECT_TRUE(s1.contains(20));
  EXPECT_TRUE(s2.contains(1));
  EXPECT_TRUE(s2.contains(2));
  EXPECT_TRUE(s2.contains(3));
  verifySortedOrder(s1);
  verifySortedOrder(s2);
}

TEST(Bset, SwapFreeFunction) {
  bset<int> s1 = {5};
  bset<int> s2 = {6, 7, 8};
  zstl::swap(s1, s2);
  EXPECT_EQ(s1.size(), 3u);
  EXPECT_EQ(s2.size(), 1u);
}

// ============================================================================
// get_allocator
// ============================================================================

TEST(Bset, GetAllocator) {
  bset<int> s;
  auto alloc = s.get_allocator();
  SUCCEED() << "get_allocator() returned successfully";
}

// ============================================================================
// key_comp and value_comp
// ============================================================================

TEST(Bset, KeyComp) {
  bset<int> s;
  auto kc = s.key_comp();
  EXPECT_TRUE(kc(1, 2));
  EXPECT_FALSE(kc(2, 1));
  EXPECT_FALSE(kc(5, 5));
}

TEST(Bset, ValueComp) {
  bset<int> s;
  auto vc = s.value_comp();
  EXPECT_TRUE(vc(1, 2));
  EXPECT_FALSE(vc(2, 1));
  EXPECT_FALSE(vc(5, 5));
}

// ============================================================================
// merge
// ============================================================================

TEST(Bset, Merge) {
  bset<int> src = {1, 3, 5};
  bset<int> dst = {2, 3, 4};

  dst.merge(src);
  // Key 3 in src should not be merged (already in dst)
  EXPECT_EQ(dst.size(), 5u);
  EXPECT_EQ(src.size(), 1u);  // only 3 remains
  EXPECT_TRUE(src.contains(3));
  EXPECT_TRUE(dst.contains(1));
  EXPECT_TRUE(dst.contains(2));
  EXPECT_TRUE(dst.contains(3));
  EXPECT_TRUE(dst.contains(4));
  EXPECT_TRUE(dst.contains(5));
  verifySortedOrder(dst);
}

// ============================================================================
// Comparison operators
// ============================================================================

TEST(Bset, Equality) {
  bset<int> s1 = {1, 2, 3};
  bset<int> s2 = {1, 2, 3};
  bset<int> s3 = {1, 2};
  bset<int> s4 = {1, 2, 4};

  EXPECT_TRUE(s1 == s2);
  EXPECT_FALSE(s1 != s2);
  EXPECT_FALSE(s1 == s3);
  EXPECT_TRUE(s1 != s3);
  EXPECT_FALSE(s1 == s4);
  EXPECT_TRUE(s1 != s4);
}

TEST(Bset, EqualityEmpty) {
  bset<int> s1, s2;
  EXPECT_TRUE(s1 == s2);
}

TEST(Bset, LessThan) {
  bset<int> s1 = {1, 2};
  bset<int> s2 = {1, 3};
  bset<int> s3 = {1, 2};

  EXPECT_TRUE(s1 < s2);
  EXPECT_FALSE(s2 < s1);
  EXPECT_FALSE(s1 < s3);
  EXPECT_TRUE(s1 <= s3);
  EXPECT_TRUE(s2 > s1);
  EXPECT_TRUE(s2 >= s1);
}

// ============================================================================
// Verify uniqueness
// ============================================================================

TEST(Bset, Uniqueness) {
  bset<int> s;
  s.insert(1);
  s.insert(1);
  s.insert(1);
  s.insert(2);
  s.insert(2);
  s.insert(3);

  EXPECT_EQ(s.size(), 3u);  // only {1, 2, 3}
  EXPECT_EQ(s.count(1), 1u);
  EXPECT_EQ(s.count(2), 1u);
  EXPECT_EQ(s.count(3), 1u);
}

TEST(Bset, UniquenessWithEmplace) {
  bset<int> s;
  for (int i = 0; i < 20; ++i) {
    s.emplace(i % 5);
  }
  EXPECT_EQ(s.size(), 5u);  // only {0, 1, 2, 3, 4}
}

// ============================================================================
// Large dataset
// ============================================================================

TEST(Bset, Large1000InsertAndIterate) {
  bset<int> s;
  const int N = 1000;
  std::vector<int> keys(N);
  for (int i = 0; i < N; ++i) keys[i] = i;
  std::mt19937 rng(42);
  std::shuffle(keys.begin(), keys.end(), rng);

  for (int k : keys) {
    s.insert(k);
  }

  EXPECT_EQ(s.size(), static_cast<size_t>(N));
  verifySortedOrder(s);

  int expected = 0;
  for (int val : s) {
    EXPECT_EQ(val, expected);
    ++expected;
  }
  EXPECT_EQ(expected, N);
}

TEST(Bset, Large5000RandomInsert) {
  bset<int> s;
  const int N = 5000;
  std::vector<int> keys(N);
  for (int i = 0; i < N; ++i) keys[i] = i * 2;  // even numbers
  std::mt19937 rng(73);
  std::shuffle(keys.begin(), keys.end(), rng);

  for (int k : keys) {
    s.insert(k);
  }

  EXPECT_EQ(s.size(), static_cast<size_t>(N));
  verifySortedOrder(s);
}

TEST(Bset, LargeInterleavedInsertErase) {
  bset<int> s;
  const int N = 500;

  for (int i = 0; i < N; ++i) s.insert(i);
  EXPECT_EQ(s.size(), static_cast<size_t>(N));

  // Erase every other
  for (int i = 0; i < N; i += 2) {
    s.erase(i);
  }
  EXPECT_EQ(s.size(), static_cast<size_t>(N / 2));

  verifySortedOrder(s);
  for (int val : s) {
    EXPECT_EQ(val % 2, 1);  // only odd remain
  }
}

TEST(Bset, LargeAllErased) {
  bset<int> s;
  for (int i = 0; i < 200; ++i) s.insert(i);
  for (int i = 0; i < 200; ++i) s.erase(i);
  EXPECT_TRUE(s.empty());
}

// ============================================================================
// String set
// ============================================================================

TEST(Bset, StringSet) {
  bset<std::string> s;
  s.insert("cherry");
  s.insert("apple");
  s.insert("banana");
  s.insert("aardvark");

  EXPECT_EQ(s.size(), 4u);

  auto it = s.begin();
  EXPECT_EQ(*it, "aardvark");
  ++it;
  EXPECT_EQ(*it, "apple");
  ++it;
  EXPECT_EQ(*it, "banana");
  ++it;
  EXPECT_EQ(*it, "cherry");
}

// ============================================================================
// Insert after clear
// ============================================================================

TEST(Bset, InsertAfterClear) {
  bset<int> s;
  s.insert(1);
  s.insert(2);
  s.clear();
  EXPECT_TRUE(s.empty());

  s.insert(3);
  s.insert(4);
  EXPECT_EQ(s.size(), 2u);
  verifySortedOrder(s);
}

// ============================================================================
// Range queries on large set
// ============================================================================

TEST(Bset, RangeQuery) {
  bset<int> s;
  for (int i = 0; i < 200; ++i) s.insert(i);

  // Query [50, 100)
  auto start = s.lower_bound(50);
  auto end = s.upper_bound(99);
  int count = 0;
  for (auto it = start; it != end; ++it) {
    EXPECT_GE(*it, 50);
    EXPECT_LE(*it, 99);
    ++count;
  }
  EXPECT_EQ(count, 50);
}
