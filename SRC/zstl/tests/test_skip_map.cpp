// ============================================================================
// zstl skip_map Unit Tests
// Tests: constructors, operator[], at, insert, emplace, try_emplace,
//        insert_or_assign, erase, find, count, contains, lower_bound,
//        upper_bound, equal_range, iteration, empty, size, clear, swap,
//        rank, at_rank, first/last, probabilistic balance, comparison
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>
#include <random>
#include <algorithm>
#include <vector>
#include <stdexcept>

using namespace zstl;

// ============================================================================
// Helper: verify sorted order of a skip_map
// ============================================================================
template<typename Map>
void verify_sorted_order(const Map& m) {
    if (m.empty()) return;
    auto it = m.begin();
    auto prev_key = (*it).first;
    ++it;
    for (; it != m.end(); ++it) {
        EXPECT_LE(prev_key, (*it).first) << "Map not sorted: " << prev_key
                                          << " > " << (*it).first;
        prev_key = (*it).first;
    }
}

// ============================================================================
// Constructors
// ============================================================================

TEST(SkipMapTest, DefaultConstructor) {
    skip_map<int, std::string> m;
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0u);
    EXPECT_EQ(m.begin(), m.end());
}

TEST(SkipMapTest, ComparatorConstructor) {
    skip_map<int, std::string, greater<int>> m;
    EXPECT_TRUE(m.empty());
    m.insert({1, "one"});
    m.insert({3, "three"});
    m.insert({2, "two"});
    // With greater<int>, order should be descending
    auto it = m.begin();
    EXPECT_EQ((*it).first, 3);
    ++it;
    EXPECT_EQ((*it).first, 2);
    ++it;
    EXPECT_EQ((*it).first, 1);
}

TEST(SkipMapTest, IteratorRangeConstructor) {
    std::vector<pair<const int, std::string>> data = {
        {3, "three"}, {1, "one"}, {2, "two"}, {5, "five"}, {4, "four"}
    };
    skip_map<int, std::string> m(data.begin(), data.end());
    EXPECT_EQ(m.size(), 5u);
    verify_sorted_order(m);
}

TEST(SkipMapTest, CopyConstructor) {
    skip_map<int, std::string> m1;
    m1.insert({1, "one"});
    m1.insert({2, "two"});
    m1.insert({3, "three"});

    skip_map<int, std::string> m2(m1);
    EXPECT_EQ(m2.size(), 3u);
    EXPECT_EQ(m2.at(1), "one");
    EXPECT_EQ(m2.at(2), "two");
    EXPECT_EQ(m2.at(3), "three");
    verify_sorted_order(m2);

    // Verify deep copy — modify m1, m2 unchanged
    m1[1] = "modified";
    EXPECT_EQ(m2.at(1), "one");
}

TEST(SkipMapTest, MoveConstructor) {
    skip_map<int, std::string> m1;
    m1.insert({1, "one"});
    m1.insert({2, "two"});

    skip_map<int, std::string> m2(zstl::move(m1));
    EXPECT_EQ(m2.size(), 2u);
    EXPECT_EQ(m2.at(1), "one");
    EXPECT_EQ(m2.at(2), "two");
    // m1 is in a valid but unspecified state
}

TEST(SkipMapTest, InitializerListConstructor) {
    skip_map<int, std::string> m = {{1, "one"}, {3, "three"}, {2, "two"}};
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m.at(1), "one");
    EXPECT_EQ(m.at(2), "two");
    EXPECT_EQ(m.at(3), "three");
    verify_sorted_order(m);
}

TEST(SkipMapTest, CopyAssignment) {
    skip_map<int, std::string> m1 = {{1, "a"}, {2, "b"}};
    skip_map<int, std::string> m2 = {{3, "c"}};
    m2 = m1;
    EXPECT_EQ(m2.size(), 2u);
    EXPECT_EQ(m2.at(1), "a");
    EXPECT_EQ(m2.at(2), "b");
    EXPECT_FALSE(m2.contains(3));
}

