// ============================================================================
// zstl unordered_multimap unit tests
// ============================================================================
// Tests for: insert (duplicate keys), emplace, erase (by key & iterator),
//           count, equal_range, find, contains, bucket interface, rehash,
//           reserve, load_factor, hash_function, key_eq, merge, swap,
//           empty, size, clear, iteration, comparison operators.
// Covers: empty, single, multiple duplicates, large dataset, edge cases.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>
#include <vector>

using namespace zstl;

// ============================================================================
// Constructors & Basic Properties
// ============================================================================

TEST(UnorderedMultimap, DefaultConstructor) {
  unordered_multimap<int, std::string> m;
  EXPECT_TRUE(m.empty());
  EXPECT_EQ(m.size(), 0u);
}

TEST(UnorderedMultimap, BucketCountConstructor) {
  unordered_multimap<int, std::string> m(16);
  EXPECT_TRUE(m.empty());
  EXPECT_GE(m.bucket_count(), 16u);
}

TEST(UnorderedMultimap, InitializerListConstructor) {
  unordered_multimap<int, std::string> m = {
    {1, "one"}, {2, "two"}, {1, "also one"}, {3, "three"}, {2, "also two"}
  };
  EXPECT_EQ(m.size(), 5u);
  EXPECT_EQ(m.count(1), 2u);
  EXPECT_EQ(m.count(2), 2u);
  EXPECT_EQ(m.count(3), 1u);
}

TEST(UnorderedMultimap, CopyConstructor) {
  unordered_multimap<int, std::string> m1 = {
    {1, "a"}, {1, "b"}, {2, "c"}
  };
  unordered_multimap<int, std::string> m2(m1);
  EXPECT_EQ(m2.size(), 3u);
  EXPECT_EQ(m2.count(1), 2u);
  EXPECT_EQ(m2.count(2), 1u);
  EXPECT_TRUE(m1 == m2);
}

TEST(UnorderedMultimap, MoveConstructor) {
  unordered_multimap<int, std::string> m1 = {
    {1, "a"}, {2, "b"}, {3, "c"}
  };
  unordered_multimap<int, std::string> m2(zstl::move(m1));
  EXPECT_EQ(m2.size(), 3u);
  EXPECT_EQ(m1.size(), 0u);
  EXPECT_TRUE(m1.empty());
}

TEST(UnorderedMultimap, CopyAssignment) {
  unordered_multimap<int, std::string> m1 = {{1, "a"}, {2, "b"}};
  unordered_multimap<int, std::string> m2;
  m2 = m1;
  EXPECT_EQ(m2.size(), 2u);
  EXPECT_EQ(m2.count(1), 1u);
}

TEST(UnorderedMultimap, MoveAssignment) {
  unordered_multimap<int, std::string> m1 = {{1, "a"}, {2, "b"}};
  unordered_multimap<int, std::string> m2;
  m2 = zstl::move(m1);
  EXPECT_EQ(m2.size(), 2u);
  EXPECT_EQ(m1.size(), 0u);
}

TEST(UnorderedMultimap, InitializerListAssignment) {
  unordered_multimap<int, std::string> m;
  m = {{5, "five"}, {5, "cinco"}, {6, "six"}};
  EXPECT_EQ(m.size(), 3u);
  EXPECT_EQ(m.count(5), 2u);
  EXPECT_EQ(m.count(6), 1u);
}

// ============================================================================
// Insert with duplicate keys
// ============================================================================

TEST(UnorderedMultimap, InsertSingle) {
  unordered_multimap<int, std::string> m;
  auto it = m.insert({1, "hello"});
  EXPECT_NE(it, m.end());
  EXPECT_EQ(it->first, 1);
  EXPECT_EQ(it->second, "hello");
  EXPECT_EQ(m.size(), 1u);
}

TEST(UnorderedMultimap, InsertDuplicateKeys) {
  unordered_multimap<int, std::string> m;
  m.insert({1, "first"});
  m.insert({1, "second"});
  m.insert({1, "third"});
  m.insert({2, "two"});
  m.insert({2, "deux"});

  EXPECT_EQ(m.size(), 5u);
  EXPECT_EQ(m.count(1), 3u);
  EXPECT_EQ(m.count(2), 2u);
  EXPECT_EQ(m.count(3), 0u);
}

