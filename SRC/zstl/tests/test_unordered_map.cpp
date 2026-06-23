// ============================================================================
// zstl unordered_map container tests
// ============================================================================
// Tests for: zstl::unordered_map — hash table with unique keys, Robin Hood
//           open-addressing, power-of-2 sizing, max_load_factor 0.8
// Covers: constructors, operator[], at(), insert (all overloads), emplace,
//         try_emplace, insert_or_assign, erase, lookup (find, count,
//         contains, equal_range), bucket interface, hash policy
//         (load_factor, max_load_factor, rehash, reserve), hash_function,
//         key_eq, iteration, capacity, clear, swap, merge
// ============================================================================

#include <gtest/gtest.h>
#include "zstl/zstl.h"

#include <string>
#include <vector>
#include <utility>
#include <stdexcept>

using namespace zstl;

// ============================================================================
// Constructor tests
// ============================================================================

TEST(UnorderedMap, DefaultConstructor) {
    unordered_map<int, std::string> m;
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0u);
    EXPECT_GT(m.bucket_count(), 0u);
}

TEST(UnorderedMap, BucketCountConstructor) {
    unordered_map<int, std::string> m(16);
    EXPECT_TRUE(m.empty());
    EXPECT_GE(m.bucket_count(), 16u);
}

TEST(UnorderedMap, RangeConstructor) {
    std::vector<pair<int, std::string>> data = {
        {1, "one"}, {2, "two"}, {3, "three"}
    };
    unordered_map<int, std::string> m(data.begin(), data.end());
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m.at(1), "one");
    EXPECT_EQ(m.at(2), "two");
    EXPECT_EQ(m.at(3), "three");
}

TEST(UnorderedMap, CopyConstructor) {
    unordered_map<int, std::string> m1;
    m1[1] = "one";
    m1[2] = "two";
    m1[3] = "three";

    unordered_map<int, std::string> m2(m1);
    EXPECT_EQ(m2.size(), 3u);
    EXPECT_EQ(m2.at(1), "one");
    EXPECT_EQ(m2.at(2), "two");
    EXPECT_EQ(m2.at(3), "three");

    // Deep copy: modifying m2 doesn't affect m1
    m2[4] = "four";
    EXPECT_EQ(m1.size(), 3u);
    EXPECT_EQ(m2.size(), 4u);
    EXPECT_EQ(m1.find(4), m1.end());
}

TEST(UnorderedMap, MoveConstructor) {
    unordered_map<int, std::string> m1;
    m1[1] = "one";
    m1[2] = "two";

    unordered_map<int, std::string> m2(zstl::move(m1));
    EXPECT_EQ(m2.size(), 2u);
    EXPECT_EQ(m2.at(1), "one");
    EXPECT_EQ(m2.at(2), "two");
}

TEST(UnorderedMap, InitializerListConstructor) {
    unordered_map<int, std::string> m = {
        {1, "one"}, {2, "two"}, {3, "three"}
    };
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m.at(1), "one");
    EXPECT_EQ(m.at(2), "two");
    EXPECT_EQ(m.at(3), "three");
}

// ============================================================================
// operator[] tests
// ============================================================================

TEST(UnorderedMap, OperatorBracketInsert) {
    unordered_map<int, std::string> m;
    m[1] = "one";
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m[1], "one");

    // Insert another
    m[2] = "two";
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m[2], "two");
}

TEST(UnorderedMap, OperatorBracketDefaultConstructs) {
    unordered_map<int, std::string> m;
    // operator[] on missing key default-constructs a string
    std::string& ref = m[42];
    EXPECT_TRUE(ref.empty());
    EXPECT_EQ(m.size(), 1u);

    // Now assign
    ref = "answer";
    EXPECT_EQ(m[42], "answer");
}

TEST(UnorderedMap, OperatorBracketOverwrite) {
    unordered_map<int, std::string> m;
    m[1] = "first";
    m[1] = "second";
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m[1], "second");
}

TEST(UnorderedMap, OperatorBracketMoveKey) {
    unordered_map<int, std::string> m;
    int key = 5;
    m[zstl::move(key)] = "five";
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m[5], "five");
}

// ============================================================================
// at() tests
// ============================================================================