TEST(SkipMapTest, MoveAssignment) {
    skip_map<int, std::string> m1 = {{1, "a"}, {2, "b"}};
    skip_map<int, std::string> m2 = {{3, "c"}};
    m2 = zstl::move(m1);
    EXPECT_EQ(m2.size(), 2u);
    EXPECT_TRUE(m2.contains(1));
    EXPECT_TRUE(m2.contains(2));
}

TEST(SkipMapTest, InitializerListAssignment) {
    skip_map<int, std::string> m;
    m.insert({10, "ten"});
    m = {{1, "one"}, {2, "two"}};
    EXPECT_EQ(m.size(), 2u);
    EXPECT_FALSE(m.contains(10));
    EXPECT_EQ(m.at(1), "one");
}

// ============================================================================
// Element access: operator[] and at
// ============================================================================

TEST(SkipMapTest, OperatorBracketInsert) {
    skip_map<int, std::string> m;
    m[1] = "one";
    m[2] = "two";
    EXPECT_EQ(m[1], "one");
    EXPECT_EQ(m[2], "two");
    EXPECT_EQ(m.size(), 2u);
}

TEST(SkipMapTest, OperatorBracketOverwrite) {
    skip_map<int, std::string> m;
    m[1] = "one";
    m[1] = "ONE";
    EXPECT_EQ(m[1], "ONE");
    EXPECT_EQ(m.size(), 1u);
}

TEST(SkipMapTest, OperatorBracketDefaultInsert) {
    skip_map<int, std::string> m;
    auto& val = m[5]; // default-constructs empty string
    EXPECT_EQ(val, "");
    EXPECT_TRUE(m.contains(5));
}

TEST(SkipMapTest, AtSuccess) {
    skip_map<int, std::string> m = {{1, "one"}, {2, "two"}};
    EXPECT_EQ(m.at(1), "one");
    EXPECT_EQ(m.at(2), "two");
}

TEST(SkipMapTest, AtThrowsOnMissing) {
    skip_map<int, std::string> m = {{1, "one"}};
    EXPECT_THROW(m.at(999), std::out_of_range);
}

TEST(SkipMapTest, AtConstThrowsOnMissing) {
    const skip_map<int, std::string> m = {{1, "one"}};
    EXPECT_THROW(m.at(999), std::out_of_range);
    EXPECT_EQ(m.at(1), "one");
}

TEST(SkipMapTest, AtModifyThroughReference) {
    skip_map<int, std::string> m = {{1, "one"}};
    m.at(1) = "modified";
    EXPECT_EQ(m.at(1), "modified");
}

// ============================================================================
// insert
// ============================================================================

TEST(SkipMapTest, InsertSingleSuccess) {
    skip_map<int, std::string> m;
    auto result = m.insert({1, "one"});
    EXPECT_TRUE(result.second);
    EXPECT_EQ((*result.first).first, 1);
    EXPECT_EQ((*result.first).second, "one");
    EXPECT_EQ(m.size(), 1u);
}

TEST(SkipMapTest, InsertDuplicate) {
    skip_map<int, std::string> m = {{1, "one"}};
    auto result = m.insert({1, "ONE"});
    EXPECT_FALSE(result.second);
    EXPECT_EQ((*result.first).second, "one"); // original value preserved
    EXPECT_EQ(m.size(), 1u);
}

TEST(SkipMapTest, InsertRange) {
    skip_map<int, std::string> m;
    std::vector<pair<const int, std::string>> data = {
        {3, "three"}, {1, "one"}, {4, "four"}, {2, "two"}
    };
    m.insert(data.begin(), data.end());
    EXPECT_EQ(m.size(), 4u);
    verify_sorted_order(m);
    EXPECT_EQ(m.at(1), "one");
    EXPECT_EQ(m.at(4), "four");
}

