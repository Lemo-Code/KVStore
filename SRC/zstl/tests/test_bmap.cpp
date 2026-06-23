// ============================================================================
// zstl bmap unit tests — B+ tree backed ordered map
// ============================================================================
// Tests for: constructors, operator[], at, insert, emplace, emplace_hint,
//           insert_or_assign, erase, find, count, contains,
//           lower_bound, upper_bound, equal_range, begin/end iteration,
//           reverse iteration, empty, size, clear, max_size, swap,
//           get_allocator, key_comp, value_comp, merge,
//           comparison operators, large dataset, range queries,
//           sorted-order verification, hint insert.
// Covers: empty, single, many elements, duplicates attempted, edge cases.
//
// NOTE: try_emplace is not tested — zstl::forward_as_tuple not defined yet.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>
#include <vector>
#include <algorithm>
#include <random>

using namespace zstl;

// ============================================================================
// Helper: verify a bmap is sorted by key
// ============================================================================
template<typename Map>
void verifySortedOrder(const Map& m) {
  if (m.empty()) return;
  auto it = m.begin();
  auto prev_key = it->first;
  ++it;
  for (; it != m.end(); ++it) {
    EXPECT_LE(prev_key, it->first) << "Keys not in sorted order: "
                                   << prev_key << " > " << it->first;
    prev_key = it->first;
  }
}

// ============================================================================
// Constructors
// ============================================================================

TEST(Bmap, DefaultConstructor) {
  bmap<int, std::string> m;
  EXPECT_TRUE(m.empty());
  EXPECT_EQ(m.size(), 0u);
}

TEST(Bmap, ComparatorConstructor) {
  bmap<int, std::string, greater<int>> m;
  m.insert({5, "five"});
  m.insert({1, "one"});
  m.insert({3, "three"});
  // With greater<int>, keys should be in descending order
  auto it = m.begin();
  EXPECT_EQ(it->first, 5);
  ++it;
  EXPECT_EQ(it->first, 3);
  ++it;
  EXPECT_EQ(it->first, 1);
}

TEST(Bmap, InitializerListConstructor) {
  bmap<int, std::string> m = {
    {3, "three"}, {1, "one"}, {4, "four"}, {2, "two"}
  };
  EXPECT_EQ(m.size(), 4u);
  verifySortedOrder(m);
}

TEST(Bmap, RangeConstructor) {
  std::vector<pair<int, std::string>> vec = {
    {5, "five"}, {2, "two"}, {8, "eight"}, {1, "one"}, {7, "seven"}
  };
  bmap<int, std::string> m(vec.begin(), vec.end());
  EXPECT_EQ(m.size(), 5u);
  verifySortedOrder(m);
}

TEST(Bmap, CopyConstructor) {
  bmap<int, std::string> m1 = {{3, "c"}, {1, "a"}, {2, "b"}};
  bmap<int, std::string> m2(m1);
  EXPECT_EQ(m2.size(), 3u);
  EXPECT_TRUE(m1 == m2);
  verifySortedOrder(m2);
}

TEST(Bmap, MoveConstructor) {
  bmap<int, std::string> m1 = {{1, "a"}, {2, "b"}, {3, "c"}};
  bmap<int, std::string> m2(zstl::move(m1));
  EXPECT_EQ(m2.size(), 3u);
  EXPECT_TRUE(m1.empty());
  verifySortedOrder(m2);
}

TEST(Bmap, CopyAssignment) {
  bmap<int, std::string> m1 = {{10, "x"}, {20, "y"}};
  bmap<int, std::string> m2;
  m2 = m1;
  EXPECT_EQ(m2.size(), 2u);
  EXPECT_EQ(m2[10], "x");
  EXPECT_EQ(m2[20], "y");
}

TEST(Bmap, MoveAssignment) {
  bmap<int, std::string> m1 = {{1, "a"}, {2, "b"}};
  bmap<int, std::string> m2;
  m2 = zstl::move(m1);
  EXPECT_EQ(m2.size(), 2u);
  EXPECT_TRUE(m1.empty());
}

