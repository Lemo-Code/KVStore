// ============================================================================
// zstl priority_queue Unit Tests
// Tests: push (copy/move), emplace, pop, top, empty, size, swap,
//        max-heap (default less), min-heap (greater), custom type/comp,
//        copy/move construct/assign, heap property verification, value_comp
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>
#include <vector>
#include <random>
#include <functional>

using namespace zstl;

// ============================================================================
// Custom type for testing
// ============================================================================
struct Task {
    std::string name;
    int priority;

    Task() : priority(0) {}
    Task(std::string n, int p) : name(std::move(n)), priority(p) {}

    bool operator<(const Task& o) const {
        return priority < o.priority; // higher priority = larger value = max-heap top
    }
    bool operator==(const Task& o) const {
        return name == o.name && priority == o.priority;
    }
};

// Reverse comparator for Task
struct TaskReverseCompare {
    bool operator()(const Task& a, const Task& b) const {
        return a.priority > b.priority; // min-heap on priority
    }
};

// ============================================================================
// Constructors
// ============================================================================

TEST(PriorityQueueTest, DefaultConstructor) {
    priority_queue<int> pq;
    EXPECT_TRUE(pq.empty());
    EXPECT_EQ(pq.size(), 0u);
}

TEST(PriorityQueueTest, ComparatorConstructor) {
    priority_queue<int, vector<int>, greater<int>> pq;
    EXPECT_TRUE(pq.empty());

    pq.push(3);
    pq.push(1);
    pq.push(2);
    EXPECT_EQ(pq.top(), 1); // min-heap: smallest is top
}

TEST(PriorityQueueTest, IteratorRangeConstructor) {
    vector<int> v = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3};
    priority_queue<int> pq(v.begin(), v.end());
    EXPECT_EQ(pq.size(), v.size());
    EXPECT_EQ(pq.top(), 9); // max-heap: largest value
}

TEST(PriorityQueueTest, IteratorRangeWithComparator) {
    vector<int> v = {3, 1, 4, 1, 5, 9};
    priority_queue<int, vector<int>, greater<int>> pq(v.begin(), v.end());
    EXPECT_EQ(pq.size(), 6u);
    EXPECT_EQ(pq.top(), 1); // min-heap
}

TEST(PriorityQueueTest, FromContainerCopy) {
    vector<int> v = {3, 1, 2};
    priority_queue<int> pq(v);
    EXPECT_EQ(pq.size(), 3u);
    EXPECT_EQ(pq.top(), 3);

    // v should be unchanged (copied)
    EXPECT_EQ(v.size(), 3u);
}

TEST(PriorityQueueTest, FromContainerMove) {
    vector<int> v = {5, 2, 8, 1};
    priority_queue<int> pq(zstl::move(v));
    EXPECT_EQ(pq.size(), 4u);
    EXPECT_EQ(pq.top(), 8);
}

TEST(PriorityQueueTest, CopyConstructor) {
    priority_queue<int> pq1;
    pq1.push(1);
    pq1.push(3);
    pq1.push(2);

    priority_queue<int> pq2(pq1);
    EXPECT_EQ(pq2.size(), 3u);
    EXPECT_EQ(pq2.top(), 3);

    // Deep copy — modify pq1
    pq1.push(10);
    EXPECT_EQ(pq2.top(), 3); // pq2 unchanged
}

TEST(PriorityQueueTest, MoveConstructor) {
    priority_queue<int> pq1;
    pq1.push(1);
    pq1.push(2);
    pq1.push(3);

    priority_queue<int> pq2(zstl::move(pq1));
    EXPECT_EQ(pq2.size(), 3u);
    EXPECT_EQ(pq2.top(), 3);
}

// ============================================================================
// Assignment
// ============================================================================

TEST(PriorityQueueTest, CopyAssignment) {
    priority_queue<int> pq1;
    pq1.push(10);
    pq1.push(20);

    priority_queue<int> pq2;
    pq2.push(99);
    pq2 = pq1;

    EXPECT_EQ(pq2.size(), 2u);
    EXPECT_EQ(pq2.top(), 20);
}

TEST(PriorityQueueTest, MoveAssignment) {
    priority_queue<int> pq1;
    pq1.push(1);
    pq1.push(5);
    pq1.push(3);

    priority_queue<int> pq2;
    pq2.push(99);
    pq2 = zstl::move(pq1);

    EXPECT_EQ(pq2.size(), 3u);
    EXPECT_EQ(pq2.top(), 5);
}

// ============================================================================
// push (copy and move), emplace
// ============================================================================

