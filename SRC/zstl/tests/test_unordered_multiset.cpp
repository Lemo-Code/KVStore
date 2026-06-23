// ============================================================================
// zstl unordered_multiset unit tests
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

TEST(UnorderedMultiset, DefaultConstructor) {
  unordered_multiset<int> s;
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);
}

TEST(UnorderedMultiset, BucketCountConstructor) {
  unordered_multiset<int> s(16);
  EXPECT_TRUE(s.empty());
  EXPECT_GE(s.bucket_count(), 16u);
}

TEST(UnorderedMultiset, InitializerListConstructor) {
  unordered_multiset<int> s = {1, 1, 2, 2, 2, 3};
  EXPECT_EQ(s.size(), 6u);
  EXPECT_EQ(s.count(1), 2u);
  EXPECT_EQ(s.count(2), 3u);
  EXPECT_EQ(s.count(3), 1u);
}

TEST(UnorderedMultiset, CopyConstructor) {
  unordered_multiset<int> s1 = {1, 1, 2, 3, 3, 3};
  unordered_multiset<int> s2(s1);
  EXPECT_EQ(s2.size(), 6u);
  EXPECT_EQ(s2.count(1), 2u);
  EXPECT_EQ(s2.count(2), 1u);
  EXPECT_EQ(s2.count(3), 3u);
  EXPECT_TRUE(s1 == s2);
}

TEST(UnorderedMultiset, MoveConstructor) {
  unordered_multiset<int> s1 = {1, 2, 3, 1, 2};
  unordered_multiset<int> s2(zstl::move(s1));
  EXPECT_EQ(s2.size(), 5u);
  EXPECT_EQ(s1.size(), 0u);
  EXPECT_TRUE(s1.empty());
}

TEST(UnorderedMultiset, CopyAssignment) {
  unordered_multiset<int> s1 = {5, 5, 5, 6};
  unordered_multiset<int> s2;
  s2 = s1;
  EXPECT_EQ(s2.size(), 4u);
  EXPECT_EQ(s2.count(5), 3u);
  EXPECT_EQ(s2.count(6), 1u);
}

TEST(UnorderedMultiset, MoveAssignment) {
  unordered_multiset<int> s1 = {7, 7, 8};
  unordered_multiset<int> s2;
  s2 = zstl::move(s1);
  EXPECT_EQ(s2.size(), 3u);
  EXPECT_EQ(s1.size(), 0u);
}

TEST(UnorderedMultiset, InitializerListAssignment) {
  unordered_multiset<int> s;
  s = {10, 10, 20, 30, 30, 30};
  EXPECT_EQ(s.size(), 6u);
  EXPECT_EQ(s.count(10), 2u);
  EXPECT_EQ(s.count(20), 1u);
  EXPECT_EQ(s.count(30), 3u);
}

// ============================================================================
// Insert duplicates
// ============================================================================

TEST(UnorderedMultiset, InsertSingle) {
  unordered_multiset<int> s;
  auto it = s.insert(42);
  EXPECT_NE(it, s.end());
  EXPECT_EQ(*it, 42);
  EXPECT_EQ(s.size(), 1u);
}

TEST(UnorderedMultiset, InsertDuplicates) {
  unordered_multiset<int> s;
  s.insert(1);
  s.insert(1);
  s.insert(1);
  s.insert(2);
  s.insert(2);

  EXPECT_EQ(s.size(), 5u);
  EXPECT_EQ(s.count(1), 3u);
  EXPECT_EQ(s.count(2), 2u);
  EXPECT_EQ(s.count(3), 0u);
}

TEST(UnorderedMultiset, InsertRange) {
  std::vector<int> vec = {1, 1, 1, 2, 2, 3};
  unordered_multiset<int> s;
  s.insert(vec.begin(), vec.end());
  EXPECT_EQ(s.size(), 6u);
  EXPECT_EQ(s.count(1), 3u);
  EXPECT_EQ(s.count(2), 2u);
  EXPECT_EQ(s.count(3), 1u);
}

TEST(UnorderedMultiset, InsertManyDuplicates) {
  unordered_multiset<int> s;
  const int numKeys = 5;
  const int dupPerKey = 10;

  for (int k = 0; k < numKeys; ++k) {
    for (int d = 0; d < dupPerKey; ++d) {
      s.insert(k);
    }
  }

  EXPECT_EQ(s.size(), static_cast<size_t>(numKeys * dupPerKey));
  for (int k = 0; k < numKeys; ++k) {
    EXPECT_EQ(s.count(k), static_cast<size_t>(dupPerKey));
  }
}

