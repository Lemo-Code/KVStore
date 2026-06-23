#include <gtest/gtest.h>
#include <lstl/container/deque.h>

TEST(LstlDequeTest, BothEnds) {
    lstl::deque<int> dq;
    dq.push_back(2);
    dq.push_front(1);
    dq.push_back(3);
    EXPECT_EQ(dq.front(), 1);
    EXPECT_EQ(dq.back(), 3);
    dq.pop_front();
    EXPECT_EQ(dq.front(), 2);
}

TEST(LstlDequeTest, RandomAccess) {
    lstl::deque<int> dq;
    for (int i = 0; i < 20; ++i) dq.push_back(i);
    EXPECT_EQ(dq[10], 10);
    dq[10] = 999;
    EXPECT_EQ(dq[10], 999);
}

TEST(LstlDequeTest, IteratorTraversal) {
    lstl::deque<int> dq;
    dq.push_back(5);
    dq.push_back(6);
    dq.push_back(7);
    int expected = 5;
    for (auto x : dq) {
        EXPECT_EQ(x, expected++);
    }
}
