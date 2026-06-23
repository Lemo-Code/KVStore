// ============================================================================
// zstl map tests — ordered key-value container backed by red-black tree
// ============================================================================
// Tests for: map<int, std::string> and map with custom comparator
// Covers: all constructors, operator[], at(), insert/emplace/try_emplace/
//         insert_or_assign, erase, find/count/contains, lower_bound/
//         upper_bound/equal_range, key_comp/value_comp, iterators
//         (begin/end/rbegin/rend), capacity (empty/size/max_size),
//         clear/swap/merge, custom comparator, comparison operators,
//         large dataset, sorted iteration order, get_allocator
// ============================================================================

#include <gtest/gtest.h>
#include "zstl/zstl.h"

#include <string>
#include <stdexcept>
#include <vector>

using namespace zstl;

// Type alias for brevity
using MapIS = map<int, std::string>;

// ============================================================================
// Constructors
// ============================================================================

TEST(Map, DefaultConstructor) {
  MapIS m;
  EXPECT_TRUE(m.empty());
  EXPECT_EQ(m.size(), 0u);
}

TEST(Map, ComparatorConstructor) {
  map<int, std::string, greater<int>> m;
  EXPECT_TRUE(m.empty());
  EXPECT_EQ(m.size(), 0u);
}

TEST(Map, RangeConstructor) {
  std::vector<pair<int, std::string>> values = {
    {3, "three"}, {1, "one"}, {4, "four"}, {2, "two"}
  };
  MapIS m(values.begin(), values.end());
  EXPECT_EQ(m.size(), 4u);
  EXPECT_EQ(m[1], "one");
  EXPECT_EQ(m[2], "two");
  EXPECT_EQ(m[3], "three");
  EXPECT_EQ(m[4], "four");
}

TEST(Map, RangeConstructorEmpty) {
  std::vector<pair<int, std::string>> empty;
  MapIS m(empty.begin(), empty.end());
  EXPECT_TRUE(m.empty());
}

TEST(Map, CopyConstructor) {
  MapIS m1 = {{1, "a"}, {2, "b"}, {3, "c"}};
  MapIS m2(m1);
  EXPECT_EQ(m2.size(), 3u);
  EXPECT_EQ(m2[1], "a");
  EXPECT_EQ(m2[2], "b");
  EXPECT_EQ(m2[3], "c");
  // Verify independence
  m2[1] = "modified";
  EXPECT_EQ(m1[1], "a");
}

TEST(Map, MoveConstructor) {
  MapIS m1 = {{1, "x"}, {2, "y"}};
  MapIS m2(std::move(m1));
  EXPECT_EQ(m2.size(), 2u);
  EXPECT_EQ(m2[1], "x");
  EXPECT_EQ(m2[2], "y");
  // m1 is in valid but unspecified state
  // We can still call size() on it
}

TEST(Map, InitializerListConstructor) {
  MapIS m = {{5, "five"}, {2, "two"}, {8, "eight"}, {1, "one"}};
  EXPECT_EQ(m.size(), 4u);
  // Should be sorted by key
  auto it = m.begin();
  EXPECT_EQ(it->first, 1); EXPECT_EQ(it->second, "one"); ++it;
  EXPECT_EQ(it->first, 2); EXPECT_EQ(it->second, "two"); ++it;
  EXPECT_EQ(it->first, 5); EXPECT_EQ(it->second, "five"); ++it;
  EXPECT_EQ(it->first, 8); EXPECT_EQ(it->second, "eight");
}

TEST(Map, InitializerListConstructorWithDuplicates) {
  // Duplicate keys should not be inserted
  MapIS m = {{1, "first"}, {1, "second"}};
  EXPECT_EQ(m.size(), 1u);
  EXPECT_EQ(m[1], "first");  // first insertion wins
}

TEST(Map, CopyAssignment) {
  MapIS m1 = {{10, "ten"}, {20, "twenty"}};
  MapIS m2;
  m2 = m1;
  EXPECT_EQ(m2.size(), 2u);
  EXPECT_EQ(m2[10], "ten");
  EXPECT_EQ(m2[20], "twenty");
}