TEST(PriorityQueueTest, PushCopy) {
    priority_queue<int> pq;
    int val = 42;
    pq.push(val);
    EXPECT_EQ(pq.top(), 42);
    EXPECT_EQ(val, 42); // original unchanged
}

TEST(PriorityQueueTest, PushMove) {
    priority_queue<std::string> pq;
    std::string val = "hello world";
    pq.push(zstl::move(val));
    EXPECT_EQ(pq.top(), "hello world");
}

TEST(PriorityQueueTest, PushMultiple) {
    priority_queue<int> pq;
    pq.push(3);
    pq.push(5);
    pq.push(1);
    pq.push(4);
    pq.push(2);

    EXPECT_EQ(pq.top(), 5); // max-heap: largest is top
    EXPECT_EQ(pq.size(), 5u);
}

TEST(PriorityQueueTest, Emplace) {
    priority_queue<std::string> pq;
    pq.emplace(5, 'x'); // string(5, 'x') = "xxxxx"
    EXPECT_EQ(pq.top(), "xxxxx");
}

TEST(PriorityQueueTest, EmplaceMultipleArgs) {
    priority_queue<std::string> pq;
    pq.emplace("hello");
    pq.emplace("world");
    EXPECT_EQ(pq.top(), "world"); // lexicographic: "world" > "hello"
}

// ============================================================================
// pop and top
// ============================================================================

TEST(PriorityQueueTest, PopSequence) {
    priority_queue<int> pq;
    pq.push(3);
    pq.push(1);
    pq.push(4);
    pq.push(1);
    pq.push(5);
    pq.push(9);
    pq.push(2);
    pq.push(6);

    // Max-heap: pop in descending order
    int expected[] = {9, 6, 5, 4, 3, 2, 1, 1};
    for (int exp : expected) {
        EXPECT_EQ(pq.top(), exp);
        pq.pop();
    }
    EXPECT_TRUE(pq.empty());
}

TEST(PriorityQueueTest, TopConst) {
    priority_queue<int> pq;
    pq.push(42);
    const auto& cpq = pq;
    EXPECT_EQ(cpq.top(), 42);
}

TEST(PriorityQueueTest, PopUntilEmpty) {
    priority_queue<int> pq;
    pq.push(1);
    pq.push(2);
    pq.pop();
    pq.pop();
    EXPECT_TRUE(pq.empty());
}

// ============================================================================
// empty, size, swap
// ============================================================================

TEST(PriorityQueueTest, EmptyAndSize) {
    priority_queue<int> pq;
    EXPECT_TRUE(pq.empty());
    EXPECT_EQ(pq.size(), 0u);

    pq.push(1);
    EXPECT_FALSE(pq.empty());
    EXPECT_EQ(pq.size(), 1u);

    pq.push(2);
    EXPECT_EQ(pq.size(), 2u);

    pq.pop();
    EXPECT_EQ(pq.size(), 1u);

    pq.pop();
    EXPECT_TRUE(pq.empty());
}

TEST(PriorityQueueTest, Swap) {
    priority_queue<int> pq1;
    pq1.push(1);
    pq1.push(5);
    pq1.push(3);

    priority_queue<int> pq2;
    pq2.push(10);
    pq2.push(20);

    pq1.swap(pq2);

    EXPECT_EQ(pq1.size(), 2u);
    EXPECT_EQ(pq1.top(), 20);

    EXPECT_EQ(pq2.size(), 3u);
    EXPECT_EQ(pq2.top(), 5);
}

TEST(PriorityQueueTest, FreeSwap) {
    priority_queue<int> a;
    a.push(100);

    priority_queue<int> b;
    b.push(200);

    zstl::swap(a, b);
    EXPECT_EQ(a.top(), 200);
    EXPECT_EQ(b.top(), 100);
}

// ============================================================================
// Max-heap behavior verification
// ============================================================================

TEST(PriorityQueueTest, MaxHeapBasic) {
    priority_queue<int> pq;

    // Insert random values, verify top is always the maximum so far
    int max_so_far = -1;
    for (int val : {5, 2, 8, 1, 9, 3, 7, 4, 6, 0}) {
        if (val > max_so_far) max_so_far = val;
        pq.push(val);
        EXPECT_EQ(pq.top(), max_so_far) << "After pushing " << val;
    }
}

TEST(PriorityQueueTest, MaxHeapAfterPopSequence) {
    priority_queue<int> pq;

    // Push sorted values
    for (int i = 1; i <= 10; ++i) {
        pq.push(i);
    }

    EXPECT_EQ(pq.top(), 10); pq.pop();
    EXPECT_EQ(pq.top(), 9);  pq.pop();
    EXPECT_EQ(pq.top(), 8);  pq.pop();
}

