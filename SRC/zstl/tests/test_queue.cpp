// ============================================================================
// zstl queue Unit Tests
// Tests: push (copy/move), emplace, pop, front, back, empty, size, swap,
//        FIFO behavior, comparison operators, copy/move construct/assign,
//        queue with list as underlying container
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>

using namespace zstl;

// ============================================================================
// Constructors
// ============================================================================

TEST(QueueTest, DefaultConstructor) {
    queue<int> q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
}

TEST(QueueTest, CopyConstructor) {
    queue<int> q1;
    q1.push(1);
    q1.push(2);
    q1.push(3);

    queue<int> q2(q1);
    EXPECT_EQ(q2.size(), 3u);
    EXPECT_EQ(q2.front(), 1);
    EXPECT_EQ(q2.back(), 3);

    // q1 unchanged
    EXPECT_EQ(q1.size(), 3u);
    EXPECT_EQ(q1.front(), 1);
}

TEST(QueueTest, MoveConstructor) {
    queue<int> q1;
    q1.push(1);
    q1.push(2);

    queue<int> q2(zstl::move(q1));
    EXPECT_EQ(q2.size(), 2u);
    EXPECT_EQ(q2.front(), 1);
    EXPECT_EQ(q2.back(), 2);
}

TEST(QueueTest, CopyFromContainer) {
    deque<int> d;
    d.push_back(10);
    d.push_back(20);
    queue<int> q(d);
    EXPECT_EQ(q.size(), 2u);
    EXPECT_EQ(q.front(), 10);
    EXPECT_EQ(q.back(), 20);
}

TEST(QueueTest, MoveFromContainer) {
    deque<int> d;
    d.push_back(1);
    d.push_back(2);
    queue<int> q(zstl::move(d));
    EXPECT_EQ(q.size(), 2u);
    EXPECT_EQ(q.front(), 1);
}

// ============================================================================
// Assignment
// ============================================================================

TEST(QueueTest, CopyAssignment) {
    queue<int> q1;
    q1.push(1);
    q1.push(2);

    queue<int> q2;
    q2.push(99);
    q2 = q1;

    EXPECT_EQ(q2.size(), 2u);
    EXPECT_EQ(q2.front(), 1);
    EXPECT_EQ(q2.back(), 2);
}

TEST(QueueTest, MoveAssignment) {
    queue<int> q1;
    q1.push(10);
    q1.push(20);

    queue<int> q2;
    q2.push(99);
    q2 = zstl::move(q1);

    EXPECT_EQ(q2.size(), 2u);
    EXPECT_EQ(q2.front(), 10);
    EXPECT_EQ(q2.back(), 20);
}

// ============================================================================
// push (copy and move), emplace
// ============================================================================

TEST(QueueTest, PushCopy) {
    queue<int> q;
    int val = 42;
    q.push(val);
    EXPECT_EQ(q.front(), 42);
    EXPECT_EQ(q.back(), 42);
    EXPECT_EQ(val, 42); // original unchanged
}

TEST(QueueTest, PushMove) {
    queue<std::string> q;
    std::string val = "hello world";
    q.push(zstl::move(val));
    EXPECT_EQ(q.front(), "hello world");
}

TEST(QueueTest, PushMultiple) {
    queue<int> q;
    q.push(1);
    q.push(2);
    q.push(3);
    EXPECT_EQ(q.front(), 1);
    EXPECT_EQ(q.back(), 3);
    EXPECT_EQ(q.size(), 3u);
}

TEST(QueueTest, Emplace) {
    queue<std::string> q;
    q.emplace(3, 'a'); // string(3, 'a') = "aaa"
    EXPECT_EQ(q.front(), "aaa");
    EXPECT_EQ(q.back(), "aaa");
    EXPECT_EQ(q.size(), 1u);
}

TEST(QueueTest, EmplaceMultiple) {
    queue<std::string> q;
    q.emplace("first");
    q.emplace("second");
    EXPECT_EQ(q.front(), "first");
    EXPECT_EQ(q.back(), "second");
}

