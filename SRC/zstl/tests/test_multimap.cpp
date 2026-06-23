// ============================================================================
// zstl multimap container tests
// ============================================================================
// Tests for: zstl::multimap — ordered map with duplicate keys, red-black tree
// Covers: constructors, insert/emplace of duplicates, erase, lookup
//         (find, count, equal_range, lower/upper_bound), iteration,
//         capacity, clear, swap, merge
// ============================================================================

#include <gtest/gtest.h>
#include "zstl/zstl.h"

#include <string>
#include <vector>
#include <utility>

using namespace zstl;

// ============================================================================
// Constructor tests
// ============================================================================

TEST(Multimap, DefaultConstructor) {
    multimap<int, std::string> m;
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0u);
}

TEST(Multimap, RangeConstructor) {
    std::vector<pair<int, std::string>> vec = {
        {3, "three"}, {1, "one"}, {2, "two"}, {1, "one-again"}
    };
    multimap<int, std::string> m(vec.begin(), vec.end());
    EXPECT_EQ(m.size(), 4u);
    EXPECT_FALSE(m.empty());

    // Verify iteration order is sorted by key
    int prev = 0;
    for (const auto& p : m) {
        EXPECT_GE(p.first, prev);
        prev = p.first;
    }
}

TEST(Multimap, CopyConstructor) {
    multimap<int, std::string> m1;
    m1.insert({1, "one"});
    m1.insert({2, "two"});
    m1.insert({2, "two-dup"});

    multimap<int, std::string> m2(m1);
    EXPECT_EQ(m2.size(), 3u);
    EXPECT_EQ(m2.count(1), 1u);
    EXPECT_EQ(m2.count(2), 2u);

    // Verify deep copy: modifying m2 doesn't affect m1
    m2.insert({3, "three"});
    EXPECT_EQ(m1.size(), 3u);
    EXPECT_EQ(m2.size(), 4u);
}

TEST(Multimap, MoveConstructor) {
    multimap<int, std::string> m1;
    m1.insert({1, "one"});
    m1.insert({2, "two"});
    m1.insert({2, "two-dup"});

    multimap<int, std::string> m2(zstl::move(m1));
    EXPECT_EQ(m2.size(), 3u);
    EXPECT_EQ(m2.count(1), 1u);
    EXPECT_EQ(m2.count(2), 2u);

    // m1 should be in valid but unspecified state after move
    EXPECT_TRUE(m1.empty() || m1.size() == 0u);
}

TEST(Multimap, InitializerListConstructor) {
    multimap<int, std::string> m = {
        {4, "four"}, {2, "two"}, {2, "two-dup"}, {1, "one"}, {3, "three"}
    };
    EXPECT_EQ(m.size(), 5u);
    EXPECT_EQ(m.count(1), 1u);
    EXPECT_EQ(m.count(2), 2u);
    EXPECT_EQ(m.count(3), 1u);
    EXPECT_EQ(m.count(4), 1u);

    // Verify sorted order
    auto it = m.begin();
    EXPECT_EQ(it->first, 1);
    ++it;
    EXPECT_EQ(it->first, 2);
    ++it;
    EXPECT_EQ(it->first, 2);
    ++it;
    EXPECT_EQ(it->first, 3);
    ++it;
    EXPECT_EQ(it->first, 4);
}

// ============================================================================
// Insert tests (duplicate keys)
// ============================================================================

TEST(Multimap, InsertSingleDuplicateKeys) {
    multimap<int, std::string> m;
    auto it1 = m.insert({1, "first-1"});
    EXPECT_EQ(it1->first, 1);
    EXPECT_EQ(it1->second, "first-1");

    auto it2 = m.insert({1, "second-1"});
    EXPECT_EQ(it2->first, 1);
    EXPECT_EQ(it2->second, "second-1");

    auto it3 = m.insert({1, "third-1"});
    EXPECT_EQ(it3->first, 1);
    EXPECT_EQ(it3->second, "third-1");

    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m.count(1), 3u);
}

TEST(Multimap, InsertRange) {
    std::vector<pair<int, std::string>> data = {
        {5, "a"}, {3, "b"}, {5, "c"}, {1, "d"}, {3, "e"}, {5, "f"}
    };
    multimap<int, std::string> m;
    m.insert(data.begin(), data.end());

    EXPECT_EQ(m.size(), 6u);
    EXPECT_EQ(m.count(1), 1u);
    EXPECT_EQ(m.count(3), 2u);
    EXPECT_EQ(m.count(5), 3u);
}