TEST(Bmap, InitializerListAssignment) {
  bmap<int, std::string> m;
  m = {{100, "hundred"}, {200, "two hundred"}, {50, "fifty"}};
  EXPECT_EQ(m.size(), 3u);
  verifySortedOrder(m);
}

// ============================================================================
// operator[]
// ============================================================================

TEST(Bmap, OperatorBracketInsert) {
  bmap<int, std::string> m;
  m[1] = "one";
  m[2] = "two";
  m[3] = "three";

  EXPECT_EQ(m.size(), 3u);
  EXPECT_EQ(m[1], "one");
  EXPECT_EQ(m[2], "two");
  EXPECT_EQ(m[3], "three");
}

TEST(Bmap, OperatorBracketOverwrite) {
  bmap<int, std::string> m;
  m[5] = "first";
  EXPECT_EQ(m[5], "first");

  m[5] = "second";
  EXPECT_EQ(m[5], "second");
  EXPECT_EQ(m.size(), 1u);
}

TEST(Bmap, OperatorBracketDefaultConstruct) {
  bmap<int, std::string> m;
  // operator[] on a non-existent key default-constructs the value
  std::string& s = m[42];
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(m.size(), 1u);
}

TEST(Bmap, OperatorBracketMoveKey) {
  bmap<int, std::string> m;
  int k = 77;
  m[zstl::move(k)] = "seventy seven";
  EXPECT_EQ(m.size(), 1u);
  EXPECT_EQ(m[77], "seventy seven");
}

// ============================================================================
// at()
// ============================================================================

TEST(Bmap, AtExisting) {
  bmap<int, std::string> m = {{1, "alpha"}, {2, "beta"}, {3, "gamma"}};
  EXPECT_EQ(m.at(1), "alpha");
  EXPECT_EQ(m.at(2), "beta");
  EXPECT_EQ(m.at(3), "gamma");
}

TEST(Bmap, AtOutOfRange) {
  bmap<int, std::string> m = {{1, "a"}};
  EXPECT_THROW(m.at(999), std::out_of_range);
}

TEST(Bmap, AtConst) {
  const bmap<int, std::string> m = {{10, "ten"}, {20, "twenty"}};
  EXPECT_EQ(m.at(10), "ten");
  EXPECT_EQ(m.at(20), "twenty");
  EXPECT_THROW(m.at(0), std::out_of_range);
}

TEST(Bmap, AtModification) {
  bmap<int, std::string> m = {{1, "old"}};
  m.at(1) = "new";
  EXPECT_EQ(m.at(1), "new");
}

// ============================================================================
// insert
// ============================================================================

TEST(Bmap, InsertSuccess) {
  bmap<int, std::string> m;
  auto result = m.insert({1, "one"});
  EXPECT_TRUE(result.second);
  EXPECT_EQ(result.first->first, 1);
  EXPECT_EQ(result.first->second, "one");
  EXPECT_EQ(m.size(), 1u);
}

TEST(Bmap, InsertDuplicate) {
  bmap<int, std::string> m;
  m.insert({5, "first"});
  auto result = m.insert({5, "second"});
  EXPECT_FALSE(result.second);
  EXPECT_EQ(result.first->first, 5);
  EXPECT_EQ(result.first->second, "first");  // unchanged
  EXPECT_EQ(m.size(), 1u);
}

TEST(Bmap, InsertMultipleUnique) {
  bmap<int, std::string> m;
  for (int i = 0; i < 100; ++i) {
    auto result = m.insert({i, "val_" + std::to_string(i)});
    EXPECT_TRUE(result.second);
  }
  EXPECT_EQ(m.size(), 100u);
  verifySortedOrder(m);
}

TEST(Bmap, InsertHint) {
  bmap<int, std::string> m = {{1, "a"}, {5, "e"}, {10, "j"}};
  auto hint = m.find(5);
  auto it = m.insert(hint, {7, "g"});
  EXPECT_EQ(it->first, 7);
  EXPECT_EQ(it->second, "g");
  EXPECT_EQ(m.size(), 4u);
  verifySortedOrder(m);
}

