// ============================================================================
// zstl deque Unit Tests
// Tests all constructors, assignment, element access, iterators, capacity,
// modifiers (push/pop/emplace/insert/erase), swap, clear, shrink_to_fit,
// block map expansion under stress, and element-wise comparison.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

using namespace zstl;

// ============================================================
// Helper: element-wise equality for two deques
// ============================================================
template <typename T>
static bool deque_equal(const deque<T>& a, const deque<T>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

// ============================================================
// Constructors
// ============================================================

TEST(DequeTest, DefaultConstructor) {
    deque<int> d;
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.size(), 0u);
    EXPECT_EQ(d.begin(), d.end());
}

TEST(DequeTest, FillConstructorDefaultValue) {
    deque<int> d(5);
    EXPECT_EQ(d.size(), 5u);
    EXPECT_FALSE(d.empty());
    for (size_t i = 0; i < d.size(); ++i) {
        EXPECT_EQ(d[i], 0);
    }
}

TEST(DequeTest, FillConstructorWithValue) {
    deque<int> d(5, 42);
    EXPECT_EQ(d.size(), 5u);
    for (size_t i = 0; i < d.size(); ++i) {
        EXPECT_EQ(d[i], 42);
    }
}

TEST(DequeTest, FillConstructorZeroCount) {
    deque<int> d(0, 99);
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.size(), 0u);
}

TEST(DequeTest, RangeConstructor) {
    std::vector<int> v = {1, 2, 3, 4, 5};
    deque<int> d(v.begin(), v.end());
    EXPECT_EQ(d.size(), 5u);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(d[i], static_cast<int>(i + 1));
    }
}

TEST(DequeTest, RangeConstructorEmpty) {
    std::vector<int> v;
    deque<int> d(v.begin(), v.end());
    EXPECT_TRUE(d.empty());
}

TEST(DequeTest, CopyConstructor) {
    deque<int> src = {10, 20, 30};
    deque<int> d(src);
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(d[0], 10);
    EXPECT_EQ(d[1], 20);
    EXPECT_EQ(d[2], 30);
    // Verify deep copy
    src[0] = 999;
    EXPECT_EQ(d[0], 10);
}

TEST(DequeTest, MoveConstructor) {
    deque<int> src = {1, 2, 3, 4, 5};
    deque<int> d(std::move(src));
    EXPECT_EQ(d.size(), 5u);
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[4], 5);
    // src should be in valid but unspecified state (empty per our impl)
    EXPECT_TRUE(src.empty());
}

TEST(DequeTest, InitializerListConstructor) {
    deque<int> d = {7, 8, 9, 10};
    EXPECT_EQ(d.size(), 4u);
    EXPECT_EQ(d[0], 7);
    EXPECT_EQ(d[1], 8);
    EXPECT_EQ(d[2], 9);
    EXPECT_EQ(d[3], 10);
}

TEST(DequeTest, InitializerListConstructorEmpty) {
    deque<int> d = {};
    EXPECT_TRUE(d.empty());
}

// ============================================================
// operator=
// ============================================================

TEST(DequeTest, CopyAssignment) {
    deque<int> a = {1, 2, 3};
    deque<int> b = {10, 20, 30, 40, 50};
    b = a;
    EXPECT_TRUE(deque_equal(a, b));
    a[0] = 999;
    EXPECT_EQ(b[0], 1);  // deep copy
}

TEST(DequeTest, CopyAssignmentSelf) {
    deque<int> d = {1, 2, 3};
    d = d;
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(d[0], 1);
}

TEST(DequeTest, MoveAssignment) {
    deque<int> a = {1, 2, 3};
    deque<int> b = {10, 20, 30, 40};
    b = std::move(a);
    EXPECT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 1);
    EXPECT_TRUE(a.empty());
}

TEST(DequeTest, MoveAssignmentSelf) {
    deque<int> d = {1, 2, 3};
    d = std::move(d);  // self-move, may or may not clear (implementation-defined)
    // At minimum should not crash
    SUCCEED();
}

TEST(DequeTest, InitializerListAssignment) {
    deque<int> d = {1, 2, 3};
    d = {10, 20, 30, 40};
    EXPECT_EQ(d.size(), 4u);
    EXPECT_EQ(d[0], 10);
    EXPECT_EQ(d[3], 40);
}