TEST(SkipMapTest, InsertInitializerList) {
    skip_map<int, std::string> m;
    m.insert({{5, "five"}, {2, "two"}, {8, "eight"}});
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m.at(2), "two");
    EXPECT_EQ(m.at(5), "five");
    EXPECT_EQ(m.at(8), "eight");
}

TEST(SkipMapTest, InsertWithHint) {
    skip_map<int, std::string> m = {{1, "one"}, {3, "three"}};
    auto it = m.insert(m.begin(), {2, "two"});
    EXPECT_EQ((*it).second, "two");
    EXPECT_EQ(m.size(), 3u);
    verify_sorted_order(m);
}

// ============================================================================
// emplace, try_emplace, insert_or_assign
// ============================================================================

TEST(SkipMapTest, EmplaceSuccess) {
    skip_map<int, std::string> m;
    auto result = m.emplace(1, "one");
    EXPECT_TRUE(result.second);
    EXPECT_EQ((*result.first).second, "one");
}

TEST(SkipMapTest, EmplaceDuplicate) {
    skip_map<int, std::string> m;
    m.emplace(1, "one");
    auto result = m.emplace(1, "ONE");
    EXPECT_FALSE(result.second);
    EXPECT_EQ((*result.first).second, "one");
}

TEST(SkipMapTest, EmplaceMultiple) {
    skip_map<int, std::string> m;
    for (int i = 0; i < 100; ++i) {
        auto result = m.emplace(i, "val" + std::to_string(i));
        EXPECT_TRUE(result.second);
    }
    EXPECT_EQ(m.size(), 100u);
    verify_sorted_order(m);
}

TEST(SkipMapTest, TryEmplaceSuccess) {
    skip_map<int, std::string> m;
    auto result = m.try_emplace(1, "one");
    EXPECT_TRUE(result.second);
    EXPECT_EQ((*result.first).second, "one");
}

TEST(SkipMapTest, TryEmplaceExisting) {
    skip_map<int, std::string> m = {{1, "one"}};
    auto result = m.try_emplace(1, "ONE");
    EXPECT_FALSE(result.second);
    EXPECT_EQ((*result.first).second, "one"); // original preserved
}

TEST(SkipMapTest, InsertOrAssignInsert) {
    skip_map<int, std::string> m;
    auto result = m.insert_or_assign(1, "one");
    EXPECT_TRUE(result.second);
    EXPECT_EQ((*result.first).second, "one");
}

TEST(SkipMapTest, InsertOrAssignUpdate) {
    skip_map<int, std::string> m = {{1, "one"}};
    auto result = m.insert_or_assign(1, "ONE");
    EXPECT_FALSE(result.second);
    EXPECT_EQ((*result.first).second, "ONE"); // value updated
    EXPECT_EQ(m.size(), 1u);
}

TEST(SkipMapTest, EmplaceHint) {
    skip_map<int, std::string> m = {{1, "one"}, {3, "three"}};
    auto it = m.emplace_hint(m.begin(), 2, "two");
    EXPECT_EQ((*it).second, "two");
    EXPECT_EQ(m.size(), 3u);
}

// ============================================================================
// erase
// ============================================================================

TEST(SkipMapTest, EraseByKey) {
    skip_map<int, std::string> m = {{1, "one"}, {2, "two"}, {3, "three"}};
    size_t erased = m.erase(2);
    EXPECT_EQ(erased, 1u);
    EXPECT_EQ(m.size(), 2u);
    EXPECT_FALSE(m.contains(2));
}

TEST(SkipMapTest, EraseMissingKey) {
    skip_map<int, std::string> m = {{1, "one"}};
    size_t erased = m.erase(999);
    EXPECT_EQ(erased, 0u);
    EXPECT_EQ(m.size(), 1u);
}

TEST(SkipMapTest, EraseByIterator) {
    skip_map<int, std::string> m = {{1, "one"}, {2, "two"}, {3, "three"}};
    auto it = m.find(2);
    ASSERT_NE(it, m.end());
    m.erase(it);
    EXPECT_EQ(m.size(), 2u);
    EXPECT_FALSE(m.contains(2));
}