TEST(PriorityQueueTest, MaxHeapRandomInsert) {
    priority_queue<int> pq;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 10000);

    int global_max = -1;
    for (int i = 0; i < 500; ++i) {
        int val = dist(rng);
        if (val > global_max) global_max = val;
        pq.push(val);
        EXPECT_EQ(pq.top(), global_max) << "Mismatch after push " << i;
    }

    // Pop all and verify descending order
    int prev = 100000; // larger than any value
    while (!pq.empty()) {
        int cur = pq.top();
        EXPECT_LE(cur, prev) << "Heap order violated: " << cur << " after " << prev;
        prev = cur;
        pq.pop();
    }
}

// ============================================================================
// Min-heap with greater<int> comparator
// ============================================================================

TEST(PriorityQueueTest, MinHeapBasic) {
    priority_queue<int, vector<int>, greater<int>> pq;

    pq.push(5);
    pq.push(2);
    pq.push(8);
    pq.push(1);

    EXPECT_EQ(pq.top(), 1); pq.pop();
    EXPECT_EQ(pq.top(), 2); pq.pop();
    EXPECT_EQ(pq.top(), 5); pq.pop();
    EXPECT_EQ(pq.top(), 8); pq.pop();
    EXPECT_TRUE(pq.empty());
}

TEST(PriorityQueueTest, MinHeapRandomInsert) {
    priority_queue<int, vector<int>, greater<int>> pq;

    std::mt19937 rng(99);
    std::uniform_int_distribution<int> dist(0, 10000);

    int global_min = 10001;
    for (int i = 0; i < 500; ++i) {
        int val = dist(rng);
        if (val < global_min) global_min = val;
        pq.push(val);
        EXPECT_EQ(pq.top(), global_min) << "Mismatch after push " << i;
    }

    // Pop all and verify ascending order
    int prev = -1;
    while (!pq.empty()) {
        int cur = pq.top();
        EXPECT_GE(cur, prev) << "Min-heap order violated: " << cur << " before " << prev;
        prev = cur;
        pq.pop();
    }
}

TEST(PriorityQueueTest, MinHeapPopSequence) {
    priority_queue<int, vector<int>, greater<int>> pq;
    pq.push(10);
    pq.push(20);
    pq.push(5);
    pq.push(15);

    EXPECT_EQ(pq.top(), 5);  pq.pop();
    EXPECT_EQ(pq.top(), 10); pq.pop();
    EXPECT_EQ(pq.top(), 15); pq.pop();
    EXPECT_EQ(pq.top(), 20); pq.pop();
}

// ============================================================================
// Custom type with default comparator
// ============================================================================

TEST(PriorityQueueTest, CustomTypeMaxHeap) {
    priority_queue<Task> pq;

    pq.emplace("low", 1);
    pq.emplace("medium", 5);
    pq.emplace("high", 10);
    pq.emplace("critical", 100);

    EXPECT_EQ(pq.top().priority, 100);
    EXPECT_EQ(pq.top().name, "critical");
    pq.pop();

    EXPECT_EQ(pq.top().priority, 10);
    EXPECT_EQ(pq.top().name, "high");
    pq.pop();

    EXPECT_EQ(pq.top().priority, 5);
    pq.pop();

    EXPECT_EQ(pq.top().priority, 1);
}

TEST(PriorityQueueTest, CustomTypeWithReverseComparator) {
    priority_queue<Task, vector<Task>, TaskReverseCompare> pq;

    pq.emplace("low", 1);
    pq.emplace("high", 10);
    pq.emplace("medium", 5);

    // Min-heap on priority: smallest first
    EXPECT_EQ(pq.top().priority, 1); pq.pop();
    EXPECT_EQ(pq.top().priority, 5); pq.pop();
    EXPECT_EQ(pq.top().priority, 10); pq.pop();
}

// ============================================================================
// String-based priority queue
// ============================================================================

TEST(PriorityQueueTest, StringMaxHeap) {
    priority_queue<std::string> pq;
    pq.push("apple");
    pq.push("zebra");
    pq.push("mango");
    pq.push("banana");

    // Lexicographic ordering: zebra is "largest"
    EXPECT_EQ(pq.top(), "zebra"); pq.pop();
    EXPECT_EQ(pq.top(), "mango"); pq.pop();
    EXPECT_EQ(pq.top(), "banana"); pq.pop();
    EXPECT_EQ(pq.top(), "apple"); pq.pop();
}