// ============================================================================
// pop, front, back
// ============================================================================

TEST(QueueTest, Pop) {
    queue<int> q;
    q.push(1);
    q.push(2);
    q.push(3);

    q.pop();
    EXPECT_EQ(q.front(), 2);
    EXPECT_EQ(q.back(), 3);
    EXPECT_EQ(q.size(), 2u);

    q.pop();
    EXPECT_EQ(q.front(), 3);
    EXPECT_EQ(q.back(), 3);
}

TEST(QueueTest, PopUntilEmpty) {
    queue<int> q;
    q.push(1);
    q.push(2);
    q.pop();
    q.pop();
    EXPECT_TRUE(q.empty());
}

TEST(QueueTest, FrontAndBackConst) {
    queue<int> q;
    q.push(10);
    q.push(20);
    q.push(30);

    const auto& cq = q;
    EXPECT_EQ(cq.front(), 10);
    EXPECT_EQ(cq.back(), 30);
}

TEST(QueueTest, FrontReturnsReference) {
    queue<int> q;
    q.push(10);
    q.front() = 20; // modify through reference
    EXPECT_EQ(q.front(), 20);
}

TEST(QueueTest, BackReturnsReference) {
    queue<int> q;
    q.push(10);
    q.push(20);
    q.back() = 30;
    EXPECT_EQ(q.back(), 30);
}

// ============================================================================
// FIFO behavior
// ============================================================================

TEST(QueueTest, FifoBehavior) {
    queue<int> q;
    q.push(1);
    q.push(2);
    q.push(3);

    // FIFO: first in, first out — expect 1, 2, 3
    EXPECT_EQ(q.front(), 1); q.pop();
    EXPECT_EQ(q.front(), 2); q.pop();
    EXPECT_EQ(q.front(), 3); q.pop();
    EXPECT_TRUE(q.empty());
}

TEST(QueueTest, FifoWithStrings) {
    queue<std::string> q;
    q.push("first");
    q.push("second");
    q.push("third");

    EXPECT_EQ(q.front(), "first"); q.pop();
    EXPECT_EQ(q.front(), "second"); q.pop();
    EXPECT_EQ(q.front(), "third"); q.pop();
}

TEST(QueueTest, FifoInterleaved) {
    queue<int> q;
    q.push(1);
    q.push(2);
    EXPECT_EQ(q.front(), 1); q.pop();
    q.push(3);
    q.push(4);
    EXPECT_EQ(q.front(), 2); q.pop();
    EXPECT_EQ(q.front(), 3); q.pop();
    EXPECT_EQ(q.front(), 4); q.pop();
    EXPECT_TRUE(q.empty());
}

// ============================================================================
// empty, size, swap
// ============================================================================

TEST(QueueTest, EmptyAndSize) {
    queue<int> q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);

    q.push(1);
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(q.size(), 1u);

    q.push(2);
    EXPECT_EQ(q.size(), 2u);

    q.pop();
    EXPECT_EQ(q.size(), 1u);

    q.pop();
    EXPECT_TRUE(q.empty());
}

TEST(QueueTest, Swap) {
    queue<int> q1;
    q1.push(1);
    q1.push(2);

    queue<int> q2;
    q2.push(10);
    q2.push(20);
    q2.push(30);

    q1.swap(q2);

    EXPECT_EQ(q1.size(), 3u);
    EXPECT_EQ(q1.front(), 10);
    EXPECT_EQ(q1.back(), 30);

    EXPECT_EQ(q2.size(), 2u);
    EXPECT_EQ(q2.front(), 1);
    EXPECT_EQ(q2.back(), 2);
}

TEST(QueueTest, FreeSwap) {
    queue<int> a;
    a.push(100);
    queue<int> b;
    b.push(200);

    zstl::swap(a, b);
    EXPECT_EQ(a.front(), 200);
    EXPECT_EQ(b.front(), 100);
}