TEST(UnorderedMultimap, InsertRange) {
  std::vector<pair<int, std::string>> vec = {
    {1, "a"}, {1, "b"}, {1, "c"}, {2, "d"}, {2, "e"}
  };
  unordered_multimap<int, std::string> m;
  m.insert(vec.begin(), vec.end());
  EXPECT_EQ(m.size(), 5u);
  EXPECT_EQ(m.count(1), 3u);
  EXPECT_EQ(m.count(2), 2u);
}

TEST(UnorderedMultimap, InsertManyDuplicatesSmallSet) {
  unordered_multimap<int, std::string> m;
  // Insert 3 copies of each of 4 keys
  for (int k = 0; k < 4; ++k) {
    for (int i = 0; i < 3; ++i) {
      m.insert({k, "val_" + std::to_string(k) + "_" + std::to_string(i)});
    }
  }
  EXPECT_EQ(m.size(), 12u);
  for (int k = 0; k < 4; ++k) {
    EXPECT_EQ(m.count(k), 3u);
  }
}

// ============================================================================
// Emplace duplicates
// ============================================================================

TEST(UnorderedMultimap, EmplaceDuplicateKeys) {
  unordered_multimap<int, std::string> m;
  m.emplace(1, "alpha");
  m.emplace(1, "beta");
  m.emplace(1, "gamma");
  m.emplace(2, "omega");

  EXPECT_EQ(m.size(), 4u);
  EXPECT_EQ(m.count(1), 3u);
  EXPECT_EQ(m.count(2), 1u);
}

TEST(UnorderedMultimap, EmplaceVsInsert) {
  unordered_multimap<int, std::string> m;
  // Insert 5, then emplace another 5 with the same key
  for (int i = 0; i < 5; ++i) {
    m.insert({42, "insert_" + std::to_string(i)});
  }
  for (int i = 0; i < 5; ++i) {
    m.emplace(42, "emplace_" + std::to_string(i));
  }
  EXPECT_EQ(m.size(), 10u);
  EXPECT_EQ(m.count(42), 10u);
}

// ============================================================================
// Erase
// ============================================================================

TEST(UnorderedMultimap, EraseByKeyAllMatching) {
  unordered_multimap<int, std::string> m = {
    {1, "a"}, {1, "b"}, {1, "c"}, {2, "d"}, {2, "e"}, {3, "f"}
  };
  EXPECT_EQ(m.size(), 6u);

  size_t erased = m.erase(1);
  EXPECT_EQ(erased, 3u);
  EXPECT_EQ(m.size(), 3u);
  EXPECT_EQ(m.count(1), 0u);
  EXPECT_EQ(m.count(2), 2u);
  EXPECT_EQ(m.count(3), 1u);

  erased = m.erase(2);
  EXPECT_EQ(erased, 2u);
  EXPECT_EQ(m.size(), 1u);
  EXPECT_EQ(m.count(2), 0u);

  erased = m.erase(999);  // non-existent key
  EXPECT_EQ(erased, 0u);
  EXPECT_EQ(m.size(), 1u);
}

TEST(UnorderedMultimap, EraseByIterator) {
  unordered_multimap<int, std::string> m = {
    {10, "x"}, {10, "y"}, {10, "z"}, {20, "w"}
  };
  EXPECT_EQ(m.size(), 4u);

  auto it = m.find(10);
  EXPECT_NE(it, m.end());
  auto next = m.erase(it);
  EXPECT_EQ(m.size(), 3u);
  EXPECT_EQ(m.count(10), 2u);
  // next should point to another element
  EXPECT_NE(next, m.end());
}

TEST(UnorderedMultimap, EraseRange) {
  unordered_multimap<int, std::string> m;
  for (int i = 0; i < 10; ++i) {
    m.insert({i % 3, "val_" + std::to_string(i)});
  }
  EXPECT_EQ(m.size(), 10u);

  // Erase all elements for key 1
  auto range = m.equal_range(1);
  auto it = m.erase(range.first, range.second);
  EXPECT_EQ(m.count(1), 0u);
  EXPECT_LT(m.size(), 10u);
}

TEST(UnorderedMultimap, EraseOnEmpty) {
  unordered_multimap<int, std::string> m;
  size_t erased = m.erase(42);
  EXPECT_EQ(erased, 0u);
  EXPECT_TRUE(m.empty());
}