TEST(Bmap, InsertRange) {
  std::vector<pair<int, std::string>> vec;
  for (int i = 0; i < 50; ++i) {
    vec.push_back({i, "v" + std::to_string(i)});
  }
  bmap<int, std::string> m;
  m.insert(vec.begin(), vec.end());
  EXPECT_EQ(m.size(), 50u);
  verifySortedOrder(m);
}

// ============================================================================
// emplace and emplace_hint
// ============================================================================

TEST(Bmap, EmplaceNewKey) {
  bmap<int, std::string> m;
  auto result = m.emplace(1, "alpha");
  EXPECT_TRUE(result.second);
  EXPECT_EQ(result.first->first, 1);
  EXPECT_EQ(result.first->second, "alpha");
  EXPECT_EQ(m.size(), 1u);
}

TEST(Bmap, EmplaceExistingKey) {
  bmap<int, std::string> m = {{10, "original"}};
  auto result = m.emplace(10, "should fail");
  EXPECT_FALSE(result.second);
  EXPECT_EQ(result.first->second, "original");
  EXPECT_EQ(m.size(), 1u);
}

TEST(Bmap, EmplaceMultiple) {
  bmap<int, std::string> m;
  for (int i = 0; i < 50; ++i) {
    m.emplace(i, "val_" + std::to_string(i));
  }
  EXPECT_EQ(m.size(), 50u);
  verifySortedOrder(m);
}

TEST(Bmap, EmplaceHint) {
  bmap<int, std::string> m = {{1, "a"}, {10, "j"}};
  auto hint = m.find(1);
  auto it = m.emplace_hint(hint, 5, "e");
  EXPECT_EQ(it->first, 5);
  EXPECT_EQ(it->second, "e");
  EXPECT_EQ(m.size(), 3u);
  verifySortedOrder(m);
}

// ============================================================================
// insert_or_assign
// ============================================================================

TEST(Bmap, InsertOrAssignNewKey) {
  bmap<int, std::string> m;
  auto result = m.insert_or_assign(1, "one");
  EXPECT_TRUE(result.second);
  EXPECT_EQ(result.first->first, 1);
  EXPECT_EQ(result.first->second, "one");
}

TEST(Bmap, InsertOrAssignOverwrite) {
  bmap<int, std::string> m = {{5, "old"}};
  auto result = m.insert_or_assign(5, "new");
  EXPECT_FALSE(result.second);
  EXPECT_EQ(result.first->second, "new");
  EXPECT_EQ(m.size(), 1u);
}

TEST(Bmap, InsertOrAssignMoveKey) {
  bmap<int, std::string> m;
  int k = 99;
  auto result = m.insert_or_assign(zstl::move(k), "value");
  EXPECT_TRUE(result.second);
  EXPECT_EQ(m[99], "value");
}

// ============================================================================
// erase
// ============================================================================

TEST(Bmap, EraseByKey) {
  bmap<int, std::string> m = {{1, "a"}, {2, "b"}, {3, "c"}};
  size_t erased = m.erase(2);
  EXPECT_EQ(erased, 1u);
  EXPECT_EQ(m.size(), 2u);
  EXPECT_EQ(m.count(2), 0u);
}

TEST(Bmap, EraseNonExistentKey) {
  bmap<int, std::string> m = {{1, "a"}, {2, "b"}};
  size_t erased = m.erase(999);
  EXPECT_EQ(erased, 0u);
  EXPECT_EQ(m.size(), 2u);
}

TEST(Bmap, EraseByIterator) {
  bmap<int, std::string> m = {{1, "a"}, {2, "b"}, {3, "c"}};
  auto it = m.find(2);
  auto next = m.erase(it);
  EXPECT_EQ(next->first, 3);
  EXPECT_EQ(m.size(), 2u);
}

TEST(Bmap, EraseRange) {
  bmap<int, std::string> m;
  for (int i = 0; i < 20; ++i) {
    m.insert({i, "v" + std::to_string(i)});
  }
  // Erase keys [5, 15)
  auto first = m.lower_bound(5);
  auto last = m.lower_bound(15);
  auto it = m.erase(first, last);
  EXPECT_EQ(m.size(), 10u);  // 20 - 10 removed
  EXPECT_EQ(m.count(5), 0u);
  EXPECT_EQ(m.count(10), 0u);
  EXPECT_EQ(m.count(0), 1u);
  EXPECT_EQ(m.count(19), 1u);
  verifySortedOrder(m);
}