TEST(Map, MoveAssignment) {
  MapIS m1 = {{5, "cinco"}, {10, "diez"}};
  MapIS m2;
  m2 = std::move(m1);
  EXPECT_EQ(m2.size(), 2u);
}

TEST(Map, InitializerListAssignment) {
  MapIS m;
  m = {{100, "hundred"}, {200, "two hundred"}, {50, "fifty"}};
  EXPECT_EQ(m.size(), 3u);
  EXPECT_EQ(m[50], "fifty");
  EXPECT_EQ(m[100], "hundred");
  EXPECT_EQ(m[200], "two hundred");
}

// ============================================================================
// operator[]
// ============================================================================

TEST(Map, OperatorBracketReadExisting) {
  MapIS m = {{1, "alpha"}, {2, "beta"}};
  EXPECT_EQ(m[1], "alpha");
  EXPECT_EQ(m[2], "beta");
}

TEST(Map, OperatorBracketInsertNew) {
  MapIS m;
  m[5] = "hello";
  EXPECT_EQ(m.size(), 1u);
  EXPECT_EQ(m[5], "hello");
}

TEST(Map, OperatorBracketDefaultConstructsValue) {
  MapIS m;
  // Access a non-existent key — should insert with default-constructed string
  std::string& val = m[42];
  EXPECT_EQ(m.size(), 1u);
  EXPECT_TRUE(val.empty());  // default constructed
  EXPECT_EQ(m[42], "");
}

TEST(Map, OperatorBracketUpdateExisting) {
  MapIS m = {{1, "old"}};
  m[1] = "new";
  EXPECT_EQ(m[1], "new");
  EXPECT_EQ(m.size(), 1u);
}

TEST(Map, OperatorBracketMultipleInsertions) {
  MapIS m;
  for (int i = 0; i < 100; ++i) {
    m[i] = "value_" + std::to_string(i);
  }
  EXPECT_EQ(m.size(), 100u);
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(m[i], "value_" + std::to_string(i));
  }
}

// ============================================================================
// at()
// ============================================================================

TEST(Map, AtReadExisting) {
  MapIS m = {{1, "one"}, {2, "two"}, {3, "three"}};
  EXPECT_EQ(m.at(1), "one");
  EXPECT_EQ(m.at(2), "two");
  EXPECT_EQ(m.at(3), "three");
}

TEST(Map, AtThrowsForMissingKey) {
  MapIS m;
  EXPECT_THROW(m.at(42), std::out_of_range);
}

TEST(Map, AtThrowsAfterErase) {
  MapIS m = {{5, "five"}};
  m.erase(5);
  EXPECT_THROW(m.at(5), std::out_of_range);
}

TEST(Map, ConstAt) {
  const MapIS m = {{7, "seven"}, {8, "eight"}};
  EXPECT_EQ(m.at(7), "seven");
  EXPECT_EQ(m.at(8), "eight");
  EXPECT_THROW(m.at(99), std::out_of_range);
}

TEST(Map, AtModifyValue) {
  MapIS m = {{1, "initial"}};
  m.at(1) = "modified";
  EXPECT_EQ(m.at(1), "modified");
}

// ============================================================================
// insert — single pair
// ============================================================================

TEST(Map, InsertSinglePair) {
  MapIS m;
  auto result = m.insert({10, "ten"});
  EXPECT_TRUE(result.second);           // insertion succeeded
  EXPECT_EQ(result.first->first, 10);
  EXPECT_EQ(result.first->second, "ten");
  EXPECT_EQ(m.size(), 1u);
}

TEST(Map, InsertDuplicateKey) {
  MapIS m = {{1, "first"}};
  auto result = m.insert({1, "second"});
  EXPECT_FALSE(result.second);          // insertion failed
  EXPECT_EQ(result.first->first, 1);
  EXPECT_EQ(result.first->second, "first");  // original value preserved
  EXPECT_EQ(m.size(), 1u);
}