TEST(UnorderedMap, AtAccess) {
    unordered_map<int, std::string> m = {{1, "one"}, {2, "two"}};

    EXPECT_EQ(m.at(1), "one");
    EXPECT_EQ(m.at(2), "two");

    // at() allows modification
    m.at(1) = "ONE";
    EXPECT_EQ(m.at(1), "ONE");
}

TEST(UnorderedMap, AtThrowsOnMissing) {
    unordered_map<int, std::string> m = {{1, "one"}};
    EXPECT_THROW(m.at(99), std::out_of_range);
}

TEST(UnorderedMap, AtConst) {
    const unordered_map<int, std::string> m = {{1, "one"}, {2, "two"}};
    EXPECT_EQ(m.at(1), "one");
    EXPECT_EQ(m.at(2), "two");
    EXPECT_THROW(m.at(99), std::out_of_range);
}

// ============================================================================
// Insert tests
// ============================================================================

TEST(UnorderedMap, InsertSingle) {
    unordered_map<int, std::string> m;
    auto result = m.insert({1, "one"});
    EXPECT_TRUE(result.second);
    EXPECT_EQ(result.first->first, 1);
    EXPECT_EQ(result.first->second, "one");
    EXPECT_EQ(m.size(), 1u);

    // Insert duplicate key should fail
    auto result2 = m.insert({1, "another"});
    EXPECT_FALSE(result2.second);
    EXPECT_EQ(result2.first->first, 1);
    EXPECT_EQ(result2.first->second, "one");  // original value preserved
    EXPECT_EQ(m.size(), 1u);
}

TEST(UnorderedMap, InsertWithHint) {
    unordered_map<int, std::string> m = {{1, "one"}, {5, "five"}};
    auto hint = m.find(5);
    auto it = m.insert(hint, {3, "three"});
    EXPECT_EQ(it->first, 3);
    EXPECT_EQ(it->second, "three");
    EXPECT_EQ(m.size(), 3u);
}

TEST(UnorderedMap, InsertRange) {
    std::vector<pair<int, std::string>> data = {
        {1, "a"}, {2, "b"}, {3, "c"}, {1, "d"}  // duplicate 1 should be ignored
    };
    unordered_map<int, std::string> m;
    m.insert(data.begin(), data.end());
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m.at(1), "a");  // first insertion wins
    EXPECT_EQ(m.at(2), "b");
    EXPECT_EQ(m.at(3), "c");
}

TEST(UnorderedMap, InsertInitializerList) {
    unordered_map<int, std::string> m;
    m.insert({{1, "a"}, {2, "b"}, {2, "c"}});  // duplicate 2
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m.at(1), "a");
    EXPECT_EQ(m.at(2), "b");
}

// ============================================================================
// Emplace tests
// ============================================================================

TEST(UnorderedMap, Emplace) {
    unordered_map<int, std::string> m;
    auto [it, inserted] = m.emplace(1, "one");
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->first, 1);
    EXPECT_EQ(it->second, "one");

    // Duplicate key
    auto [it2, inserted2] = m.emplace(1, "uno");
    EXPECT_FALSE(inserted2);
    EXPECT_EQ(it2->second, "one");  // original preserved
}

TEST(UnorderedMap, EmplacePiecewise) {
    unordered_map<int, std::string> m;
    auto [it, inserted] = m.emplace(
        zstl::piecewise_construct,
        zstl::forward_as_tuple(42),
        zstl::forward_as_tuple("hello")
    );
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->first, 42);
    EXPECT_EQ(it->second, "hello");
}

TEST(UnorderedMap, EmplaceHint) {
    unordered_map<int, std::string> m = {{1, "one"}, {5, "five"}};
    auto hint = m.find(5);
    auto it = m.emplace_hint(hint, 3, "three");
    EXPECT_EQ(it->first, 3);
    EXPECT_EQ(it->second, "three");
}

// ============================================================================
// try_emplace tests
// ============================================================================

TEST(UnorderedMap, TryEmplaceInsertsWhenMissing) {
    unordered_map<int, std::string> m;
    auto [it, inserted] = m.try_emplace(1, "one");
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->first, 1);
    EXPECT_EQ(it->second, "one");
}

TEST(UnorderedMap, TryEmplaceDoesNothingWhenPresent) {
    unordered_map<int, std::string> m = {{1, "original"}};
    auto [it, inserted] = m.try_emplace(1, "new");
    EXPECT_FALSE(inserted);
    EXPECT_EQ(it->second, "original");  // value unchanged
}

