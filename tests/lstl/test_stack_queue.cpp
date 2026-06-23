#include <gtest/gtest.h>
#include <lstl/container/stack.h>
#include <lstl/container/queue.h>
#include <lstl/container/priority_queue.h>

TEST(LstlStackQueueTest, StackLIFO) {
    lstl::stack<int> st;
    st.push(1);
    st.push(2);
    EXPECT_EQ(st.top(), 2);
    st.pop();
    EXPECT_EQ(st.top(), 1);
}

TEST(LstlStackQueueTest, QueueFIFO) {
    lstl::queue<int> q;
    q.push(1);
    q.push(2);
    EXPECT_EQ(q.front(), 1);
    q.pop();
    EXPECT_EQ(q.front(), 2);
}

TEST(LstlStackQueueTest, PriorityQueueMaxHeap) {
    lstl::priority_queue<int> pq;
    pq.push(3);
    pq.push(1);
    pq.push(5);
    EXPECT_EQ(pq.top(), 5);
    pq.pop();
    EXPECT_EQ(pq.top(), 3);
}