TEST(Multimap, InsertWithHint) {
    multimap<int, std::string> m = {
        {1, "one"}, {5, "five"}, {10, "ten"}
    };

    // Insert with hint near the correct position
    auto hint = m.find(5);
    auto it = m.insert(hint, {5, "five-dup"});
    EXPECT_EQ(it->first, 5);
    EXPECT_EQ(it->second, "five-dup");
    EXPECT_EQ(m.count(5), 2u);
}

TEST(Multimap, InsertInitializerList) {
    multimap<int, std::string> m;
    m.insert({{1, "a"}, {2, "b"}, {1, "c"}, {2, "d"}, {2, "e"}});

    EXPECT_EQ(m.size(), 5u);
    EXPECT_EQ(m.count(1), 2u);
    EXPECT_EQ(m.count(2), 3u);
}

// ============================================================================
// Emplace tests
// ============================================================================

TEST(Multimap, EmplaceDuplicateKeys) {
    multimap<int, std::string> m;
    auto it1 = m.emplace(1, "apple");
    EXPECT_EQ(it1->first, 1);
    EXPECT_EQ(it1->second, "apple");

    auto it2 = m.emplace(1, "banana");
    EXPECT_EQ(it2->first, 1);
    EXPECT_EQ(it2->second, "banana");

    auto it3 = m.emplace(2, "cherry");
    EXPECT_EQ(it3->first, 2);

    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m.count(1), 2u);
    EXPECT_EQ(m.count(2), 1u);
}

TEST(Multimap, EmplaceHint) {
    multimap<int, std::string> m = {{1, "a"}, {3, "c"}};
    auto hint = m.find(1);
    auto it = m.emplace_hint(hint, 1, "b");
    EXPECT_EQ(it->first, 1);
    EXPECT_EQ(it->second, "b");
    EXPECT_EQ(m.count(1), 2u);
}

// ============================================================================
// Erase tests
// ============================================================================

TEST(Multimap, EraseByKeyRemovesAllDuplicates) {
    multimap<int, std::string> m = {
        {1, "a"}, {2, "b"}, {2, "c"}, {2, "d"}, {3, "e"}
    };
    EXPECT_EQ(m.size(), 5u);

    size_t removed = m.erase(2);
    EXPECT_EQ(removed, 3u);
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m.count(2), 0u);
    EXPECT_EQ(m.count(1), 1u);
    EXPECT_EQ(m.count(3), 1u);
}

TEST(Multimap, EraseByIterator) {
    multimap<int, std::string> m = {
        {1, "a"}, {1, "b"}, {1, "c"}
    };
    EXPECT_EQ(m.size(), 3u);

    auto it = m.find(1);
    EXPECT_NE(it, m.end());
    auto next = m.erase(it);
    EXPECT_EQ(m.size(), 2u);
    // next should point to the next element (another 1)
    EXPECT_EQ(next->first, 1);

    // Erase remaining
    next = m.erase(next);
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(next->first, 1);

    m.erase(next);
    EXPECT_EQ(m.size(), 0u);
}

TEST(Multimap, EraseRange) {
    multimap<int, std::string> m = {
        {1, "one"}, {2, "a"}, {2, "b"}, {2, "c"}, {3, "three"}
    };

    // Erase the range of key 2 duplicates
    auto first = m.lower_bound(2);
    auto last = m.upper_bound(2);
    auto it = m.erase(first, last);
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m.count(2), 0u);
    // it should point to element after the erased range
    EXPECT_EQ(it->first, 3);
}

TEST(Multimap, EraseOnEmptyDoesNothing) {
    multimap<int, std::string> m;
    size_t removed = m.erase(42);
    EXPECT_EQ(removed, 0u);
    EXPECT_TRUE(m.empty());
}

TEST(Multimap, EraseNonExistentKey) {
    multimap<int, std::string> m = {{1, "a"}, {2, "b"}};
    size_t removed = m.erase(99);
    EXPECT_EQ(removed, 0u);
    EXPECT_EQ(m.size(), 2u);
}

// ============================================================================
// Lookup tests: find, count, contains
// ============================================================================

TEST(Multimap, FindReturnsFirstMatch) {
    multimap<int, std::string> m = {
        {1, "first"}, {1, "second"}, {1, "third"}, {2, "two"}
    };
    auto it = m.find(1);
    EXPECT_NE(it, m.end());
    EXPECT_EQ(it->first, 1);
    EXPECT_EQ(it->second, "first");  // first insertion with key 1

    auto it2 = m.find(99);
    EXPECT_EQ(it2, m.end());
}