TEST(Map, InsertMultiplePairs) {
  MapIS m;
  m.insert({5, "a"});
  m.insert({3, "b"});
  m.insert({7, "c"});
  m.insert({1, "d"});
  EXPECT_EQ(m.size(), 4u);
  // Verify sorted order
  auto it = m.begin();
  EXPECT_EQ(it->first, 1); ++it;
  EXPECT_EQ(it->first, 3); ++it;
  EXPECT_EQ(it->first, 5); ++it;
  EXPECT_EQ(it->first, 7);
}

TEST(Map, InsertMove) {
  MapIS m;
  std::string val = "move_me";
  m.insert({20, std::move(val)});
  EXPECT_EQ(m[20], "move_me");
  // val is in moved-from state
}

// ============================================================================
// insert — hint
// ============================================================================

TEST(Map, InsertWithHint) {
  MapIS m = {{1, "a"}, {5, "e"}, {10, "j"}};
  auto it = m.find(5);
  auto result = m.insert(it, {3, "c"});
  EXPECT_EQ(result->first, 3);
  EXPECT_EQ(result->second, "c");
  EXPECT_EQ(m.size(), 4u);
}

TEST(Map, InsertWithHintEnd) {
  MapIS m = {{1, "a"}, {2, "b"}};
  auto it = m.insert(m.end(), {3, "c"});
  EXPECT_EQ(it->first, 3);
  EXPECT_TRUE(m.contains(3));
}

// ============================================================================
// insert — range
// ============================================================================

TEST(Map, InsertRange) {
  MapIS m;
  std::vector<pair<int, std::string>> values = {
    {5, "five"}, {1, "one"}, {3, "three"}, {7, "seven"}
  };
  m.insert(values.begin(), values.end());
  EXPECT_EQ(m.size(), 4u);
  EXPECT_EQ(m[1], "one");
  EXPECT_EQ(m[3], "three");
  EXPECT_EQ(m[5], "five");
  EXPECT_EQ(m[7], "seven");
}

TEST(Map, InsertRangeWithDuplicates) {
  MapIS m = {{1, "existing"}};
  std::vector<pair<int, std::string>> values = {{1, "new_one"}, {2, "two"}};
  m.insert(values.begin(), values.end());
  EXPECT_EQ(m.size(), 2u);
  EXPECT_EQ(m[1], "existing");  // original preserved
  EXPECT_EQ(m[2], "two");
}

// ============================================================================
// insert — initializer_list
// ============================================================================

TEST(Map, InsertInitializerList) {
  MapIS m;
  m.insert({{10, "ten"}, {20, "twenty"}, {30, "thirty"}});
  EXPECT_EQ(m.size(), 3u);
  EXPECT_EQ(m[10], "ten");
  EXPECT_EQ(m[20], "twenty");
  EXPECT_EQ(m[30], "thirty");
}

// ============================================================================
// emplace()
// ============================================================================

TEST(Map, EmplaceSingle) {
  MapIS m;
  auto result = m.emplace(1, "one");
  EXPECT_TRUE(result.second);
  EXPECT_EQ(result.first->first, 1);
  EXPECT_EQ(result.first->second, "one");
  EXPECT_EQ(m.size(), 1u);
}

TEST(Map, EmplaceDuplicate) {
  MapIS m = {{1, "original"}};
  auto result = m.emplace(1, "duplicate");
  EXPECT_FALSE(result.second);
  EXPECT_EQ(result.first->second, "original");
  EXPECT_EQ(m.size(), 1u);
}

TEST(Map, EmplaceMultiple) {
  MapIS m;
  m.emplace(5, "five");
  m.emplace(3, "three");
  m.emplace(7, "seven");
  m.emplace(1, "one");
  EXPECT_EQ(m.size(), 4u);
  auto it = m.begin();
  EXPECT_EQ(it->first, 1); ++it;
  EXPECT_EQ(it->first, 3); ++it;
  EXPECT_EQ(it->first, 5); ++it;
  EXPECT_EQ(it->first, 7);
}

// ============================================================================
// try_emplace()
// ============================================================================

TEST(Map, TryEmplaceNewKey) {
  MapIS m;
  auto result = m.try_emplace(10, "ten");
  EXPECT_TRUE(result.second);
  EXPECT_EQ(result.first->first, 10);
  EXPECT_EQ(result.first->second, "ten");
}