TEST(SkipMapTest, EraseByConstIterator) {
    skip_map<int, std::string> m = {{1, "one"}, {2, "two"}};
    const auto& cm = m;
    auto it = cm.find(1);
    ASSERT_NE(it, cm.end());
    m.erase(it);
    EXPECT_EQ(m.size(), 1u);
}

TEST(SkipMapTest, EraseRange) {
    skip_map<int, std::string> m;
    for (int i = 0; i < 10; ++i) {
        m.insert({i, "val" + std::to_string(i)});
    }
    auto first = m.find(3);
    auto last = m.find(7);
    auto result = m.erase(first, last);
    EXPECT_EQ(m.size(), 6u);
    EXPECT_FALSE(m.contains(3));
    EXPECT_FALSE(m.contains(6));
    EXPECT_TRUE(m.contains(7));
}

// ============================================================================
// find, count, contains
// ============================================================================

TEST(SkipMapTest, FindSuccess) {
    skip_map<int, std::string> m = {{1, "one"}, {2, "two"}, {3, "three"}};
    auto it = m.find(2);
    ASSERT_NE(it, m.end());
    EXPECT_EQ((*it).second, "two");
}

TEST(SkipMapTest, FindFail) {
    skip_map<int, std::string> m = {{1, "one"}};
    auto it = m.find(999);
    EXPECT_EQ(it, m.end());
}

TEST(SkipMapTest, FindConst) {
    const skip_map<int, std::string> m = {{1, "one"}, {2, "two"}};
    auto it = m.find(2);
    ASSERT_NE(it, m.end());
    EXPECT_EQ((*it).second, "two");
}

TEST(SkipMapTest, Count) {
    skip_map<int, std::string> m = {{1, "one"}, {2, "two"}, {3, "three"}};
    EXPECT_EQ(m.count(2), 1u);
    EXPECT_EQ(m.count(999), 0u);
}

TEST(SkipMapTest, Contains) {
    skip_map<int, std::string> m = {{1, "one"}, {2, "two"}};
    EXPECT_TRUE(m.contains(1));
    EXPECT_TRUE(m.contains(2));
    EXPECT_FALSE(m.contains(3));
}

// ============================================================================
// lower_bound, upper_bound, equal_range
// ============================================================================

TEST(SkipMapTest, LowerBoundExact) {
    skip_map<int, std::string> m = {{1, "a"}, {3, "c"}, {5, "e"}};
    auto it = m.lower_bound(3);
    EXPECT_EQ((*it).first, 3);
}

TEST(SkipMapTest, LowerBoundBetween) {
    skip_map<int, std::string> m = {{1, "a"}, {3, "c"}, {5, "e"}};
    auto it = m.lower_bound(2);
    EXPECT_EQ((*it).first, 3); // first >= 2
}

TEST(SkipMapTest, LowerBoundBeyond) {
    skip_map<int, std::string> m = {{1, "a"}, {3, "c"}};
    auto it = m.lower_bound(10);
    EXPECT_EQ(it, m.end());
}

TEST(SkipMapTest, UpperBoundExact) {
    skip_map<int, std::string> m = {{1, "a"}, {3, "c"}, {5, "e"}};
    auto it = m.upper_bound(3);
    EXPECT_EQ((*it).first, 5); // first > 3
}

TEST(SkipMapTest, UpperBoundBeyond) {
    skip_map<int, std::string> m = {{1, "a"}, {3, "c"}};
    auto it = m.upper_bound(10);
    EXPECT_EQ(it, m.end());
}

TEST(SkipMapTest, EqualRange) {
    skip_map<int, std::string> m = {{1, "a"}, {3, "c"}, {5, "e"}};
    auto [first, last] = m.equal_range(3);
    EXPECT_NE(first, m.end());
    EXPECT_EQ((*first).first, 3);
    EXPECT_NE(last, m.end());
    EXPECT_EQ((*last).first, 5); // one past the key
}

