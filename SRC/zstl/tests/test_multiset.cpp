// ============================================================================
// zstl multiset container tests
// ============================================================================
// Tests for: zstl::multiset — ordered set with duplicate values, red-black tree
// Covers: constructors, insert/emplace of duplicates, erase, lookup
//         (find, count, contains, equal_range, lower/upper_bound),
//         iteration, capacity, clear, swap, merge
// ============================================================================

#include <gtest/gtest.h>
#include "zstl/zstl.h"

#include <vector>
#include <string>

using namespace zstl;

// ============================================================================
// Constructor tests
// ============================================================================

TEST(Multiset, DefaultConstructor) {
    multiset<int> s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
}

TEST(Multiset, RangeConstructor) {
    std::vector<int> vec = {3, 1, 2, 1, 4, 2, 1};
    multiset<int> s(vec.begin(), vec.end());
    EXPECT_EQ(s.size(), 7u);
    EXPECT_FALSE(s.empty());

    // Verify sorted order
    int prev = 0;
    for (const auto& val : s) {
        EXPECT_GE(val, prev);
        prev = val;
    }
}

TEST(Multiset, CopyConstructor) {
    multiset<int> s1;
    s1.insert(1);
    s1.insert(2);
    s1.insert(2);
    s1.insert(3);

    multiset<int> s2(s1);
    EXPECT_EQ(s2.size(), 4u);
    EXPECT_EQ(s2.count(1), 1u);
    EXPECT_EQ(s2.count(2), 2u);
    EXPECT_EQ(s2.count(3), 1u);

    // Deep copy: modifying s2 doesn't affect s1
    s2.insert(4);
    EXPECT_EQ(s1.size(), 4u);
    EXPECT_EQ(s2.size(), 5u);
}

TEST(Multiset, MoveConstructor) {
    multiset<int> s1;
    s1.insert(1);
    s1.insert(2);
    s1.insert(2);

    multiset<int> s2(zstl::move(s1));
    EXPECT_EQ(s2.size(), 3u);
    EXPECT_EQ(s2.count(1), 1u);
    EXPECT_EQ(s2.count(2), 2u);
}

TEST(Multiset, InitializerListConstructor) {
    multiset<int> s = {4, 2, 2, 1, 3, 4, 1};
    EXPECT_EQ(s.size(), 7u);
    EXPECT_EQ(s.count(1), 2u);
    EXPECT_EQ(s.count(2), 2u);
    EXPECT_EQ(s.count(3), 1u);
    EXPECT_EQ(s.count(4), 2u);

    // Verify sorted order
    auto it = s.begin();
    EXPECT_EQ(*it, 1); ++it;
    EXPECT_EQ(*it, 1); ++it;
    EXPECT_EQ(*it, 2); ++it;
    EXPECT_EQ(*it, 2); ++it;
    EXPECT_EQ(*it, 3); ++it;
    EXPECT_EQ(*it, 4); ++it;
    EXPECT_EQ(*it, 4);
}

// ============================================================================
// Insert tests
// ============================================================================

TEST(Multiset, InsertSingleDuplicates) {
    multiset<int> s;
    auto it1 = s.insert(42);
    EXPECT_NE(it1, s.end());
    EXPECT_EQ(*it1, 42);

    auto it2 = s.insert(42);
    EXPECT_NE(it2, s.end());
    EXPECT_EQ(*it2, 42);

    auto it3 = s.insert(42);
    EXPECT_NE(it3, s.end());
    EXPECT_EQ(*it3, 42);

    EXPECT_EQ(s.size(), 3u);
    EXPECT_EQ(s.count(42), 3u);
}

TEST(Multiset, InsertRange) {
    std::vector<int> data = {5, 3, 5, 1, 3, 5};
    multiset<int> s;
    s.insert(data.begin(), data.end());

    EXPECT_EQ(s.size(), 6u);
    EXPECT_EQ(s.count(1), 1u);
    EXPECT_EQ(s.count(3), 2u);
    EXPECT_EQ(s.count(5), 3u);
}

TEST(Multiset, InsertWithHint) {
    multiset<int> s = {1, 5, 10};

    auto hint = s.find(5);
    auto it = s.insert(hint, 5);
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(s.count(5), 2u);
}

TEST(Multiset, InsertInitializerList) {
    multiset<int> s;
    s.insert({1, 2, 1, 2, 2, 3});
    EXPECT_EQ(s.size(), 6u);
    EXPECT_EQ(s.count(1), 2u);
    EXPECT_EQ(s.count(2), 3u);
    EXPECT_EQ(s.count(3), 1u);
}