TEST(Map, TryEmplaceExistingKey) {
  MapIS m = {{10, "original"}};
  auto result = m.try_emplace(10, "should_not_appear");
  EXPECT_FALSE(result.second);
  EXPECT_EQ(result.first->first, 10);
  EXPECT_EQ(result.first->second, "original");  // unchanged
}

TEST(Map, TryEmplaceLvalueKey) {
  MapIS m;
  int key = 42;
  auto result = m.try_emplace(key, "answer");
  EXPECT_TRUE(result.second);
  EXPECT_EQ(m[42], "answer");
}

// ============================================================================
// insert_or_assign()
// ============================================================================

TEST(Map, InsertOrAssignNewKey) {
  MapIS m;
  auto result = m.insert_or_assign(1, "one");
  EXPECT_TRUE(result.second);
  EXPECT_EQ(m[1], "one");
  EXPECT_EQ(m.size(), 1u);
}

TEST(Map, InsertOrAssignExistingKey) {
  MapIS m = {{1, "old"}};
  auto result = m.insert_or_assign(1, "new");
  EXPECT_FALSE(result.second);  // assignment, not insertion
  EXPECT_EQ(m[1], "new");
  EXPECT_EQ(m.size(), 1u);
}

TEST(Map, InsertOrAssignRvalueKey) {
  MapIS m;
  auto result = m.insert_or_assign(15, std::string("fifteen"));
  EXPECT_TRUE(result.second);
  EXPECT_EQ(m.size(), 1u);
}

// ============================================================================
// erase()
// ============================================================================

TEST(Map, EraseByKey) {
  MapIS m = {{1, "a"}, {2, "b"}, {3, "c"}};
  size_t removed = m.erase(2);
  EXPECT_EQ(removed, 1u);
  EXPECT_EQ(m.size(), 2u);
  EXPECT_FALSE(m.contains(2));
  EXPECT_TRUE(m.contains(1));
  EXPECT_TRUE(m.contains(3));
}

TEST(Map, EraseByKeyNotFound) {
  MapIS m = {{1, "a"}};
  size_t removed = m.erase(99);
  EXPECT_EQ(removed, 0u);
  EXPECT_EQ(m.size(), 1u);
}

TEST(Map, EraseByIterator) {
  MapIS m = {{1, "a"}, {2, "b"}, {3, "c"}};
  auto it = m.find(2);
  auto next = m.erase(it);
  EXPECT_EQ(m.size(), 2u);
  EXPECT_FALSE(m.contains(2));
  EXPECT_EQ(next->first, 3);  // next points to element after erased
}

TEST(Map, EraseByConstIterator) {
  MapIS m = {{5, "x"}, {10, "y"}, {15, "z"}};
  const auto& cm = m;
  auto it = cm.find(10);
  auto next = m.erase(it);
  EXPECT_EQ(m.size(), 2u);
  EXPECT_EQ(next->first, 15);
}

TEST(Map, EraseByRange) {
  MapIS m = {{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}, {5, "e"}};
  auto first = m.find(2);
  auto last = m.find(4);
  auto next = m.erase(first, last);  // erases 2, 3
  EXPECT_EQ(m.size(), 3u);
  EXPECT_TRUE(m.contains(1));
  EXPECT_FALSE(m.contains(2));
  EXPECT_FALSE(m.contains(3));
  EXPECT_TRUE(m.contains(4));
  EXPECT_TRUE(m.contains(5));
  EXPECT_EQ(next->first, 4);
}

TEST(Map, EraseBeginToEnd) {
  MapIS m = {{1, "a"}, {2, "b"}, {3, "c"}};
  auto next = m.erase(m.begin(), m.end());
  EXPECT_TRUE(m.empty());
  EXPECT_EQ(next, m.end());
}

// ============================================================================
// find(), count(), contains()
// ============================================================================

