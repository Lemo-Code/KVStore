// ============================================================================
// zstl unordered_set container tests
// ============================================================================
// Tests for: zstl::unordered_set — hash table with unique keys, Robin Hood
//           open-addressing, power-of-2 sizing, max_load_factor 0.8
// Covers: constructors, insert (all overloads), emplace, try_emplace,
//         emplace_hint, erase, lookup (find, count, contains, equal_range),
//         bucket interface (bucket_count, bucket, bucket_size, begin(n)),
//         hash policy (load_factor, max_load_factor, rehash, reserve),
//         hash_function, key_eq, iteration, capacity, clear, swap, merge
// ============================================================================

#include <gtest/gtest.h>
#include "zstl/zstl.h"

#include <vector>
#include <string>

using namespace zstl;

// ============================================================================
// Constructor tests
// ============================================================================

TEST(UnorderedSet, DefaultConstructor) {
    unordered_set<int> s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_GT(s.bucket_count(), 0u);
}

TEST(UnorderedSet, BucketCountConstructor) {
    unordered_set<int> s(32);
    EXPECT_TRUE(s.empty());
    EXPECT_GE(s.bucket_count(), 32u);
}

TEST(UnorderedSet, RangeConstructor) {
    std::vector<int> data = {3, 1, 4, 1, 5};  // duplicate 1
    unordered_set<int> s(data.begin(), data.end());
    EXPECT_EQ(s.size(), 4u);  // unique elements: 1, 3, 4, 5
    EXPECT_TRUE(s.contains(1));
    EXPECT_TRUE(s.contains(3));
    EXPECT_TRUE(s.contains(4));
    EXPECT_TRUE(s.contains(5));
}

TEST(UnorderedSet, CopyConstructor) {
    unordered_set<int> s1;
    s1.insert(1);
    s1.insert(2);
    s1.insert(3);

    unordered_set<int> s2(s1);
    EXPECT_EQ(s2.size(), 3u);
    EXPECT_TRUE(s2.contains(1));
    EXPECT_TRUE(s2.contains(2));
    EXPECT_TRUE(s2.contains(3));

    // Deep copy
    s2.insert(4);
    EXPECT_EQ(s1.size(), 3u);
    EXPECT_EQ(s2.size(), 4u);
    EXPECT_FALSE(s1.contains(4));
}

TEST(UnorderedSet, MoveConstructor) {
    unordered_set<int> s1;
    s1.insert(1);
    s1.insert(2);

    unordered_set<int> s2(zstl::move(s1));
    EXPECT_EQ(s2.size(), 2u);
    EXPECT_TRUE(s2.contains(1));
    EXPECT_TRUE(s2.contains(2));
}

TEST(UnorderedSet, InitializerListConstructor) {
    unordered_set<int> s = {5, 2, 8, 2, 5, 1};  // duplicates should be ignored
    EXPECT_EQ(s.size(), 4u);
    EXPECT_TRUE(s.contains(1));
    EXPECT_TRUE(s.contains(2));
    EXPECT_TRUE(s.contains(5));
    EXPECT_TRUE(s.contains(8));
}

// ============================================================================
// Insert tests
// ============================================================================

TEST(UnorderedSet, InsertSingle) {
    unordered_set<int> s;
    auto [it, inserted] = s.insert(42);
    EXPECT_TRUE(inserted);
    EXPECT_EQ(*it, 42);
    EXPECT_EQ(s.size(), 1u);

    // Duplicate insertion fails
    auto [it2, inserted2] = s.insert(42);
    EXPECT_FALSE(inserted2);
    EXPECT_EQ(*it2, 42);
    EXPECT_EQ(s.size(), 1u);
}

TEST(UnorderedSet, InsertWithHint) {
    unordered_set<int> s = {1, 10, 20};
    auto hint = s.find(10);
    auto it = s.insert(hint, 5);
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(s.size(), 4u);

    // Duplicate with hint
    auto it2 = s.insert(hint, 10);
    EXPECT_EQ(*it2, 10);
    EXPECT_EQ(s.size(), 4u);  // size unchanged
}

TEST(UnorderedSet, InsertRange) {
    std::vector<int> data = {1, 2, 3, 2, 4, 1, 5};
    unordered_set<int> s;
    s.insert(data.begin(), data.end());
    EXPECT_EQ(s.size(), 5u);

    for (int i = 1; i <= 5; ++i) {
        EXPECT_TRUE(s.contains(i));
    }
}

