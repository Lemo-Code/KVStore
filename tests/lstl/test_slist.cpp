#include <gtest/gtest.h>
#include <lstl/container/slist.h>

TEST(LstlSlistTest, PushPopFront) {
    lstl::slist<int> sl;
    sl.push_front(2);
    sl.push_front(1);
    EXPECT_EQ(sl.front(), 1);
    sl.pop_front();
    EXPECT_EQ(sl.front(), 2);
}

TEST(LstlSlistTest, SizeAfterInsert) {
    lstl::slist<std::string> sl;
    sl.push_front("a");
    sl.push_front("b");
    EXPECT_EQ(sl.size(), 2u);
}
