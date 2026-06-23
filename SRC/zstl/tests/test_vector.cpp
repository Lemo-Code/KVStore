// ============================================================================
// zstl::vector Comprehensive Unit Tests
// ============================================================================
// Covers: constructors, assignment, element access, iterators, capacity,
// modifiers (insert/emplace/erase/push_back/pop_back/resize), swap,
// comparisons, self-assignment, iterator invalidation on reallocation,
// POD memmove optimization, iteration order, and large datasets.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>
#include <stdexcept>
#include <algorithm>
#include <memory>

using namespace zstl;

// ============================================================================
// Move-only type for testing move semantics
// ============================================================================
namespace {
struct MoveOnly {
    int value;
    explicit MoveOnly(int v = 0) : value(v) {}
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&& other) noexcept : value(other.value) { other.value = -1; }
    MoveOnly& operator=(MoveOnly&& other) noexcept {
        if (this != &other) { value = other.value; other.value = -1; }
        return *this;
    }
    ~MoveOnly() = default;

    friend bool operator==(const MoveOnly& a, const MoveOnly& b) { return a.value == b.value; }
    friend bool operator!=(const MoveOnly& a, const MoveOnly& b) { return a.value != b.value; }
    friend bool operator<(const MoveOnly& a, const MoveOnly& b) { return a.value < b.value; }
};
}

// ============================================================================
// vector<int> tests
// ============================================================================

// ---- Constructors ----

TEST(VectorInt, DefaultConstructor) {
    vector<int> v;
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
    EXPECT_EQ(v.capacity(), 0u);
    EXPECT_EQ(v.begin(), v.end());
}

TEST(VectorInt, SizeConstructor) {
    vector<int> v(5);
    EXPECT_EQ(v.size(), 5u);
    EXPECT_GE(v.capacity(), 5u);
    for (size_t i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v[i], 0);  // default-initialized to T()
    }
}

TEST(VectorInt, SizeValueConstructor) {
    vector<int> v(4, 42);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_GE(v.capacity(), 4u);
    for (size_t i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v[i], 42);
    }
}

TEST(VectorInt, ZeroSizeConstructor) {
    vector<int> v(0);
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
}

TEST(VectorInt, ZeroSizeValueConstructor) {
    vector<int> v(0, 99);
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
}

TEST(VectorInt, CopyConstructor) {
    vector<int> original = {1, 2, 3, 4, 5};
    vector<int> copy(original);
    EXPECT_EQ(copy.size(), 5u);
    EXPECT_EQ(copy[0], 1);
    EXPECT_EQ(copy[4], 5);
    // Verify deep copy — modifying original does not affect copy
    original[0] = 100;
    EXPECT_EQ(copy[0], 1);
}

TEST(VectorInt, CopyConstructorEmpty) {
    vector<int> original;
    vector<int> copy(original);
    EXPECT_TRUE(copy.empty());
    EXPECT_EQ(copy.size(), 0u);
}

TEST(VectorInt, MoveConstructor) {
    vector<int> original = {10, 20, 30};
    int* orig_data = original.data();
    vector<int> moved(std::move(original));
    EXPECT_EQ(moved.size(), 3u);
    EXPECT_EQ(moved[0], 10);
    EXPECT_EQ(moved[1], 20);
    EXPECT_EQ(moved[2], 30);
    EXPECT_EQ(moved.data(), orig_data);  // pilfered pointer
    EXPECT_TRUE(original.empty());
    EXPECT_EQ(original.size(), 0u);
}

TEST(VectorInt, InitializerListConstructor) {
    vector<int> v = {5, 4, 3, 2, 1};
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0], 5);
    EXPECT_EQ(v[1], 4);
    EXPECT_EQ(v[2], 3);
    EXPECT_EQ(v[3], 2);
    EXPECT_EQ(v[4], 1);
}

TEST(VectorInt, InitializerListConstructorEmpty) {
    vector<int> v = {};
    EXPECT_TRUE(v.empty());
}

TEST(VectorInt, RangeConstructor) {
    int arr[] = {100, 200, 300, 400};
    vector<int> v(arr, arr + 4);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[0], 100);
    EXPECT_EQ(v[3], 400);
}

TEST(VectorInt, RangeConstructorEmpty) {
    int arr[] = {1, 2, 3};
    vector<int> v(arr, arr);
    EXPECT_TRUE(v.empty());
}

// ---- operator= ----

TEST(VectorInt, CopyAssignment) {
    vector<int> v1 = {1, 2, 3};
    vector<int> v2 = {10, 20};
    v2 = v1;
    EXPECT_EQ(v2.size(), 3u);
    EXPECT_EQ(v2[0], 1);
    EXPECT_EQ(v2[1], 2);
    EXPECT_EQ(v2[2], 3);
    // Verify v1 unchanged
    EXPECT_EQ(v1.size(), 3u);
}

TEST(VectorInt, MoveAssignment) {
    vector<int> v1 = {1, 2, 3};
    vector<int> v2 = {10, 20};
    v2 = std::move(v1);
    EXPECT_EQ(v2.size(), 3u);
    EXPECT_EQ(v2[0], 1);
    EXPECT_TRUE(v1.empty());
}