// ============================================================================
// find, count, contains
// ============================================================================

TEST(Bmap, FindExisting) {
  bmap<int, std::string> m = {{1, "a"}, {2, "b"}, {3, "c"}};
  auto it = m.find(2);
  EXPECT_NE(it, m.end());
  EXPECT_EQ(it->second, "b");
}

TEST(Bmap, FindNonExisting) {
  bmap<int, std::string> m = {{1, "a"}, {2, "b"}};
  auto it = m.find(999);
  EXPECT_EQ(it, m.end());
}

TEST(Bmap, FindConst) {
  const bmap<int, std::string> m = {{42, "answer"}};
  auto it = m.find(42);
  EXPECT_NE(it, m.end());
  EXPECT_EQ(it->second, "answer");
}

TEST(Bmap, Count) {
  bmap<int, std::string> m = {{1, "a"}, {2, "b"}, {3, "c"}};
  EXPECT_EQ(m.count(1), 1u);
  EXPECT_EQ(m.count(2), 1u);
  EXPECT_EQ(m.count(999), 0u);
}

TEST(Bmap, Contains) {
  bmap<int, std::string> m = {{10, "x"}, {20, "y"}, {30, "z"}};
  EXPECT_TRUE(m.contains(10));
  EXPECT_TRUE(m.contains(20));
  EXPECT_TRUE(m.contains(30));
  EXPECT_FALSE(m.contains(0));
  EXPECT_FALSE(m.contains(100));
}

// ============================================================================
// lower_bound, upper_bound, equal_range
// ============================================================================

TEST(Bmap, LowerBoundExactMatch) {
  bmap<int, std::string> m = {{10, "a"}, {20, "b"}, {30, "c"}, {40, "d"}};
  auto it = m.lower_bound(20);
  EXPECT_EQ(it->first, 20);
}

TEST(Bmap, LowerBoundNoExactMatch) {
  bmap<int, std::string> m = {{10, "a"}, {30, "b"}};
  auto it = m.lower_bound(20);
  EXPECT_EQ(it->first, 30);  // first >= 20
}

TEST(Bmap, LowerBoundPastEnd) {
  bmap<int, std::string> m = {{1, "a"}, {2, "b"}};
  auto it = m.lower_bound(100);
  EXPECT_EQ(it, m.end());
}

TEST(Bmap, UpperBoundExact) {
  bmap<int, std::string> m = {{10, "a"}, {20, "b"}, {30, "c"}};
  auto it = m.upper_bound(20);
  EXPECT_EQ(it->first, 30);  // first > 20
}

TEST(Bmap, UpperBoundPastEnd) {
  bmap<int, std::string> m = {{1, "a"}, {2, "b"}};
  auto it = m.upper_bound(100);
  EXPECT_EQ(it, m.end());
}

TEST(Bmap, EqualRange) {
  bmap<int, std::string> m = {{1, "a"}, {2, "b"}, {2, "b2"}, {3, "c"}};
  // Note: since keys are unique, equal_range for 2 should return [find(2), next]
  auto range = m.equal_range(2);
  EXPECT_NE(range.first, m.end());
  EXPECT_EQ(range.first->first, 2);
}

TEST(Bmap, EqualRangeEmpty) {
  bmap<int, std::string> m = {{1, "a"}, {3, "b"}};
  auto range = m.equal_range(2);
  EXPECT_EQ(range.first, range.second);
  EXPECT_EQ(range.first->first, 3);  // lower_bound returns first >=
}

TEST(Bmap, EqualRangeConst) {
  const bmap<int, std::string> m = {{5, "five"}, {10, "ten"}};
  auto range = m.equal_range(5);
  EXPECT_NE(range.first, m.end());
  EXPECT_EQ(range.first->second, "five");
}

// ============================================================================
// Iteration: begin/end — verify sorted order
// ============================================================================

