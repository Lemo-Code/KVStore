// ============================================================================
// zstl skip_set Unit Tests
// Tests: constructors, insert, emplace, erase, find, count, contains,
//        lower_bound, upper_bound, equal_range, iteration, uniqueness,
//        empty, size, clear, swap, rank/at_rank, comparison operators
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>
#include <vector>
#include <random>
#include <algorithm>

using namespace zstl;

// ============================================================================
// Helper
// ============================================================================
template<typename Set>
void verify_sorted_ascending(const Set& s) {
    if (s.empty()) return;
    auto it = s.begin();
    auto prev = *it;
    ++it;
    for (; it != s.end(); ++it) {
        EXPECT_LT(prev, *it) << "Set not strictly ascending: " << prev
                              << " followed by " << *it;
        prev = *it;
    }
}

// ============================================================================
// Constructors
// ============================================================================

TEST(SkipSetTest, DefaultConstructor) {
    skip_set<int> s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_EQ(s.begin(), s.end());
}

TEST(SkipSetTest, ComparatorConstructor) {
    skip_set<int, greater<int>> s;
    s.insert(1);
    s.insert(3);
    s.insert(2);
    auto it = s.begin();
    EXPECT_EQ(*it, 3); ++it;
    EXPECT_EQ(*it, 2); ++it;
    EXPECT_EQ(*it, 1);
}

TEST(SkipSetTest, IteratorRangeConstructor) {
    std::vector<int> data = {3, 1, 2, 5, 4};
    skip_set<int> s(data.begin(), data.end());
    EXPECT_EQ(s.size(), 5u);
    verify_sorted_ascending(s);
}

TEST(SkipSetTest, CopyConstructor) {
    skip_set<int> s1;
    s1.insert(1);
    s1.insert(2);
    s1.insert(3);

    skip_set<int> s2(s1);
    EXPECT_EQ(s2.size(), 3u);
    EXPECT_TRUE(s2.contains(1));
    EXPECT_TRUE(s2.contains(2));
    EXPECT_TRUE(s2.contains(3));

    // Deep copy: modify s1, s2 unchanged
    s1.erase(2);
    EXPECT_TRUE(s2.contains(2));
}

TEST(SkipSetTest, MoveConstructor) {
    skip_set<int> s1;
    s1.insert(1);
    s1.insert(2);

    skip_set<int> s2(zstl::move(s1));
    EXPECT_EQ(s2.size(), 2u);
    EXPECT_TRUE(s2.contains(1));
    EXPECT_TRUE(s2.contains(2));
}

TEST(SkipSetTest, InitializerListConstructor) {
    skip_set<int> s = {3, 1, 2, 5, 4};
    EXPECT_EQ(s.size(), 5u);
    verify_sorted_ascending(s);
}

TEST(SkipSetTest, CopyAssignment) {
    skip_set<int> s1 = {1, 2, 3};
    skip_set<int> s2 = {4};
    s2 = s1;
    EXPECT_EQ(s2.size(), 3u);
    EXPECT_TRUE(s2.contains(1));
    EXPECT_FALSE(s2.contains(4));
}

TEST(SkipSetTest, MoveAssignment) {
    skip_set<int> s1 = {1, 2, 3};
    skip_set<int> s2 = {4};
    s2 = zstl::move(s1);
    EXPECT_EQ(s2.size(), 3u);
    EXPECT_TRUE(s2.contains(1));
}

TEST(SkipSetTest, InitializerListAssignment) {
    skip_set<int> s = {10, 20};
    s = {1, 2, 3};
    EXPECT_EQ(s.size(), 3u);
    EXPECT_FALSE(s.contains(10));
    EXPECT_TRUE(s.contains(1));
}

// ============================================================================
// insert
// ============================================================================

TEST(SkipSetTest, InsertSuccess) {
    skip_set<int> s;
    auto result = s.insert(42);
    EXPECT_TRUE(result.second);
    EXPECT_EQ(*result.first, 42);
    EXPECT_EQ(s.size(), 1u);
}

