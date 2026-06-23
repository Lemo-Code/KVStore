#include <gtest/gtest.h>
#include <lstl/memory/allocator.h>
#include <lstl/memory/pool.h>
#include <lstl/container/vector.h>

TEST(LstlAllocTest, AllocatorConstructDestroy) {
    lstl::allocator<int> alloc;
    int* p = alloc.allocate(10);
    ASSERT_NE(p, nullptr);
    for (int i = 0; i < 10; ++i)
        lstl::construct(p + i, i);
    for (int i = 0; i < 10; ++i)
        EXPECT_EQ(p[i], i);
    for (int i = 0; i < 10; ++i)
        lstl::destroy(p + i);
    alloc.deallocate(p, 10);
}

TEST(LstlAllocTest, VectorWithCustomAllocator) {
    lstl::vector<int, lstl::allocator<int>> v;
    for (int i = 0; i < 50; ++i) v.push_back(i);
    EXPECT_EQ(v.size(), 50u);
}

TEST(LstlAllocTest, PoolSingle) {
    lstl::pool_single pool(64, 4);
    void* a = pool.allocate();
    void* b = pool.allocate();
    EXPECT_NE(a, nullptr);
    EXPECT_NE(b, nullptr);
    pool.deallocate(a);
    pool.deallocate(b);
}
