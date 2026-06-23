// ============================================================================
// zstl stack Unit Tests
// Tests: push (copy/move), emplace, pop, top, empty, size, swap, LIFO,
//        comparison operators, copy/move construct/assign, _get_container,
//        stack with vector as underlying container
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>
#include <vector>

using namespace zstl;

// Custom type to verify move semantics
struct MoveCounter {
    int value;
    static int move_count;
    static int copy_count;

    MoveCounter() : value(0) {}
    explicit MoveCounter(int v) : value(v) {}
    MoveCounter(const MoveCounter& o) : value(o.value) { ++copy_count; }
    MoveCounter(MoveCounter&& o) noexcept : value(o.value) { o.value = -1; ++move_count; }
    MoveCounter& operator=(const MoveCounter& o) {
        value = o.value; ++copy_count; return *this;
    }
    MoveCounter& operator=(MoveCounter&& o) noexcept {
        value = o.value; o.value = -1; ++move_count; return *this;
    }
    bool operator==(const MoveCounter& o) const { return value == o.value; }
    bool operator<(const MoveCounter& o) const { return value < o.value; }

    static void reset() { move_count = 0; copy_count = 0; }
};
int MoveCounter::move_count = 0;
int MoveCounter::copy_count = 0;

// ============================================================================
// Constructors
// ============================================================================

TEST(StackTest, DefaultConstructor) {
    stack<int> s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
}

TEST(StackTest, CopyConstructor) {
    stack<int> s1;
    s1.push(1);
    s1.push(2);
    s1.push(3);

    stack<int> s2(s1);
    EXPECT_EQ(s2.size(), 3u);
    EXPECT_EQ(s2.top(), 3);
    s2.pop();
    EXPECT_EQ(s2.top(), 2);

    // s1 unchanged
    EXPECT_EQ(s1.size(), 3u);
    EXPECT_EQ(s1.top(), 3);
}

TEST(StackTest, MoveConstructor) {
    stack<int> s1;
    s1.push(1);
    s1.push(2);

    stack<int> s2(zstl::move(s1));
    EXPECT_EQ(s2.size(), 2u);
    EXPECT_EQ(s2.top(), 2);
}

TEST(StackTest, CopyFromContainer) {
    deque<int> d;
    d.push_back(10);
    d.push_back(20);
    stack<int> s(d);
    EXPECT_EQ(s.size(), 2u);
    EXPECT_EQ(s.top(), 20);
}

TEST(StackTest, MoveFromContainer) {
    deque<int> d;
    d.push_back(1);
    d.push_back(2);
    stack<int> s(zstl::move(d));
    EXPECT_EQ(s.size(), 2u);
    EXPECT_EQ(s.top(), 2);
}

// ============================================================================
// Assignment
// ============================================================================

TEST(StackTest, CopyAssignment) {
    stack<int> s1;
    s1.push(10);
    s1.push(20);

    stack<int> s2;
    s2.push(99);
    s2 = s1;

    EXPECT_EQ(s2.size(), 2u);
    EXPECT_EQ(s2.top(), 20);
    s2.pop();
    EXPECT_EQ(s2.top(), 10);
}

TEST(StackTest, MoveAssignment) {
    stack<int> s1;
    s1.push(1);
    s1.push(2);

    stack<int> s2;
    s2.push(99);
    s2 = zstl::move(s1);

    EXPECT_EQ(s2.size(), 2u);
    EXPECT_EQ(s2.top(), 2);
}

// ============================================================================
// push (copy and move), emplace
// ============================================================================

TEST(StackTest, PushCopy) {
    stack<int> s;
    int val = 42;
    s.push(val);
    EXPECT_EQ(s.top(), 42);
    EXPECT_EQ(s.size(), 1u);
    // Verify val was copied, not moved
    EXPECT_EQ(val, 42);
}

TEST(StackTest, PushMove) {
    stack<std::string> s;
    std::string val = "hello world";
    s.push(zstl::move(val));
    EXPECT_EQ(s.top(), "hello world");
    EXPECT_EQ(s.size(), 1u);
    // val is moved-from
}