TEST(UnorderedSet, InsertInitializerList) {
    unordered_set<int> s;
    s.insert({1, 2, 3, 1, 2});
    EXPECT_EQ(s.size(), 3u);
    EXPECT_TRUE(s.contains(1));
    EXPECT_TRUE(s.contains(2));
    EXPECT_TRUE(s.contains(3));
}

// ============================================================================
// Emplace tests
// ============================================================================

TEST(UnorderedSet, Emplace) {
    unordered_set<int> s;
    auto [it, inserted] = s.emplace(100);
    EXPECT_TRUE(inserted);
    EXPECT_EQ(*it, 100);

    // Duplicate emplace fails
    auto [it2, inserted2] = s.emplace(100);
    EXPECT_FALSE(inserted2);
    EXPECT_EQ(*it2, 100);
    EXPECT_EQ(s.size(), 1u);
}

TEST(UnorderedSet, EmplaceMultipleArgs) {
    // For types that require constructor args
    unordered_set<std::string> s;
    auto [it, inserted] = s.emplace("hello");
    EXPECT_TRUE(inserted);
    EXPECT_EQ(*it, "hello");
}

TEST(UnorderedSet, EmplaceHint) {
    unordered_set<int> s = {1, 5};
    auto hint = s.find(5);
    auto it = s.emplace_hint(hint, 3);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(s.size(), 3u);

    // Duplicate with hint
    auto it2 = s.emplace_hint(hint, 5);
    EXPECT_EQ(*it2, 5);
    EXPECT_EQ(s.size(), 3u);
}

// ============================================================================
// try_emplace tests
// ============================================================================

TEST(UnorderedSet, TryEmplaceInsertsWhenMissing) {
    unordered_set<int> s;
    auto [it, inserted] = s.try_emplace(42);
    EXPECT_TRUE(inserted);
    EXPECT_EQ(*it, 42);
}

TEST(UnorderedSet, TryEmplaceDoesNothingWhenPresent) {
    unordered_set<int> s = {10};
    auto [it, inserted] = s.try_emplace(10);
    EXPECT_FALSE(inserted);
    EXPECT_EQ(*it, 10);
    EXPECT_EQ(s.size(), 1u);
}

TEST(UnorderedSet, TryEmplaceMoveKey) {
    unordered_set<int> s;
    int key = 99;
    auto [it, inserted] = s.try_emplace(zstl::move(key));
    EXPECT_TRUE(inserted);
    EXPECT_EQ(*it, 99);
}

// ============================================================================
// Erase tests
// ============================================================================

TEST(UnorderedSet, EraseByKey) {
    unordered_set<int> s = {1, 2, 3, 4, 5};
    EXPECT_EQ(s.erase(3), 1u);
    EXPECT_EQ(s.size(), 4u);
    EXPECT_FALSE(s.contains(3));

    // Erase non-existent
    EXPECT_EQ(s.erase(99), 0u);
    EXPECT_EQ(s.size(), 4u);
}

TEST(UnorderedSet, EraseByIterator) {
    unordered_set<int> s = {1, 2, 3};
    auto it = s.find(2);
    ASSERT_NE(it, s.end());

    auto next = s.erase(it);
    EXPECT_EQ(s.size(), 2u);
    EXPECT_NE(next, s.end());  // next valid iterator
}

TEST(UnorderedSet, EraseRange) {
    unordered_set<int> s = {1, 2, 3, 4};
    auto first = s.find(2);
    auto last = s.find(4);
    s.erase(first, last);
    EXPECT_LT(s.size(), 4u);
}

// ============================================================================
// Lookup tests: find, count, contains
// ============================================================================

TEST(UnorderedSet, Find) {
    unordered_set<int> s = {10, 20, 30};

    auto it = s.find(20);
    EXPECT_NE(it, s.end());
    EXPECT_EQ(*it, 20);

    EXPECT_EQ(s.find(99), s.end());
}

TEST(UnorderedSet, FindConst) {
    const unordered_set<int> s = {1, 2, 3};
    auto it = s.find(2);
    EXPECT_NE(it, s.end());
    EXPECT_EQ(*it, 2);
}

TEST(UnorderedSet, Count) {
    unordered_set<int> s = {1, 2, 3};
    EXPECT_EQ(s.count(1), 1u);
    EXPECT_EQ(s.count(2), 1u);
    EXPECT_EQ(s.count(5), 0u);
}