TEST(SkipMapTest, EqualRangeMissing) {
    skip_map<int, std::string> m = {{1, "a"}, {3, "c"}};
    auto [first, last] = m.equal_range(2);
    EXPECT_EQ(first, last); // empty range
}

TEST(SkipMapTest, EqualRangeConst) {
    const skip_map<int, std::string> m = {{1, "a"}, {3, "c"}, {5, "e"}};
    auto [first, last] = m.equal_range(3);
    EXPECT_NE(first, m.end());
    EXPECT_EQ((*first).first, 3);
}

// ============================================================================
// Iteration
// ============================================================================

TEST(SkipMapTest, BeginEndIteration) {
    skip_map<int, std::string> m = {{3, "c"}, {1, "a"}, {2, "b"}};
    int expected_keys[] = {1, 2, 3};
    std::string expected_vals[] = {"a", "b", "c"};
    int idx = 0;
    for (auto& kv : m) {
        EXPECT_EQ(kv.first, expected_keys[idx]);
        EXPECT_EQ(kv.second, expected_vals[idx]);
        ++idx;
    }
    EXPECT_EQ(idx, 3);
}

TEST(SkipMapTest, ConstIteration) {
    const skip_map<int, std::string> m = {{3, "c"}, {1, "a"}, {2, "b"}};
    int count = 0;
    for (auto it = m.cbegin(); it != m.cend(); ++it) {
        ++count;
    }
    EXPECT_EQ(count, 3);
}

TEST(SkipMapTest, EmptyIteration) {
    skip_map<int, std::string> m;
    EXPECT_EQ(m.begin(), m.end());
    int count = 0;
    for (auto it = m.begin(); it != m.end(); ++it) {
        ++count;
    }
    EXPECT_EQ(count, 0);
}

TEST(SkipMapTest, LargeIteration) {
    skip_map<int, std::string> m;
    for (int i = 0; i < 1000; ++i) {
        m.insert({i, std::to_string(i)});
    }
    int prev = -1;
    for (auto& kv : m) {
        EXPECT_GT(kv.first, prev);
        prev = kv.first;
    }
}

// ============================================================================
// empty, size, clear, swap
// ============================================================================

TEST(SkipMapTest, EmptyAndSize) {
    skip_map<int, std::string> m;
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0u);

    m.insert({1, "one"});
    EXPECT_FALSE(m.empty());
    EXPECT_EQ(m.size(), 1u);

    m.clear();
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0u);
}

TEST(SkipMapTest, Clear) {
    skip_map<int, std::string> m;
    for (int i = 0; i < 500; ++i) {
        m.insert({i, "value"});
    }
    EXPECT_EQ(m.size(), 500u);
    m.clear();
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0u);
    EXPECT_EQ(m.begin(), m.end());
}

TEST(SkipMapTest, Swap) {
    skip_map<int, std::string> m1 = {{1, "one"}, {2, "two"}};
    skip_map<int, std::string> m2 = {{3, "three"}, {4, "four"}, {5, "five"}};

    m1.swap(m2);

    EXPECT_EQ(m1.size(), 3u);
    EXPECT_TRUE(m1.contains(3));
    EXPECT_TRUE(m1.contains(4));
    EXPECT_TRUE(m1.contains(5));

    EXPECT_EQ(m2.size(), 2u);
    EXPECT_TRUE(m2.contains(1));
    EXPECT_TRUE(m2.contains(2));
}

TEST(SkipMapTest, FreeSwap) {
    skip_map<int, std::string> m1 = {{1, "one"}};
    skip_map<int, std::string> m2 = {{2, "two"}};
    zstl::swap(m1, m2);
    EXPECT_TRUE(m1.contains(2));
    EXPECT_TRUE(m2.contains(1));
}

// ============================================================================
// ZSet-style operations: rank, at_rank, first, last
// ============================================================================