// ============================================================================
// Comparison operators
// ============================================================================

TEST(QueueTest, Equality) {
    queue<int> q1;
    q1.push(1); q1.push(2);

    queue<int> q2;
    q2.push(1); q2.push(2);

    queue<int> q3;
    q3.push(1); q3.push(3);

    EXPECT_TRUE(q1 == q2);
    EXPECT_FALSE(q1 == q3);
    EXPECT_TRUE(q1 != q3);
    EXPECT_FALSE(q1 != q2);
}

TEST(QueueTest, LessThan) {
    queue<int> q1;
    q1.push(1); q1.push(2);

    queue<int> q2;
    q2.push(1); q2.push(3);

    queue<int> q3;
    q3.push(1); q3.push(2); q3.push(0);

    EXPECT_TRUE(q1 < q2);
    EXPECT_TRUE(q1 < q3);
    EXPECT_FALSE(q2 < q1);
}

TEST(QueueTest, GreaterThan) {
    queue<int> q1;
    q1.push(1); q1.push(5);

    queue<int> q2;
    q2.push(1); q2.push(3);

    EXPECT_TRUE(q1 > q2);
    EXPECT_FALSE(q2 > q1);
}

TEST(QueueTest, LessEqualAndGreaterEqual) {
    queue<int> q1;
    q1.push(1); q1.push(2);

    queue<int> q2;
    q2.push(1); q2.push(2);

    EXPECT_TRUE(q1 <= q2);
    EXPECT_TRUE(q2 <= q1);
    EXPECT_TRUE(q1 >= q2);
    EXPECT_TRUE(q2 >= q1);

    queue<int> q3;
    q3.push(1); q3.push(1);

    EXPECT_TRUE(q3 <= q1);
    EXPECT_FALSE(q1 <= q3);
    EXPECT_TRUE(q1 >= q3);
    EXPECT_FALSE(q3 >= q1);
}

// ============================================================================
// Queue with list as underlying container
// ============================================================================

TEST(QueueListTest, DefaultConstructor) {
    queue<int, list<int>> q;
    EXPECT_TRUE(q.empty());
}

TEST(QueueListTest, PushAndPop) {
    queue<int, list<int>> q;
    q.push(10);
    q.push(20);
    q.push(30);

    EXPECT_EQ(q.front(), 10); q.pop();
    EXPECT_EQ(q.front(), 20); q.pop();
    EXPECT_EQ(q.front(), 30); q.pop();
    EXPECT_TRUE(q.empty());
}

TEST(QueueListTest, Swap) {
    queue<int, list<int>> q1;
    q1.push(1);

    queue<int, list<int>> q2;
    q2.push(2);

    q1.swap(q2);
    EXPECT_EQ(q1.front(), 2);
    EXPECT_EQ(q2.front(), 1);
}

TEST(QueueListTest, Emplace) {
    queue<std::string, list<std::string>> q;
    q.emplace("hello");
    EXPECT_EQ(q.front(), "hello");
}

TEST(QueueListTest, FifoBehavior) {
    queue<int, list<int>> q;
    q.push(1);
    q.push(2);
    q.push(3);

    EXPECT_EQ(q.front(), 1); q.pop();
    EXPECT_EQ(q.front(), 2); q.pop();
    EXPECT_EQ(q.front(), 3); q.pop();
    EXPECT_TRUE(q.empty());
}

// ============================================================================
// Edge cases
// ============================================================================

TEST(QueueTest, LargeFifo) {
    queue<int> q;
    const int N = 5000;
    for (int i = 0; i < N; ++i) {
        q.push(i);
    }
    EXPECT_EQ(q.size(), static_cast<size_t>(N));
    EXPECT_EQ(q.front(), 0);
    EXPECT_EQ(q.back(), N - 1);

    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(q.front(), i);
        q.pop();
    }
    EXPECT_TRUE(q.empty());
}
