#include <gtest/gtest.h>
#include <lstl/memory/construct.h>
#include <lstl/memory/uninitialized.h>
#include <lstl/memory/allocator.h>

struct NonTrivial {
    int v;
    NonTrivial(int x) : v(x) {}
};

TEST(LstlConstructTest, ConstructDestroy) {
    lstl::allocator<NonTrivial> alloc;
    NonTrivial* p = alloc.allocate(1);
    lstl::construct(p, 99);
    EXPECT_EQ(p->v, 99);
    lstl::destroy(p);
    alloc.deallocate(p, 1);
}

TEST(LstlConstructTest, UninitializedFill) {
    lstl::allocator<int> alloc;
    int* buf = alloc.allocate(5);
    lstl::uninitialized_fill(buf, buf + 5, 7);
    for (int i = 0; i < 5; ++i) EXPECT_EQ(buf[i], 7);
    lstl::destroy(buf, buf + 5);
    alloc.deallocate(buf, 5);
}