TEST(Map, FindExisting) {
  MapIS m = {{1, "a"}, {3, "c"}, {5, "e"}};
  auto it = m.find(3);
  EXPECT_NE(it, m.end());
  EXPECT_EQ(it->second, "c");
}

TEST(Map, FindNonExisting) {
  MapIS m = {{1, "a"}, {3, "c"}};
  auto it = m.find(42);
  EXPECT_EQ(it, m.end());
}

TEST(Map, ConstFind) {
  const MapIS m = {{10, "ten"}, {20, "twenty"}};
  auto it = m.find(10);
  EXPECT_NE(it, m.end());
  EXPECT_EQ(it->second, "ten");
}

TEST(Map, Count) {
  MapIS m = {{1, "a"}, {2, "b"}, {2, "c"}};
  EXPECT_EQ(m.count(1), 1u);
  EXPECT_EQ(m.count(2), 1u);  // unique keys, so at most 1
  EXPECT_EQ(m.count(99), 0u);
}

TEST(Map, Contains) {
  MapIS m = {{100, "hundred"}};
  EXPECT_TRUE(m.contains(100));
  EXPECT_FALSE(m.contains(200));
}

// ============================================================================
// lower_bound(), upper_bound(), equal_range()
// ============================================================================

TEST(Map, LowerBoundExact) {
  MapIS m = {{1, "a"}, {3, "c"}, {5, "e"}};
  auto it = m.lower_bound(3);
  EXPECT_EQ(it->first, 3);
}

TEST(Map, LowerBoundBetween) {
  MapIS m = {{1, "a"}, {5, "e"}};
  auto it = m.lower_bound(3);
  EXPECT_EQ(it->first, 5);  // first element >= 3
}

TEST(Map, LowerBoundBeyondEnd) {
  MapIS m = {{1, "a"}};
  auto it = m.lower_bound(10);
  EXPECT_EQ(it, m.end());
}

TEST(Map, UpperBoundExact) {
  MapIS m = {{1, "a"}, {3, "c"}, {5, "e"}};
  auto it = m.upper_bound(3);
  EXPECT_EQ(it->first, 5);  // first element > 3
}

TEST(Map, UpperBoundBeyondEnd) {
  MapIS m = {{1, "a"}};
  auto it = m.upper_bound(5);
  EXPECT_EQ(it, m.end());
}

TEST(Map, EqualRange) {
  MapIS m = {{1, "a"}, {3, "c"}, {5, "e"}};
  auto range = m.equal_range(3);
  EXPECT_EQ(range.first->first, 3);
  EXPECT_EQ(range.second->first, 5);  // upper_bound(3)
}

TEST(Map, EqualRangeMissingKey) {
  MapIS m = {{1, "a"}, {5, "e"}};
  auto range = m.equal_range(3);
  EXPECT_EQ(range.first->first, 5);
  EXPECT_EQ(range.second->first, 5);
  // first == second when key not found
}

TEST(Map, ConstLowerBoundUpperBound) {
  const MapIS m = {{2, "b"}, {4, "d"}, {6, "f"}};
  auto lb = m.lower_bound(4);
  EXPECT_EQ(lb->first, 4);
  auto ub = m.upper_bound(4);
  EXPECT_EQ(ub->first, 6);
}

// ============================================================================
// key_comp(), value_comp()
// ============================================================================

TEST(Map, KeyComp) {
  MapIS m;
  auto comp = m.key_comp();
  EXPECT_TRUE(comp(1, 5));
  EXPECT_FALSE(comp(5, 1));
  EXPECT_FALSE(comp(3, 3));
}

TEST(Map, ValueComp) {
  MapIS m;
  auto comp = m.value_comp();
  pair<const int, std::string> a{1, "a"};
  pair<const int, std::string> b{3, "b"};
  EXPECT_TRUE(comp(a, b));
  EXPECT_FALSE(comp(b, a));
}

// ============================================================================
// iterators: begin()/end(), rbegin()/rend()
// ============================================================================