TEST(StackTest, PushMoveCount) {
    MoveCounter::reset();
    stack<MoveCounter> s;
    MoveCounter mc(42);
    s.push(zstl::move(mc));
    EXPECT_EQ(s.top().value, 42);
    // At least one move (into the container), no copies
    EXPECT_GE(MoveCounter::move_count, 1);
    EXPECT_EQ(MoveCounter::copy_count, 0);
}

TEST(StackTest, Emplace) {
    stack<std::string> s;
    s.emplace(5, 'x'); // string(5, 'x') = "xxxxx"
    EXPECT_EQ(s.top(), "xxxxx");
    EXPECT_EQ(s.size(), 1u);
}

TEST(StackTest, EmplaceMultipleArgs) {
    stack<std::string> s;
    s.emplace("hello");
    EXPECT_EQ(s.top(), "hello");
}

// ============================================================================
// pop and top
// ============================================================================

TEST(StackTest, Pop) {
    stack<int> s;
    s.push(1);
    s.push(2);
    s.push(3);

    s.pop();
    EXPECT_EQ(s.top(), 2);
    EXPECT_EQ(s.size(), 2u);
}

TEST(StackTest, PopUntilEmpty) {
    stack<int> s;
    s.push(1);
    s.push(2);
    s.pop();
    s.pop();
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
}

TEST(StackTest, TopConst) {
    stack<int> s;
    s.push(42);
    const auto& cs = s;
    EXPECT_EQ(cs.top(), 42);
}

TEST(StackTest, TopReturnsReference) {
    stack<int> s;
    s.push(10);
    s.top() = 20; // modify through reference
    EXPECT_EQ(s.top(), 20);
}

// ============================================================================
// LIFO behavior
// ============================================================================

TEST(StackTest, LifoBehavior) {
    stack<int> s;
    s.push(1);
    s.push(2);
    s.push(3);

    // LIFO: last in, first out — expect 3, 2, 1
    EXPECT_EQ(s.top(), 3); s.pop();
    EXPECT_EQ(s.top(), 2); s.pop();
    EXPECT_EQ(s.top(), 1); s.pop();
    EXPECT_TRUE(s.empty());
}

TEST(StackTest, LifoWithStrings) {
    stack<std::string> s;
    s.push("first");
    s.push("second");
    s.push("third");

    EXPECT_EQ(s.top(), "third"); s.pop();
    EXPECT_EQ(s.top(), "second"); s.pop();
    EXPECT_EQ(s.top(), "first"); s.pop();
}

TEST(StackTest, LifoInterleaved) {
    stack<int> s;
    s.push(1);
    s.push(2);
    EXPECT_EQ(s.top(), 2); s.pop();
    s.push(3);
    EXPECT_EQ(s.top(), 3); s.pop();
    s.push(4);
    s.push(5);
    EXPECT_EQ(s.top(), 5); s.pop();
    EXPECT_EQ(s.top(), 4); s.pop();
    EXPECT_EQ(s.top(), 1); s.pop();
    EXPECT_TRUE(s.empty());
}

// ============================================================================
// empty, size, swap
// ============================================================================

TEST(StackTest, EmptyAndSize) {
    stack<int> s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);

    s.push(1);
    EXPECT_FALSE(s.empty());
    EXPECT_EQ(s.size(), 1u);

    s.push(2);
    EXPECT_EQ(s.size(), 2u);

    s.pop();
    EXPECT_EQ(s.size(), 1u);

    s.pop();
    EXPECT_TRUE(s.empty());
}

TEST(StackTest, Swap) {
    stack<int> s1;
    s1.push(1);
    s1.push(2);

    stack<int> s2;
    s2.push(10);
    s2.push(20);
    s2.push(30);

    s1.swap(s2);

    EXPECT_EQ(s1.size(), 3u);
    EXPECT_EQ(s1.top(), 30);

    EXPECT_EQ(s2.size(), 2u);
    EXPECT_EQ(s2.top(), 2);
}

TEST(StackTest, FreeSwap) {
    stack<int> a;
    a.push(100);
    stack<int> b;
    b.push(200);

    zstl::swap(a, b);
    EXPECT_EQ(a.top(), 200);
    EXPECT_EQ(b.top(), 100);
}