TEST(SkipMapTest, Rank) {
    skip_map<int, std::string> m;
    for (int i = 10; i <= 100; i += 10) {
        m.insert({i, "val" + std::to_string(i)});
    }
    // Keys: 10, 20, 30, 40, 50, 60, 70, 80, 90, 100
    // Rank of smallest key should be 0
    EXPECT_EQ(m.rank(10), 0u);
    EXPECT_EQ(m.rank(50), 4u);
    EXPECT_EQ(m.rank(100), 9u);
}

TEST(SkipMapTest, AtRank) {
    skip_map<int, std::string> m;
    m.insert({10, "ten"});
    m.insert({20, "twenty"});
    m.insert({30, "thirty"});

    auto* p = m.at_rank(0);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->first, 10);

    p = m.at_rank(1);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->first, 20);

    p = m.at_rank(2);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->first, 30);
}

TEST(SkipMapTest, FirstAndLast) {
    skip_map<int, std::string> m = {{5, "e"}, {3, "c"}, {7, "g"}, {1, "a"}};
    auto* f = m.first();
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->first, 1);

    auto* l = m.last();
    ASSERT_NE(l, nullptr);
    EXPECT_EQ(l->first, 7);
}

TEST(SkipMapTest, FirstAndLastEmpty) {
    skip_map<int, std::string> m;
    EXPECT_EQ(m.first(), nullptr);
    EXPECT_EQ(m.last(), nullptr);
}

// ============================================================================
// Key comparator and value comparator
// ============================================================================

TEST(SkipMapTest, KeyComp) {
    skip_map<int, std::string> m;
    auto comp = m.key_comp();
    EXPECT_TRUE(comp(1, 2));
    EXPECT_FALSE(comp(2, 1));
    EXPECT_FALSE(comp(2, 2));
}

TEST(SkipMapTest, ValueComp) {
    skip_map<int, std::string> m;
    auto comp = m.value_comp();
    EXPECT_TRUE(comp({1, "a"}, {2, "b"}));
    EXPECT_FALSE(comp({2, "b"}, {1, "a"}));
}

// ============================================================================
// Probabilistic balance — insert many random keys
// ============================================================================

TEST(SkipMapTest, ProbabilisticBalance) {
    skip_map<int, std::string> m;

    std::mt19937 rng(42); // fixed seed for reproducibility
    std::uniform_int_distribution<int> dist(1, 100000);

    for (int i = 0; i < 5000; ++i) {
        int key = dist(rng);
        m.insert({key, "v" + std::to_string(key)});
    }

    // Verify sorted order is maintained
    verify_sorted_order(m);

    // Verify all inserted elements are retrievable
    rng.seed(42);
    for (int i = 0; i < 5000; ++i) {
        int key = dist(rng);
        EXPECT_TRUE(m.contains(key)) << "Missing key: " << key;
    }

    // Spot-check ordered iteration
    int prev = -1;
    for (auto& kv : m) {
        EXPECT_GT(kv.first, prev) << "Order violated: " << prev << " then " << kv.first;
        prev = kv.first;
    }
}

// ============================================================================
// Large dataset
// ============================================================================

TEST(SkipMapTest, LargeDataset) {
    skip_map<int, int> m;
    const int N = 5000;
    for (int i = N; i >= 1; --i) {
        m.insert({i, i * 10});
    }

    EXPECT_EQ(m.size(), static_cast<size_t>(N));

    // Verify sorted order
    int prev = 0;
    for (auto& kv : m) {
        EXPECT_GT(kv.first, prev);
        EXPECT_EQ(kv.second, kv.first * 10);
        prev = kv.first;
    }

    // Verify lookup
    for (int i = 1; i <= N; ++i) {
        EXPECT_EQ(m.at(i), i * 10);
    }

    // Verify rank
    EXPECT_EQ(m.rank(1), 0u);
    EXPECT_EQ(m.rank(N), static_cast<size_t>(N - 1));
}

// ============================================================================
// Comparison operators
// ============================================================================