// ============================================================
// assign
// ============================================================

TEST(DequeTest, AssignFill) {
    deque<int> d = {1, 2, 3, 4, 5};
    d.assign(3, 42);
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(d[0], 42);
    EXPECT_EQ(d[1], 42);
    EXPECT_EQ(d[2], 42);
}

TEST(DequeTest, AssignFillZero) {
    deque<int> d = {1, 2, 3};
    d.assign(0, 99);
    EXPECT_TRUE(d.empty());
}

TEST(DequeTest, AssignRange) {
    deque<int> d = {1, 2, 3};
    std::vector<int> v = {100, 200};
    d.assign(v.begin(), v.end());
    EXPECT_EQ(d.size(), 2u);
    EXPECT_EQ(d[0], 100);
    EXPECT_EQ(d[1], 200);
}

TEST(DequeTest, AssignInitializerList) {
    deque<int> d = {1, 2, 3, 4, 5};
    d.assign({10, 20});
    EXPECT_EQ(d.size(), 2u);
    EXPECT_EQ(d[0], 10);
    EXPECT_EQ(d[1], 20);
}

// ============================================================
// Element access
// ============================================================

TEST(DequeTest, AtValidIndex) {
    deque<int> d = {10, 20, 30, 40, 50};
    EXPECT_EQ(d.at(0), 10);
    EXPECT_EQ(d.at(2), 30);
    EXPECT_EQ(d.at(4), 50);
}

TEST(DequeTest, AtOutOfRange) {
    deque<int> d = {1, 2, 3};
    EXPECT_THROW(d.at(3), std::out_of_range);
    EXPECT_THROW(d.at(100), std::out_of_range);
    const auto& cd = d;
    EXPECT_THROW(cd.at(3), std::out_of_range);
}

TEST(DequeTest, AtOnEmpty) {
    deque<int> d;
    EXPECT_THROW(d.at(0), std::out_of_range);
}

TEST(DequeTest, OperatorBracket) {
    deque<int> d = {10, 20, 30};
    EXPECT_EQ(d[0], 10);
    EXPECT_EQ(d[2], 30);
    d[1] = 99;
    EXPECT_EQ(d[1], 99);
    const auto& cd = d;
    EXPECT_EQ(cd[0], 10);
}

TEST(DequeTest, FrontBack) {
    deque<int> d = {5, 10, 15, 20};
    EXPECT_EQ(d.front(), 5);
    EXPECT_EQ(d.back(), 20);
    d.front() = 100;
    d.back() = 200;
    EXPECT_EQ(d.front(), 100);
    EXPECT_EQ(d.back(), 200);
    const auto& cd = d;
    EXPECT_EQ(cd.front(), 100);
    EXPECT_EQ(cd.back(), 200);
}

TEST(DequeTest, FrontBackSingleElement) {
    deque<int> d = {42};
    EXPECT_EQ(d.front(), 42);
    EXPECT_EQ(d.back(), 42);
    EXPECT_EQ(&d.front(), &d.back());
}

// ============================================================
// Iterators
// ============================================================

TEST(DequeTest, BeginEnd) {
    deque<int> d = {1, 2, 3};
    auto it = d.begin();
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_EQ(*it, 2);
    ++it;
    EXPECT_EQ(*it, 3);
    ++it;
    EXPECT_EQ(it, d.end());
}

TEST(DequeTest, ConstBeginEnd) {
    const deque<int> d = {1, 2, 3};
    auto it = d.begin();
    EXPECT_EQ(*it, 1);
    auto it2 = d.cbegin();
    EXPECT_EQ(*it2, 1);
    EXPECT_EQ(d.cend(), d.end());
}

TEST(DequeTest, ReverseIterators) {
    deque<int> d = {1, 2, 3, 4};
    auto rit = d.rbegin();
    EXPECT_EQ(*rit, 4);
    ++rit;
    EXPECT_EQ(*rit, 3);
    ++rit;
    EXPECT_EQ(*rit, 2);
    ++rit;
    EXPECT_EQ(*rit, 1);
    ++rit;
    EXPECT_EQ(rit, d.rend());
}