TEST(Multimap, FindConst) {
    const multimap<int, std::string> m = {
        {1, "a"}, {2, "b"}, {2, "c"}
    };
    auto it = m.find(2);
    EXPECT_NE(it, m.end());
    EXPECT_EQ(it->first, 2);
}

TEST(Multimap, CountDuplicates) {
    multimap<int, std::string> m;
    EXPECT_EQ(m.count(1), 0u);

    m.insert({1, "a"});
    EXPECT_EQ(m.count(1), 1u);

    m.insert({1, "b"});
    EXPECT_EQ(m.count(1), 2u);

    m.insert({1, "c"});
    EXPECT_EQ(m.count(1), 3u);

    EXPECT_EQ(m.count(99), 0u);
}

TEST(Multimap, Contains) {
    multimap<int, std::string> m = {{1, "a"}, {2, "b"}, {2, "c"}};
    EXPECT_TRUE(m.contains(1));
    EXPECT_TRUE(m.contains(2));
    EXPECT_FALSE(m.contains(3));
    EXPECT_FALSE(m.contains(0));
}

// ============================================================================
// equal_range tests
// ============================================================================

TEST(Multimap, EqualRangeAllDuplicatesInRange) {
    multimap<int, std::string> m = {
        {1, "one"},
        {2, "a"}, {2, "b"}, {2, "c"}, {2, "d"},
        {3, "three"}
    };

    auto range = m.equal_range(2);
    int count = 0;
    for (auto it = range.first; it != range.second; ++it) {
        EXPECT_EQ(it->first, 2);
        ++count;
    }
    EXPECT_EQ(count, 4);
}

TEST(Multimap, EqualRangeNonExistentKey) {
    multimap<int, std::string> m = {{1, "a"}, {2, "b"}};
    auto range = m.equal_range(99);
    EXPECT_EQ(range.first, range.second);
    EXPECT_EQ(range.first, m.end());
}

TEST(Multimap, EqualRangeSingleElement) {
    multimap<int, std::string> m = {{1, "a"}, {2, "b"}, {3, "c"}};
    auto range = m.equal_range(2);
    int count = 0;
    for (auto it = range.first; it != range.second; ++it) {
        EXPECT_EQ(it->first, 2);
        ++count;
    }
    EXPECT_EQ(count, 1);
}

TEST(Multimap, EqualRangeConst) {
    const multimap<int, std::string> m = {
        {1, "one"}, {1, "uno"}, {2, "two"}
    };
    auto range = m.equal_range(1);
    int count = 0;
    for (auto it = range.first; it != range.second; ++it) {
        EXPECT_EQ(it->first, 1);
        ++count;
    }
    EXPECT_EQ(count, 2);
}

// ============================================================================
// lower_bound / upper_bound tests
// ============================================================================

TEST(Multimap, LowerBound) {
    multimap<int, std::string> m = {
        {10, "a"}, {20, "b"}, {20, "c"}, {30, "d"}
    };

    auto lb = m.lower_bound(20);
    EXPECT_NE(lb, m.end());
    EXPECT_EQ(lb->first, 20);  // first element with key >= 20

    auto lb2 = m.lower_bound(25);
    EXPECT_NE(lb2, m.end());
    EXPECT_EQ(lb2->first, 30);  // first element with key >= 25

    auto lb3 = m.lower_bound(50);
    EXPECT_EQ(lb3, m.end());  // no element >= 50

    auto lb4 = m.lower_bound(0);
    EXPECT_EQ(lb4->first, 10);  // first element with key >= 0
}

TEST(Multimap, UpperBound) {
    multimap<int, std::string> m = {
        {10, "a"}, {20, "b"}, {20, "c"}, {30, "d"}
    };

    auto ub = m.upper_bound(20);
    EXPECT_NE(ub, m.end());
    EXPECT_EQ(ub->first, 30);  // first element with key > 20

    auto ub2 = m.upper_bound(25);
    EXPECT_NE(ub2, m.end());
    EXPECT_EQ(ub2->first, 30);  // first element with key > 25

    auto ub3 = m.upper_bound(30);
    EXPECT_EQ(ub3, m.end());  // no element > 30

    auto ub4 = m.upper_bound(0);
    EXPECT_EQ(ub4->first, 10);  // first element with key > 0
}

TEST(Multimap, LowerBoundUpperBoundWorkTogether) {
    multimap<int, std::string> m = {
        {1, "a"}, {2, "b"}, {2, "c"}, {2, "d"}, {3, "e"}
    };

    auto lb = m.lower_bound(2);
    auto ub = m.upper_bound(2);
    int count = 0;
    for (auto it = lb; it != ub; ++it) {
        EXPECT_EQ(it->first, 2);
        ++count;
    }
    EXPECT_EQ(count, 3);  // three elements with key 2
}