// ============================================================================
// count()
// ============================================================================

TEST(UnorderedMultimap, CountEmpty) {
  unordered_multimap<int, std::string> m;
  EXPECT_EQ(m.count(0), 0u);
  EXPECT_EQ(m.count(42), 0u);
}

TEST(UnorderedMultimap, CountAfterInsert) {
  unordered_multimap<int, std::string> m;
  m.insert({7, "a"});
  EXPECT_EQ(m.count(7), 1u);
  m.insert({7, "b"});
  EXPECT_EQ(m.count(7), 2u);
  m.insert({7, "c"});
  EXPECT_EQ(m.count(7), 3u);
  m.insert({8, "d"});
  EXPECT_EQ(m.count(7), 3u);
  EXPECT_EQ(m.count(8), 1u);
}

TEST(UnorderedMultimap, CountAfterErase) {
  unordered_multimap<int, std::string> m = {
    {5, "a"}, {5, "b"}, {5, "c"}, {6, "d"}
  };
  EXPECT_EQ(m.count(5), 3u);
  m.erase(5);
  EXPECT_EQ(m.count(5), 0u);
  EXPECT_EQ(m.count(6), 1u);
}

// ============================================================================
// equal_range()
// ============================================================================

TEST(UnorderedMultimap, EqualRangeMultipleMatches) {
  unordered_multimap<int, std::string> m = {
    {1, "a"}, {1, "b"}, {1, "c"}, {2, "d"}
  };
  auto range = m.equal_range(1);
  int count = 0;
  for (auto it = range.first; it != range.second; ++it) {
    EXPECT_EQ(it->first, 1);
    ++count;
  }
  EXPECT_EQ(count, 3);
}

TEST(UnorderedMultimap, EqualRangeNoMatch) {
  unordered_multimap<int, std::string> m = {
    {1, "a"}, {2, "b"}
  };
  auto range = m.equal_range(99);
  EXPECT_EQ(range.first, range.second);
}

TEST(UnorderedMultimap, EqualRangeEmpty) {
  unordered_multimap<int, std::string> m;
  auto range = m.equal_range(0);
  EXPECT_EQ(range.first, range.second);
}

TEST(UnorderedMultimap, EqualRangeConst) {
  unordered_multimap<int, std::string> m = {
    {10, "x"}, {10, "y"}, {20, "z"}
  };
  const auto& cm = m;
  auto range = cm.equal_range(10);
  int count = 0;
  for (auto it = range.first; it != range.second; ++it) {
    ++count;
  }
  EXPECT_EQ(count, 2);
}

// ============================================================================
// find() and contains()
// ============================================================================

TEST(UnorderedMultimap, FindExistingKey) {
  unordered_multimap<int, std::string> m = {
    {1, "a"}, {1, "b"}, {2, "c"}
  };
  auto it = m.find(1);
  EXPECT_NE(it, m.end());
  EXPECT_EQ(it->first, 1);
  // It returns the first matching element
}

TEST(UnorderedMultimap, FindNonExistingKey) {
  unordered_multimap<int, std::string> m = {
    {1, "a"}, {2, "b"}
  };
  auto it = m.find(999);
  EXPECT_EQ(it, m.end());
}

TEST(UnorderedMultimap, FindConst) {
  unordered_multimap<int, std::string> m = {{42, "answer"}};
  const auto& cm = m;
  auto it = cm.find(42);
  EXPECT_NE(it, cm.end());
  EXPECT_EQ(it->second, "answer");
}

TEST(UnorderedMultimap, Contains) {
  unordered_multimap<int, std::string> m = {
    {5, "a"}, {5, "b"}, {10, "c"}
  };
  EXPECT_TRUE(m.contains(5));
  EXPECT_TRUE(m.contains(10));
  EXPECT_FALSE(m.contains(0));
  EXPECT_FALSE(m.contains(999));
}

// ============================================================================
// Bucket interface
// ============================================================================

TEST(UnorderedMultimap, BucketCount) {
  unordered_multimap<int, std::string> m(32);
  EXPECT_GE(m.bucket_count(), 32u);
}