TEST(SkipMapTest, Equality) {
    skip_map<int, std::string> m1 = {{1, "a"}, {2, "b"}};
    skip_map<int, std::string> m2 = {{1, "a"}, {2, "b"}};
    skip_map<int, std::string> m3 = {{1, "a"}, {2, "c"}}; // different value
    skip_map<int, std::string> m4 = {{1, "a"}};            // different size

    EXPECT_TRUE(m1 == m2);
    EXPECT_FALSE(m1 == m3);
    EXPECT_FALSE(m1 == m4);
}

TEST(SkipMapTest, Inequality) {
    skip_map<int, std::string> m1 = {{1, "a"}};
    skip_map<int, std::string> m2 = {{2, "b"}};
    EXPECT_TRUE(m1 != m2);
    EXPECT_FALSE(m1 != m1);
}

TEST(SkipMapTest, LessThan) {
    skip_map<int, std::string> m1 = {{1, "a"}};
    skip_map<int, std::string> m2 = {{1, "a"}, {2, "b"}};
    skip_map<int, std::string> m3 = {{2, "b"}};

    EXPECT_TRUE(m1 < m2); // m1 is a proper prefix of m2
    EXPECT_TRUE(m1 < m3); // lexicographic comparison
    EXPECT_FALSE(m2 < m1);
}

TEST(SkipMapTest, GreaterThan) {
    skip_map<int, std::string> m1 = {{1, "a"}, {2, "b"}};
    skip_map<int, std::string> m2 = {{1, "a"}};
    EXPECT_TRUE(m1 > m2);
    EXPECT_FALSE(m2 > m1);
}

TEST(SkipMapTest, LessEqualAndGreaterEqual) {
    skip_map<int, std::string> m1 = {{1, "a"}, {2, "b"}};
    skip_map<int, std::string> m2 = {{1, "a"}, {2, "b"}};

    EXPECT_TRUE(m1 <= m2);
    EXPECT_TRUE(m2 <= m1);
    EXPECT_TRUE(m1 >= m2);
    EXPECT_TRUE(m2 >= m1);

    skip_map<int, std::string> m3 = {{1, "a"}};
    EXPECT_TRUE(m3 <= m1);
    EXPECT_FALSE(m1 <= m3);
    EXPECT_TRUE(m1 >= m3);
    EXPECT_FALSE(m3 >= m1);
}

// ============================================================================
// Edge cases
// ============================================================================

TEST(SkipMapTest, InsertIntoEmpty) {
    skip_map<int, std::string> m;
    m.insert({42, "answer"});
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m.at(42), "answer");
}

TEST(SkipMapTest, MaxSize) {
    skip_map<int, std::string> m;
    EXPECT_GT(m.max_size(), 0u);
}

TEST(SkipMapTest, StringKeys) {
    skip_map<std::string, int> m;
    m.insert({"banana", 2});
    m.insert({"apple", 1});
    m.insert({"cherry", 3});

    auto it = m.begin();
    EXPECT_EQ((*it).first, "apple");
    ++it;
    EXPECT_EQ((*it).first, "banana");
    ++it;
    EXPECT_EQ((*it).first, "cherry");
}

TEST(SkipMapTest, MoveSemanticsValue) {
    skip_map<int, std::string> m;
    std::string val = "hello world this is a long string";
    m.insert({1, zstl::move(val)});
    EXPECT_EQ(m.at(1), "hello world this is a long string");
}

// ============================================================================
// merge
// ============================================================================

TEST(SkipMapTest, Merge) {
    skip_map<int, std::string> m1 = {{1, "one"}, {2, "two"}};
    skip_map<int, std::string> m2 = {{2, "deux"}, {3, "three"}};

    m1.merge(m2);

    EXPECT_EQ(m1.size(), 3u);
    EXPECT_EQ(m1.at(1), "one");
    EXPECT_EQ(m1.at(2), "two"); // m1's value for key 2 is preserved
    EXPECT_EQ(m1.at(3), "three");
    EXPECT_TRUE(m2.empty()); // all elements moved or skipped
}