// ============================================================================
// Iteration tests
// ============================================================================

TEST(Multimap, BeginEndIteration) {
    multimap<int, std::string> m = {
        {3, "c"}, {1, "a"}, {2, "b"}, {1, "a2"}
    };

    std::vector<int> keys;
    for (auto it = m.begin(); it != m.end(); ++it) {
        keys.push_back(it->first);
    }
    ASSERT_EQ(keys.size(), 4u);
    EXPECT_EQ(keys[0], 1);
    EXPECT_EQ(keys[1], 1);
    EXPECT_EQ(keys[2], 2);
    EXPECT_EQ(keys[3], 3);
}

TEST(Multimap, ConstBeginEndIteration) {
    const multimap<int, std::string> m = {
        {3, "c"}, {1, "a"}, {2, "b"}
    };
    std::vector<int> keys;
    for (auto it = m.cbegin(); it != m.cend(); ++it) {
        keys.push_back(it->first);
    }
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0], 1);
    EXPECT_EQ(keys[1], 2);
    EXPECT_EQ(keys[2], 3);
}

TEST(Multimap, ReverseIteration) {
    multimap<int, std::string> m = {
        {1, "a"}, {2, "b"}, {3, "c"}
    };
    std::vector<int> keys;
    for (auto it = m.rbegin(); it != m.rend(); ++it) {
        keys.push_back(it->first);
    }
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0], 3);
    EXPECT_EQ(keys[1], 2);
    EXPECT_EQ(keys[2], 1);
}

TEST(Multimap, IterationOrderForEqualKeys) {
    // Insertion order for duplicate keys should be preserved
    multimap<int, std::string> m;
    m.insert({1, "first"});
    m.insert({1, "second"});
    m.insert({1, "third"});

    std::vector<std::string> values;
    for (auto it = m.begin(); it != m.end(); ++it) {
        values.push_back(it->second);
    }
    ASSERT_EQ(values.size(), 3u);
    EXPECT_EQ(values[0], "first");
    EXPECT_EQ(values[1], "second");
    EXPECT_EQ(values[2], "third");
}

// ============================================================================
// Capacity tests
// ============================================================================

TEST(Multimap, Empty) {
    multimap<int, std::string> m;
    EXPECT_TRUE(m.empty());

    m.insert({1, "one"});
    EXPECT_FALSE(m.empty());

    m.erase(1);
    EXPECT_TRUE(m.empty());
}

TEST(Multimap, Size) {
    multimap<int, std::string> m;
    EXPECT_EQ(m.size(), 0u);

    m.insert({1, "a"});
    EXPECT_EQ(m.size(), 1u);

    m.insert({1, "b"});
    EXPECT_EQ(m.size(), 2u);

    m.insert({2, "c"});
    EXPECT_EQ(m.size(), 3u);

    m.erase(1);
    EXPECT_EQ(m.size(), 1u);

    m.erase(2);
    EXPECT_EQ(m.size(), 0u);
}

TEST(Multimap, MaxSize) {
    multimap<int, std::string> m;
    EXPECT_GT(m.max_size(), 0u);
}

// ============================================================================
// Clear and swap tests
// ============================================================================

TEST(Multimap, Clear) {
    multimap<int, std::string> m = {
        {1, "a"}, {2, "b"}, {2, "c"}, {3, "d"}
    };
    EXPECT_EQ(m.size(), 4u);

    m.clear();
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0u);

    // After clear, we can insert again
    m.insert({1, "new"});
    EXPECT_EQ(m.size(), 1u);
}

TEST(Multimap, Swap) {
    multimap<int, std::string> m1 = {{1, "a"}, {2, "b"}, {2, "c"}};
    multimap<int, std::string> m2 = {{10, "x"}, {20, "y"}};

    m1.swap(m2);

    EXPECT_EQ(m1.size(), 2u);
    EXPECT_EQ(m1.count(10), 1u);
    EXPECT_EQ(m1.count(20), 1u);

    EXPECT_EQ(m2.size(), 3u);
    EXPECT_EQ(m2.count(1), 1u);
    EXPECT_EQ(m2.count(2), 2u);
}

TEST(Multimap, SwapFreeFunction) {
    multimap<int, std::string> m1 = {{1, "a"}};
    multimap<int, std::string> m2 = {{2, "b"}};

    zstl::swap(m1, m2);

    EXPECT_EQ(m1.count(2), 1u);
    EXPECT_EQ(m2.count(1), 1u);
}