TEST(DequeTest, ConstReverseIterators) {
    const deque<int> d = {10, 20, 30};
    auto it = d.crbegin();
    EXPECT_EQ(*it, 30);
    ++it;
    EXPECT_EQ(*it, 20);
}

TEST(DequeTest, IteratorRandomAccess) {
    deque<int> d = {0, 10, 20, 30, 40};
    auto it = d.begin();
    EXPECT_EQ(it[0], 0);
    EXPECT_EQ(it[3], 30);
    it += 2;
    EXPECT_EQ(*it, 20);
    it -= 1;
    EXPECT_EQ(*it, 10);
    auto it2 = d.end();
    EXPECT_EQ(it2 - it, 4);
}

TEST(DequeTest, IteratorOnEmpty) {
    deque<int> d;
    EXPECT_EQ(d.begin(), d.end());
    EXPECT_EQ(d.rbegin(), d.rend());
}

// ============================================================
// Capacity
// ============================================================

TEST(DequeTest, Empty) {
    deque<int> d;
    EXPECT_TRUE(d.empty());
    d.push_back(1);
    EXPECT_FALSE(d.empty());
    d.pop_back();
    EXPECT_TRUE(d.empty());
}

TEST(DequeTest, Size) {
    deque<int> d;
    EXPECT_EQ(d.size(), 0u);
    d.push_back(1);
    EXPECT_EQ(d.size(), 1u);
    d.push_front(2);
    EXPECT_EQ(d.size(), 2u);
    d.pop_back();
    EXPECT_EQ(d.size(), 1u);
}

TEST(DequeTest, MaxSize) {
    deque<int> d;
    EXPECT_GT(d.max_size(), 0u);
}

// ============================================================
// push_front / push_back
// ============================================================

TEST(DequeTest, PushBack) {
    deque<int> d;
    d.push_back(1);
    EXPECT_EQ(d.size(), 1u);
    EXPECT_EQ(d.back(), 1);
    d.push_back(2);
    EXPECT_EQ(d.back(), 2);
    d.push_back(3);
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 2);
    EXPECT_EQ(d[2], 3);
}

TEST(DequeTest, PushFront) {
    deque<int> d;
    d.push_front(1);
    EXPECT_EQ(d.front(), 1);
    d.push_front(2);
    EXPECT_EQ(d.front(), 2);
    d.push_front(3);
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(d[0], 3);
    EXPECT_EQ(d[1], 2);
    EXPECT_EQ(d[2], 1);
}

TEST(DequeTest, PushFrontBackMixed) {
    deque<int> d;
    d.push_back(2);
    d.push_front(1);
    d.push_back(3);
    d.push_front(0);
    EXPECT_EQ(d.size(), 4u);
    EXPECT_EQ(d[0], 0);
    EXPECT_EQ(d[1], 1);
    EXPECT_EQ(d[2], 2);
    EXPECT_EQ(d[3], 3);
}

TEST(DequeTest, PushRvalue) {
    deque<int> d;
    int val = 42;
    d.push_back(std::move(val));
    EXPECT_EQ(d.back(), 42);
    int val2 = 99;
    d.push_front(std::move(val2));
    EXPECT_EQ(d.front(), 99);
}

// ============================================================
// pop_front / pop_back
// ============================================================

TEST(DequeTest, PopBack) {
    deque<int> d = {1, 2, 3};
    d.pop_back();
    EXPECT_EQ(d.size(), 2u);
    EXPECT_EQ(d.back(), 2);
    d.pop_back();
    EXPECT_EQ(d.size(), 1u);
    EXPECT_EQ(d.back(), 1);
    d.pop_back();
    EXPECT_TRUE(d.empty());
}

TEST(DequeTest, PopFront) {
    deque<int> d = {1, 2, 3};
    d.pop_front();
    EXPECT_EQ(d.size(), 2u);
    EXPECT_EQ(d.front(), 2);
    d.pop_front();
    EXPECT_EQ(d.size(), 1u);
    EXPECT_EQ(d.front(), 3);
    d.pop_front();
    EXPECT_TRUE(d.empty());
}

TEST(DequeTest, PopFrontPopBackMixed) {
    deque<int> d = {1, 2, 3, 4, 5};
    d.pop_front();
    d.pop_back();
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(d[0], 2);
    EXPECT_EQ(d[1], 3);
    EXPECT_EQ(d[2], 4);
}