// ============================================================================
// Emplace tests
// ============================================================================

TEST(Multiset, EmplaceDuplicates) {
    multiset<int> s;
    auto it1 = s.emplace(10);
    EXPECT_EQ(*it1, 10);

    auto it2 = s.emplace(10);
    EXPECT_EQ(*it2, 10);

    auto it3 = s.emplace(20);
    EXPECT_EQ(*it3, 20);

    EXPECT_EQ(s.size(), 3u);
    EXPECT_EQ(s.count(10), 2u);
    EXPECT_EQ(s.count(20), 1u);
}

TEST(Multiset, EmplaceHint) {
    multiset<int> s = {1, 3};
    auto hint = s.find(1);
    auto it = s.emplace_hint(hint, 1);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(s.count(1), 2u);
}

// ============================================================================
// Erase tests
// ============================================================================

TEST(Multiset, EraseByKeyRemovesAllDuplicates) {
    multiset<int> s = {1, 2, 2, 2, 3};
    EXPECT_EQ(s.size(), 5u);

    size_t removed = s.erase(2);
    EXPECT_EQ(removed, 3u);
    EXPECT_EQ(s.size(), 2u);
    EXPECT_EQ(s.count(2), 0u);
    EXPECT_EQ(s.count(1), 1u);
    EXPECT_EQ(s.count(3), 1u);
}

TEST(Multiset, EraseByIterator) {
    multiset<int> s = {1, 1, 1};
    EXPECT_EQ(s.size(), 3u);

    auto it = s.find(1);
    auto next = s.erase(it);
    EXPECT_EQ(s.size(), 2u);
    EXPECT_EQ(*next, 1);

    next = s.erase(next);
    EXPECT_EQ(s.size(), 1u);

    s.erase(next);
    EXPECT_EQ(s.size(), 0u);
}

TEST(Multiset, EraseRange) {
    multiset<int> s = {1, 2, 2, 2, 3};
    auto first = s.lower_bound(2);
    auto last = s.upper_bound(2);
    auto it = s.erase(first, last);
    EXPECT_EQ(s.size(), 2u);
    EXPECT_EQ(s.count(2), 0u);
    EXPECT_EQ(*it, 3);
}

TEST(Multiset, EraseNonExistent) {
    multiset<int> s = {1, 2, 3};
    EXPECT_EQ(s.erase(99), 0u);
    EXPECT_EQ(s.size(), 3u);
}

// ============================================================================
// Lookup tests
// ============================================================================

TEST(Multiset, Find) {
    multiset<int> s = {1, 1, 2, 3, 3, 3};

    auto it = s.find(1);
    EXPECT_NE(it, s.end());
    EXPECT_EQ(*it, 1);

    auto it2 = s.find(3);
    EXPECT_NE(it2, s.end());
    EXPECT_EQ(*it2, 3);

    EXPECT_EQ(s.find(99), s.end());
}

TEST(Multiset, FindConst) {
    const multiset<int> s = {1, 2, 2, 3};
    auto it = s.find(2);
    EXPECT_NE(it, s.end());
    EXPECT_EQ(*it, 2);
}

TEST(Multiset, Count) {
    multiset<int> s;
    EXPECT_EQ(s.count(5), 0u);

    s.insert(5);
    EXPECT_EQ(s.count(5), 1u);

    s.insert(5);
    s.insert(5);
    EXPECT_EQ(s.count(5), 3u);

    EXPECT_EQ(s.count(99), 0u);
}

TEST(Multiset, Contains) {
    multiset<int> s = {1, 2, 2, 3};
    EXPECT_TRUE(s.contains(1));
    EXPECT_TRUE(s.contains(2));
    EXPECT_TRUE(s.contains(3));
    EXPECT_FALSE(s.contains(0));
    EXPECT_FALSE(s.contains(4));
}

// ============================================================================
// equal_range tests
// ============================================================================

TEST(Multiset, EqualRangeDuplicates) {
    multiset<int> s = {1, 2, 2, 2, 2, 3};
    auto range = s.equal_range(2);
    int count = 0;
    for (auto it = range.first; it != range.second; ++it) {
        EXPECT_EQ(*it, 2);
        ++count;
    }
    EXPECT_EQ(count, 4);
}

TEST(Multiset, EqualRangeNonExistent) {
    multiset<int> s = {1, 2, 3};
    auto range = s.equal_range(99);
    EXPECT_EQ(range.first, range.second);
}