TEST(UnorderedMap, TryEmplaceMoveKey) {
    unordered_map<int, std::string> m;
    int key = 10;
    auto [it, inserted] = m.try_emplace(zstl::move(key), "ten");
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->first, 10);
    EXPECT_EQ(it->second, "ten");
}

TEST(UnorderedMap, TryEmplaceMultipleArgsForValue) {
    unordered_map<int, std::string> m;
    auto [it, inserted] = m.try_emplace(1, 5, 'x');  // string(5, 'x')
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->second, "xxxxx");
}

// ============================================================================
// insert_or_assign tests
// ============================================================================

TEST(UnorderedMap, InsertOrAssignInsertsWhenMissing) {
    unordered_map<int, std::string> m;
    auto [it, inserted] = m.insert_or_assign(1, "one");
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->second, "one");
}

TEST(UnorderedMap, InsertOrAssignUpdatesWhenPresent) {
    unordered_map<int, std::string> m = {{1, "original"}};
    auto [it, inserted] = m.insert_or_assign(1, "updated");
    EXPECT_FALSE(inserted);
    EXPECT_EQ(it->second, "updated");  // value is assigned
}

TEST(UnorderedMap, InsertOrAssignMoveKey) {
    unordered_map<int, std::string> m;
    int key = 5;
    auto [it, inserted] = m.insert_or_assign(zstl::move(key), "five");
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->first, 5);
}

// ============================================================================
// Erase tests
// ============================================================================

TEST(UnorderedMap, EraseByKey) {
    unordered_map<int, std::string> m = {{1, "one"}, {2, "two"}, {3, "three"}};
    EXPECT_EQ(m.erase(2), 1u);
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m.find(2), m.end());

    // Erase non-existent key returns 0
    EXPECT_EQ(m.erase(99), 0u);
    EXPECT_EQ(m.size(), 2u);
}

TEST(UnorderedMap, EraseByIterator) {
    unordered_map<int, std::string> m = {{1, "one"}, {2, "two"}, {3, "three"}};
    auto it = m.find(2);
    ASSERT_NE(it, m.end());

    auto next = m.erase(it);
    EXPECT_EQ(m.size(), 2u);
    // next is a valid iterator
    EXPECT_NE(next, m.end());
}

TEST(UnorderedMap, EraseRange) {
    unordered_map<int, std::string> m = {{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}};
    auto first = m.find(2);
    auto last = m.find(4);

    // Convert const_iterator to iterator by getting next iterator
    auto it = m.erase(first, last);
    EXPECT_LT(m.size(), 4u);  // at least 2 removed
}

// ============================================================================
// Lookup tests: find, count, contains
// ============================================================================

TEST(UnorderedMap, Find) {
    unordered_map<int, std::string> m = {{1, "a"}, {2, "b"}, {3, "c"}};

    auto it = m.find(2);
    EXPECT_NE(it, m.end());
    EXPECT_EQ(it->first, 2);
    EXPECT_EQ(it->second, "b");

    EXPECT_EQ(m.find(99), m.end());
}

TEST(UnorderedMap, FindConst) {
    const unordered_map<int, std::string> m = {{1, "a"}, {2, "b"}};
    auto it = m.find(1);
    EXPECT_NE(it, m.end());
    EXPECT_EQ(it->second, "a");
}

TEST(UnorderedMap, Count) {
    unordered_map<int, std::string> m = {{1, "a"}, {2, "b"}};
    EXPECT_EQ(m.count(1), 1u);
    EXPECT_EQ(m.count(2), 1u);
    EXPECT_EQ(m.count(99), 0u);
}

TEST(UnorderedMap, Contains) {
    unordered_map<int, std::string> m = {{1, "a"}, {2, "b"}};
    EXPECT_TRUE(m.contains(1));
    EXPECT_TRUE(m.contains(2));
    EXPECT_FALSE(m.contains(0));
    EXPECT_FALSE(m.contains(3));
}

TEST(UnorderedMap, EqualRange) {
    unordered_map<int, std::string> m = {{1, "one"}, {2, "two"}};
    auto range = m.equal_range(1);
    int count = 0;
    for (auto it = range.first; it != range.second; ++it) {
        EXPECT_EQ(it->first, 1);
        ++count;
    }
    EXPECT_EQ(count, 1);

    auto range2 = m.equal_range(99);
    EXPECT_EQ(range2.first, range2.second);
}

