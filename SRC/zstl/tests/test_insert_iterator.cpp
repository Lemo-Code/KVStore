// ============================================================================
// zstl Insert Iterator Unit Tests
// Tests: back_insert_iterator/back_inserter, front_insert_iterator/front_inserter,
// insert_iterator/inserter. Test with vector, list, set.
// Verify elements are inserted correctly.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <vector>
#include <list>
#include <set>
#include <string>
#include <algorithm>

// ============================================================
// back_insert_iterator
// ============================================================

TEST(InsertIteratorTest, BackInsertIteratorConstruct) {
    std::vector<int> v;
    zstl::back_insert_iterator<std::vector<int>> it(v);
    EXPECT_EQ(&it.container(), &v);
}

TEST(InsertIteratorTest, BackInsertIteratorAssign) {
    std::vector<int> v{1, 2, 3};
    auto it = zstl::back_inserter(v);

    *it = 4;
    ++it; // no-op for output iterators
    *it = 5;
    ++it; // no-op
    *it = 6;

    EXPECT_EQ(v, (std::vector<int>{1, 2, 3, 4, 5, 6}));
}

TEST(InsertIteratorTest, BackInsertIteratorMoveAssign) {
    std::vector<std::string> v;
    auto it = zstl::back_inserter(v);

    std::string s = "hello world";
    *it = zstl::move(s);

    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], "hello world");
}

TEST(InsertIteratorTest, BackInsertIteratorNoOp) {
    // operator* and operator++ return *this (no-op)
    std::vector<int> v;
    auto it = zstl::back_inserter(v);

    ++it;
    it++;
    *it;
    // None of these should do anything

    EXPECT_TRUE(v.empty());
}

TEST(InsertIteratorTest, BackInsertIteratorWithCopy) {
    std::vector<int> src{1, 2, 3, 4, 5};
    std::vector<int> dst;

    std::copy(src.begin(), src.end(), zstl::back_inserter(dst));

    EXPECT_EQ(dst, src);
}

TEST(InsertIteratorTest, BackInsertIteratorList) {
    std::list<int> l{10, 20, 30};
    auto it = zstl::back_inserter(l);

    *it = 40;
    EXPECT_EQ(l.back(), 40);

    *it = 50;
    EXPECT_EQ(l.back(), 50);

    EXPECT_EQ(l.size(), 5u);
}

// ============================================================
// front_insert_iterator
// ============================================================

TEST(InsertIteratorTest, FrontInsertIteratorConstruct) {
    std::list<int> l;
    auto it = zstl::front_inserter(l);
    EXPECT_EQ(&it.container(), &l);
}

TEST(InsertIteratorTest, FrontInsertIteratorAssign) {
    std::list<int> l;
    auto it = zstl::front_inserter(l);

    *it = 1;
    *it = 2;
    *it = 3;

    // Front insertion: each new element goes to the front
    // Insertion order: first 1, then 2 in front, then 3 in front → 3, 2, 1
    EXPECT_EQ(l.size(), 3u);
    auto iter = l.begin();
    EXPECT_EQ(*iter++, 3);
    EXPECT_EQ(*iter++, 2);
    EXPECT_EQ(*iter++, 1);
}

TEST(InsertIteratorTest, FrontInsertIteratorMoveAssign) {
    std::list<std::string> l;
    auto it = zstl::front_inserter(l);

    *it = std::string("world");
    *it = std::string("hello");

    auto iter = l.begin();
    EXPECT_EQ(*iter++, "hello");
    EXPECT_EQ(*iter++, "world");
}

TEST(InsertIteratorTest, FrontInsertIteratorWithCopy) {
    std::vector<int> src{1, 2, 3, 4, 5};
    std::list<int> dst;

    std::copy(src.begin(), src.end(), zstl::front_inserter(dst));

    // Elements inserted at front in order: 1, 2, 3, 4, 5
    // But front insertion reverses: 5, 4, 3, 2, 1
    auto it = dst.begin();
    EXPECT_EQ(*it++, 5);
    EXPECT_EQ(*it++, 4);
    EXPECT_EQ(*it++, 3);
    EXPECT_EQ(*it++, 2);
    EXPECT_EQ(*it++, 1);
}

// ============================================================
// insert_iterator
// ============================================================

TEST(InsertIteratorTest, InsertIteratorConstruct) {
    std::vector<int> v{10, 20, 30};
    auto it = zstl::inserter(v, v.begin() + 1);
    SUCCEED();
}

TEST(InsertIteratorTest, InsertIteratorAssign) {
    std::vector<int> v{1, 5, 9};
    auto it = zstl::inserter(v, v.begin() + 1);

    *it = 2;
    *it = 3;
    *it = 4;

    EXPECT_EQ(v, (std::vector<int>{1, 2, 3, 4, 5, 9}));
}

TEST(InsertIteratorTest, InsertIteratorAtBegin) {
    std::vector<int> v{4, 5, 6};
    auto it = zstl::inserter(v, v.begin());

    *it = 1;
    *it = 2;
    *it = 3;

    EXPECT_EQ(v, (std::vector<int>{1, 2, 3, 4, 5, 6}));
}

TEST(InsertIteratorTest, InsertIteratorAtEnd) {
    std::vector<int> v{1, 2, 3};
    auto it = zstl::inserter(v, v.end());

    *it = 4;
    *it = 5;

    EXPECT_EQ(v, (std::vector<int>{1, 2, 3, 4, 5}));
}

TEST(InsertIteratorTest, InsertIteratorMoveAssign) {
    std::vector<std::string> v{"start", "end"};
    auto it = zstl::inserter(v, v.begin() + 1);

    *it = std::string("middle");

    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], "start");
    EXPECT_EQ(v[1], "middle");
    EXPECT_EQ(v[2], "end");
}

TEST(InsertIteratorTest, InsertIteratorSet) {
    std::set<int> s{1, 5, 10};
    auto it = zstl::inserter(s, s.begin());

    *it = 3;
    *it = 7;
    *it = 12;

    EXPECT_EQ(s.size(), 6u);
    EXPECT_TRUE(s.count(3));
    EXPECT_TRUE(s.count(7));
    EXPECT_TRUE(s.count(12));
}

TEST(InsertIteratorTest, InsertIteratorList) {
    std::list<int> l{1, 5};
    auto it = zstl::inserter(l, ++l.begin()); // between 1 and 5

    *it = 2;
    *it = 3;
    *it = 4;

    auto iter = l.begin();
    EXPECT_EQ(*iter++, 1);
    EXPECT_EQ(*iter++, 2);
    EXPECT_EQ(*iter++, 3);
    EXPECT_EQ(*iter++, 4);
    EXPECT_EQ(*iter++, 5);
}

TEST(InsertIteratorTest, InsertIteratorWithCopy) {
    std::vector<int> src{10, 20, 30};
    std::vector<int> dst{1, 2, 3, 100, 200};

    std::copy(src.begin(), src.end(), zstl::inserter(dst, dst.begin() + 3));

    // 1, 2, 3, [10, 20, 30], 100, 200
    EXPECT_EQ(dst, (std::vector<int>{1, 2, 3, 10, 20, 30, 100, 200}));
}