TEST(SkipSetTest, InsertDuplicate) {
    skip_set<int> s = {1, 2, 3};
    auto result = s.insert(2);
    EXPECT_FALSE(result.second);
    EXPECT_EQ(*result.first, 2);
    EXPECT_EQ(s.size(), 3u); // size unchanged
}

TEST(SkipSetTest, InsertRange) {
    skip_set<int> s;
    std::vector<int> data = {5, 1, 4, 2, 3};
    s.insert(data.begin(), data.end());
    EXPECT_EQ(s.size(), 5u);
    verify_sorted_ascending(s);
}

TEST(SkipSetTest, InsertInitializerList) {
    skip_set<int> s;
    s.insert({7, 3, 9, 1, 5});
    EXPECT_EQ(s.size(), 5u);
    verify_sorted_ascending(s);
}

TEST(SkipSetTest, InsertWithHint) {
    skip_set<int> s = {1, 3, 5};
    auto it = s.insert(s.begin(), 2);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(s.size(), 4u);
    verify_sorted_ascending(s);
}

TEST(SkipSetTest, InsertManyUnique) {
    skip_set<int> s;
    for (int i = 100; i >= 1; --i) {
        auto result = s.insert(i);
        EXPECT_TRUE(result.second);
    }
    EXPECT_EQ(s.size(), 100u);
    verify_sorted_ascending(s);
}

// ============================================================================
// emplace
// ============================================================================

TEST(SkipSetTest, EmplaceSuccess) {
    skip_set<int> s;
    auto result = s.emplace(10);
    EXPECT_TRUE(result.second);
    EXPECT_EQ(*result.first, 10);
}

TEST(SkipSetTest, EmplaceDuplicate) {
    skip_set<int> s;
    s.emplace(5);
    auto result = s.emplace(5);
    EXPECT_FALSE(result.second);
    EXPECT_EQ(s.size(), 1u);
}

TEST(SkipSetTest, EmplaceHint) {
    skip_set<int> s = {1, 3};
    auto it = s.emplace_hint(s.begin(), 2);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(s.size(), 3u);
}

// ============================================================================
// erase
// ============================================================================

TEST(SkipSetTest, EraseByKey) {
    skip_set<int> s = {1, 2, 3, 4, 5};
    size_t erased = s.erase(3);
    EXPECT_EQ(erased, 1u);
    EXPECT_EQ(s.size(), 4u);
    EXPECT_FALSE(s.contains(3));
}

TEST(SkipSetTest, EraseMissingKey) {
    skip_set<int> s = {1, 2, 3};
    size_t erased = s.erase(999);
    EXPECT_EQ(erased, 0u);
    EXPECT_EQ(s.size(), 3u);
}

TEST(SkipSetTest, EraseByIterator) {
    skip_set<int> s = {1, 2, 3};
    auto it = s.find(2);
    ASSERT_NE(it, s.end());
    s.erase(it);
    EXPECT_EQ(s.size(), 2u);
    EXPECT_FALSE(s.contains(2));
}

TEST(SkipSetTest, EraseByConstIterator) {
    skip_set<int> s = {1, 2, 3};
    const auto& cs = s;
    auto it = cs.find(1);
    ASSERT_NE(it, cs.end());
    s.erase(it);
    EXPECT_EQ(s.size(), 2u);
}

TEST(SkipSetTest, EraseRange) {
    skip_set<int> s;
    for (int i = 0; i < 10; ++i) s.insert(i);
    auto first = s.find(3);
    auto last = s.find(7);
    s.erase(first, last);
    EXPECT_EQ(s.size(), 6u);
    EXPECT_FALSE(s.contains(3));
    EXPECT_FALSE(s.contains(6));
    EXPECT_TRUE(s.contains(7));
}

TEST(SkipSetTest, EraseRangeWithIntKey) {
    skip_set<int> s;
    for (int i = 10; i <= 100; i += 10) s.insert(i);
    // erase_range is a different API
    size_t count = s.erase_range(30, 70);
    EXPECT_EQ(count, 3u); // 30, 40, 50 erased
    EXPECT_FALSE(s.contains(30));
    EXPECT_TRUE(s.contains(20));
    EXPECT_TRUE(s.contains(70));
}