TEST(VectorInt, InitializerListAssignment) {
    vector<int> v = {1, 2, 3};
    v = {10, 20, 30, 40};
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[0], 10);
    EXPECT_EQ(v[3], 40);
}

// ---- assign ----

TEST(VectorInt, AssignFill) {
    vector<int> v = {1, 2, 3};
    v.assign(5, 99);
    EXPECT_EQ(v.size(), 5u);
    for (size_t i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v[i], 99);
    }
}

TEST(VectorInt, AssignFillZeroCount) {
    vector<int> v = {1, 2, 3};
    v.assign(0, 99);
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
}

TEST(VectorInt, AssignRange) {
    vector<int> v = {1, 2, 3};
    int arr[] = {10, 20, 30, 40, 50};
    v.assign(arr, arr + 5);
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0], 10);
    EXPECT_EQ(v[4], 50);
}

TEST(VectorInt, AssignRangeEmpty) {
    vector<int> v = {1, 2, 3};
    int arr[] = {10, 20};
    v.assign(arr, arr);
    EXPECT_TRUE(v.empty());
}

TEST(VectorInt, AssignInitializerList) {
    vector<int> v = {1, 2, 3};
    v.assign({100, 200, 300});
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 100);
    EXPECT_EQ(v[2], 300);
}

// ---- Element Access ----

TEST(VectorInt, AtWithValidIndex) {
    vector<int> v = {10, 20, 30};
    EXPECT_EQ(v.at(0), 10);
    EXPECT_EQ(v.at(1), 20);
    EXPECT_EQ(v.at(2), 30);
}

TEST(VectorInt, AtWithInvalidIndexThrows) {
    vector<int> v = {10, 20, 30};
    EXPECT_THROW(v.at(3), std::out_of_range);
    EXPECT_THROW(v.at(100), std::out_of_range);
    vector<int> empty;
    EXPECT_THROW(empty.at(0), std::out_of_range);
}

TEST(VectorInt, ConstAt) {
    const vector<int> v = {10, 20, 30};
    EXPECT_EQ(v.at(0), 10);
    EXPECT_EQ(v.at(1), 20);
    EXPECT_EQ(v.at(2), 30);
    EXPECT_THROW(v.at(3), std::out_of_range);
}

TEST(VectorInt, OperatorBrackets) {
    vector<int> v = {5, 10, 15};
    EXPECT_EQ(v[0], 5);
    EXPECT_EQ(v[1], 10);
    v[1] = 99;
    EXPECT_EQ(v[1], 99);
}

TEST(VectorInt, ConstOperatorBrackets) {
    const vector<int> v = {5, 10, 15};
    EXPECT_EQ(v[0], 5);
    EXPECT_EQ(v[2], 15);
}

TEST(VectorInt, Front) {
    vector<int> v = {1, 2, 3};
    EXPECT_EQ(v.front(), 1);
    v.front() = 99;
    EXPECT_EQ(v[0], 99);
}

TEST(VectorInt, ConstFront) {
    const vector<int> v = {1, 2, 3};
    EXPECT_EQ(v.front(), 1);
}

TEST(VectorInt, Back) {
    vector<int> v = {1, 2, 3};
    EXPECT_EQ(v.back(), 3);
    v.back() = 99;
    EXPECT_EQ(v[2], 99);
}

TEST(VectorInt, ConstBack) {
    const vector<int> v = {1, 2, 3};
    EXPECT_EQ(v.back(), 3);
}

TEST(VectorInt, Data) {
    vector<int> v = {1, 2, 3};
    int* ptr = v.data();
    EXPECT_EQ(ptr[0], 1);
    EXPECT_EQ(ptr[1], 2);
    EXPECT_EQ(ptr[2], 3);
    ptr[1] = 99;
    EXPECT_EQ(v[1], 99);
}

TEST(VectorInt, ConstData) {
    const vector<int> v = {1, 2, 3};
    const int* ptr = v.data();
    EXPECT_EQ(ptr[0], 1);
    EXPECT_EQ(ptr[1], 2);
}

// ---- Iterators ----

TEST(VectorInt, BeginEnd) {
    vector<int> v = {1, 2, 3, 4, 5};
    int expected = 1;
    for (auto it = v.begin(); it != v.end(); ++it) {
        EXPECT_EQ(*it, expected++);
    }
    EXPECT_EQ(expected, 6);
}