// ============================================================================
// Bucket interface tests
// ============================================================================

TEST(UnorderedMap, BucketCount) {
    unordered_map<int, std::string> m;
    EXPECT_GT(m.bucket_count(), 0u);

    m = {{1, "a"}, {2, "b"}, {3, "c"}};
    EXPECT_GE(m.bucket_count(), 3u);
}

TEST(UnorderedMap, MaxBucketCount) {
    unordered_map<int, std::string> m;
    EXPECT_GT(m.max_bucket_count(), 0u);
}

TEST(UnorderedMap, Bucket) {
    unordered_map<int, std::string> m(16);
    m[1] = "one";
    m[2] = "two";

    size_t b1 = m.bucket(1);
    size_t b2 = m.bucket(2);
    EXPECT_LT(b1, m.bucket_count());
    EXPECT_LT(b2, m.bucket_count());

    // Same key always hashes to same bucket
    EXPECT_EQ(m.bucket(1), b1);
}

TEST(UnorderedMap, BucketSize) {
    unordered_map<int, std::string> m;
    m[1] = "one";
    m[2] = "two";
    m[3] = "three";

    // Sum of all bucket sizes equals total size
    size_t total = 0;
    for (size_t i = 0; i < m.bucket_count(); ++i) {
        total += m.bucket_size(i);
    }
    EXPECT_EQ(total, m.size());
}

// ============================================================================
// Hash policy tests: load_factor, max_load_factor, rehash, reserve
// ============================================================================

TEST(UnorderedMap, LoadFactor) {
    unordered_map<int, std::string> m;
    EXPECT_FLOAT_EQ(m.load_factor(), 0.0f);

    m[1] = "one";
    EXPECT_GT(m.load_factor(), 0.0f);
}

TEST(UnorderedMap, MaxLoadFactorDefault) {
    unordered_map<int, std::string> m;
    EXPECT_GT(m.max_load_factor(), 0.0f);
}

TEST(UnorderedMap, SetMaxLoadFactor) {
    unordered_map<int, std::string> m;
    m.max_load_factor(0.5f);
    EXPECT_FLOAT_EQ(m.max_load_factor(), 0.5f);

    m.max_load_factor(0.8f);
    EXPECT_FLOAT_EQ(m.max_load_factor(), 0.8f);
}

TEST(UnorderedMap, RehashPreservesElements) {
    unordered_map<int, std::string> m;
    for (int i = 0; i < 50; ++i) {
        m[i] = "val-" + std::to_string(i);
    }
    EXPECT_EQ(m.size(), 50u);

    size_t old_bucket_count = m.bucket_count();
    m.rehash(200);
    EXPECT_GE(m.bucket_count(), 200u);
    EXPECT_GT(m.bucket_count(), old_bucket_count);
    EXPECT_EQ(m.size(), 50u);  // all elements preserved

    // Verify all elements still accessible
    for (int i = 0; i < 50; ++i) {
        EXPECT_EQ(m.at(i), "val-" + std::to_string(i));
    }
}

TEST(UnorderedMap, RehashSmaller) {
    unordered_map<int, std::string> m;
    for (int i = 0; i < 100; ++i) {
        m[i] = "val-" + std::to_string(i);
    }
    size_t old_size = m.size();

    // Rehash to smaller bucket count should still keep elements
    m.rehash(8);
    EXPECT_EQ(m.size(), old_size);

    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(m.at(i), "val-" + std::to_string(i));
    }
}

TEST(UnorderedMap, Reserve) {
    unordered_map<int, std::string> m;
    m.reserve(100);
    size_t bc = m.bucket_count();

    // After reserve, bucket_count should accommodate at least 100 elements
    // at max_load_factor
    EXPECT_GE(bc * m.max_load_factor(), 100u);

    // Inserting up to reserved amount shouldn't trigger rehash
    for (int i = 0; i < 100; ++i) {
        m[i] = std::to_string(i);
    }
    EXPECT_EQ(m.size(), 100u);

    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(m.at(i), std::to_string(i));
    }
}

// ============================================================================
// Hash function and key equality tests
// ============================================================================

TEST(UnorderedMap, HashFunction) {
    unordered_map<int, std::string> m;
    auto hf = m.hash_function();
    EXPECT_EQ(hf(42), hf(42));  // same input, same hash
}

