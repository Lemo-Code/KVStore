#include <gtest/gtest.h>
#include <lstl/container/list.h>

TEST(LstlListTest, PushFrontBack) {
    lstl::list<int> lst;
    lst.push_back(2);
    lst.push_front(1);
    lst.push_back(3);
    auto it = lst.begin();
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(*it++, 2);
    EXPECT_EQ(*it, 3);
}

TEST(LstlListTest, Erase) {
    lstl::list<int> lst{1, 2, 3, 4};
    auto it = lst.begin();
    ++it;
    lst.erase(it);
    int sum = 0;
    for (auto x : lst) sum += x;
    EXPECT_EQ(sum, 8); // 1+3+4
}

TEST(LstlListTest, SizeAndEmpty) {
    lstl::list<int> lst;
    EXPECT_TRUE(lst.empty());
    lst.push_back(42);
    EXPECT_EQ(lst.size(), 1u);
}