TEST(VectorInt, CBeginCEnd) {
    const vector<int> v = {10, 20, 30};
    int sum = 0;
    for (auto it = v.cbegin(); it != v.cend(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 60);
}

TEST(VectorInt, ReverseIterators) {
    vector<int> v = {1, 2, 3, 4, 5};
    int expected = 5;
    for (auto it = v.rbegin(); it != v.rend(); ++it) {
        EXPECT_EQ(*it, expected--);
    }
    EXPECT_EQ(expected, 0);
}

TEST(VectorInt, ConstReverseIterators) {
    const vector<int> v = {10, 20, 30};
    int expected = 30;
    for (auto it = v.crbegin(); it != v.crend(); ++it) {
        EXPECT_EQ(*it, expected);
        expected -= 10;
    }
    EXPECT_EQ(expected, 0);
}

TEST(VectorInt, ReverseIteratorEquivalence) {
    vector<int> v = {1, 2, 3};
    EXPECT_EQ(*v.rbegin(), v.back());
    EXPECT_EQ(*(--v.rend()), v.front());
}

// ---- Capacity ----

TEST(VectorInt, Empty) {
    vector<int> v;
    EXPECT_TRUE(v.empty());
    v.push_back(1);
    EXPECT_FALSE(v.empty());
    v.clear();
    EXPECT_TRUE(v.empty());
}

TEST(VectorInt, Size) {
    vector<int> v;
    EXPECT_EQ(v.size(), 0u);
    v.push_back(1);
    EXPECT_EQ(v.size(), 1u);
    v.push_back(2);
    EXPECT_EQ(v.size(), 2u);
    v.pop_back();
    EXPECT_EQ(v.size(), 1u);
}

TEST(VectorInt, MaxSize) {
    vector<int> v;
    EXPECT_GT(v.max_size(), 0u);
    EXPECT_GE(v.max_size(), 1024u * 1024u);
}

TEST(VectorInt, Capacity) {
    vector<int> v;
    EXPECT_EQ(v.capacity(), 0u);
    v.reserve(10);
    EXPECT_GE(v.capacity(), 10u);
}

// ---- reserve ----

TEST(VectorInt, ReserveIncreasesCapacity) {
    vector<int> v;
    v.reserve(100);
    EXPECT_GE(v.capacity(), 100u);
    EXPECT_EQ(v.size(), 0u);  // size unchanged
}

TEST(VectorInt, ReserveDoesNotShrink) {
    vector<int> v;
    v.reserve(100);
    size_t cap = v.capacity();
    v.reserve(50);
    EXPECT_EQ(v.capacity(), cap);  // capacity should not shrink
}

TEST(VectorInt, ReserveOnNonEmpty) {
    vector<int> v = {1, 2, 3};
    size_t old_size = v.size();
    v.reserve(100);
    EXPECT_GE(v.capacity(), 100u);
    EXPECT_EQ(v.size(), old_size);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
}

// ---- shrink_to_fit ----

TEST(VectorInt, ShrinkToFit) {
    vector<int> v;
    v.reserve(100);
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    EXPECT_GT(v.capacity(), v.size());
    v.shrink_to_fit();
    EXPECT_EQ(v.capacity(), v.size());
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[2], 3);
}

TEST(VectorInt, ShrinkToFitEmpty) {
    vector<int> v;
    v.reserve(50);
    EXPECT_GT(v.capacity(), v.size());
    v.shrink_to_fit();
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.capacity(), 0u);
    EXPECT_EQ(v.size(), 0u);
}

// ---- clear ----

TEST(VectorInt, Clear) {
    vector<int> v = {1, 2, 3, 4, 5};
    EXPECT_FALSE(v.empty());
    v.clear();
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
    // capacity should be unchanged
    EXPECT_GT(v.capacity(), 0u);
}

TEST(VectorInt, ClearEmpty) {
    vector<int> v;
    v.clear();
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
}

// ---- insert: single element ----

TEST(VectorInt, InsertSingleAtBegin) {
    vector<int> v = {2, 3, 4};
    auto it = v.insert(v.begin(), 1);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
    EXPECT_EQ(v[3], 4);
}

TEST(VectorInt, InsertSingleAtEnd) {
    vector<int> v = {1, 2, 3};
    auto it = v.insert(v.end(), 4);
    EXPECT_EQ(*it, 4);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[3], 4);
}

TEST(VectorInt, InsertSingleInMiddle) {
    vector<int> v = {1, 3, 4};
    auto it = v.insert(v.begin() + 1, 2);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
    EXPECT_EQ(v[3], 4);
}

TEST(VectorInt, InsertSingleIntoEmpty) {
    vector<int> v;
    auto it = v.insert(v.begin(), 42);
    EXPECT_EQ(*it, 42);
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], 42);
}

TEST(VectorInt, InsertSingleByMove) {
    vector<int> v = {1, 3};
    int val = 2;
    auto it = v.insert(v.begin() + 1, std::move(val));
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[1], 2);
}

// ---- insert: fill ----

TEST(VectorInt, InsertFillInMiddle) {
    vector<int> v = {1, 5};
    auto it = v.insert(v.begin() + 1, 3, 9);
    EXPECT_EQ(*it, 9);
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 9);
    EXPECT_EQ(v[2], 9);
    EXPECT_EQ(v[3], 9);
    EXPECT_EQ(v[4], 5);
}

TEST(VectorInt, InsertFillAtEnd) {
    vector<int> v = {1, 2};
    auto it = v.insert(v.end(), 3, 99);
    EXPECT_EQ(*it, 99);
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[2], 99);
    EXPECT_EQ(v[3], 99);
    EXPECT_EQ(v[4], 99);
}

TEST(VectorInt, InsertFillZeroCount) {
    vector<int> v = {1, 2, 3};
    auto it = v.insert(v.begin() + 1, 0, 99);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(v.size(), 3u);
}

// ---- insert: range ----

TEST(VectorInt, InsertRangeAtBegin) {
    vector<int> v = {4, 5};
    int arr[] = {1, 2, 3};
    v.insert(v.begin(), arr, arr + 3);
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
    EXPECT_EQ(v[3], 4);
    EXPECT_EQ(v[4], 5);
}