TEST(PriorityQueueTest, StringMinHeap) {
    priority_queue<std::string, vector<std::string>, greater<std::string>> pq;
    pq.push("cherry");
    pq.push("apple");
    pq.push("banana");

    EXPECT_EQ(pq.top(), "apple"); pq.pop();
    EXPECT_EQ(pq.top(), "banana"); pq.pop();
    EXPECT_EQ(pq.top(), "cherry"); pq.pop();
}

// ============================================================================
// Custom comparator with lambda-like functor
// ============================================================================

struct AbsCompare {
    bool operator()(int a, int b) const {
        return std::abs(a) < std::abs(b);
    }
};

TEST(PriorityQueueTest, CustomAbsComparator) {
    priority_queue<int, vector<int>, AbsCompare> pq;

    pq.push(-5);
    pq.push(3);
    pq.push(-10);
    pq.push(7);
    pq.push(-3);

    // Largest by absolute value
    EXPECT_EQ(pq.top(), -10); pq.pop();
    EXPECT_EQ(pq.top(), 7);   pq.pop();
    EXPECT_EQ(pq.top(), -5);  pq.pop();
    EXPECT_EQ(pq.top(), 3);   pq.pop();
    // Note: order of 3 and -3 depends on heap implementation, both are valid
}

// ============================================================================
// value_comp
// ============================================================================

TEST(PriorityQueueTest, ValueCompDefault) {
    priority_queue<int> pq;
    auto comp = pq.value_comp();
    EXPECT_TRUE(comp(1, 2));  // less<int>: 1 < 2
    EXPECT_FALSE(comp(2, 1));
    EXPECT_FALSE(comp(2, 2));
}

TEST(PriorityQueueTest, ValueCompGreater) {
    priority_queue<int, vector<int>, greater<int>> pq;
    auto comp = pq.value_comp();
    EXPECT_TRUE(comp(2, 1));  // greater<int>: 2 > 1
    EXPECT_FALSE(comp(1, 2));
    EXPECT_FALSE(comp(1, 1));
}

TEST(PriorityQueueTest, ValueCompCustom) {
    priority_queue<Task> pq;
    auto comp = pq.value_comp();
    EXPECT_TRUE(comp(Task("a", 1), Task("b", 2))); // less: 1 < 2
    EXPECT_FALSE(comp(Task("a", 3), Task("b", 2)));
}

// ============================================================================
// Edge cases
// ============================================================================

TEST(PriorityQueueTest, SingleElement) {
    priority_queue<int> pq;
    pq.push(42);
    EXPECT_FALSE(pq.empty());
    EXPECT_EQ(pq.top(), 42);
    pq.pop();
    EXPECT_TRUE(pq.empty());
}

TEST(PriorityQueueTest, DuplicateValues) {
    priority_queue<int> pq;
    pq.push(5);
    pq.push(5);
    pq.push(5);

    EXPECT_EQ(pq.size(), 3u);
    EXPECT_EQ(pq.top(), 5); pq.pop();
    EXPECT_EQ(pq.top(), 5); pq.pop();
    EXPECT_EQ(pq.top(), 5); pq.pop();
    EXPECT_TRUE(pq.empty());
}

TEST(PriorityQueueTest, LargeNumberOfElements) {
    priority_queue<int> pq;
    const int N = 2000;

    // Insert in descending order
    for (int i = N; i >= 1; --i) {
        pq.push(i);
    }

    EXPECT_EQ(pq.size(), static_cast<size_t>(N));
    EXPECT_EQ(pq.top(), N);

    // Pop all and verify descending order
    int prev = N + 1;
    for (int i = 0; i < N; ++i) {
        int cur = pq.top();
        EXPECT_LE(cur, prev);
        prev = cur;
        pq.pop();
    }
    EXPECT_TRUE(pq.empty());
}

TEST(PriorityQueueTest, AllElementsEqual) {
    priority_queue<int> pq;
    for (int i = 0; i < 100; ++i) {
        pq.push(7);
    }
    EXPECT_EQ(pq.size(), 100u);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(pq.top(), 7);
        pq.pop();
    }
    EXPECT_TRUE(pq.empty());
}

TEST(PriorityQueueTest, BooleanNegatives) {
    // Test with negative values in max-heap
    priority_queue<int> pq;
    pq.push(-10);
    pq.push(-5);
    pq.push(-20);
    pq.push(-1);

    EXPECT_EQ(pq.top(), -1);  pq.pop();
    EXPECT_EQ(pq.top(), -5);  pq.pop();
    EXPECT_EQ(pq.top(), -10); pq.pop();
    EXPECT_EQ(pq.top(), -20); pq.pop();
}