// ============================================================================
// Emplace duplicates
// ============================================================================

TEST(UnorderedMultiset, EmplaceDuplicates) {
  unordered_multiset<int> s;
  s.emplace(7);
  s.emplace(7);
  s.emplace(7);
  s.emplace(8);

  EXPECT_EQ(s.size(), 4u);
  EXPECT_EQ(s.count(7), 3u);
  EXPECT_EQ(s.count(8), 1u);
}

TEST(UnorderedMultiset, EmplaceHint) {
  unordered_multiset<int> s;
  auto it = s.emplace_hint(s.begin(), 99);
  EXPECT_NE(it, s.end());
  EXPECT_EQ(*it, 99);

  s.emplace_hint(it, 99);  // duplicate
  EXPECT_EQ(s.count(99), 2u);
}

// ============================================================================
// count() for duplicates
// ============================================================================

TEST(UnorderedMultiset, CountEmpty) {
  unordered_multiset<int> s;
  EXPECT_EQ(s.count(0), 0u);
  EXPECT_EQ(s.count(100), 0u);
}

TEST(UnorderedMultiset, CountAfterInsert) {
  unordered_multiset<int> s;
  s.insert(5);
  EXPECT_EQ(s.count(5), 1u);
  s.insert(5);
  EXPECT_EQ(s.count(5), 2u);
  s.insert(5);
  EXPECT_EQ(s.count(5), 3u);
  s.insert(6);
  EXPECT_EQ(s.count(5), 3u);
  EXPECT_EQ(s.count(6), 1u);
}

TEST(UnorderedMultiset, CountAfterErase) {
  unordered_multiset<int> s = {1, 1, 1, 2, 2, 3};
  EXPECT_EQ(s.count(1), 3u);
  s.erase(1);
  EXPECT_EQ(s.count(1), 0u);
  EXPECT_EQ(s.count(2), 2u);
  EXPECT_EQ(s.count(3), 1u);
}

// ============================================================================
// equal_range() for duplicates
// ============================================================================

TEST(UnorderedMultiset, EqualRangeMultipleMatch) {
  unordered_multiset<int> s = {5, 5, 5, 10, 10};
  auto range = s.equal_range(5);
  int cnt = 0;
  for (auto it = range.first; it != range.second; ++it) {
    EXPECT_EQ(*it, 5);
    ++cnt;
  }
  EXPECT_EQ(cnt, 3);
}

TEST(UnorderedMultiset, EqualRangeNoMatch) {
  unordered_multiset<int> s = {1, 2, 3};
  auto range = s.equal_range(99);
  EXPECT_EQ(range.first, range.second);
}

TEST(UnorderedMultiset, EqualRangeEmpty) {
  unordered_multiset<int> s;
  auto range = s.equal_range(0);
  EXPECT_EQ(range.first, range.second);
}

TEST(UnorderedMultiset, EqualRangeConst) {
  unordered_multiset<int> s = {42, 42, 42};
  const auto& cs = s;
  auto range = cs.equal_range(42);
  int cnt = 0;
  for (auto it = range.first; it != range.second; ++it) {
    ++cnt;
  }
  EXPECT_EQ(cnt, 3);
}

// ============================================================================
// Erase by key and by iterator
// ============================================================================

TEST(UnorderedMultiset, EraseByKeyAllMatching) {
  unordered_multiset<int> s = {1, 1, 1, 2, 2, 3};
  size_t erased = s.erase(1);
  EXPECT_EQ(erased, 3u);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(s.count(1), 0u);
  EXPECT_EQ(s.count(2), 2u);
  EXPECT_EQ(s.count(3), 1u);
}

TEST(UnorderedMultiset, EraseByIterator) {
  unordered_multiset<int> s = {10, 10, 10, 20};
  auto it = s.find(10);
  EXPECT_NE(it, s.end());
  auto next = s.erase(it);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(s.count(10), 2u);
  EXPECT_NE(next, s.end());
}

TEST(UnorderedMultiset, EraseRange) {
  unordered_multiset<int> s;
  for (int i = 0; i < 15; ++i) {
    s.insert(i % 5);
  }
  auto range = s.equal_range(2);
  size_t before = s.size();
  s.erase(range.first, range.second);
  EXPECT_EQ(s.count(2), 0u);
  EXPECT_LT(s.size(), before);
}

TEST(UnorderedMultiset, EraseNonExistent) {
  unordered_multiset<int> s = {1, 2, 3};
  size_t erased = s.erase(999);
  EXPECT_EQ(erased, 0u);
  EXPECT_EQ(s.size(), 3u);
}

// ============================================================================
// find() and contains()
// ============================================================================