TEST(VectorInt, InsertRangeAtEnd) {
    vector<int> v = {1, 2};
    int arr[] = {3, 4, 5};
    v.insert(v.end(), arr, arr + 3);
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[3], 3);
    EXPECT_EQ(v[4], 5);
}

TEST(VectorInt, InsertRangeEmpty) {
    vector<int> v = {1, 2, 3};
    int arr[] = {10, 20};
    auto it = v.insert(v.begin(), arr, arr);
    EXPECT_EQ(*it, v[0]);
    EXPECT_EQ(v.size(), 3u);
}

TEST(VectorInt, InsertRangeInMiddle) {
    vector<int> v = {1, 4};
    int arr[] = {2, 3};
    v.insert(v.begin() + 1, arr, arr + 2);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
    EXPECT_EQ(v[3], 4);
}

// ---- insert: initializer_list ----

TEST(VectorInt, InsertInitializerList) {
    vector<int> v = {1, 5};
    auto it = v.insert(v.begin() + 1, {2, 3, 4});
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
    EXPECT_EQ(v[3], 4);
    EXPECT_EQ(v[4], 5);
}

TEST(VectorInt, InsertInitializerListEmpty) {
    vector<int> v = {1, 2, 3};
    auto it = v.insert(v.begin() + 1, {});
    EXPECT_EQ(v.size(), 3u);
}

// ---- emplace ----

TEST(VectorInt, EmplaceInMiddle) {
    vector<int> v = {1, 2, 4, 5};
    auto it = v.emplace(v.begin() + 2, 3);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
    EXPECT_EQ(v[3], 4);
    EXPECT_EQ(v[4], 5);
}

TEST(VectorInt, EmplaceAtBegin) {
    vector<int> v = {2, 3};
    auto it = v.emplace(v.begin(), 1);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
}

TEST(VectorInt, EmplaceAtEnd) {
    vector<int> v = {1, 2};
    auto it = v.emplace(v.end(), 3);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[2], 3);
}

TEST(VectorInt, EmplaceIntoEmpty) {
    vector<int> v;
    auto it = v.emplace(v.begin(), 99);
    EXPECT_EQ(*it, 99);
    EXPECT_EQ(v.size(), 1u);
}

// ---- emplace_back ----

TEST(VectorInt, EmplaceBack) {
    vector<int> v;
    v.emplace_back(1);
    v.emplace_back(2);
    v.emplace_back(3);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
}

TEST(VectorInt, EmplaceBackReturnsReference) {
    vector<int> v;
    auto& ref = v.emplace_back(42);
    EXPECT_EQ(ref, 42);
    ref = 99;
    EXPECT_EQ(v.back(), 99);
}

TEST(VectorInt, EmplaceBackCausesReallocation) {
    vector<int> v;
    size_t cap = v.capacity();
    // Push enough to force reallocation
    for (int i = 0; i < 100; ++i) {
        v.emplace_back(i);
    }
    EXPECT_EQ(v.size(), 100u);
    EXPECT_GT(v.capacity(), cap);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(v[i], i);
    }
}

// ---- push_back ----

TEST(VectorInt, PushBackCopy) {
    vector<int> v;
    int val = 42;
    v.push_back(val);
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], 42);
    EXPECT_EQ(val, 42);  // val unchanged
}

TEST(VectorInt, PushBackMove) {
    vector<int> v;
    int val = 42;
    v.push_back(std::move(val));
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], 42);
}

// ---- pop_back ----

TEST(VectorInt, PopBack) {
    vector<int> v = {1, 2, 3};
    v.pop_back();
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    v.pop_back();
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], 1);
    v.pop_back();
    EXPECT_TRUE(v.empty());
}

// ---- erase: single ----

TEST(VectorInt, EraseSingle) {
    vector<int> v = {1, 2, 3, 4, 5};
    auto it = v.erase(v.begin() + 2);  // erase 3
    EXPECT_EQ(*it, 4);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 4);
    EXPECT_EQ(v[3], 5);
}

TEST(VectorInt, EraseFirst) {
    vector<int> v = {1, 2, 3};
    auto it = v.erase(v.begin());
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 2);
}

TEST(VectorInt, EraseLast) {
    vector<int> v = {1, 2, 3};
    auto it = v.erase(v.end() - 1);
    EXPECT_EQ(it, v.end());
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
}

TEST(VectorInt, EraseOnlyElement) {
    vector<int> v = {42};
    auto it = v.erase(v.begin());
    EXPECT_EQ(it, v.end());
    EXPECT_TRUE(v.empty());
}

// ---- erase: range ----

TEST(VectorInt, EraseRange) {
    vector<int> v = {1, 2, 3, 4, 5};
    auto it = v.erase(v.begin() + 1, v.begin() + 4);  // erase 2,3,4
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 5);
}

TEST(VectorInt, EraseAll) {
    vector<int> v = {1, 2, 3};
    auto it = v.erase(v.begin(), v.end());
    EXPECT_EQ(it, v.end());
    EXPECT_TRUE(v.empty());
}