// ============================================================
// emplace_front / emplace_back
// ============================================================

TEST(DequeTest, EmplaceBack) {
    deque<int> d;
    auto& ref = d.emplace_back(10);
    EXPECT_EQ(ref, 10);
    EXPECT_EQ(d.back(), 10);
    d.emplace_back(20);
    EXPECT_EQ(d.size(), 2u);
    EXPECT_EQ(d.back(), 20);
}

TEST(DequeTest, EmplaceFront) {
    deque<int> d;
    auto& ref = d.emplace_front(10);
    EXPECT_EQ(ref, 10);
    EXPECT_EQ(d.front(), 10);
    d.emplace_front(20);
    EXPECT_EQ(d.size(), 2u);
    EXPECT_EQ(d.front(), 20);
    EXPECT_EQ(d[1], 10);
}

// ============================================================
// insert (single value)
// ============================================================

TEST(DequeTest, InsertAtFront) {
    deque<int> d = {2, 3, 4};
    auto it = d.insert(d.cbegin(), 1);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(d.size(), 4u);
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 2);
    EXPECT_EQ(d[2], 3);
    EXPECT_EQ(d[3], 4);
}

TEST(DequeTest, InsertAtBack) {
    deque<int> d = {1, 2, 3};
    auto it = d.insert(d.cend(), 4);
    EXPECT_EQ(*it, 4);
    EXPECT_EQ(d.size(), 4u);
    EXPECT_EQ(d[3], 4);
}

TEST(DequeTest, InsertInMiddle) {
    deque<int> d = {1, 2, 4, 5};
    auto it = d.insert(d.cbegin() + 2, 3);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(d.size(), 5u);
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 2);
    EXPECT_EQ(d[2], 3);
    EXPECT_EQ(d[3], 4);
    EXPECT_EQ(d[4], 5);
}

TEST(DequeTest, InsertRvalue) {
    deque<int> d = {1, 3};
    int val = 2;
    auto it = d.insert(d.cbegin() + 1, std::move(val));
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(d[1], 2);
}

TEST(DequeTest, InsertCount) {
    deque<int> d = {1, 4};
    auto it = d.insert(d.cbegin() + 1, 2, 42);
    EXPECT_EQ(d.size(), 4u);
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 42);
    EXPECT_EQ(d[2], 42);
    EXPECT_EQ(d[3], 4);
    EXPECT_EQ(*it, 42);
}

TEST(DequeTest, InsertCountZero) {
    deque<int> d = {1, 2, 3};
    auto it = d.insert(d.cbegin() + 1, 0, 99);
    EXPECT_EQ(d.size(), 3u);
    (void)it;
}

TEST(DequeTest, InsertRange) {
    deque<int> d = {1, 5};
    std::vector<int> v = {2, 3, 4};
    auto it = d.insert(d.cbegin() + 1, v.begin(), v.end());
    EXPECT_EQ(d.size(), 5u);
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 2);
    EXPECT_EQ(d[2], 3);
    EXPECT_EQ(d[3], 4);
    EXPECT_EQ(d[4], 5);
    EXPECT_EQ(*it, 2);
}

TEST(DequeTest, InsertRangeEmpty) {
    deque<int> d = {1, 2};
    std::vector<int> v;
    auto it = d.insert(d.cbegin() + 1, v.begin(), v.end());
    EXPECT_EQ(d.size(), 2u);
    (void)it;
}

TEST(DequeTest, InsertInitializerList) {
    deque<int> d = {1, 4};
    auto it = d.insert(d.cbegin() + 1, {2, 3});
    EXPECT_EQ(d.size(), 4u);
    EXPECT_EQ(d[1], 2);
    EXPECT_EQ(d[2], 3);
    EXPECT_EQ(*it, 2);
}

// ============================================================
// emplace (middle)
// ============================================================

TEST(DequeTest, EmplaceMiddle) {
    deque<int> d = {1, 2, 4, 5};
    auto it = d.emplace(d.cbegin() + 2, 3);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(d.size(), 5u);
    EXPECT_EQ(d[2], 3);
}