TEST(Bmap, IterationSortedOrder) {
  bmap<int, std::string> m;
  for (int i = 99; i >= 0; --i) {
    m.insert({i, "v" + std::to_string(i)});
  }
  EXPECT_EQ(m.size(), 100u);

  int expected_key = 0;
  for (const auto& kv : m) {
    EXPECT_EQ(kv.first, expected_key);
    EXPECT_EQ(kv.second, "v" + std::to_string(expected_key));
    ++expected_key;
  }
  EXPECT_EQ(expected_key, 100);
}

TEST(Bmap, IteratorBeginEnd) {
  bmap<int, std::string> m = {{5, "five"}};
  auto it = m.begin();
  EXPECT_NE(it, m.end());
  EXPECT_EQ(it->first, 5);
  ++it;
  EXPECT_EQ(it, m.end());
}

TEST(Bmap, IteratorEmpty) {
  bmap<int, std::string> m;
  EXPECT_EQ(m.begin(), m.end());
}

TEST(Bmap, CbeginCend) {
  const bmap<int, std::string> m = {{1, "a"}, {2, "b"}, {3, "c"}};
  int count = 0;
  for (auto it = m.cbegin(); it != m.cend(); ++it) {
    ++count;
  }
  EXPECT_EQ(count, 3);
}

// ============================================================================
// Reverse iteration
// ============================================================================

TEST(Bmap, ReverseIteration) {
  bmap<int, std::string> m;
  for (int i = 0; i < 10; ++i) {
    m.insert({i, "v" + std::to_string(i)});
  }
  // Reverse: 9 -> 0
  int expected_key = 9;
  for (auto it = m.rbegin(); it != m.rend(); ++it) {
    EXPECT_EQ(it->first, expected_key);
    --expected_key;
  }
  EXPECT_EQ(expected_key, -1);
}

TEST(Bmap, ConstReverseIteration) {
  const bmap<int, std::string> m = {{1, "a"}, {3, "c"}, {5, "e"}};
  auto it = m.crbegin();
  EXPECT_EQ(it->first, 5);
  ++it;
  EXPECT_EQ(it->first, 3);
  ++it;
  EXPECT_EQ(it->first, 1);
  ++it;
  EXPECT_EQ(it, m.crend());
}

// ============================================================================
// empty, size, clear, max_size
// ============================================================================

TEST(Bmap, EmptySizeClear) {
  bmap<int, std::string> m;
  EXPECT_TRUE(m.empty());
  EXPECT_EQ(m.size(), 0u);

  m.insert({1, "a"});
  m.insert({2, "b"});
  m.insert({3, "c"});
  EXPECT_FALSE(m.empty());
  EXPECT_EQ(m.size(), 3u);

  m.clear();
  EXPECT_TRUE(m.empty());
  EXPECT_EQ(m.size(), 0u);
}

TEST(Bmap, MaxSize) {
  bmap<int, std::string> m;
  EXPECT_GT(m.max_size(), 0u);
}

// ============================================================================
// swap
// ============================================================================

TEST(Bmap, SwapMember) {
  bmap<int, std::string> m1 = {{1, "a"}, {2, "b"}, {3, "c"}};
  bmap<int, std::string> m2 = {{10, "x"}, {20, "y"}};

  m1.swap(m2);
  EXPECT_EQ(m1.size(), 2u);
  EXPECT_EQ(m2.size(), 3u);
  EXPECT_EQ(m1[10], "x");
  EXPECT_EQ(m1[20], "y");
  EXPECT_EQ(m2[1], "a");
  EXPECT_EQ(m2[2], "b");
  EXPECT_EQ(m2[3], "c");
  verifySortedOrder(m1);
  verifySortedOrder(m2);
}

TEST(Bmap, SwapFreeFunction) {
  bmap<int, std::string> m1 = {{1, "a"}};
  bmap<int, std::string> m2 = {{2, "b"}, {3, "c"}};
  zstl::swap(m1, m2);
  EXPECT_EQ(m1.size(), 2u);
  EXPECT_EQ(m2.size(), 1u);
}

// ============================================================================
// get_allocator
// ============================================================================