TEST(VectorInt, EraseRangeEmpty) {
    vector<int> v = {1, 2, 3};
    auto it = v.erase(v.begin() + 1, v.begin() + 1);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(v.size(), 3u);
}

// ---- resize ----

TEST(VectorInt, ResizeGrowDefaultInit) {
    vector<int> v = {1, 2, 3};
    v.resize(5);
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
    EXPECT_EQ(v[3], 0);
    EXPECT_EQ(v[4], 0);
}

TEST(VectorInt, ResizeShrink) {
    vector<int> v = {1, 2, 3, 4, 5};
    v.resize(3);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
}

TEST(VectorInt, ResizeGrowWithValue) {
    vector<int> v = {1, 2};
    v.resize(5, 99);
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 99);
    EXPECT_EQ(v[3], 99);
    EXPECT_EQ(v[4], 99);
}

TEST(VectorInt, ResizeShrinkWithValue) {
    vector<int> v = {1, 2, 3, 4, 5};
    v.resize(2, 99);
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
}

TEST(VectorInt, ResizeSameSize) {
    vector<int> v = {1, 2, 3};
    v.resize(3);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
}

// ---- swap ----

TEST(VectorInt, SwapMember) {
    vector<int> v1 = {1, 2, 3};
    vector<int> v2 = {10, 20};
    int* data1 = v1.data();
    int* data2 = v2.data();
    v1.swap(v2);
    EXPECT_EQ(v1.size(), 2u);
    EXPECT_EQ(v1[0], 10);
    EXPECT_EQ(v1[1], 20);
    EXPECT_EQ(v1.data(), data2);
    EXPECT_EQ(v2.size(), 3u);
    EXPECT_EQ(v2[0], 1);
    EXPECT_EQ(v2[1], 2);
    EXPECT_EQ(v2[2], 3);
    EXPECT_EQ(v2.data(), data1);
}

TEST(VectorInt, SwapNonMember) {
    vector<int> v1 = {1, 2, 3};
    vector<int> v2 = {10, 20};
    swap(v1, v2);
    EXPECT_EQ(v1.size(), 2u);
    EXPECT_EQ(v1[0], 10);
    EXPECT_EQ(v2.size(), 3u);
    EXPECT_EQ(v2[0], 1);
}

TEST(VectorInt, SwapEmpty) {
    vector<int> v1;
    vector<int> v2 = {1, 2, 3};
    v1.swap(v2);
    EXPECT_EQ(v1.size(), 3u);
    EXPECT_EQ(v1[0], 1);
    EXPECT_TRUE(v2.empty());
}

TEST(VectorInt, SwapSelf) {
    vector<int> v = {1, 2, 3};
    v.swap(v);  // self-swap should be nop
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
}

// ---- Comparison operators ----

TEST(VectorInt, OperatorEquals) {
    vector<int> v1 = {1, 2, 3};
    vector<int> v2 = {1, 2, 3};
    vector<int> v3 = {1, 2, 4};
    vector<int> v4 = {1, 2};
    EXPECT_TRUE(v1 == v2);
    EXPECT_FALSE(v1 == v3);
    EXPECT_FALSE(v1 == v4);
}

TEST(VectorInt, OperatorNotEquals) {
    vector<int> v1 = {1, 2, 3};
    vector<int> v2 = {1, 2, 4};
    EXPECT_TRUE(v1 != v2);
    EXPECT_FALSE(v1 != v1);
}

TEST(VectorInt, OperatorLess) {
    vector<int> v1 = {1, 2, 3};
    vector<int> v2 = {1, 2, 4};
    vector<int> v3 = {1, 2};
    EXPECT_TRUE(v1 < v2);
    EXPECT_FALSE(v2 < v1);
    EXPECT_TRUE(v3 < v1);
    EXPECT_FALSE(v1 < v3);
}

TEST(VectorInt, OperatorGreater) {
    vector<int> v1 = {5, 6, 7};
    vector<int> v2 = {1, 2, 3};
    EXPECT_TRUE(v1 > v2);
    EXPECT_FALSE(v2 > v1);
}

TEST(VectorInt, OperatorLessEqual) {
    vector<int> v1 = {1, 2, 3};
    vector<int> v2 = {1, 2, 3};
    vector<int> v3 = {1, 2, 4};
    EXPECT_TRUE(v1 <= v2);
    EXPECT_TRUE(v1 <= v3);
    EXPECT_FALSE(v3 <= v1);
}

TEST(VectorInt, OperatorGreaterEqual) {
    vector<int> v1 = {1, 2, 3};
    vector<int> v2 = {1, 2, 3};
    vector<int> v3 = {1, 2, 4};
    EXPECT_TRUE(v1 >= v2);
    EXPECT_TRUE(v3 >= v1);
    EXPECT_FALSE(v1 >= v3);
}

TEST(VectorInt, EmptyVectorComparison) {
    vector<int> v1;
    vector<int> v2;
    EXPECT_TRUE(v1 == v2);
    EXPECT_FALSE(v1 != v2);
    EXPECT_FALSE(v1 < v2);
    EXPECT_TRUE(v1 <= v2);
    EXPECT_TRUE(v1 >= v2);
    vector<int> v3 = {1};
    EXPECT_TRUE(v1 < v3);
    EXPECT_FALSE(v3 < v1);
}