TEST(UnorderedMultimap, BucketSize) {
  unordered_multimap<int, std::string> m;
  m.insert({1, "a"});
  m.insert({2, "b"});
  // Sum of all bucket sizes should equal size()
  size_t total = 0;
  for (size_t i = 0; i < m.bucket_count(); ++i) {
    total += m.bucket_size(i);
  }
  EXPECT_EQ(total, m.size());
}

TEST(UnorderedMultimap, BucketFunction) {
  unordered_multimap<int, std::string> m;
  size_t b = m.bucket(42);
  EXPECT_LT(b, m.bucket_count());
}

TEST(UnorderedMultimap, MaxBucketCount) {
  unordered_multimap<int, std::string> m;
  EXPECT_GT(m.max_bucket_count(), 0u);
}

// ============================================================================
// Hash policy / rehash / reserve / load_factor
// ============================================================================

TEST(UnorderedMultimap, RehashWithDuplicatesPreserved) {
  unordered_multimap<int, std::string> m = {
    {1, "a"}, {1, "b"}, {1, "c"}, {2, "d"}, {2, "e"}, {3, "f"}
  };
  size_t old_size = m.size();
  m.rehash(128);
  EXPECT_EQ(m.size(), old_size);
  EXPECT_GE(m.bucket_count(), 128u);
  EXPECT_EQ(m.count(1), 3u);
  EXPECT_EQ(m.count(2), 2u);
  EXPECT_EQ(m.count(3), 1u);
}

TEST(UnorderedMultimap, Reserve) {
  unordered_multimap<int, std::string> m;
  m.reserve(100);
  EXPECT_GT(m.bucket_count(), 0u);
}

TEST(UnorderedMultimap, LoadFactor) {
  unordered_multimap<int, std::string> m;
  EXPECT_EQ(m.load_factor(), 0.0f);

  m.insert({1, "a"});
  m.insert({1, "b"});
  EXPECT_GT(m.load_factor(), 0.0f);

  float mlf = m.max_load_factor();
  EXPECT_GT(mlf, 0.0f);
  m.max_load_factor(2.0f);
  EXPECT_EQ(m.max_load_factor(), 2.0f);
}

TEST(UnorderedMultimap, HashFunctionAndKeyEq) {
  unordered_multimap<int, std::string> m;
  auto hf = m.hash_function();
  auto ke = m.key_eq();
  EXPECT_EQ(hf(42), hf(42));
  EXPECT_TRUE(ke(1, 1));
  EXPECT_FALSE(ke(1, 2));
}

// ============================================================================
// empty, size, clear, max_size
// ============================================================================

TEST(UnorderedMultimap, EmptySizeClear) {
  unordered_multimap<int, std::string> m;
  EXPECT_TRUE(m.empty());
  EXPECT_EQ(m.size(), 0u);

  m.insert({1, "a"});
  m.insert({2, "b"});
  m.insert({2, "c"});
  EXPECT_FALSE(m.empty());
  EXPECT_EQ(m.size(), 3u);

  m.clear();
  EXPECT_TRUE(m.empty());
  EXPECT_EQ(m.size(), 0u);
}

TEST(UnorderedMultimap, MaxSize) {
  unordered_multimap<int, std::string> m;
  EXPECT_GT(m.max_size(), 0u);
}

// ============================================================================
// swap
// ============================================================================

TEST(UnorderedMultimap, SwapMember) {
  unordered_multimap<int, std::string> m1 = {
    {1, "a"}, {1, "b"}, {2, "c"}
  };
  unordered_multimap<int, std::string> m2 = {
    {10, "x"}, {20, "y"}
  };

  m1.swap(m2);
  EXPECT_EQ(m1.size(), 2u);
  EXPECT_EQ(m2.size(), 3u);
  EXPECT_EQ(m2.count(1), 2u);
  EXPECT_EQ(m1.count(10), 1u);
}

TEST(UnorderedMultimap, SwapFreeFunction) {
  unordered_multimap<int, std::string> m1 = {{1, "a"}};
  unordered_multimap<int, std::string> m2 = {{2, "b"}, {2, "c"}};
  zstl::swap(m1, m2);
  EXPECT_EQ(m1.size(), 2u);
  EXPECT_EQ(m2.size(), 1u);
}

// ============================================================================
// merge
// ============================================================================