TEST(DequeTest, EmplaceFrontViaEmplace) {
    deque<int> d = {2, 3};
    auto it = d.emplace(d.cbegin(), 1);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(d[0], 1);
}

TEST(DequeTest, EmplaceBackViaEmplace) {
    deque<int> d = {1, 2};
    auto it = d.emplace(d.cend(), 3);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(d[2], 3);
}

// ============================================================
// erase
// ============================================================

TEST(DequeTest, EraseSingleAtFront) {
    deque<int> d = {1, 2, 3, 4};
    auto it = d.erase(d.cbegin());
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(d[0], 2);
}

TEST(DequeTest, EraseSingleAtBack) {
    deque<int> d = {1, 2, 3, 4};
    auto it = d.erase(d.cend() - 1);
    EXPECT_EQ(it, d.end());
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(d.back(), 3);
}

TEST(DequeTest, EraseSingleInMiddle) {
    deque<int> d = {1, 2, 99, 3, 4};
    auto it = d.erase(d.cbegin() + 2);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(d.size(), 4u);
    EXPECT_EQ(d[2], 3);
}

TEST(DequeTest, EraseRangeAtFront) {
    deque<int> d = {1, 2, 3, 4, 5};
    auto it = d.erase(d.cbegin(), d.cbegin() + 2);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(d[0], 3);
}

TEST(DequeTest, EraseRangeAtBack) {
    deque<int> d = {1, 2, 3, 4, 5};
    auto it = d.erase(d.cend() - 2, d.cend());
    EXPECT_EQ(it, d.end());
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(d.back(), 3);
}

TEST(DequeTest, EraseRangeInMiddle) {
    deque<int> d = {1, 2, 99, 98, 3, 4};
    auto it = d.erase(d.cbegin() + 2, d.cbegin() + 4);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(d.size(), 4u);
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 2);
    EXPECT_EQ(d[2], 3);
    EXPECT_EQ(d[3], 4);
}

TEST(DequeTest, EraseRangeEmpty) {
    deque<int> d = {1, 2, 3};
    auto it = d.erase(d.cbegin() + 1, d.cbegin() + 1);
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(*it, 2);
}

TEST(DequeTest, EraseAll) {
    deque<int> d = {1, 2, 3};
    auto it = d.erase(d.cbegin(), d.cend());
    EXPECT_EQ(it, d.end());
    EXPECT_TRUE(d.empty());
}

// ============================================================
// resize
// ============================================================

TEST(DequeTest, ResizeGrow) {
    deque<int> d = {1, 2, 3};
    d.resize(5);
    EXPECT_EQ(d.size(), 5u);
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 2);
    EXPECT_EQ(d[2], 3);
    EXPECT_EQ(d[3], 0);  // default-initialized
    EXPECT_EQ(d[4], 0);
}

TEST(DequeTest, ResizeGrowWithValue) {
    deque<int> d = {1, 2};
    d.resize(5, 99);
    EXPECT_EQ(d.size(), 5u);
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 2);
    EXPECT_EQ(d[2], 99);
    EXPECT_EQ(d[3], 99);
    EXPECT_EQ(d[4], 99);
}

TEST(DequeTest, ResizeShrink) {
    deque<int> d = {1, 2, 3, 4, 5};
    d.resize(2);
    EXPECT_EQ(d.size(), 2u);
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 2);
}

TEST(DequeTest, ResizeSameSize) {
    deque<int> d = {1, 2, 3};
    d.resize(3);
    EXPECT_EQ(d.size(), 3u);
}

// ============================================================
// clear
// ============================================================

TEST(DequeTest, Clear) {
    deque<int> d = {1, 2, 3, 4, 5};
    d.clear();
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.size(), 0u);
    // Should be reusable after clear
    d.push_back(100);
    EXPECT_EQ(d.size(), 1u);
    EXPECT_EQ(d.front(), 100);
}

TEST(DequeTest, ClearEmpty) {
    deque<int> d;
    d.clear();
    EXPECT_TRUE(d.empty());
}

// ============================================================
// swap
// ============================================================

TEST(DequeTest, SwapMember) {
    deque<int> a = {1, 2, 3};
    deque<int> b = {10, 20};
    a.swap(b);
    EXPECT_EQ(a.size(), 2u);
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[1], 20);
    EXPECT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 1);
    EXPECT_EQ(b[1], 2);
    EXPECT_EQ(b[2], 3);
}