// ---- Self-assignment ----

TEST(VectorInt, SelfCopyAssignment) {
    vector<int> v = {1, 2, 3, 4, 5};
    v = v;
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[4], 5);
}

TEST(VectorInt, SelfMoveAssignment) {
    vector<int> v = {10, 20, 30};
    v = std::move(v);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 10);
    EXPECT_EQ(v[2], 30);
}

// ---- Iterator invalidation on reallocation ----

TEST(VectorInt, IteratorInvalidationOnPushBackPastCapacity) {
    vector<int> v;
    v.reserve(3);
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    auto it = v.begin();  // points to v[0]
    EXPECT_EQ(*it, 1);
    v.push_back(4);  // triggers reallocation
    // it is now invalid; accessing would be UB. Verify data integrity instead.
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
    EXPECT_EQ(v[3], 4);
}

// ---- Iteration order verification ----

TEST(VectorInt, ForwardIterationOrder) {
    vector<int> v;
    for (int i = 0; i < 50; ++i) v.push_back(i * 10);
    int expected = 0;
    for (auto it = v.begin(); it != v.end(); ++it) {
        EXPECT_EQ(*it, expected);
        expected += 10;
    }
}

TEST(VectorInt, ReverseIterationOrder) {
    vector<int> v;
    for (int i = 0; i < 50; ++i) v.push_back(i);
    int expected = 49;
    for (auto it = v.rbegin(); it != v.rend(); ++it) {
        EXPECT_EQ(*it, expected);
        --expected;
    }
}

TEST(VectorInt, RangeForLoop) {
    vector<int> v = {1, 2, 3, 4, 5};
    int expected = 1;
    for (const auto& val : v) {
        EXPECT_EQ(val, expected++);
    }
}

// ---- Large dataset ----

TEST(VectorInt, LargeDataset10000) {
    constexpr size_t N = 10000;
    vector<int> v;
    v.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        v.push_back(static_cast<int>(i));
    }
    EXPECT_EQ(v.size(), N);
    EXPECT_GE(v.capacity(), N);
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(v[i], static_cast<int>(i));
    }
    // Reverse check
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(v[N - 1 - i], static_cast<int>(N - 1 - i));
    }
}

TEST(VectorInt, LargeDatasetInsertAndErase) {
    constexpr size_t N = 1000;
    vector<int> v;
    for (size_t i = 0; i < N; ++i) {
        v.insert(v.end(), static_cast<int>(i));
    }
    EXPECT_EQ(v.size(), N);
    // Erase even numbers
    for (size_t i = 0; i < v.size(); ) {
        if (v[i] % 2 == 0) {
            v.erase(v.begin() + static_cast<ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
    EXPECT_EQ(v.size(), N / 2);
    for (size_t i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v[i] % 2, 1);
    }
}

// ---- Growth policy verification ----

TEST(VectorInt, GrowthPolicy) {
    vector<int> v;
    EXPECT_EQ(v.capacity(), 0u);
    v.push_back(1);
    EXPECT_GE(v.capacity(), 4u);  // min capacity of 4
    // Fill up to trigger growth
    size_t prev_cap = v.capacity();
    size_t growth_count = 0;
    for (int i = 0; i < 1000; ++i) {
        v.push_back(i);
        if (v.capacity() != prev_cap) {
            prev_cap = v.capacity();
            ++growth_count;
        }
    }
    EXPECT_GT(growth_count, 0u);
    EXPECT_GE(v.capacity(), 1000u);
}

// ---- const_iterator-based insert/erase ----

TEST(VectorInt, InsertUsingConstIterator) {
    vector<int> v = {1, 3, 4};
    const_iterator pos = v.cbegin() + 1;
    v.insert(pos, 2);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[1], 2);
}

TEST(VectorInt, EraseUsingConstIterator) {
    vector<int> v = {1, 2, 3};
    const_iterator pos = v.cbegin() + 1;
    v.erase(pos);
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 3);
}


// ============================================================================
// vector<std::string> tests (non-trivial type)
// ============================================================================

TEST(VectorString, DefaultConstructor) {
    vector<std::string> v;
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
}

TEST(VectorString, SizeConstructor) {
    vector<std::string> v(3);
    EXPECT_EQ(v.size(), 3u);
    for (size_t i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v[i], std::string());  // default initialized
    }
}

TEST(VectorString, SizeValueConstructor) {
    vector<std::string> v(3, "hello");
    EXPECT_EQ(v.size(), 3u);
    for (size_t i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v[i], "hello");
    }
}

TEST(VectorString, CopyConstructor) {
    vector<std::string> original = {"a", "b", "c"};
    vector<std::string> copy(original);
    EXPECT_EQ(copy.size(), 3u);
    EXPECT_EQ(copy[0], "a");
    EXPECT_EQ(copy[1], "b");
    EXPECT_EQ(copy[2], "c");
    // Deep copy verification
    original[0] = "zzz";
    EXPECT_EQ(copy[0], "a");
}

TEST(VectorString, MoveConstructor) {
    vector<std::string> original = {"x", "y", "z"};
    vector<std::string> moved(std::move(original));
    EXPECT_EQ(moved.size(), 3u);
    EXPECT_EQ(moved[0], "x");
    EXPECT_EQ(moved[1], "y");
    EXPECT_EQ(moved[2], "z");
    EXPECT_TRUE(original.empty());
}