// ============================================================================
// Merge tests
// ============================================================================

TEST(Multimap, Merge) {
    multimap<int, std::string> m1 = {
        {1, "a"}, {2, "b"}, {2, "c"}
    };
    multimap<int, std::string> m2 = {
        {2, "b2"}, {3, "d"}, {3, "e"}
    };

    m1.merge(m2);

    EXPECT_EQ(m1.size(), 6u);
    EXPECT_EQ(m1.count(1), 1u);
    EXPECT_EQ(m1.count(2), 3u);
    EXPECT_EQ(m1.count(3), 2u);

    EXPECT_TRUE(m2.empty());
}

// ============================================================================
// Many duplicates tests
// ============================================================================

TEST(Multimap, ManyDuplicatesAllPreserved) {
    multimap<int, std::string> m;
    const int num_duplicates = 100;

    for (int i = 0; i < num_duplicates; ++i) {
        m.insert({42, "val-" + std::to_string(i)});
    }

    EXPECT_EQ(m.size(), static_cast<size_t>(num_duplicates));
    EXPECT_EQ(m.count(42), static_cast<size_t>(num_duplicates));

    // Verify all are iterable
    int iterated = 0;
    for (auto it = m.begin(); it != m.end(); ++it) {
        EXPECT_EQ(it->first, 42);
        ++iterated;
    }
    EXPECT_EQ(iterated, num_duplicates);
}

TEST(Multimap, MixedKeysAndDuplicates) {
    multimap<int, std::string> m;

    // Insert keys 0..9, each with 5 duplicates
    for (int key = 0; key < 10; ++key) {
        for (int dup = 0; dup < 5; ++dup) {
            m.insert({key, "v" + std::to_string(dup)});
        }
    }

    EXPECT_EQ(m.size(), 50u);
    for (int key = 0; key < 10; ++key) {
        EXPECT_EQ(m.count(key), 5u);
    }

    // Verify iteration order: grouped by key, sorted
    auto range = m.equal_range(5);
    int count = 0;
    for (auto it = range.first; it != range.second; ++it) {
        EXPECT_EQ(it->first, 5);
        ++count;
    }
    EXPECT_EQ(count, 5);
}

// ============================================================================
// Comparison tests
// ============================================================================

TEST(Multimap, Equality) {
    multimap<int, std::string> m1 = {{1, "a"}, {2, "b"}, {2, "c"}};
    multimap<int, std::string> m2 = {{1, "a"}, {2, "b"}, {2, "c"}};
    multimap<int, std::string> m3 = {{1, "a"}, {2, "b"}};

    EXPECT_TRUE(m1 == m2);
    EXPECT_FALSE(m1 != m2);
    EXPECT_TRUE(m1 != m3);
    EXPECT_FALSE(m1 == m3);
}

// ============================================================================
// Observers tests
// ============================================================================

TEST(Multimap, KeyComp) {
    multimap<int, std::string> m;
    auto comp = m.key_comp();
    EXPECT_TRUE(comp(1, 2));
    EXPECT_FALSE(comp(2, 1));
    EXPECT_FALSE(comp(1, 1));
}

TEST(Multimap, ValueComp) {
    multimap<int, std::string> m;
    auto vcomp = m.value_comp();
    EXPECT_TRUE(vcomp({1, "a"}, {2, "b"}));
    EXPECT_FALSE(vcomp({2, "a"}, {1, "b"}));
}

// ============================================================================
// Copy/Move assignment tests
// ============================================================================

TEST(Multimap, CopyAssignment) {
    multimap<int, std::string> m1 = {{1, "a"}, {2, "b"}, {2, "c"}};
    multimap<int, std::string> m2;
    m2 = m1;

    EXPECT_EQ(m2.size(), 3u);
    EXPECT_EQ(m2.count(2), 2u);

    // Independent copies
    m2.insert({3, "d"});
    EXPECT_EQ(m1.size(), 3u);
    EXPECT_EQ(m2.size(), 4u);
}

TEST(Multimap, MoveAssignment) {
    multimap<int, std::string> m1 = {{1, "a"}, {2, "b"}};
    multimap<int, std::string> m2;
    m2 = zstl::move(m1);

    EXPECT_EQ(m2.size(), 2u);
}

TEST(Multimap, InitializerListAssignment) {
    multimap<int, std::string> m;
    m = {{1, "a"}, {2, "b"}, {2, "c"}};

    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m.count(1), 1u);
    EXPECT_EQ(m.count(2), 2u);
}
