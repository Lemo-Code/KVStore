#include <gtest/gtest.h>
#include <lstl/container/vector.h>
#include <lstl/container/list.h>
#include <lstl/container/deque.h>
#include <lstl/container/unordered_map.h>
#include <lstl/memory/pool.h>
#include <lstl/memory/uninitialized.h>

// lstl 已知缺陷 / 边界回归 — 对应 BUG_AUDIT.md

TEST(LstlBugRegressionTest, ListCopyConstructorPreservesSize) {
    lstl::list<int> src;
    src.push_back(1);
    src.push_back(2);
    src.push_back(3);
    lstl::list<int> copy(src);
    EXPECT_EQ(copy.size(), src.size());
    EXPECT_EQ(copy.size(), 3u);
    int sum = 0;
    for (auto x : copy) sum += x;
    EXPECT_EQ(sum, 6);
}

TEST(LstlBugRegressionTest, ListMoveConstructorEmptiesSource) {
    lstl::list<int> src;
    src.push_back(10);
    src.push_back(20);
    lstl::list<int> moved(std::move(src));
    EXPECT_EQ(moved.size(), 2u);
    EXPECT_TRUE(src.empty());
}

TEST(LstlBugRegressionTest, DequeClearThenReuse) {
    lstl::deque<int> d;
    for (int i = 0; i < 500; ++i)
        d.push_back(i);
    d.clear();
    EXPECT_TRUE(d.empty());
    for (int i = 0; i < 100; ++i)
        d.push_back(i);
    EXPECT_EQ(d.size(), 100u);
    EXPECT_EQ(d[99], 99);
}

TEST(LstlBugRegressionTest, UnorderedMapEraseByKey) {
    lstl::unordered_map<int, int> m;
    for (int i = 0; i < 100; ++i) m[i] = i;
    EXPECT_EQ(m.erase(50), 1u);
    EXPECT_EQ(m.find(50), m.end());
    EXPECT_EQ(m.size(), 99u);
}

TEST(LstlBugRegressionTest, UnorderedMapEraseDoesNotLeakEntry) {
    lstl::unordered_map<int, int> m;
    m[1] = 1;
    m[2] = 2;
    EXPECT_EQ(m.erase(1), 1u);
    EXPECT_EQ(m.find(1), m.end());
    EXPECT_EQ(m.size(), 1u);
    EXPECT_NE(m.find(2), m.end());
}

TEST(LstlBugRegressionTest, VectorMovePushBackKnownDegradation) {
    lstl::vector<lstl::vector<int>> outer;
    outer.push_back(lstl::vector<int>{1, 2, 3});
    lstl::vector<int> inner;
    inner.push_back(9);
    outer.push_back(std::move(inner));
    EXPECT_EQ(outer.size(), 2u);
    EXPECT_EQ(outer.back()[0], 9);
    // BUG_AUDIT #9: move 可能退化为拷贝，此处只验证不崩溃
}

TEST(LstlBugRegressionTest, PoolAllocateDeallocateCycle) {
    lstl::pool_single pool(128, 8);
    void* a = pool.allocate();
    void* b = pool.allocate();
    void* c = pool.allocate();
    pool.deallocate(b);
    pool.deallocate(a);
    pool.deallocate(c);
    void* d = pool.allocate();
    EXPECT_NE(d, nullptr);
    pool.deallocate(d);
}

TEST(LstlBugRegressionTest, UninitializedFillOnPOD) {
    lstl::allocator<int> alloc;
    int* buf = alloc.allocate(8);
    lstl::uninitialized_fill(buf, buf + 8, 42);
    for (int i = 0; i < 8; ++i)
        EXPECT_EQ(buf[i], 42);
    lstl::destroy(buf, buf + 8);
    alloc.deallocate(buf, 8);
}