TEST(VectorString, InitializerListConstructor) {
    vector<std::string> v = {"one", "two", "three"};
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], "one");
    EXPECT_EQ(v[1], "two");
    EXPECT_EQ(v[2], "three");
}

TEST(VectorString, PushBackCopy) {
    vector<std::string> v;
    std::string s = "test";
    v.push_back(s);
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], "test");
    EXPECT_EQ(s, "test");  // s unchanged
}

TEST(VectorString, PushBackMove) {
    vector<std::string> v;
    std::string s = "move_me";
    v.push_back(std::move(s));
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], "move_me");
}

TEST(VectorString, EmplaceBack) {
    vector<std::string> v;
    v.emplace_back("constructed");
    v.emplace_back(5, 'x');
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], "constructed");
    EXPECT_EQ(v[1], "xxxxx");
}

TEST(VectorString, Emplace) {
    vector<std::string> v = {"a", "c"};
    v.emplace(v.begin() + 1, "b");
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], "a");
    EXPECT_EQ(v[1], "b");
    EXPECT_EQ(v[2], "c");
}

TEST(VectorString, InsertFill) {
    vector<std::string> v = {"start", "end"};
    v.insert(v.begin() + 1, 3, "mid");
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0], "start");
    EXPECT_EQ(v[1], "mid");
    EXPECT_EQ(v[2], "mid");
    EXPECT_EQ(v[3], "mid");
    EXPECT_EQ(v[4], "end");
}

TEST(VectorString, InsertRange) {
    vector<std::string> v = {"a", "d"};
    std::string arr[] = {"b", "c"};
    v.insert(v.begin() + 1, arr, arr + 2);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[0], "a");
    EXPECT_EQ(v[1], "b");
    EXPECT_EQ(v[2], "c");
    EXPECT_EQ(v[3], "d");
}

TEST(VectorString, EraseSingle) {
    vector<std::string> v = {"a", "b", "c"};
    v.erase(v.begin() + 1);
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], "a");
    EXPECT_EQ(v[1], "c");
}

TEST(VectorString, EraseRange) {
    vector<std::string> v = {"a", "x1", "x2", "x3", "b"};
    v.erase(v.begin() + 1, v.begin() + 4);
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], "a");
    EXPECT_EQ(v[1], "b");
}

TEST(VectorString, ResizeString) {
    vector<std::string> v = {"a", "b"};
    v.resize(4, "z");
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[0], "a");
    EXPECT_EQ(v[1], "b");
    EXPECT_EQ(v[2], "z");
    EXPECT_EQ(v[3], "z");
    v.resize(1);
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], "a");
}

TEST(VectorString, ClearWithStrings) {
    vector<std::string> v = {"a", "b", "c"};
    v.clear();
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
}

TEST(VectorString, ComparisonStrings) {
    vector<std::string> v1 = {"a", "b", "c"};
    vector<std::string> v2 = {"a", "b", "c"};
    vector<std::string> v3 = {"a", "b", "d"};
    EXPECT_TRUE(v1 == v2);
    EXPECT_TRUE(v1 < v3);
    EXPECT_TRUE(v3 > v1);
    EXPECT_TRUE(v1 <= v2);
    EXPECT_TRUE(v1 >= v2);
}

TEST(VectorString, LargeStringDataset) {
    vector<std::string> v;
    v.reserve(500);
    for (int i = 0; i < 500; ++i) {
        v.emplace_back(std::to_string(i));
    }
    EXPECT_EQ(v.size(), 500u);
    for (int i = 0; i < 500; ++i) {
        EXPECT_EQ(v[i], std::to_string(i));
    }
}


// ============================================================================
// vector<MoveOnly> tests (move-only type)
// ============================================================================

TEST(VectorMoveOnly, DefaultConstructor) {
    vector<MoveOnly> v;
    EXPECT_TRUE(v.empty());
}

TEST(VectorMoveOnly, SizeConstructor) {
    // vector(n) calls resize(n) which default-constructs T()
    // MoveOnly has a default constructor, so this should work
    vector<MoveOnly> v(3);
    EXPECT_EQ(v.size(), 3u);
}

TEST(VectorMoveOnly, MoveConstructor) {
    vector<MoveOnly> v;
    v.emplace_back(10);
    v.emplace_back(20);
    v.emplace_back(30);

    vector<MoveOnly> moved(std::move(v));
    EXPECT_EQ(moved.size(), 3u);
    EXPECT_EQ(moved[0].value, 10);
    EXPECT_EQ(moved[1].value, 20);
    EXPECT_EQ(moved[2].value, 30);
    EXPECT_TRUE(v.empty());
}

TEST(VectorMoveOnly, MoveAssignment) {
    vector<MoveOnly> v1;
    v1.emplace_back(1);
    v1.emplace_back(2);

    vector<MoveOnly> v2;
    v2.emplace_back(99);

    v2 = std::move(v1);
    EXPECT_EQ(v2.size(), 2u);
    EXPECT_EQ(v2[0].value, 1);
    EXPECT_EQ(v2[1].value, 2);
    EXPECT_TRUE(v1.empty());
}