TEST(Map, BeginEndForwardIteration) {
  MapIS m = {{3, "c"}, {1, "a"}, {2, "b"}};
  auto it = m.begin();
  EXPECT_EQ(it->first, 1);
  EXPECT_EQ(it->second, "a"); ++it;
  EXPECT_EQ(it->first, 2);
  EXPECT_EQ(it->second, "b"); ++it;
  EXPECT_EQ(it->first, 3);
  EXPECT_EQ(it->second, "c"); ++it;
  EXPECT_EQ(it, m.end());
}

TEST(Map, ReverseIteration) {
  MapIS m = {{3, "c"}, {1, "a"}, {5, "e"}};
  auto rit = m.rbegin();
  EXPECT_EQ(rit->first, 5); ++rit;
  EXPECT_EQ(rit->first, 3); ++rit;
  EXPECT_EQ(rit->first, 1); ++rit;
  EXPECT_EQ(rit, m.rend());
}

TEST(Map, ConstBeginEnd) {
  const MapIS m = {{2, "b"}, {1, "a"}};
  auto it = m.begin();
  EXPECT_EQ(it->first, 1); ++it;
  EXPECT_EQ(it->first, 2); ++it;
  EXPECT_EQ(it, m.end());
}

TEST(Map, ConstReverseIteration) {
  const MapIS m = {{3, "c"}, {1, "a"}};
  auto rit = m.crbegin();
  EXPECT_EQ(rit->first, 3); ++rit;
  EXPECT_EQ(rit->first, 1); ++rit;
  EXPECT_EQ(rit, m.crend());
}

TEST(Map, EmptyIterator) {
  MapIS m;
  EXPECT_EQ(m.begin(), m.end());
  EXPECT_EQ(m.rbegin(), m.rend());
}

// ============================================================================
// empty(), size(), max_size()
// ============================================================================

TEST(Map, EmptyAndSize) {
  MapIS m;
  EXPECT_TRUE(m.empty());
  EXPECT_EQ(m.size(), 0u);

  m[1] = "one";
  EXPECT_FALSE(m.empty());
  EXPECT_EQ(m.size(), 1u);

  m[2] = "two";
  EXPECT_EQ(m.size(), 2u);

  m.clear();
  EXPECT_TRUE(m.empty());
}

TEST(Map, MaxSize) {
  MapIS m;
  EXPECT_GT(m.max_size(), 0u);
}

// ============================================================================
// clear(), swap(), merge()
// ============================================================================

TEST(Map, Clear) {
  MapIS m = {{1, "a"}, {2, "b"}, {3, "c"}};
  EXPECT_FALSE(m.empty());
  m.clear();
  EXPECT_TRUE(m.empty());
  EXPECT_EQ(m.size(), 0u);
  EXPECT_EQ(m.find(1), m.end());
}

TEST(Map, ClearThenInsert) {
  MapIS m = {{1, "a"}};
  m.clear();
  m[10] = "ten";
  EXPECT_EQ(m.size(), 1u);
  EXPECT_EQ(m[10], "ten");
}

TEST(Map, Swap) {
  MapIS a = {{1, "a1"}, {2, "a2"}};
  MapIS b = {{10, "b10"}, {20, "b20"}, {30, "b30"}};

  a.swap(b);

  EXPECT_EQ(a.size(), 3u);
  EXPECT_EQ(b.size(), 2u);
  EXPECT_TRUE(a.contains(10));
  EXPECT_TRUE(a.contains(20));
  EXPECT_TRUE(a.contains(30));
  EXPECT_TRUE(b.contains(1));
  EXPECT_TRUE(b.contains(2));
}

TEST(Map, FreeFunctionSwap) {
  MapIS a = {{1, "x"}};
  MapIS b = {{2, "y"}};
  zstl::swap(a, b);
  EXPECT_TRUE(a.contains(2));
  EXPECT_TRUE(b.contains(1));
}

TEST(Map, Merge) {
  MapIS a = {{1, "a1"}, {3, "a3"}};
  MapIS b = {{2, "b2"}, {3, "b3_dup"}, {4, "b4"}};

  a.merge(b);

  EXPECT_EQ(a.size(), 4u);   // a1, a3, b2, b4 (b3_dup not merged)
  EXPECT_EQ(b.size(), 1u);   // b3_dup remains
  EXPECT_TRUE(a.contains(1));
  EXPECT_TRUE(a.contains(2));
  EXPECT_TRUE(a.contains(3));
  EXPECT_TRUE(a.contains(4));
  EXPECT_TRUE(b.contains(3)); // duplicate stays in source
}