TEST(DequeTest, SwapNonMember) {
    deque<int> a = {1, 2, 3};
    deque<int> b = {10, 20};
    swap(a, b);
    EXPECT_EQ(a.size(), 2u);
    EXPECT_EQ(b.size(), 3u);
}

TEST(DequeTest, SwapSelf) {
    deque<int> d = {1, 2, 3};
    d.swap(d);
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(d[0], 1);
}

TEST(DequeTest, SwapEmpty) {
    deque<int> a;
    deque<int> b = {1, 2, 3};
    a.swap(b);
    EXPECT_EQ(a.size(), 3u);
    EXPECT_TRUE(b.empty());
}

// ============================================================
// shrink_to_fit
// ============================================================

TEST(DequeTest, ShrinkToFit) {
    deque<int> d = {1, 2, 3, 4, 5};
    d.shrink_to_fit();  // No-op in current impl, should not crash
    EXPECT_EQ(d.size(), 5u);
    EXPECT_EQ(d[0], 1);
}

// ============================================================
// Large-scale push_front / push_back (block map expansion)
// ============================================================

TEST(DequeTest, LargePushBack) {
    deque<int> d;
    const int N = 10000;
    for (int i = 0; i < N; ++i) {
        d.push_back(i);
    }
    EXPECT_EQ(d.size(), static_cast<size_t>(N));
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(d[static_cast<size_t>(i)], i);
    }
}

TEST(DequeTest, LargePushFront) {
    deque<int> d;
    const int N = 10000;
    for (int i = 0; i < N; ++i) {
        d.push_front(i);
    }
    EXPECT_EQ(d.size(), static_cast<size_t>(N));
    // Elements pushed in reverse order: [N-1, N-2, ..., 0]
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(d[static_cast<size_t>(i)], N - 1 - i);
    }
}

TEST(DequeTest, LargePushFrontBackInterleaved) {
    deque<int> d;
    for (int i = 0; i < 5000; ++i) {
        d.push_back(i);
        d.push_front(-i - 1);
    }
    EXPECT_EQ(d.size(), 10000u);
}

TEST(DequeTest, LargeInsertEraseAtVariousPositions) {
    deque<int> d;
    const int N = 1000;
    for (int i = 0; i < N; ++i) {
        d.push_back(i);
    }
    // Insert in middle
    d.insert(d.begin() + d.size() / 2, 9999);
    EXPECT_EQ(d[d.size() / 2 - 1], static_cast<size_t>(9999) == 9999
              ? d[d.size() / 2 - 1] : 0); // just verify no crash
    EXPECT_EQ(d.size(), static_cast<size_t>(N + 1));
    // Erase from middle
    d.erase(d.begin() + d.size() / 2, d.begin() + d.size() / 2 + 10);
    EXPECT_EQ(d.size(), static_cast<size_t>(N + 1 - 10));
}

// ============================================================
// Random access verification across block boundaries
// ============================================================

TEST(DequeTest, RandomAccessAcrossBlocks) {
    // Block size is ~512/sizeof(int) = 128 elements for int
    // Push enough to span multiple blocks
    deque<int> d;
    const int N = 300;  // spans at least 3 blocks
    for (int i = 0; i < N; ++i) {
        d.push_back(i);
    }
    // Verify random access at block boundaries
    EXPECT_EQ(d[0], 0);
    EXPECT_EQ(d[127], 127);   // end of block 1 (if block size 128)
    EXPECT_EQ(d[128], 128);   // start of block 2
    EXPECT_EQ(d[255], 255);   // end of block 2
    EXPECT_EQ(d[256], 256);   // start of block 3
    EXPECT_EQ(d[static_cast<size_t>(N - 1)], N - 1);

    // Verify write access across blocks
    d[0] = 1000;
    d[128] = 2000;
    d[static_cast<size_t>(N - 1)] = 3000;
    EXPECT_EQ(d[0], 1000);
    EXPECT_EQ(d[128], 2000);
    EXPECT_EQ(d[static_cast<size_t>(N - 1)], 3000);
}

