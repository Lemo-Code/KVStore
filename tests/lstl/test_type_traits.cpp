#include <gtest/gtest.h>
#include <lstl/memory/type_traits.h>
#include <lstl/memory/utility.h>

struct NonTrivial {
    int v;
    NonTrivial() : v(0) {}
    explicit NonTrivial(int x) : v(x) {}
};

TEST(LstlTypeTraitsTest, BasicTraits) {
    EXPECT_TRUE((lstl::is_trivially_copyable<int>::value));
    EXPECT_TRUE((lstl::is_same<int, int>::value));
    EXPECT_FALSE((lstl::is_same<int, double>::value));
}

TEST(LstlTypeTraitsTest, MoveForward) {
    int x = 42;
    int&& rref = lstl::move(x);
    EXPECT_EQ(rref, 42);
    EXPECT_EQ(lstl::forward<int&>(x), 42);
}
