#include "lstl_test_common.h"

#include <cstdio>

#include "container.h"

namespace {

struct Counter {
  static int alive;
  int id;

  Counter() : id(-1) { ++alive; }
  explicit Counter(int x) : id(x) { ++alive; }
  Counter(const Counter& o) : id(o.id) { ++alive; }
  Counter(Counter&& o) throw() : id(o.id) { o.id = -2; ++alive; }
  ~Counter() { --alive; }

  Counter& operator=(const Counter& o) {
    id = o.id;
    return *this;
  }

  Counter& operator=(Counter&& o) throw() {
    id = o.id;
    o.id = -2;
    return *this;
  }
};

int Counter::alive = 0;

lstl::deque<int> make_int_deque(const int* data, size_t n) {
  return lstl::deque<int>(data, data + n);
}

}  // namespace

int main() {
  {
    lstl::deque<int> d;
    LSTL_CHECK(d.empty());
    LSTL_CHECK(d.size() == 0);
  }

  {
    lstl::deque<int> d(5, 42);
    LSTL_CHECK(d.size() == 5);
    for (size_t i = 0; i < d.size(); ++i) {
      LSTL_CHECK(d[i] == 42);
    }
  }

  {
    const int data[] = {1, 2, 3, 4, 5};
    lstl::deque<int> d = make_int_deque(data, 5);
    LSTL_CHECK(d.size() == 5);
    LSTL_CHECK(d.front() == 1);
    LSTL_CHECK(d.back() == 5);
    LSTL_CHECK(d[2] == 3);
  }

  {
    const int data[] = {1, 2, 3};
    lstl::deque<int> a = make_int_deque(data, 3);
    lstl::deque<int> b(a);
    LSTL_CHECK(b == a);
    lstl::deque<int> c;
    c = a;
    LSTL_CHECK(c == a);
  }

  {
    const int data[] = {1, 2, 3};
    lstl::deque<int> a = make_int_deque(data, 3);
    lstl::deque<int> b(lstl::move(a));
    LSTL_CHECK(b.size() == 3);
    LSTL_CHECK(a.empty());
  }

  {
    lstl::deque<int> d;
    d.push_back(1);
    d.push_back(2);
    d.push_front(0);
    LSTL_CHECK(d.size() == 3);
    LSTL_CHECK(d.front() == 0);
    LSTL_CHECK(d.back() == 2);
    d.pop_back();
    LSTL_CHECK(d.size() == 2);
    LSTL_CHECK(d.back() == 1);
    d.pop_front();
    LSTL_CHECK(d.size() == 1);
    LSTL_CHECK(d.front() == 1);
  }

  {
    lstl::deque<int> d;
    for (int i = 0; i < 100; ++i) {
      if (i % 2 == 0) {
        d.push_back(i);
      } else {
        d.push_front(i);
      }
    }
    LSTL_CHECK(d.size() == 100);
    LSTL_CHECK(d.front() == 99);
    LSTL_CHECK(d.back() == 98);
  }

  {
    const int data[] = {1, 2, 3, 4, 5};
    lstl::deque<int> d = make_int_deque(data, 5);
    lstl::deque<int>::iterator it = d.begin();
    ++it;
    ++it;
    d.insert(it, 99);
    LSTL_CHECK(d.size() == 6);
    LSTL_CHECK(d[2] == 99);
    d.insert(d.end(), 3, 7);
    LSTL_CHECK(d.size() == 9);
    LSTL_CHECK(d.back() == 7);
    it = d.begin();
    ++it;
    d.erase(it);
    LSTL_CHECK(d.size() == 8);
  }

  {
    const int data[] = {1, 2, 3};
    lstl::deque<int> d = make_int_deque(data, 3);
    d.resize(5, 0);
    LSTL_CHECK(d.size() == 5);
    LSTL_CHECK(d[4] == 0);
    d.resize(2);
    LSTL_CHECK(d.size() == 2);
    d.clear();
    LSTL_CHECK(d.empty());
  }

  {
    const int da[] = {1, 2};
    const int db[] = {3, 4};
    lstl::deque<int> a = make_int_deque(da, 2);
    lstl::deque<int> b = make_int_deque(db, 2);
    a.swap(b);
    LSTL_CHECK(a.front() == 3);
    LSTL_CHECK(b.front() == 1);
    lstl::swap(a, b);
    LSTL_CHECK(a.front() == 1);
  }

  {
    lstl::deque<Counter> d;
    d.emplace_back(1);
    d.emplace_back(2);
    LSTL_CHECK(d.size() == 2);
    LSTL_CHECK(Counter::alive == 2);
    d.clear();
    LSTL_CHECK(Counter::alive == 0);
  }

  {
    const int data[] = {1, 2, 3, 4};
    lstl::deque<int> d = make_int_deque(data, 4);
    LSTL_CHECK(lstl::distance(d.begin(), d.end()) == 4);
    LSTL_CHECK(*d.rbegin() == 4);
  }

  std::printf("PASS test_deque\n");
  return 0;
}