TEST(UnorderedSet, Contains) {
    unordered_set<int> s = {1, 2, 3};
    EXPECT_TRUE(s.contains(1));
    EXPECT_TRUE(s.contains(2));
    EXPECT_TRUE(s.contains(3));
    EXPECT_FALSE(s.contains(0));
    EXPECT_FALSE(s.contains(4));
}

TEST(UnorderedSet, EqualRange) {
    unordered_set<int> s = {1, 2, 3};
    auto range = s.equal_range(2);
    int count = 0;
    for (auto it = range.first; it != range.second; ++it) {
        EXPECT_EQ(*it, 2);
        ++count;
    }
    EXPECT_EQ(count, 1);

    auto range2 = s.equal_range(99);
    EXPECT_EQ(range2.first, range2.second);
}

// ============================================================================
// Bucket interface tests
// ============================================================================

TEST(UnorderedSet, BucketCount) {
    unordered_set<int> s;
    EXPECT_GT(s.bucket_count(), 0u);

    for (int i = 0; i < 20; ++i) s.insert(i);
    EXPECT_GE(s.bucket_count(), 20u);
}

TEST(UnorderedSet, MaxBucketCount) {
    unordered_set<int> s;
    EXPECT_GT(s.max_bucket_count(), 0u);
}

TEST(UnorderedSet, Bucket) {
    unordered_set<int> s(16);
    s.insert(1);
    s.insert(2);

    size_t b1 = s.bucket(1);
    size_t b2 = s.bucket(2);
    EXPECT_LT(b1, s.bucket_count());
    EXPECT_LT(b2, s.bucket_count());

    // Same key maps to same bucket
    EXPECT_EQ(s.bucket(1), b1);
}

TEST(UnorderedSet, BucketSize) {
    unordered_set<int> s;
    s.insert(1);
    s.insert(2);
    s.insert(3);

    size_t total = 0;
    for (size_t i = 0; i < s.bucket_count(); ++i) {
        total += s.bucket_size(i);
    }
    EXPECT_EQ(total, s.size());
}

TEST(UnorderedSet, BucketBeginEnd) {
    unordered_set<int> s;
    s.insert(1);
    s.insert(2);
    s.insert(3);

    size_t total = 0;
    for (size_t i = 0; i < s.bucket_count(); ++i) {
        for (auto it = s.begin(i); it != s.end(i); ++it) {
            ++total;
            EXPECT_TRUE(s.contains(*it));
        }
    }
    EXPECT_EQ(total, s.size());
}

TEST(UnorderedSet, ConstBucketBeginEnd) {
    const unordered_set<int> s = {1, 2, 3};

    size_t total = 0;
    for (size_t i = 0; i < s.bucket_count(); ++i) {
        for (auto it = s.cbegin(i); it != s.cend(i); ++it) {
            ++total;
        }
    }
    EXPECT_EQ(total, s.size());
}

// ============================================================================
// Hash policy tests: load_factor, max_load_factor, rehash, reserve
// ============================================================================

TEST(UnorderedSet, LoadFactor) {
    unordered_set<int> s;
    EXPECT_FLOAT_EQ(s.load_factor(), 0.0f);

    s.insert(1);
    EXPECT_GT(s.load_factor(), 0.0f);
    EXPECT_LE(s.load_factor(), s.max_load_factor() + 0.01f);
}

TEST(UnorderedSet, MaxLoadFactor) {
    unordered_set<int> s;
    float default_mlf = s.max_load_factor();
    EXPECT_GT(default_mlf, 0.0f);

    s.max_load_factor(0.5f);
    EXPECT_FLOAT_EQ(s.max_load_factor(), 0.5f);

    // Restore
    s.max_load_factor(default_mlf);
    EXPECT_FLOAT_EQ(s.max_load_factor(), default_mlf);
}

TEST(UnorderedSet, RehashPreservesElements) {
    unordered_set<int> s;
    for (int i = 0; i < 100; ++i) {
        s.insert(i);
    }
    EXPECT_EQ(s.size(), 100u);

    size_t old_bc = s.bucket_count();
    s.rehash(500);
    EXPECT_GE(s.bucket_count(), 500u);
    EXPECT_GT(s.bucket_count(), old_bc);
    EXPECT_EQ(s.size(), 100u);  // all preserved

    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(s.contains(i));
    }
}

TEST(UnorderedSet, RehashSmaller) {
    unordered_set<int> s;
    for (int i = 0; i < 50; ++i) {
        s.insert(i);
    }
    size_t old_size = s.size();
    s.rehash(8);
    EXPECT_EQ(s.size(), old_size);
    for (int i = 0; i < 50; ++i) {
        EXPECT_TRUE(s.contains(i));
    }
}