TEST(UnorderedMultiset, FindExisting) {
  unordered_multiset<int> s = {1, 1, 2, 3};
  auto it = s.find(1);
  EXPECT_NE(it, s.end());
  EXPECT_EQ(*it, 1);
}

TEST(UnorderedMultiset, FindNonExisting) {
  unordered_multiset<int> s = {1, 2, 3};
  auto it = s.find(999);
  EXPECT_EQ(it, s.end());
}

TEST(UnorderedMultiset, FindConst) {
  unordered_multiset<int> s = {42};
  const auto& cs = s;
  auto it = cs.find(42);
  EXPECT_NE(it, cs.end());
  EXPECT_EQ(*it, 42);
}

TEST(UnorderedMultiset, Contains) {
  unordered_multiset<int> s = {1, 1, 2};
  EXPECT_TRUE(s.contains(1));
  EXPECT_TRUE(s.contains(2));
  EXPECT_FALSE(s.contains(0));
  EXPECT_FALSE(s.contains(99));
}

// ============================================================================
// Bucket interface
// ============================================================================

TEST(UnorderedMultiset, BucketCount) {
  unordered_multiset<int> s(64);
  EXPECT_GE(s.bucket_count(), 64u);
}

TEST(UnorderedMultiset, BucketSizeSum) {
  unordered_multiset<int> s = {1, 1, 2, 2, 2};
  size_t total = 0;
  for (size_t i = 0; i < s.bucket_count(); ++i) {
    total += s.bucket_size(i);
  }
  EXPECT_EQ(total, s.size());
}

TEST(UnorderedMultiset, BucketOfKey) {
  unordered_multiset<int> s;
  size_t b = s.bucket(42);
  EXPECT_LT(b, s.bucket_count());
}

TEST(UnorderedMultiset, MaxBucketCount) {
  unordered_multiset<int> s;
  EXPECT_GT(s.max_bucket_count(), 0u);
}

// ============================================================================
// Hash policy: rehash, reserve, load_factor
// ============================================================================

TEST(UnorderedMultiset, RehashPreservesDuplicates) {
  unordered_multiset<int> s = {1, 1, 1, 2, 2, 3};
  size_t old_size = s.size();
  s.rehash(256);
  EXPECT_EQ(s.size(), old_size);
  EXPECT_GE(s.bucket_count(), 256u);
  EXPECT_EQ(s.count(1), 3u);
  EXPECT_EQ(s.count(2), 2u);
  EXPECT_EQ(s.count(3), 1u);
}

TEST(UnorderedMultiset, Reserve) {
  unordered_multiset<int> s;
  s.reserve(50);
  EXPECT_GT(s.bucket_count(), 0u);
}

TEST(UnorderedMultiset, LoadFactor) {
  unordered_multiset<int> s;
  EXPECT_EQ(s.load_factor(), 0.0f);
  s.insert(1);
  s.insert(1);
  EXPECT_GT(s.load_factor(), 0.0f);

  float mlf = s.max_load_factor();
  EXPECT_GT(mlf, 0.0f);
  s.max_load_factor(1.5f);
  EXPECT_EQ(s.max_load_factor(), 1.5f);
}

TEST(UnorderedMultiset, HashFunctionAndKeyEq) {
  unordered_multiset<int> s;
  auto hf = s.hash_function();
  auto ke = s.key_eq();
  EXPECT_EQ(hf(10), hf(10));
  EXPECT_TRUE(ke(5, 5));
  EXPECT_FALSE(ke(5, 6));
}

// ============================================================================
// empty, size, clear
// ============================================================================

TEST(UnorderedMultiset, EmptySizeClear) {
  unordered_multiset<int> s;
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);

  s.insert(1);
  s.insert(1);
  s.insert(2);
  EXPECT_FALSE(s.empty());
  EXPECT_EQ(s.size(), 3u);

  s.clear();
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);
}

TEST(UnorderedMultiset, MaxSize) {
  unordered_multiset<int> s;
  EXPECT_GT(s.max_size(), 0u);
}

// ============================================================================
// swap
// ============================================================================

TEST(UnorderedMultiset, SwapMember) {
  unordered_multiset<int> s1 = {1, 1, 1, 2};
  unordered_multiset<int> s2 = {10, 20, 20};

  s1.swap(s2);
  EXPECT_EQ(s1.size(), 3u);
  EXPECT_EQ(s2.size(), 4u);
  EXPECT_EQ(s1.count(10), 1u);
  EXPECT_EQ(s1.count(20), 2u);
  EXPECT_EQ(s2.count(1), 3u);
}

