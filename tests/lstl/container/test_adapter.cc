#include "lstl_test_common.h"

#include <cstdio>

#include "container.h"

int main() {
  {
    lstl::queue<int> q;
    LSTL_CHECK(q.empty());
    q.push(1);
    q.push(2);
    q.push(3);
    LSTL_CHECK(q.size() == 3);
    LSTL_CHECK(q.front() == 1);
    LSTL_CHECK(q.back() == 3);
    q.pop();
    LSTL_CHECK(q.front() == 2);
    q.pop();
    q.pop();
    LSTL_CHECK(q.empty());
  }

  {
    lstl::queue<int, lstl::deque<int> > q;
    q.emplace(10);
    q.emplace(20);
    LSTL_CHECK(q.size() == 2);
    LSTL_CHECK(q.front() == 10);
    q.pop();
    LSTL_CHECK(q.front() == 20);

    const int data[] = {1, 2, 3};
    lstl::queue<int> q2;
    for (size_t i = 0; i < 3; ++i) {
      q2.push(data[i]);
    }
    LSTL_CHECK(q2.size() == 3);
    LSTL_CHECK(q2.back() == 3);
  }

  {
    lstl::stack<int> s;
    s.push(1);
    s.push(2);
    s.push(3);
    LSTL_CHECK(s.size() == 3);
    LSTL_CHECK(s.top() == 3);
    s.pop();
    LSTL_CHECK(s.top() == 2);
    s.pop();
    s.pop();
    LSTL_CHECK(s.empty());
  }

  {
    lstl::stack<int, lstl::deque<int> > s;
    s.emplace(42);
    LSTL_CHECK(s.top() == 42);
    s.push(100);
    LSTL_CHECK(s.top() == 100);
  }

  {
    const int data[] = {3, 1, 4, 1, 5, 9, 2, 6};
    lstl::priority_queue<int> pq(data, data + 8);
    LSTL_CHECK(pq.size() == 8);
    LSTL_CHECK(pq.top() == 9);
    pq.pop();
    LSTL_CHECK(pq.top() == 6);
    pq.pop();
    LSTL_CHECK(pq.top() == 5);
    pq.push(10);
    LSTL_CHECK(pq.top() == 10);
    while (!pq.empty()) {
      pq.pop();
    }
    LSTL_CHECK(pq.empty());
  }

  {
    const int data[] = {3, 1, 4, 2};
    lstl::priority_queue<int, lstl::vector<int>, lstl::greater<int> > pq(
        data, data + 4, lstl::greater<int>());
    LSTL_CHECK(pq.top() == 1);
    pq.pop();
    LSTL_CHECK(pq.top() == 2);
    pq.pop();
    pq.pop();
    LSTL_CHECK(pq.top() == 4);
  }

  {
    lstl::queue<int> a;
    a.push(1);
    a.push(2);
    lstl::queue<int> b;
    b.push(1);
    b.push(2);
    LSTL_CHECK(a == b);
    b.push(3);
    LSTL_CHECK(a != b);
    LSTL_CHECK(a < b);
    LSTL_CHECK(b > a);
  }

  {
    lstl::stack<int> sa;
    sa.push(1);
    sa.push(2);
    lstl::stack<int> sb;
    sb.push(1);
    sb.push(2);
    LSTL_CHECK(sa == sb);
    sb.push(3);
    LSTL_CHECK(sa != sb);
  }

  std::printf("PASS test_adapter\n");
  return 0;
}