TEST(UnorderedMap, KeyEq) {
    unordered_map<int, std::string> m;
    auto eq = m.key_eq();
    EXPECT_TRUE(eq(1, 1));
    EXPECT_FALSE(eq(1, 2));
}

// ============================================================================
// Iteration tests
// ============================================================================

TEST(UnorderedMap, BeginEndIteration) {
    unordered_map<int, std::string> m = {{1, "a"}, {2, "b"}, {3, "c"}};
    size_t count = 0;
    for (auto it = m.begin(); it != m.end(); ++it) {
        ++count;
        EXPECT_GE(it->first, 1);
        EXPECT_LE(it->first, 3);
    }
    EXPECT_EQ(count, m.size());
}

TEST(UnorderedMap, ConstIteration) {
    const unordered_map<int, std::string> m = {{1, "a"}, {2, "b"}};
    size_t count = 0;
    for (auto it = m.cbegin(); it != m.cend(); ++it) {
        ++count;
    }
    EXPECT_EQ(count, 2u);
}

TEST(UnorderedMap, RangeBasedFor) {
    unordered_map<int, std::string> m = {{1, "one"}, {2, "two"}};
    size_t count = 0;
    for (const auto& p : m) {
        EXPECT_FALSE(p.second.empty());
        ++count;
    }
    EXPECT_EQ(count, m.size());
}

// ============================================================================
// Capacity tests
// ============================================================================

TEST(UnorderedMap, Empty) {
    unordered_map<int, std::string> m;
    EXPECT_TRUE(m.empty());
    m[1] = "one";
    EXPECT_FALSE(m.empty());
    m.erase(1);
    EXPECT_TRUE(m.empty());
}

TEST(UnorderedMap, Size) {
    unordered_map<int, std::string> m;
    EXPECT_EQ(m.size(), 0u);
    m[1] = "a";
    EXPECT_EQ(m.size(), 1u);
    m[2] = "b";
    EXPECT_EQ(m.size(), 2u);
    m.erase(1);
    EXPECT_EQ(m.size(), 1u);
}

TEST(UnorderedMap, MaxSize) {
    unordered_map<int, std::string> m;
    EXPECT_GT(m.max_size(), 0u);
}

// ============================================================================
// Clear and swap tests
// ============================================================================

TEST(UnorderedMap, Clear) {
    unordered_map<int, std::string> m = {{1, "a"}, {2, "b"}, {3, "c"}};
    EXPECT_EQ(m.size(), 3u);
    m.clear();
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0u);
    EXPECT_EQ(m.find(1), m.end());

    // Can insert after clear
    m[10] = "ten";
    EXPECT_EQ(m.size(), 1u);
}

TEST(UnorderedMap, SwapMember) {
    unordered_map<int, std::string> m1 = {{1, "a"}, {2, "b"}};
    unordered_map<int, std::string> m2 = {{10, "x"}, {20, "y"}, {30, "z"}};

    m1.swap(m2);
    EXPECT_EQ(m1.size(), 3u);
    EXPECT_EQ(m1.at(10), "x");
    EXPECT_EQ(m1.at(20), "y");
    EXPECT_EQ(m1.at(30), "z");

    EXPECT_EQ(m2.size(), 2u);
    EXPECT_EQ(m2.at(1), "a");
    EXPECT_EQ(m2.at(2), "b");
}

TEST(UnorderedMap, SwapFreeFunction) {
    unordered_map<int, std::string> m1 = {{1, "a"}};
    unordered_map<int, std::string> m2 = {{2, "b"}};
    zstl::swap(m1, m2);
    EXPECT_EQ(m1.at(2), "b");
    EXPECT_EQ(m2.at(1), "a");
}

// ============================================================================
// Merge tests
// ============================================================================

TEST(UnorderedMap, Merge) {
    unordered_map<int, std::string> m1 = {{1, "a"}, {2, "b"}};
    unordered_map<int, std::string> m2 = {{2, "b2"}, {3, "c"}};

    m1.merge(m2);

    EXPECT_EQ(m1.size(), 3u);
    EXPECT_EQ(m1.at(1), "a");
    EXPECT_EQ(m1.at(2), "b");   // original m1 value for key 2 preserved
    EXPECT_EQ(m1.at(3), "c");   // key 3 moved from m2

    // Key 2 stayed in m2 since it already existed in m1
    EXPECT_EQ(m2.size(), 1u);
    EXPECT_TRUE(m2.contains(2));
}