// ============================================================================
// Comparison operators
// ============================================================================

TEST(StackTest, Equality) {
    stack<int> s1;
    s1.push(1); s1.push(2);

    stack<int> s2;
    s2.push(1); s2.push(2);

    stack<int> s3;
    s3.push(1); s3.push(3);

    EXPECT_TRUE(s1 == s2);
    EXPECT_FALSE(s1 == s3);
    EXPECT_TRUE(s1 != s3);
    EXPECT_FALSE(s1 != s2);
}

TEST(StackTest, LessThan) {
    stack<int> s1;
    s1.push(1); s1.push(2);

    stack<int> s2;
    s2.push(1); s2.push(3);

    stack<int> s3;
    s3.push(1); s3.push(2); s3.push(0);

    EXPECT_TRUE(s1 < s2);
    EXPECT_TRUE(s1 < s3);
    EXPECT_FALSE(s2 < s1);
}

TEST(StackTest, GreaterThan) {
    stack<int> s1;
    s1.push(1); s1.push(5);

    stack<int> s2;
    s2.push(1); s2.push(3);

    EXPECT_TRUE(s1 > s2);
    EXPECT_FALSE(s2 > s1);
}

TEST(StackTest, LessEqualAndGreaterEqual) {
    stack<int> s1;
    s1.push(1); s1.push(2);

    stack<int> s2;
    s2.push(1); s2.push(2);

    EXPECT_TRUE(s1 <= s2);
    EXPECT_TRUE(s2 <= s1);
    EXPECT_TRUE(s1 >= s2);
    EXPECT_TRUE(s2 >= s1);

    stack<int> s3;
    s3.push(1); s3.push(1);

    EXPECT_TRUE(s3 <= s1);
    EXPECT_FALSE(s1 <= s3);
    EXPECT_TRUE(s1 >= s3);
    EXPECT_FALSE(s3 >= s1);
}

// ============================================================================
// _get_container
// ============================================================================

TEST(StackTest, GetContainer) {
    stack<int> s;
    s.push(1);
    s.push(2);
    s.push(3);

    // Copy the underlying container
    auto c = s._get_container();
    EXPECT_EQ(c.size(), 3u);
    EXPECT_EQ(c.back(), 3);
    EXPECT_EQ(c.front(), 1);
}

TEST(StackTest, GetContainerAfterMove) {
    stack<int> s;
    s.push(1);

    auto c = zstl::move(s)._get_container();
    EXPECT_EQ(c.size(), 1u);
    EXPECT_EQ(c.front(), 1);
}

// ============================================================================
// Stack with vector as underlying container
// ============================================================================

TEST(StackVectorTest, DefaultConstructor) {
    stack<int, vector<int>> s;
    EXPECT_TRUE(s.empty());
}

TEST(StackVectorTest, PushAndPop) {
    stack<int, vector<int>> s;
    s.push(10);
    s.push(20);
    s.push(30);

    EXPECT_EQ(s.top(), 30); s.pop();
    EXPECT_EQ(s.top(), 20); s.pop();
    EXPECT_EQ(s.top(), 10); s.pop();
    EXPECT_TRUE(s.empty());
}

TEST(StackVectorTest, Swap) {
    stack<int, vector<int>> s1;
    s1.push(1);

    stack<int, vector<int>> s2;
    s2.push(2);

    s1.swap(s2);
    EXPECT_EQ(s1.top(), 2);
    EXPECT_EQ(s2.top(), 1);
}

TEST(StackVectorTest, Emplace) {
    stack<std::string, vector<std::string>> s;
    s.emplace("test");
    EXPECT_EQ(s.top(), "test");
}

// ============================================================================
// Edge cases
// ============================================================================

TEST(StackTest, LargePushPop) {
    stack<int> s;
    const int N = 10000;
    for (int i = 0; i < N; ++i) {
        s.push(i);
    }
    EXPECT_EQ(s.size(), static_cast<size_t>(N));
    EXPECT_EQ(s.top(), N - 1);

    for (int i = N - 1; i >= 0; --i) {
        EXPECT_EQ(s.top(), i);
        s.pop();
    }
    EXPECT_TRUE(s.empty());
}