TEST(UnorderedMultimap, Merge) {
  unordered_multimap<int, std::string> src = {
    {1, "a"}, {1, "b"}, {3, "f"}
  };
  unordered_multimap<int, std::string> dst = {
    {2, "c"}, {2, "d"}, {3, "e"}
  };

  dst.merge(src);
  EXPECT_TRUE(src.empty());
  EXPECT_EQ(dst.size(), 6u);
  EXPECT_EQ(dst.count(1), 2u);
  EXPECT_EQ(dst.count(2), 2u);
  EXPECT_EQ(dst.count(3), 2u);
}

// ============================================================================
// Comparison operators
// ============================================================================

TEST(UnorderedMultimap, Equality) {
  unordered_multimap<int, std::string> m1 = {{1, "a"}, {1, "b"}, {2, "c"}};
  unordered_multimap<int, std::string> m2 = {{1, "a"}, {1, "b"}, {2, "c"}};
  unordered_multimap<int, std::string> m3 = {{1, "a"}, {2, "c"}};
  unordered_multimap<int, std::string> m4 = {{1, "a"}, {1, "b"}, {3, "d"}};

  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 == m3);
  EXPECT_TRUE(m1 != m3);
  EXPECT_FALSE(m1 == m4);
  EXPECT_TRUE(m1 != m4);
}

TEST(UnorderedMultimap, EqualityEmpty) {
  unordered_multimap<int, std::string> m1, m2;
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
}

// ============================================================================
// Iteration over all elements
// ============================================================================

TEST(UnorderedMultimap, IterationVisitsAll) {
  unordered_multimap<int, std::string> m = {
    {5, "a"}, {5, "b"}, {5, "c"}, {5, "d"},
    {7, "e"}, {7, "f"}
  };
  size_t count = 0;
  int count5 = 0;
  int count7 = 0;
  for (const auto& kv : m) {
    ++count;
    if (kv.first == 5) ++count5;
    if (kv.first == 7) ++count7;
  }
  EXPECT_EQ(count, 6u);
  EXPECT_EQ(count5, 4);
  EXPECT_EQ(count7, 2);
}

TEST(UnorderedMultimap, IteratorBeginEnd) {
  unordered_multimap<int, std::string> m = {{1, "x"}, {2, "y"}};
  auto it = m.begin();
  EXPECT_NE(it, m.end());
  ++it;
  EXPECT_NE(it, m.end());
  ++it;
  EXPECT_EQ(it, m.end());
}

TEST(UnorderedMultimap, ConstIteration) {
  const unordered_multimap<int, std::string> m = {
    {1, "a"}, {1, "b"}, {2, "c"}
  };
  size_t count = 0;
  for (auto it = m.cbegin(); it != m.cend(); ++it) {
    ++count;
  }
  EXPECT_EQ(count, 3u);
}

// ============================================================================
// Large dataset: insert many duplicates and verify all accessible
// ============================================================================

TEST(UnorderedMultimap, LargeManyDuplicates) {
  unordered_multimap<int, std::string> m;
  const int numKeys = 10;
  const int dupPerKey = 50;

  for (int k = 0; k < numKeys; ++k) {
    for (int d = 0; d < dupPerKey; ++d) {
      m.insert({k, "v" + std::to_string(d)});
    }
  }

  EXPECT_EQ(m.size(), static_cast<size_t>(numKeys * dupPerKey));
  for (int k = 0; k < numKeys; ++k) {
    EXPECT_EQ(m.count(k), static_cast<size_t>(dupPerKey));
  }

  // Verify all accessible through equal_range
  for (int k = 0; k < numKeys; ++k) {
    auto range = m.equal_range(k);
    int cnt = 0;
    for (auto it = range.first; it != range.second; ++it) {
      EXPECT_EQ(it->first, k);
      ++cnt;
    }
    EXPECT_EQ(cnt, dupPerKey);
  }
}

TEST(UnorderedMultimap, LargeRehashPreservesAll) {
  unordered_multimap<int, std::string> m;
  for (int i = 0; i < 500; ++i) {
    m.insert({i % 20, "value_" + std::to_string(i)});
  }
  EXPECT_EQ(m.size(), 500u);

  m.rehash(1024);
  EXPECT_EQ(m.size(), 500u);
  EXPECT_GE(m.bucket_count(), 1024u);

  // All keys accessible
  for (int k = 0; k < 20; ++k) {
    EXPECT_GT(m.count(k), 0u);
  }
}