TEST(UnorderedSet, Reserve) {
    unordered_set<int> s;
    s.reserve(200);
    size_t bc = s.bucket_count();
    EXPECT_GE(bc * s.max_load_factor(), 200u);

    for (int i = 0; i < 200; ++i) {
        s.insert(i);
    }
    EXPECT_EQ(s.size(), 200u);
    for (int i = 0; i < 200; ++i) {
        EXPECT_TRUE(s.contains(i));
    }
}

// ============================================================================
// Hash function and key equality tests
// ============================================================================

TEST(UnorderedSet, HashFunction) {
    unordered_set<int> s;
    auto hf = s.hash_function();
    size_t h1 = hf(42);
    size_t h2 = hf(42);
    EXPECT_EQ(h1, h2);

    // Different values may or may not hash to same value, just verify it compiles
    size_t h3 = hf(43);
    (void)h3;
}

TEST(UnorderedSet, KeyEq) {
    unordered_set<int> s;
    auto eq = s.key_eq();
    EXPECT_TRUE(eq(10, 10));
    EXPECT_FALSE(eq(10, 20));
}

// ============================================================================
// Iteration tests
// ============================================================================

TEST(UnorderedSet, BeginEndIteration) {
    unordered_set<int> s = {3, 1, 2};
    size_t count = 0;
    for (auto it = s.begin(); it != s.end(); ++it) {
        ++count;
        EXPECT_TRUE(*it >= 1 && *it <= 3);
    }
    EXPECT_EQ(count, s.size());
}

TEST(UnorderedSet, ConstIteration) {
    const unordered_set<int> s = {1, 2, 3};
    size_t count = 0;
    for (auto it = s.cbegin(); it != s.cend(); ++it) {
        ++count;
    }
    EXPECT_EQ(count, 3u);
}

TEST(UnorderedSet, RangeBasedFor) {
    unordered_set<int> s = {10, 20, 30};
    size_t count = 0;
    for (const auto& val : s) {
        EXPECT_TRUE(val == 10 || val == 20 || val == 30);
        ++count;
    }
    EXPECT_EQ(count, 3u);
}

// ============================================================================
// Capacity tests
// ============================================================================

TEST(UnorderedSet, Empty) {
    unordered_set<int> s;
    EXPECT_TRUE(s.empty());
    s.insert(1);
    EXPECT_FALSE(s.empty());
    s.erase(1);
    EXPECT_TRUE(s.empty());
}

TEST(UnorderedSet, Size) {
    unordered_set<int> s;
    EXPECT_EQ(s.size(), 0u);
    s.insert(1);
    EXPECT_EQ(s.size(), 1u);
    s.insert(2);
    EXPECT_EQ(s.size(), 2u);
    s.erase(1);
    EXPECT_EQ(s.size(), 1u);
}

TEST(UnorderedSet, MaxSize) {
    unordered_set<int> s;
    EXPECT_GT(s.max_size(), 0u);
}

// ============================================================================
// Clear and swap tests
// ============================================================================

TEST(UnorderedSet, Clear) {
    unordered_set<int> s = {1, 2, 3, 4, 5};
    EXPECT_EQ(s.size(), 5u);
    s.clear();
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_EQ(s.find(1), s.end());

    s.insert(10);
    EXPECT_EQ(s.size(), 1u);
}

TEST(UnorderedSet, SwapMember) {
    unordered_set<int> s1 = {1, 2, 3};
    unordered_set<int> s2 = {10, 20};

    s1.swap(s2);
    EXPECT_EQ(s1.size(), 2u);
    EXPECT_TRUE(s1.contains(10));
    EXPECT_TRUE(s1.contains(20));

    EXPECT_EQ(s2.size(), 3u);
    EXPECT_TRUE(s2.contains(1));
    EXPECT_TRUE(s2.contains(2));
    EXPECT_TRUE(s2.contains(3));
}

TEST(UnorderedSet, SwapFreeFunction) {
    unordered_set<int> s1 = {1};
    unordered_set<int> s2 = {2};
    zstl::swap(s1, s2);
    EXPECT_TRUE(s1.contains(2));
    EXPECT_TRUE(s2.contains(1));
}

// ============================================================================
// Merge tests
// ============================================================================