// ============================================================================
// find, count, contains
// ============================================================================

TEST(SkipSetTest, FindSuccess) {
    skip_set<int> s = {1, 2, 3, 4, 5};
    auto it = s.find(3);
    ASSERT_NE(it, s.end());
    EXPECT_EQ(*it, 3);
}

TEST(SkipSetTest, FindFail) {
    skip_set<int> s = {1, 2, 3};
    auto it = s.find(999);
    EXPECT_EQ(it, s.end());
}

TEST(SkipSetTest, FindConst) {
    const skip_set<int> s = {1, 2, 3};
    auto it = s.find(2);
    ASSERT_NE(it, s.end());
    EXPECT_EQ(*it, 2);
}

TEST(SkipSetTest, Count) {
    skip_set<int> s = {1, 2, 3};
    EXPECT_EQ(s.count(2), 1u);
    EXPECT_EQ(s.count(999), 0u);
}

TEST(SkipSetTest, Contains) {
    skip_set<int> s = {1, 2, 3};
    EXPECT_TRUE(s.contains(1));
    EXPECT_TRUE(s.contains(3));
    EXPECT_FALSE(s.contains(0));
    EXPECT_FALSE(s.contains(4));
}

// ============================================================================
// lower_bound, upper_bound, equal_range
// ============================================================================

TEST(SkipSetTest, LowerBound) {
    skip_set<int> s = {10, 20, 30, 40};
    EXPECT_EQ(*s.lower_bound(20), 20);
    EXPECT_EQ(*s.lower_bound(25), 30);
    EXPECT_EQ(s.lower_bound(50), s.end());
}

TEST(SkipSetTest, LowerBoundFirst) {
    skip_set<int> s = {5, 10, 15};
    EXPECT_EQ(*s.lower_bound(3), 5);
}

TEST(SkipSetTest, UpperBound) {
    skip_set<int> s = {10, 20, 30, 40};
    EXPECT_EQ(*s.upper_bound(20), 30);
    EXPECT_EQ(*s.upper_bound(25), 30);
    EXPECT_EQ(s.upper_bound(50), s.end());
}

TEST(SkipSetTest, EqualRange) {
    skip_set<int> s = {10, 20, 30};
    auto [first, last] = s.equal_range(20);
    EXPECT_NE(first, s.end());
    EXPECT_EQ(*first, 20);
    // For unique keys, last is the next element or end
    if (last != s.end()) {
        EXPECT_EQ(*last, 30);
    }
}

TEST(SkipSetTest, EqualRangeMissing) {
    skip_set<int> s = {10, 20, 30};
    auto [first, last] = s.equal_range(15);
    EXPECT_EQ(first, last);
}

TEST(SkipSetTest, EqualRangeConst) {
    const skip_set<int> s = {10, 20, 30};
    auto [first, last] = s.equal_range(20);
    EXPECT_NE(first, s.end());
    EXPECT_EQ(*first, 20);
}

// ============================================================================
// Iteration — sorted and unique
// ============================================================================

TEST(SkipSetTest, SortedIteration) {
    skip_set<int> s = {5, 1, 4, 2, 3};
    int expected = 1;
    for (int val : s) {
        EXPECT_EQ(val, expected);
        ++expected;
    }
    EXPECT_EQ(expected, 6);
}

TEST(SkipSetTest, ConstIteration) {
    const skip_set<int> s = {3, 1, 2};
    int sum = 0;
    for (auto it = s.cbegin(); it != s.cend(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 6);
}

TEST(SkipSetTest, Uniqueness) {
    skip_set<int> s;
    s.insert(1);
    s.insert(1);
    s.insert(1);
    s.insert(2);
    s.insert(2);
    EXPECT_EQ(s.size(), 2u);

    // Verify exactly one of each
    int count1 = 0, count2 = 0;
    for (int val : s) {
        if (val == 1) ++count1;
        if (val == 2) ++count2;
    }
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);
}

TEST(SkipSetTest, LargeAscendingIteration) {
    skip_set<int> s;
    for (int i = 1000; i >= 1; --i) {
        s.insert(i);
    }
    int prev = 0;
    for (int val : s) {
        EXPECT_GT(val, prev);
        prev = val;
    }
}