TEST(Map, MergeEmptySource) {
  MapIS a = {{1, "a"}};
  MapIS b;
  a.merge(b);
  EXPECT_EQ(a.size(), 1u);
  EXPECT_TRUE(b.empty());
}

TEST(Map, MergeEmptyTarget) {
  MapIS a;
  MapIS b = {{5, "five"}, {10, "ten"}};
  a.merge(b);
  EXPECT_EQ(a.size(), 2u);
  EXPECT_TRUE(b.empty());
}

// ============================================================================
// Custom comparator (greater<int>)
// ============================================================================

TEST(Map, CustomComparatorGreater) {
  map<int, std::string, greater<int>> m;

  m[1] = "one";
  m[3] = "three";
  m[5] = "five";
  m[2] = "two";
  m[4] = "four";

  // Should iterate in descending key order
  auto it = m.begin();
  EXPECT_EQ(it->first, 5); EXPECT_EQ(it->second, "five"); ++it;
  EXPECT_EQ(it->first, 4); EXPECT_EQ(it->second, "four"); ++it;
  EXPECT_EQ(it->first, 3); EXPECT_EQ(it->second, "three"); ++it;
  EXPECT_EQ(it->first, 2); EXPECT_EQ(it->second, "two"); ++it;
  EXPECT_EQ(it->first, 1); EXPECT_EQ(it->second, "one");
}

TEST(Map, CustomComparatorFind) {
  map<int, std::string, greater<int>> m = {{1, "a"}, {10, "j"}, {5, "e"}};
  auto it = m.find(5);
  EXPECT_NE(it, m.end());
  EXPECT_EQ(it->second, "e");
}

TEST(Map, CustomComparatorKeyComp) {
  map<int, std::string, greater<int>> m;
  auto comp = m.key_comp();
  EXPECT_TRUE(comp(5, 1));
  EXPECT_FALSE(comp(1, 5));
}

// ============================================================================
// Iterator stability
// ============================================================================

TEST(Map, IteratorStabilityAfterInsert) {
  MapIS m = {{1, "a"}, {3, "c"}, {5, "e"}};
  auto it = m.find(3);
  EXPECT_EQ(it->second, "c");

  // Insert new elements
  m[2] = "b";
  m[4] = "d";
  m[6] = "f";
  m[0] = "z";

  // Iterator to existing element should still be valid
  EXPECT_EQ(it->first, 3);
  EXPECT_EQ(it->second, "c");
}

TEST(Map, IteratorStabilityAfterEraseOther) {
  MapIS m = {{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}};
  auto it = m.find(3);

  m.erase(1);  // erase a different element

  // Iterator to 3 should still be valid
  EXPECT_EQ(it->first, 3);
  EXPECT_EQ(it->second, "c");
}

// ============================================================================
// Comparison operators
// ============================================================================

TEST(Map, Equality) {
  MapIS a = {{1, "a"}, {2, "b"}};
  MapIS b = {{1, "a"}, {2, "b"}};
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
}

TEST(Map, InequalityDifferentValue) {
  MapIS a = {{1, "a"}};
  MapIS b = {{1, "different"}};
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
}

TEST(Map, InequalityDifferentSize) {
  MapIS a = {{1, "a"}};
  MapIS b = {{1, "a"}, {2, "b"}};
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
}

TEST(Map, LessThan) {
  MapIS a = {{1, "a"}};
  MapIS b = {{1, "a"}, {2, "b"}};
  EXPECT_TRUE(a < b);   // a is prefix of b

  MapIS c = {{2, "b"}}; // larger key
  EXPECT_TRUE(a < c);
}

TEST(Map, GreaterThan) {
  MapIS a = {{1, "a"}, {2, "b"}};
  MapIS b = {{1, "a"}};
  EXPECT_TRUE(a > b);
}