TEST(Bmap, GetAllocator) {
  bmap<int, std::string> m;
  auto alloc = m.get_allocator();
  // Just verify it returns something valid (no crash)
  SUCCEED() << "get_allocator() returned successfully";
}

// ============================================================================
// key_comp and value_comp
// ============================================================================

TEST(Bmap, KeyComp) {
  bmap<int, std::string> m;
  auto kc = m.key_comp();
  EXPECT_TRUE(kc(1, 2));
  EXPECT_FALSE(kc(2, 1));
  EXPECT_FALSE(kc(5, 5));
}

TEST(Bmap, ValueComp) {
  bmap<int, std::string> m;
  auto vc = m.value_comp();
  EXPECT_TRUE(vc({1, "a"}, {2, "b"}));
  EXPECT_FALSE(vc({2, "b"}, {1, "a"}));
  EXPECT_FALSE(vc({5, "x"}, {5, "y"}));
}

// ============================================================================
// merge
// ============================================================================

TEST(Bmap, Merge) {
  bmap<int, std::string> src = {{1, "a"}, {3, "c"}, {5, "e"}};
  bmap<int, std::string> dst = {{2, "b"}, {3, "existing_c"}, {4, "d"}};

  dst.merge(src);
  // Key 3 in src should not be merged (already exists in dst)
  EXPECT_EQ(dst.size(), 5u);
  EXPECT_EQ(src.size(), 1u);  // only key 3 remains in src
  EXPECT_TRUE(src.contains(3));

  verifySortedOrder(dst);
}

// ============================================================================
// Comparison operators
// ============================================================================

TEST(Bmap, Equality) {
  bmap<int, std::string> m1 = {{1, "a"}, {2, "b"}, {3, "c"}};
  bmap<int, std::string> m2 = {{1, "a"}, {2, "b"}, {3, "c"}};
  bmap<int, std::string> m3 = {{1, "a"}, {2, "b"}};
  bmap<int, std::string> m4 = {{1, "a"}, {2, "x"}, {3, "c"}};

  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 == m3);
  EXPECT_TRUE(m1 != m3);
  EXPECT_FALSE(m1 == m4);
  EXPECT_TRUE(m1 != m4);
}

TEST(Bmap, EqualityEmpty) {
  bmap<int, std::string> m1, m2;
  EXPECT_TRUE(m1 == m2);
}

TEST(Bmap, LessThan) {
  bmap<int, std::string> m1 = {{1, "a"}, {2, "b"}};
  bmap<int, std::string> m2 = {{1, "a"}, {3, "c"}};
  bmap<int, std::string> m3 = {{1, "a"}, {2, "b"}};

  EXPECT_TRUE(m1 < m2);   // {1,2} < {1,3}
  EXPECT_FALSE(m2 < m1);
  EXPECT_FALSE(m1 < m3);  // equal
  EXPECT_TRUE(m1 <= m3);
  EXPECT_TRUE(m2 > m1);
  EXPECT_TRUE(m2 >= m1);
}

// ============================================================================
// Range queries: iterate through a range of keys
// ============================================================================

TEST(Bmap, RangeQuery) {
  bmap<int, std::string> m;
  for (int i = 0; i < 100; ++i) {
    m.insert({i, "value_" + std::to_string(i)});
  }

  // Query range [25, 50)
  auto start = m.lower_bound(25);
  auto end = m.upper_bound(49);
  int count = 0;
  for (auto it = start; it != end; ++it) {
    EXPECT_GE(it->first, 25);
    EXPECT_LE(it->first, 49);
    ++count;
  }
  EXPECT_EQ(count, 25);
}

TEST(Bmap, RangeQueryEmpty) {
  bmap<int, std::string> m = {{1, "a"}, {5, "b"}, {10, "c"}};
  auto range_start = m.lower_bound(6);
  auto range_end = m.upper_bound(9);
  EXPECT_EQ(range_start, range_end);
}

// ============================================================================
// Large dataset: verify B+ tree properties hold
// ============================================================================

