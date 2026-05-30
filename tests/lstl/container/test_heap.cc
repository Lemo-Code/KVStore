#include "lstl_test_common.h"

#include <cstdio>

#include "container.h"

namespace {

void check_max_heap(const lstl::heap<int>& h) {
  const lstl::heap<int>& ch = h;
  for (lstl::heap<int>::const_iterator it = ch.begin(); it != ch.end(); ++it) {
    const size_t i = static_cast<size_t>(it - ch.begin());
    const size_t left = 2 * i + 1;
    const size_t right = 2 * i + 2;
    if (left < ch.size()) {
      LSTL_CHECK(!lstl::less<int>()(*it, ch.container()[left]));
    }
    if (right < ch.size()) {
      LSTL_CHECK(!lstl::less<int>()(*it, ch.container()[right]));
    }
  }
}

}  // namespace

int main() {
  {
    lstl::heap<int> h;
    LSTL_CHECK(h.empty());
    LSTL_CHECK(h.size() == 0);

    h.push(3);
    h.push(1);
    h.push(4);
    h.push(1);
    h.push(5);
    LSTL_CHECK(h.size() == 5);
    LSTL_CHECK(h.top() == 5);
    check_max_heap(h);

    h.pop();
    LSTL_CHECK(h.top() == 4);
    h.pop();
    LSTL_CHECK(h.top() == 3);
    h.emplace(10);
    LSTL_CHECK(h.top() == 10);

    h.clear();
    LSTL_CHECK(h.empty());
  }

  {
    lstl::heap<int> h;
    h.push(5);
    h.push(3);
    h.push(7);
    LSTL_CHECK(*h.begin() == h.top());

    const int data[] = {1, 9, 2, 8, 3};
    lstl::heap<int> h2(data, data + 5);
    LSTL_CHECK(h2.size() == 5);
    LSTL_CHECK(h2.top() == 9);
    check_max_heap(h2);
  }

  {
    lstl::heap<int, lstl::greater<int> > h;
    h.push(3);
    h.push(1);
    h.push(4);
    LSTL_CHECK(h.top() == 1);
  }

  {
    lstl::heap<int> h;
    const int data[] = {3, 1, 4, 1, 5, 9, 2, 6};
    for (size_t i = 0; i < sizeof(data) / sizeof(data[0]); ++i) {
      h.push(data[i]);
    }
    h.sort();
    for (size_t i = 1; i < h.size(); ++i) {
      LSTL_CHECK(h.container()[i - 1] <= h.container()[i]);
    }
  }

  {
    lstl::heap<int> a;
    a.push(1);
    a.push(5);
    lstl::heap<int> b;
    b.swap(a);
    LSTL_CHECK(b.top() == 5);
    LSTL_CHECK(a.empty());
  }

  {
    lstl::priority_queue<int> pq;
    pq.push(2);
    pq.push(8);
    pq.push(1);
    LSTL_CHECK(pq.top() == 8);
    pq.pop();
    LSTL_CHECK(pq.top() == 2);

    const int data[] = {3, 1, 4, 1, 5};
    lstl::priority_queue<int> pq2(data, data + 5);
    LSTL_CHECK(pq2.top() == 5);
    pq2.pop();
    LSTL_CHECK(pq2.top() == 4);
  }

  std::printf("PASS test_heap\n");
  return 0;
}