TEST(Multiset, EqualRangeSingle) {
    multiset<int> s = {1, 2, 3};
    auto range = s.equal_range(2);
    int count = 0;
    for (auto it = range.first; it != range.second; ++it) {
        EXPECT_EQ(*it, 2);
        ++count;
    }
    EXPECT_EQ(count, 1);
}

TEST(Multiset, EqualRangeConst) {
    const multiset<int> s = {1, 1, 2};
    auto range = s.equal_range(1);
    int count = 0;
    for (auto it = range.first; it != range.second; ++it) {
        EXPECT_EQ(*it, 1);
        ++count;
    }
    EXPECT_EQ(count, 2);
}

// ============================================================================
// lower_bound / upper_bound tests
// ============================================================================

TEST(Multiset, LowerBound) {
    multiset<int> s = {10, 20, 20, 30};

    auto lb = s.lower_bound(20);
    EXPECT_EQ(*lb, 20);

    auto lb2 = s.lower_bound(25);
    EXPECT_EQ(*lb2, 30);

    auto lb3 = s.lower_bound(50);
    EXPECT_EQ(lb3, s.end());

    auto lb4 = s.lower_bound(0);
    EXPECT_EQ(*lb4, 10);
}

TEST(Multiset, UpperBound) {
    multiset<int> s = {10, 20, 20, 30};

    auto ub = s.upper_bound(20);
    EXPECT_EQ(*ub, 30);

    auto ub2 = s.upper_bound(25);
    EXPECT_EQ(*ub2, 30);

    auto ub3 = s.upper_bound(30);
    EXPECT_EQ(ub3, s.end());
}

TEST(Multiset, LowerBoundUpperBoundTogether) {
    multiset<int> s = {1, 2, 2, 2, 3};
    auto lb = s.lower_bound(2);
    auto ub = s.upper_bound(2);
    int count = 0;
    for (auto it = lb; it != ub; ++it) {
        EXPECT_EQ(*it, 2);
        ++count;
    }
    EXPECT_EQ(count, 3);
}

// ============================================================================
// Iteration tests
// ============================================================================

TEST(Multiset, ForwardIteration) {
    multiset<int> s = {3, 1, 2, 1};
    std::vector<int> result(s.begin(), s.end());
    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 2);
    EXPECT_EQ(result[3], 3);
}

TEST(Multiset, ConstIteration) {
    const multiset<int> s = {3, 1, 2};
    std::vector<int> result;
    for (auto it = s.cbegin(); it != s.cend(); ++it) {
        result.push_back(*it);
    }
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
}

TEST(Multiset, ReverseIteration) {
    multiset<int> s = {1, 2, 3};
    std::vector<int> result;
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        result.push_back(*it);
    }
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], 3);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 1);
}

TEST(Multiset, InsertionOrderPreservedForDuplicates) {
    multiset<int> s;
    s.insert(1);
    s.insert(2);
    s.insert(1);  // duplicate at later position
    s.insert(2);  // another duplicate

    // Iterate and count: all 1s appear before 2s
    bool seen_two = false;
    for (auto it = s.begin(); it != s.end(); ++it) {
        if (*it == 2) seen_two = true;
        if (seen_two) {
            EXPECT_NE(*it, 1);  // No 1 appears after a 2
        }
    }
}

// ============================================================================
// Capacity tests
// ============================================================================

TEST(Multiset, Empty) {
    multiset<int> s;
    EXPECT_TRUE(s.empty());
    s.insert(1);
    EXPECT_FALSE(s.empty());
    s.erase(1);
    EXPECT_TRUE(s.empty());
}

TEST(Multiset, Size) {
    multiset<int> s;
    EXPECT_EQ(s.size(), 0u);
    s.insert(1);
    EXPECT_EQ(s.size(), 1u);
    s.insert(1);
    EXPECT_EQ(s.size(), 2u);
    s.insert(2);
    EXPECT_EQ(s.size(), 3u);
    s.erase(1);
    EXPECT_EQ(s.size(), 1u);
    s.erase(2);
    EXPECT_EQ(s.size(), 0u);
}

TEST(Multiset, MaxSize) {
    multiset<int> s;
    EXPECT_GT(s.max_size(), 0u);
}

// ============================================================================
// Clear and swap tests
// ============================================================================

TEST(Multiset, Clear) {
    multiset<int> s = {1, 2, 2, 3};
    EXPECT_EQ(s.size(), 4u);
    s.clear();
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);

    // Can insert after clear
    s.insert(42);
    EXPECT_EQ(s.size(), 1u);
}