TEST(Bmap, Large1000InsertAndIterate) {
  bmap<int, std::string> m;
  const int N = 1000;

  // Insert in random order
  std::vector<int> keys(N);
  for (int i = 0; i < N; ++i) keys[i] = i;
  std::mt19937 rng(42);
  std::shuffle(keys.begin(), keys.end(), rng);

  for (int k : keys) {
    m.insert({k, "v_" + std::to_string(k)});
  }

  EXPECT_EQ(m.size(), static_cast<size_t>(N));

  // Verify sorted order
  verifySortedOrder(m);

  // Verify all keys accessible
  for (int i = 0; i < N; ++i) {
    EXPECT_EQ(m[i], "v_" + std::to_string(i));
  }

  // Verify correct keys in iteration
  int expected = 0;
  for (const auto& kv : m) {
    EXPECT_EQ(kv.first, expected);
    ++expected;
  }
  EXPECT_EQ(expected, N);
}

TEST(Bmap, Large5000RandomInsert) {
  bmap<int, int> m;
  const int N = 5000;
  std::vector<int> keys(N);
  for (int i = 0; i < N; ++i) keys[i] = i;
  std::mt19937 rng(123);
  std::shuffle(keys.begin(), keys.end(), rng);

  for (int k : keys) {
    m.insert({k, k * 10});
  }

  EXPECT_EQ(m.size(), static_cast<size_t>(N));
  verifySortedOrder(m);

  // Spot check
  for (int i = 0; i < N; ++i) {
    EXPECT_EQ(m[i], i * 10);
  }

  // Verify lower_bound/upper_bound on large set
  auto lb = m.lower_bound(2500);
  EXPECT_EQ(lb->first, 2500);

  auto ub = m.upper_bound(2500);
  EXPECT_EQ(ub->first, 2501);
}

TEST(Bmap, LargeInterleavedInsertErase) {
  bmap<int, std::string> m;
  const int N = 500;

  // Insert all
  for (int i = 0; i < N; ++i) {
    m.insert({i, "v" + std::to_string(i)});
  }
  EXPECT_EQ(m.size(), static_cast<size_t>(N));

  // Erase every other key
  for (int i = 0; i < N; i += 2) {
    m.erase(i);
  }
  EXPECT_EQ(m.size(), static_cast<size_t>(N / 2));

  verifySortedOrder(m);

  // Remaining keys should be odd
  for (const auto& kv : m) {
    EXPECT_EQ(kv.first % 2, 1);
  }
}

TEST(Bmap, LargeAllErased) {
  bmap<int, std::string> m;
  for (int i = 0; i < 200; ++i) {
    m.insert({i, "v" + std::to_string(i)});
  }
  for (int i = 0; i < 200; ++i) {
    m.erase(i);
  }
  EXPECT_TRUE(m.empty());
  EXPECT_EQ(m.size(), 0u);
}

// ============================================================================
// Different key/value types
// ============================================================================

TEST(Bmap, StringKeyIntValue) {
  bmap<std::string, int> m;
  m["apple"] = 100;
  m["banana"] = 200;
  m["cherry"] = 300;
  m["aardvark"] = 50;

  EXPECT_EQ(m.size(), 4u);

  // Verify lexicographic order
  auto it = m.begin();
  EXPECT_EQ(it->first, "aardvark");
  ++it;
  EXPECT_EQ(it->first, "apple");
  ++it;
  EXPECT_EQ(it->first, "banana");
  ++it;
  EXPECT_EQ(it->first, "cherry");
}

// ============================================================================
// Operator[] with rvalue key
// ============================================================================

TEST(Bmap, OperatorBracketRvalueKey) {
  bmap<std::string, int> m;
  m[std::string("hello")] = 42;
  EXPECT_EQ(m["hello"], 42);
}

// ============================================================================
// Insert after clear
// ============================================================================

TEST(Bmap, InsertAfterClear) {
  bmap<int, std::string> m;
  m.insert({1, "a"});
  m.insert({2, "b"});
  m.clear();
  EXPECT_TRUE(m.empty());

  m.insert({3, "c"});
  m.insert({4, "d"});
  EXPECT_EQ(m.size(), 2u);
  verifySortedOrder(m);
}