TEST(UnorderedSet, Merge) {
    unordered_set<int> s1 = {1, 2, 3};
    unordered_set<int> s2 = {3, 4, 5};  // key 3 overlaps

    s1.merge(s2);

    EXPECT_EQ(s1.size(), 5u);  // 1, 2, 3, 4, 5
    EXPECT_TRUE(s1.contains(1));
    EXPECT_TRUE(s1.contains(2));
    EXPECT_TRUE(s1.contains(3));
    EXPECT_TRUE(s1.contains(4));
    EXPECT_TRUE(s1.contains(5));

    // Key 3 stays in s2 since it already existed in s1
    EXPECT_EQ(s2.size(), 1u);
    EXPECT_TRUE(s2.contains(3));
}

// ============================================================================
// Many elements / rehash trigger tests
// ============================================================================

TEST(UnorderedSet, ManyElementsTriggerRehash) {
    unordered_set<int> s;
    const int N = 1000;

    for (int i = 0; i < N; ++i) {
        s.insert(i);
    }
    EXPECT_EQ(s.size(), static_cast<size_t>(N));

    for (int i = 0; i < N; ++i) {
        EXPECT_TRUE(s.contains(i));
    }
}

TEST(UnorderedSet, ManyInsertionsStability) {
    unordered_set<int> s;
    const int N = 500;

    for (int i = 0; i < N; ++i) {
        s.insert(i);
    }
    EXPECT_EQ(s.size(), static_cast<size_t>(N));

    // Delete half
    for (int i = 0; i < N; i += 2) {
        s.erase(i);
    }
    EXPECT_EQ(s.size(), static_cast<size_t>(N / 2));

    // Verify remaining odds
    for (int i = 1; i < N; i += 2) {
        EXPECT_TRUE(s.contains(i));
    }

    // Insert more to force rehashes
    for (int i = N; i < N + 200; ++i) {
        s.insert(i);
    }
    EXPECT_EQ(s.size(), static_cast<size_t>(N / 2 + 200));

    // Verify all
    for (int i = 1; i < N; i += 2) {
        EXPECT_TRUE(s.contains(i));
    }
    for (int i = N; i < N + 200; ++i) {
        EXPECT_TRUE(s.contains(i));
    }
}

// ============================================================================
// String key tests
// ============================================================================

TEST(UnorderedSet, StringKeys) {
    unordered_set<std::string> s;
    s.insert("hello");
    s.insert("world");
    s.insert("zstl");

    EXPECT_EQ(s.size(), 3u);
    EXPECT_TRUE(s.contains("hello"));
    EXPECT_TRUE(s.contains("world"));
    EXPECT_TRUE(s.contains("zstl"));
    EXPECT_FALSE(s.contains("missing"));

    s.erase("world");
    EXPECT_EQ(s.size(), 2u);
    EXPECT_FALSE(s.contains("world"));
}

// ============================================================================
// Comparison tests
// ============================================================================

TEST(UnorderedSet, Equality) {
    unordered_set<int> s1 = {1, 2, 3};
    unordered_set<int> s2 = {3, 2, 1};  // different insertion order
    unordered_set<int> s3 = {1, 2};
    unordered_set<int> s4 = {1, 2, 4};

    EXPECT_TRUE(s1 == s2);    // same elements regardless of order
    EXPECT_FALSE(s1 == s3);   // different size
    EXPECT_FALSE(s1 == s4);   // same size, different elements
    EXPECT_TRUE(s1 != s3);
    EXPECT_TRUE(s1 != s4);
}

// ============================================================================
// Assignment tests
// ============================================================================

TEST(UnorderedSet, CopyAssignment) {
    unordered_set<int> s1 = {1, 2, 3};
    unordered_set<int> s2;
    s2 = s1;
    EXPECT_EQ(s2.size(), 3u);
    EXPECT_TRUE(s2.contains(2));
}

TEST(UnorderedSet, MoveAssignment) {
    unordered_set<int> s1 = {1, 2};
    unordered_set<int> s2;
    s2 = zstl::move(s1);
    EXPECT_EQ(s2.size(), 2u);
}

TEST(UnorderedSet, InitializerListAssignment) {
    unordered_set<int> s;
    s = {1, 2, 3, 1};  // duplicate 1
    EXPECT_EQ(s.size(), 3u);
    EXPECT_TRUE(s.contains(1));
    EXPECT_TRUE(s.contains(2));
    EXPECT_TRUE(s.contains(3));
}

// ============================================================================
// Get allocator test
// ============================================================================

TEST(UnorderedSet, GetAllocator) {
    unordered_set<int> s;
    auto alloc = s.get_allocator();
    (void)alloc;
    EXPECT_TRUE(true);
}