TEST(Map, CompareWithComparator) {
  map<int, std::string, greater<int>> a = {{3, "c"}, {1, "a"}};
  map<int, std::string, greater<int>> b = {{3, "c"}, {1, "a"}};
  EXPECT_TRUE(a == b);
}

// ============================================================================
// Large dataset
// ============================================================================

TEST(Map, LargeDataset1000) {
  MapIS m;
  const int N = 1000;

  // Insert in reverse order
  for (int i = N - 1; i >= 0; --i) {
    m[i] = "value_" + std::to_string(i);
  }

  EXPECT_EQ(m.size(), static_cast<size_t>(N));

  // Verify sorted iteration
  int expected = 0;
  for (const auto& kv : m) {
    EXPECT_EQ(kv.first, expected);
    EXPECT_EQ(kv.second, "value_" + std::to_string(expected));
    ++expected;
  }
  EXPECT_EQ(expected, N);

  // Verify all keys present
  for (int i = 0; i < N; ++i) {
    EXPECT_TRUE(m.contains(i));
  }

  // Erase half
  for (int i = 0; i < N; i += 2) {
    m.erase(i);
  }
  EXPECT_EQ(m.size(), static_cast<size_t>(N / 2));
  for (int i = 0; i < N; ++i) {
    EXPECT_EQ(m.contains(i), (i % 2 != 0));
  }
}

// ============================================================================
// Verify sorted iteration order
// ============================================================================

TEST(Map, SortedIterationOrder) {
  // Insert in random order
  MapIS m;
  int keys[] = {42, 7, 19, 3, 88, 56, 1, 100, 33, 67};
  for (int k : keys) {
    m[k] = std::to_string(k);
  }

  int prev = -1;
  for (const auto& kv : m) {
    EXPECT_GT(kv.first, prev);
    prev = kv.first;
  }
}

TEST(Map, SortedReverseIterationOrder) {
  MapIS m = {{3, "c"}, {1, "a"}, {5, "e"}, {2, "b"}, {4, "d"}};

  int prev = 999999;
  for (auto rit = m.rbegin(); rit != m.rend(); ++rit) {
    EXPECT_LT(rit->first, prev);
    prev = rit->first;
  }
}

// ============================================================================
// get_allocator()
// ============================================================================

TEST(Map, GetAllocator) {
  MapIS m;
  auto alloc = m.get_allocator();
  // Just verify it returns something callable
  EXPECT_TRUE(noexcept(m.get_allocator()));
  // The allocator should be usable
  (void)alloc;
}

// ============================================================================
// Mixed operations / stress
// ============================================================================

TEST(Map, MixedInsertEraseFind) {
  MapIS m;

  // Insert some values
  m[10] = "ten";
  m[20] = "twenty";
  m[30] = "thirty";
  EXPECT_EQ(m.size(), 3u);

  // Erase one
  m.erase(20);
  EXPECT_FALSE(m.contains(20));
  EXPECT_EQ(m.size(), 2u);

  // Find remaining
  EXPECT_EQ(m.at(10), "ten");
  EXPECT_EQ(m.at(30), "thirty");

  // Insert again (was erased)
  m[20] = "inserted_again";
  EXPECT_TRUE(m.contains(20));
  EXPECT_EQ(m[20], "inserted_again");
}

TEST(Map, ManyInsertEraseCycles) {
  MapIS m;
  for (int cycle = 0; cycle < 10; ++cycle) {
    for (int i = 0; i < 50; ++i) {
      m[i] = "c" + std::to_string(cycle) + "_" + std::to_string(i);
    }
    EXPECT_GE(m.size(), 50u);  // at least has keys 0-49
    for (int i = 0; i < 25; ++i) {
      m.erase(i);
    }
  }
  // Should still be consistent
  EXPECT_TRUE(m.contains(25));
  EXPECT_FALSE(m.contains(0));
}

TEST(Map, ValueUpdateViaIterator) {
  MapIS m = {{1, "old1"}, {2, "old2"}};
  auto it = m.find(1);
  it->second = "new1";
  EXPECT_EQ(m[1], "new1");
}