TEST(VectorMoveOnly, EmplaceBack) {
    vector<MoveOnly> v;
    auto& ref = v.emplace_back(42);
    EXPECT_EQ(ref.value, 42);
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0].value, 42);

    v.emplace_back(99);
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[1].value, 99);
}

TEST(VectorMoveOnly, Emplace) {
    vector<MoveOnly> v;
    v.emplace_back(1);
    v.emplace_back(3);
    v.emplace(v.begin() + 1, 2);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0].value, 1);
    EXPECT_EQ(v[1].value, 2);
    EXPECT_EQ(v[2].value, 3);
}

TEST(VectorMoveOnly, PushBackMove) {
    vector<MoveOnly> v;
    MoveOnly m(77);
    v.push_back(std::move(m));
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0].value, 77);
    EXPECT_EQ(m.value, -1);  // moved-from
}

TEST(VectorMoveOnly, PopBack) {
    vector<MoveOnly> v;
    v.emplace_back(1);
    v.emplace_back(2);
    v.emplace_back(3);
    v.pop_back();
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v.back().value, 2);
}

TEST(VectorMoveOnly, EraseSingle) {
    vector<MoveOnly> v;
    v.emplace_back(1);
    v.emplace_back(2);
    v.emplace_back(3);
    v.erase(v.begin() + 1);
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0].value, 1);
    EXPECT_EQ(v[1].value, 3);
}

TEST(VectorMoveOnly, EraseRange) {
    vector<MoveOnly> v;
    v.emplace_back(1);
    v.emplace_back(999);
    v.emplace_back(999);
    v.emplace_back(4);
    v.erase(v.begin() + 1, v.begin() + 3);
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0].value, 1);
    EXPECT_EQ(v[1].value, 4);
}

TEST(VectorMoveOnly, Clear) {
    vector<MoveOnly> v;
    v.emplace_back(1);
    v.emplace_back(2);
    v.clear();
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
}

TEST(VectorMoveOnly, Swap) {
    vector<MoveOnly> v1;
    v1.emplace_back(10);
    vector<MoveOnly> v2;
    v2.emplace_back(20);
    v2.emplace_back(30);

    v1.swap(v2);
    EXPECT_EQ(v1.size(), 2u);
    EXPECT_EQ(v1[0].value, 20);
    EXPECT_EQ(v1[1].value, 30);
    EXPECT_EQ(v2.size(), 1u);
    EXPECT_EQ(v2[0].value, 10);
}

TEST(VectorMoveOnly, ReserveAndEmplace) {
    vector<MoveOnly> v;
    v.reserve(50);
    for (int i = 0; i < 50; ++i) {
        v.emplace_back(i);
    }
    EXPECT_EQ(v.size(), 50u);
    for (int i = 0; i < 50; ++i) {
        EXPECT_EQ(v[i].value, i);
    }
}

TEST(VectorMoveOnly, Resize) {
    vector<MoveOnly> v;
    v.resize(5);
    EXPECT_EQ(v.size(), 5u);
    v.resize(2);
    EXPECT_EQ(v.size(), 2u);
}


// ============================================================================
// POD memmove optimization tests
// ============================================================================
// Trivially copyable types (int, etc.) benefit from memmove-based shifting.
// These tests verify correctness under various operations.

TEST(VectorPOptimization, InsertAtFrontTriggersMove) {
    vector<int> v;
    v.reserve(100);
    for (int i = 0; i < 50; ++i) v.push_back(i);

    v.insert(v.begin(), -1);
    EXPECT_EQ(v.size(), 51u);
    EXPECT_EQ(v[0], -1);
    for (int i = 0; i < 50; ++i) {
        EXPECT_EQ(v[i + 1], i);
    }
}

TEST(VectorPOptimization, InsertManyInMiddle) {
    vector<int> v;
    v.reserve(200);
    for (int i = 0; i < 100; ++i) v.push_back(i);
    v.insert(v.begin() + 50, 10, 999);
    EXPECT_EQ(v.size(), 110u);
    for (int i = 0; i < 50; ++i) EXPECT_EQ(v[i], i);
    for (int i = 50; i < 60; ++i) EXPECT_EQ(v[i], 999);
    for (int i = 60; i < 110; ++i) EXPECT_EQ(v[i], i - 10);
}

TEST(VectorPOptimization, EraseManyInMiddle) {
    vector<int> v;
    for (int i = 0; i < 100; ++i) v.push_back(i);
    v.erase(v.begin() + 20, v.begin() + 80);
    EXPECT_EQ(v.size(), 40u);
    for (int i = 0; i < 20; ++i) EXPECT_EQ(v[i], i);
    for (int i = 20; i < 40; ++i) EXPECT_EQ(v[i], i + 60);
}

TEST(VectorPOptimization, LargeReallocationPreservesData) {
    vector<int> v;
    size_t total = 5000;
    for (size_t i = 0; i < total; ++i) {
        v.push_back(static_cast<int>(i * 3));
    }
    EXPECT_EQ(v.size(), total);
    for (size_t i = 0; i < total; ++i) {
        EXPECT_EQ(v[i], static_cast<int>(i * 3));
    }
}