// ============================================================================
// empty, size, clear, swap
// ============================================================================

TEST(SkipSetTest, EmptyAndSize) {
    skip_set<int> s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);

    s.insert(42);
    EXPECT_FALSE(s.empty());
    EXPECT_EQ(s.size(), 1u);

    s.erase(42);
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
}

TEST(SkipSetTest, Clear) {
    skip_set<int> s;
    for (int i = 0; i < 200; ++i) s.insert(i);
    EXPECT_EQ(s.size(), 200u);
    s.clear();
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_EQ(s.begin(), s.end());
}

TEST(SkipSetTest, Swap) {
    skip_set<int> s1 = {1, 2, 3};
    skip_set<int> s2 = {4, 5, 6, 7};

    s1.swap(s2);
    EXPECT_EQ(s1.size(), 4u);
    EXPECT_EQ(s2.size(), 3u);
    EXPECT_TRUE(s1.contains(4));
    EXPECT_TRUE(s2.contains(1));
}

TEST(SkipSetTest, FreeSwap) {
    skip_set<int> a = {10, 20};
    skip_set<int> b = {30};
    zstl::swap(a, b);
    EXPECT_EQ(a.size(), 1u);
    EXPECT_EQ(b.size(), 2u);
    EXPECT_TRUE(a.contains(30));
    EXPECT_TRUE(b.contains(10));
}

// ============================================================================
// rank, at_rank, first, last
// ============================================================================

TEST(SkipSetTest, Rank) {
    skip_set<int> s = {10, 30, 20, 50, 40};
    EXPECT_EQ(s.rank(10), 0u);
    EXPECT_EQ(s.rank(30), 2u);
    EXPECT_EQ(s.rank(50), 4u);
}

TEST(SkipSetTest, AtRank) {
    skip_set<int> s = {10, 20, 30};
    int* p = s.at_rank(0);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 10);

    p = s.at_rank(1);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 20);
}

TEST(SkipSetTest, FirstAndLast) {
    skip_set<int> s = {3, 1, 4, 1, 5}; // duplicates filtered
    EXPECT_EQ(*s.first(), 1);
    EXPECT_EQ(*s.last(), 5);
}

TEST(SkipSetTest, FirstAndLastEmpty) {
    skip_set<int> s;
    EXPECT_EQ(s.first(), nullptr);
    EXPECT_EQ(s.last(), nullptr);
}

// ============================================================================
// erase_if
// ============================================================================

TEST(SkipSetTest, EraseIf) {
    skip_set<int> s;
    for (int i = 1; i <= 100; ++i) s.insert(i);

    s.erase_if([](int x) { return x % 2 == 0; }); // remove even numbers
    EXPECT_EQ(s.size(), 50u);

    for (int val : s) {
        EXPECT_TRUE(val % 2 != 0) << "Found even value: " << val;
    }
    verify_sorted_ascending(s);
}

// ============================================================================
// insert_range
// ============================================================================

TEST(SkipSetTest, InsertRangeAlias) {
    skip_set<int> s;
    std::vector<int> v = {1, 3, 5, 2, 4};
    s.insert_range(v.begin(), v.end());
    EXPECT_EQ(s.size(), 5u);
    verify_sorted_ascending(s);
}

// ============================================================================
// Probabilistic balance / large dataset
// ============================================================================

TEST(SkipSetTest, RandomInsertBalance) {
    skip_set<int> s;
    std::mt19937 rng(123);
    std::uniform_int_distribution<int> dist(1, 100000);

    for (int i = 0; i < 3000; ++i) {
        s.insert(dist(rng));
    }

    verify_sorted_ascending(s);

    // Re-verify with same seed
    rng.seed(123);
    for (int i = 0; i < 3000; ++i) {
        int key = dist(rng);
        EXPECT_TRUE(s.contains(key)) << "Missing key: " << key;
    }
}