TEST(Multiset, Swap) {
    multiset<int> s1 = {1, 2, 2};
    multiset<int> s2 = {10, 20};

    s1.swap(s2);
    EXPECT_EQ(s1.size(), 2u);
    EXPECT_EQ(s1.count(10), 1u);
    EXPECT_EQ(s1.count(20), 1u);

    EXPECT_EQ(s2.size(), 3u);
    EXPECT_EQ(s2.count(1), 1u);
    EXPECT_EQ(s2.count(2), 2u);
}

TEST(Multiset, SwapFreeFunction) {
    multiset<int> s1 = {1, 1};
    multiset<int> s2 = {2, 2};
    zstl::swap(s1, s2);
    EXPECT_EQ(s1.count(2), 2u);
    EXPECT_EQ(s2.count(1), 2u);
}

// ============================================================================
// Merge tests
// ============================================================================

TEST(Multiset, Merge) {
    multiset<int> s1 = {1, 2, 2};
    multiset<int> s2 = {2, 3, 3};

    s1.merge(s2);
    EXPECT_EQ(s1.size(), 6u);
    EXPECT_EQ(s1.count(1), 1u);
    EXPECT_EQ(s1.count(2), 3u);
    EXPECT_EQ(s1.count(3), 2u);
    EXPECT_TRUE(s2.empty());
}

// ============================================================================
// Many duplicates tests
// ============================================================================

TEST(Multiset, ManyDuplicatesAllPreserved) {
    multiset<int> s;
    const int num = 200;
    for (int i = 0; i < num; ++i) {
        s.insert(99);
    }
    EXPECT_EQ(s.size(), static_cast<size_t>(num));
    EXPECT_EQ(s.count(99), static_cast<size_t>(num));

    int iterated = 0;
    for (const auto& val : s) {
        EXPECT_EQ(val, 99);
        ++iterated;
    }
    EXPECT_EQ(iterated, num);
}

TEST(Multiset, MixedDuplicates) {
    multiset<int> s;
    // Insert keys 0..19, each 10 times
    for (int key = 0; key < 20; ++key) {
        for (int i = 0; i < 10; ++i) {
            s.insert(key);
        }
    }
    EXPECT_EQ(s.size(), 200u);
    for (int key = 0; key < 20; ++key) {
        EXPECT_EQ(s.count(key), 10u);
    }

    // Verify sorted order
    int prev = 0;
    for (const auto& val : s) {
        EXPECT_GE(val, prev);
        prev = val;
    }
}

// ============================================================================
// Comparison tests
// ============================================================================

TEST(Multiset, Equality) {
    multiset<int> s1 = {1, 2, 2, 3};
    multiset<int> s2 = {1, 2, 2, 3};
    multiset<int> s3 = {1, 2, 3};

    EXPECT_TRUE(s1 == s2);
    EXPECT_FALSE(s1 == s3);
    EXPECT_TRUE(s1 != s3);
}

// ============================================================================
// Observers tests
// ============================================================================

TEST(Multiset, KeyComp) {
    multiset<int> s;
    auto comp = s.key_comp();
    EXPECT_TRUE(comp(1, 2));
    EXPECT_FALSE(comp(2, 1));
    EXPECT_FALSE(comp(1, 1));
}

TEST(Multiset, ValueComp) {
    multiset<int> s;
    auto comp = s.value_comp();
    EXPECT_TRUE(comp(1, 2));
    EXPECT_FALSE(comp(2, 1));
}

// ============================================================================
// Assignment tests
// ============================================================================

TEST(Multiset, CopyAssignment) {
    multiset<int> s1 = {1, 2, 2};
    multiset<int> s2;
    s2 = s1;
    EXPECT_EQ(s2.size(), 3u);
    EXPECT_EQ(s2.count(2), 2u);
    s2.insert(3);
    EXPECT_EQ(s1.size(), 3u);
    EXPECT_EQ(s2.size(), 4u);
}

TEST(Multiset, MoveAssignment) {
    multiset<int> s1 = {1, 2, 2};
    multiset<int> s2;
    s2 = zstl::move(s1);
    EXPECT_EQ(s2.size(), 3u);
}

TEST(Multiset, InitializerListAssignment) {
    multiset<int> s;
    s = {1, 2, 2, 3};
    EXPECT_EQ(s.size(), 4u);
    EXPECT_EQ(s.count(2), 2u);
}