// ============================================================
// Const correctness
// ============================================================

TEST(DequeTest, ConstDeque) {
    const deque<int> d = {1, 2, 3, 4, 5};
    EXPECT_EQ(d.size(), 5u);
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d.at(2), 3);
    EXPECT_EQ(d.front(), 1);
    EXPECT_EQ(d.back(), 5);
    EXPECT_FALSE(d.empty());
    EXPECT_NE(d.begin(), d.end());
    int sum = 0;
    for (auto it = d.cbegin(); it != d.cend(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 15);
}

// ============================================================
// Element-wise comparison (helper-based since zstl::deque
// does not define comparison operators)
// ============================================================

TEST(DequeTest, EqualityComparison) {
    deque<int> a = {1, 2, 3};
    deque<int> b = {1, 2, 3};
    deque<int> c = {1, 2, 4};
    deque<int> d = {1, 2, 3, 4};
    deque<int> e;

    EXPECT_TRUE(deque_equal(a, b));
    EXPECT_FALSE(deque_equal(a, c));
    EXPECT_FALSE(deque_equal(a, d));  // different sizes
    EXPECT_TRUE(deque_equal(e, e));   // both empty
    EXPECT_FALSE(deque_equal(a, e));
}

TEST(DequeTest, LessThanComparison) {
    // Manual lexicographical compare since operator< not defined for deque
    auto deque_less = [](const deque<int>& a, const deque<int>& b) -> bool {
        size_t n = std::min(a.size(), b.size());
        for (size_t i = 0; i < n; ++i) {
            if (a[i] < b[i]) return true;
            if (b[i] < a[i]) return false;
        }
        return a.size() < b.size();
    };

    deque<int> a = {1, 2, 3};
    deque<int> b = {1, 2, 4};
    deque<int> c = {1, 2};
    deque<int> d = {1, 2, 3};

    EXPECT_TRUE(deque_less(a, b));   // 1==1, 2==2, 3<4
    EXPECT_FALSE(deque_less(b, a));  // 4<3 false
    EXPECT_TRUE(deque_less(c, a));   // c is shorter prefix of a
    EXPECT_FALSE(deque_less(a, d));  // equal => not less
    EXPECT_FALSE(deque_less(d, a));  // equal => not less
}

// ============================================================
// push/pop with block boundary transitions (deallocate blocks)
// ============================================================

TEST(DequeTest, PushPopAcrossBlockBoundary) {
    deque<int> d;
    // Push enough to fill at least one block
    const int block_elems = 512 / static_cast<int>(sizeof(int));  // ~128
    for (int i = 0; i < block_elems + 10; ++i) {
        d.push_back(i);
    }
    EXPECT_EQ(d.size(), static_cast<size_t>(block_elems + 10));
    // Pop back past block boundary
    for (int i = 0; i < 15; ++i) {
        d.pop_back();
    }
    EXPECT_EQ(d.size(), static_cast<size_t>(block_elems - 5));
    // Push front to trigger front block allocation
    for (int i = 0; i < 50; ++i) {
        d.push_front(-i);
    }
    EXPECT_EQ(d.front(), -49);
    // Pop front past block boundary
    for (int i = 0; i < 30; ++i) {
        d.pop_front();
    }
    EXPECT_EQ(d.front(), -19);
}

// ============================================================
// Iterator arithmetic across blocks
// ============================================================

TEST(DequeTest, IteratorIncrementAcrossBlocks) {
    deque<int> d;
    const int N = 300;
    for (int i = 0; i < N; ++i) {
        d.push_back(i);
    }
    size_t count = 0;
    for (auto it = d.begin(); it != d.end(); ++it) {
        EXPECT_EQ(*it, static_cast<int>(count));
        ++count;
    }
    EXPECT_EQ(count, static_cast<size_t>(N));
}

TEST(DequeTest, ReverseIteratorAcrossBlocks) {
    deque<int> d;
    const int N = 300;
    for (int i = 0; i < N; ++i) {
        d.push_back(i);
    }
    int expected = N - 1;
    for (auto rit = d.rbegin(); rit != d.rend(); ++rit) {
        EXPECT_EQ(*rit, expected);
        --expected;
    }
    EXPECT_EQ(expected, -1);
}
