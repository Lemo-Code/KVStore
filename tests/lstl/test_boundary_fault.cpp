#include <gtest/gtest.h>
#include <lstl/container/vector.h>
#include <lstl/container/deque.h>
#include <lstl/container/list.h>
#include <lstl/container/map.h>
#include <lstl/container/unordered_map.h>
#include <lstl/memory/construct.h>
#include <lstl/memory/uninitialized.h>

namespace {

struct MoveOnly {
    int v;
    explicit MoveOnly(int x = 0) : v(x) {}
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&& o) noexcept : v(o.v) { o.v = -1; }
    MoveOnly& operator=(MoveOnly&& o) noexcept {
        v = o.v;
        o.v = -1;
        return *this;
    }
};

} // namespace

// ---- vector 边界 ----

TEST(VectorBoundaryFaultTest, EmptyContainerQueries) {
    lstl::vector<int> v;
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
    EXPECT_EQ(v.capacity(), 0u);
    v.reserve(0);
    EXPECT_EQ(v.capacity(), 0u);
}

TEST(VectorBoundaryFaultTest, SingleElementErase) {
    lstl::vector<int> v;
    v.push_back(42);
    v.pop_back();
    EXPECT_TRUE(v.empty());
    v.push_back(1);
    v.erase(v.begin());
    EXPECT_TRUE(v.empty());
}

TEST(VectorBoundaryFaultTest, InsertAtBeginAndEnd) {
    lstl::vector<int> v;
    v.insert(v.begin(), 1);
    v.insert(v.end(), 2);
    ASSERT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
}

TEST(VectorBoundaryFaultTest, ResizeShrinkGrow) {
    lstl::vector<int> v(10, 7);
    v.resize(3);
    EXPECT_EQ(v.size(), 3u);
    v.resize(6, 9);
    EXPECT_EQ(v.size(), 6u);
    EXPECT_EQ(v[5], 9);
}

// ---- deque 边界 ----

TEST(DequeBoundaryFaultTest, EmptyAfterClear) {
    lstl::deque<int> dq;
    for (int i = 0; i < 100; ++i) dq.push_back(i);
    dq.clear();
    EXPECT_TRUE(dq.empty());
    dq.push_front(1);
    EXPECT_EQ(dq.front(), 1);
}

TEST(DequeBoundaryFaultTest, PopBothEnds) {
    lstl::deque<int> dq;
    dq.push_back(1);
    dq.push_back(2);
    dq.pop_front();
    dq.pop_back();
    EXPECT_TRUE(dq.empty());
}

// ---- list 边界 ----

TEST(ListBoundaryFaultTest, SpliceAndRemove) {
    lstl::list<int> a, b;
    a.push_back(1);
    a.push_back(2);
    b.push_back(99);
    a.splice(a.end(), b);
    EXPECT_EQ(a.size(), 3u);
    EXPECT_TRUE(b.empty());
    a.remove(2);
    EXPECT_EQ(a.size(), 2u);
}

TEST(ListBoundaryFaultTest, ReverseAndUnique) {
    lstl::list<int> lst;
    lst.push_back(1);
    lst.push_back(1);
    lst.push_back(2);
    lst.unique();
    EXPECT_EQ(lst.size(), 2u);
    lst.reverse();
    EXPECT_EQ(lst.front(), 2);
}

// ---- map / unordered_map 边界 ----

TEST(MapBoundaryFaultTest, DuplicateInsertAndEraseMissing) {
    lstl::map<int, int> m;
    auto r1 = m.insert(lstl::make_pair(1, 10));
    auto r2 = m.insert(lstl::make_pair(1, 20));
    EXPECT_FALSE(r2.second);
    EXPECT_EQ(r1.first->second, 10);
    EXPECT_EQ(m.erase(999), 0u);
}

TEST(UnorderedMapBoundaryFaultTest, RehashUnderLoad) {
    lstl::unordered_map<int, int> m;
    for (int i = 0; i < 500; ++i) m[i] = i * 2;
    EXPECT_EQ(m.size(), 500u);
    EXPECT_EQ(m.find(499)->second, 998);
    m.erase(250);
    EXPECT_EQ(m.find(250), m.end());
}

// ---- 内存 / 构造故障边界 ----

TEST(MemoryBoundaryFaultTest, UninitializedFillZero) {
    lstl::allocator<int> alloc;
    int* p = alloc.allocate(4);
    lstl::uninitialized_fill(p, p + 4, 0);
    for (int i = 0; i < 4; ++i) EXPECT_EQ(p[i], 0);
    lstl::destroy(p, p + 4);
    alloc.deallocate(p, 4);
}

TEST(MemoryBoundaryFaultTest, ConstructDestroySingle) {
    lstl::allocator<MoveOnly> alloc;
    MoveOnly* p = alloc.allocate(1);
    lstl::construct(p, 7);
    EXPECT_EQ(p->v, 7);
    lstl::destroy(p);
    alloc.deallocate(p, 1);
}