TEST(SkipSetTest, StringSet) {
    skip_set<std::string> s;
    s.insert("zebra");
    s.insert("apple");
    s.insert("mango");
    s.insert("banana");

    auto it = s.begin();
    EXPECT_EQ(*it, "apple"); ++it;
    EXPECT_EQ(*it, "banana"); ++it;
    EXPECT_EQ(*it, "mango"); ++it;
    EXPECT_EQ(*it, "zebra");
}

// ============================================================================
// merge
// ============================================================================

TEST(SkipSetTest, Merge) {
    skip_set<int> s1 = {1, 2, 3};
    skip_set<int> s2 = {3, 4, 5};

    s1.merge(s2);
    EXPECT_EQ(s1.size(), 5u);
    EXPECT_TRUE(s1.contains(1));
    EXPECT_TRUE(s1.contains(5));
    EXPECT_TRUE(s2.empty());
}

// ============================================================================
// Comparison operators
// ============================================================================

TEST(SkipSetTest, Equality) {
    skip_set<int> s1 = {1, 2, 3};
    skip_set<int> s2 = {1, 2, 3};
    skip_set<int> s3 = {1, 2};
    skip_set<int> s4 = {1, 2, 4};

    EXPECT_TRUE(s1 == s2);
    EXPECT_FALSE(s1 == s3);
    EXPECT_FALSE(s1 == s4);
}

TEST(SkipSetTest, Inequality) {
    skip_set<int> s1 = {1, 2};
    skip_set<int> s2 = {3, 4};
    EXPECT_TRUE(s1 != s2);
    EXPECT_FALSE(s1 != s1);
}

TEST(SkipSetTest, LessThan) {
    skip_set<int> s1 = {1, 2};
    skip_set<int> s2 = {1, 2, 3};
    skip_set<int> s3 = {2, 3};

    EXPECT_TRUE(s1 < s2);  // prefix
    EXPECT_TRUE(s1 < s3);  // lexicographic
    EXPECT_FALSE(s2 < s1);
}

TEST(SkipSetTest, GreaterThan) {
    skip_set<int> s1 = {1, 2, 3};
    skip_set<int> s2 = {1, 2};
    EXPECT_TRUE(s1 > s2);
    EXPECT_FALSE(s2 > s1);
}

TEST(SkipSetTest, LessEqualAndGreaterEqual) {
    skip_set<int> s1 = {1, 2};
    skip_set<int> s2 = {1, 2};
    skip_set<int> s3 = {1};

    EXPECT_TRUE(s1 <= s2);
    EXPECT_TRUE(s2 <= s1);
    EXPECT_TRUE(s1 >= s2);
    EXPECT_TRUE(s2 >= s1);

    EXPECT_TRUE(s3 <= s1);
    EXPECT_FALSE(s1 <= s3);
}

// ============================================================================
// Edge cases
// ============================================================================

TEST(SkipSetTest, EmptySetOperations) {
    skip_set<int> s;
    EXPECT_EQ(s.find(1), s.end());
    EXPECT_EQ(s.lower_bound(1), s.end());
    EXPECT_EQ(s.upper_bound(1), s.end());
    EXPECT_EQ(s.begin(), s.end());
}

TEST(SkipSetTest, MaxSize) {
    skip_set<int> s;
    EXPECT_GT(s.max_size(), 0u);
}

TEST(SkipSetTest, KeyComp) {
    skip_set<int> s;
    auto comp = s.key_comp();
    EXPECT_TRUE(comp(1, 2));
    EXPECT_FALSE(comp(2, 2));
}

TEST(SkipSetTest, ValueComp) {
    skip_set<int> s;
    auto comp = s.value_comp();
    EXPECT_TRUE(comp(1, 2));
    EXPECT_FALSE(comp(3, 2));
}

// ============================================================================
// Reverse-sorted set via custom comparator
// ============================================================================

TEST(SkipSetTest, GreaterComparator) {
    skip_set<int, greater<int>> s = {1, 3, 2, 5, 4};
    auto it = s.begin();
    EXPECT_EQ(*it, 5); ++it;
    EXPECT_EQ(*it, 4); ++it;
    EXPECT_EQ(*it, 3); ++it;
    EXPECT_EQ(*it, 2); ++it;
    EXPECT_EQ(*it, 1);
}