// ============================================================================
// Assignment tests
// ============================================================================

TEST(UnorderedMap, CopyAssignment) {
    unordered_map<int, std::string> m1 = {{1, "a"}, {2, "b"}};
    unordered_map<int, std::string> m2;
    m2 = m1;
    EXPECT_EQ(m2.size(), 2u);
    EXPECT_EQ(m2.at(1), "a");
    EXPECT_EQ(m2.at(2), "b");
}

TEST(UnorderedMap, MoveAssignment) {
    unordered_map<int, std::string> m1 = {{1, "a"}, {2, "b"}};
    unordered_map<int, std::string> m2;
    m2 = zstl::move(m1);
    EXPECT_EQ(m2.size(), 2u);
}

TEST(UnorderedMap, InitializerListAssignment) {
    unordered_map<int, std::string> m;
    m = {{1, "a"}, {2, "b"}};
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m.at(1), "a");
    EXPECT_EQ(m.at(2), "b");
}

// ============================================================================
// Many elements / rehash trigger tests
// ============================================================================

TEST(UnorderedMap, ManyElementsTriggerRehash) {
    unordered_map<int, int> m;
    const int N = 1000;

    for (int i = 0; i < N; ++i) {
        m[i] = i * 10;
    }
    EXPECT_EQ(m.size(), static_cast<size_t>(N));

    // Verify all elements are accessible
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(m.at(i), i * 10);
    }
}

TEST(UnorderedMap, ManyInsertionsStability) {
    unordered_map<int, std::string> m;
    const int N = 500;

    // Insert many elements to force rehashes
    for (int i = 0; i < N; ++i) {
        m.insert({i, "val-" + std::to_string(i)});
    }
    EXPECT_EQ(m.size(), static_cast<size_t>(N));

    // Delete half
    for (int i = 0; i < N; i += 2) {
        m.erase(i);
    }
    EXPECT_EQ(m.size(), static_cast<size_t>(N / 2));

    // Verify remaining
    for (int i = 1; i < N; i += 2) {
        EXPECT_EQ(m.at(i), "val-" + std::to_string(i));
    }

    // Insert more to trigger further rehashes
    for (int i = N; i < N + 200; ++i) {
        m[i] = "extra-" + std::to_string(i);
    }
    EXPECT_EQ(m.size(), static_cast<size_t>(N / 2 + 200));

    // Verify all
    for (int i = 1; i < N; i += 2) {
        EXPECT_EQ(m.at(i), "val-" + std::to_string(i));
    }
    for (int i = N; i < N + 200; ++i) {
        EXPECT_EQ(m.at(i), "extra-" + std::to_string(i));
    }
}

// ============================================================================
// String key tests
// ============================================================================

TEST(UnorderedMap, StringKeys) {
    unordered_map<std::string, int> m;
    m["hello"] = 1;
    m["world"] = 2;
    m["zstl"] = 3;

    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m["hello"], 1);
    EXPECT_EQ(m["world"], 2);
    EXPECT_EQ(m["zstl"], 3);

    EXPECT_TRUE(m.contains("hello"));
    EXPECT_FALSE(m.contains("missing"));

    m.erase("world");
    EXPECT_EQ(m.size(), 2u);
    EXPECT_FALSE(m.contains("world"));
}

// ============================================================================
// Comparison tests
// ============================================================================

TEST(UnorderedMap, Equality) {
    unordered_map<int, std::string> m1 = {{1, "a"}, {2, "b"}};
    unordered_map<int, std::string> m2 = {{2, "b"}, {1, "a"}};  // different order
    unordered_map<int, std::string> m3 = {{1, "a"}};
    unordered_map<int, std::string> m4 = {{1, "a"}, {2, "c"}};

    EXPECT_TRUE(m1 == m2);   // same elements, regardless of order
    EXPECT_FALSE(m1 == m3);  // different size
    EXPECT_FALSE(m1 == m4);  // same size, different value
    EXPECT_TRUE(m1 != m3);
    EXPECT_TRUE(m1 != m4);
}

// ============================================================================
// Get allocator test
// ============================================================================

TEST(UnorderedMap, GetAllocator) {
    unordered_map<int, std::string> m;
    auto alloc = m.get_allocator();
    (void)alloc;  // just verify it compiles and returns
    EXPECT_TRUE(true);
}