TEST(UnorderedMultiset, SwapFreeFunction) {
  unordered_multiset<int> s1 = {5, 5};
  unordered_multiset<int> s2 = {6, 6, 6};
  zstl::swap(s1, s2);
  EXPECT_EQ(s1.size(), 3u);
  EXPECT_EQ(s2.size(), 2u);
}

// ============================================================================
// merge
// ============================================================================

TEST(UnorderedMultiset, Merge) {
  unordered_multiset<int> src = {1, 1, 3, 3};
  unordered_multiset<int> dst = {2, 2, 3};

  dst.merge(src);
  EXPECT_TRUE(src.empty());
  EXPECT_EQ(dst.size(), 7u);
  EXPECT_EQ(dst.count(1), 2u);
  EXPECT_EQ(dst.count(2), 2u);
  EXPECT_EQ(dst.count(3), 3u);
}

// ============================================================================
// Comparison operators
// ============================================================================

TEST(UnorderedMultiset, Equality) {
  unordered_multiset<int> s1 = {1, 1, 2, 3, 3, 3};
  unordered_multiset<int> s2 = {1, 1, 2, 3, 3, 3};
  unordered_multiset<int> s3 = {1, 2, 3};
  unordered_multiset<int> s4 = {1, 1, 2, 2};

  EXPECT_TRUE(s1 == s2);
  EXPECT_FALSE(s1 != s2);
  EXPECT_FALSE(s1 == s3);
  EXPECT_TRUE(s1 != s3);
  EXPECT_FALSE(s1 == s4);
  EXPECT_TRUE(s1 != s4);
}

TEST(UnorderedMultiset, EqualityEmpty) {
  unordered_multiset<int> s1, s2;
  EXPECT_TRUE(s1 == s2);
}

TEST(UnorderedMultiset, EqualityDifferentCounts) {
  unordered_multiset<int> s1 = {1, 1};
  unordered_multiset<int> s2 = {1};
  EXPECT_FALSE(s1 == s2);
}

// ============================================================================
// Iteration over all elements
// ============================================================================

TEST(UnorderedMultiset, IterationVisitsAll) {
  unordered_multiset<int> s = {10, 10, 10, 20, 20};
  size_t count = 0;
  int count10 = 0;
  int count20 = 0;
  for (const auto& val : s) {
    ++count;
    if (val == 10) ++count10;
    if (val == 20) ++count20;
  }
  EXPECT_EQ(count, 5u);
  EXPECT_EQ(count10, 3);
  EXPECT_EQ(count20, 2);
}

TEST(UnorderedMultiset, IteratorForward) {
  unordered_multiset<int> s = {1, 1, 2};
  auto it = s.begin();
  EXPECT_NE(it, s.end());
  ++it;
  EXPECT_NE(it, s.end());
  ++it;
  EXPECT_NE(it, s.end());
  ++it;
  EXPECT_EQ(it, s.end());
}

TEST(UnorderedMultiset, ConstIteration) {
  const unordered_multiset<int> s = {1, 1, 2, 2, 2};
  size_t count = 0;
  for (auto it = s.cbegin(); it != s.cend(); ++it) {
    ++count;
  }
  EXPECT_EQ(count, 5u);
}

// ============================================================================
// Verify duplicates preserved through operations
// ============================================================================

TEST(UnorderedMultiset, DuplicatesPreservedAfterRehash) {
  unordered_multiset<int> s;
  for (int i = 0; i < 100; ++i) {
    s.insert(i % 10);
  }
  EXPECT_EQ(s.size(), 100u);
  s.rehash(512);
  EXPECT_EQ(s.size(), 100u);
  for (int k = 0; k < 10; ++k) {
    EXPECT_EQ(s.count(k), 10u);
  }
}

TEST(UnorderedMultiset, DuplicatesPreservedAfterSwap) {
  unordered_multiset<int> s1;
  for (int i = 0; i < 20; ++i) {
    s1.insert(i % 4);
  }
  unordered_multiset<int> s2;
  s1.swap(s2);
  EXPECT_EQ(s2.size(), 20u);
  for (int k = 0; k < 4; ++k) {
    EXPECT_EQ(s2.count(k), 5u);
  }
}

TEST(UnorderedMultiset, LargeRehashPreservesAll) {
  unordered_multiset<int> s;
  for (int i = 0; i < 300; ++i) {
    s.insert(i % 15);
  }
  EXPECT_EQ(s.size(), 300u);
  s.rehash(1024);
  EXPECT_EQ(s.size(), 300u);
  for (int k = 0; k < 15; ++k) {
    EXPECT_EQ(s.count(k), 20u);
  }
}
