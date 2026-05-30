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

lstl::list<int> make_int_list(const int* data, size_t n) {
  return lstl::list<int>(data, data + n);
}

}  // namespace

int main() {
  {
    lstl::list<int> l;
    LSTL_CHECK(l.empty());
    LSTL_CHECK(l.size() == 0);
  }

  {
    lstl::list<int> l(5, 42);
    LSTL_CHECK(l.size() == 5);
    for (lstl::list<int>::iterator it = l.begin(); it != l.end(); ++it) {
      LSTL_CHECK(*it == 42);
    }
  }

  {
    const int data[] = {1, 2, 3, 4, 5};
    lstl::list<int> l = make_int_list(data, 5);
    LSTL_CHECK(l.size() == 5);
    LSTL_CHECK(l.front() == 1);
    LSTL_CHECK(l.back() == 5);
    lstl::list<int>::iterator it = l.begin();
    LSTL_CHECK(*it == 1);
    ++it;
    LSTL_CHECK(*it == 2);
  }

  {
    const int data[] = {1, 2, 3};
    lstl::list<int> a = make_int_list(data, 3);
    lstl::list<int> b(a);
    LSTL_CHECK(b == a);
    lstl::list<int> c;
    c = a;
    LSTL_CHECK(c == a);
  }

  {
    const int data[] = {1, 2, 3};
    lstl::list<int> a = make_int_list(data, 3);
    lstl::list<int> b(lstl::move(a));
    LSTL_CHECK(b.size() == 3);
    LSTL_CHECK(a.empty());
  }

  {
    lstl::list<int> l;
    l.push_back(1);
    l.push_back(2);
    l.push_front(0);
    LSTL_CHECK(l.size() == 3);
    LSTL_CHECK(l.front() == 0);
    LSTL_CHECK(l.back() == 2);
    l.pop_back();
    LSTL_CHECK(l.size() == 2);
    LSTL_CHECK(l.back() == 1);
    l.pop_front();
    LSTL_CHECK(l.size() == 1);
    LSTL_CHECK(l.front() == 1);
  }

  {
    const int data[] = {1, 2, 3, 4, 5};
    lstl::list<int> l = make_int_list(data, 5);
    lstl::list<int>::iterator it = l.begin();
    ++it;
    ++it;
    l.insert(it, 99);
    LSTL_CHECK(l.size() == 6);
    lstl::list<int>::iterator check = l.begin();
    ++check;
    ++check;
    LSTL_CHECK(*check == 99);
    l.insert(l.end(), 3, 7);
    LSTL_CHECK(l.size() == 9);
    LSTL_CHECK(l.back() == 7);
    it = l.begin();
    ++it;
    l.erase(it);
    LSTL_CHECK(l.size() == 8);
  }

  {
    const int data[] = {1, 2, 3};
    lstl::list<int> l = make_int_list(data, 3);
    l.resize(5, 0);
    LSTL_CHECK(l.size() == 5);
    l.resize(2);
    LSTL_CHECK(l.size() == 2);
    l.clear();
    LSTL_CHECK(l.empty());
  }

  {
    const int data[] = {1, 2};
    const int db[] = {3, 4};
    lstl::list<int> a = make_int_list(data, 2);
    lstl::list<int> b = make_int_list(db, 2);
    a.swap(b);
    LSTL_CHECK(a.front() == 3);
    LSTL_CHECK(b.front() == 1);
    lstl::swap(a, b);
    LSTL_CHECK(a.front() == 1);
  }

  {
    lstl::list<Counter> l;
    l.emplace_back(1);
    l.emplace_back(2);
    LSTL_CHECK(l.size() == 2);
    LSTL_CHECK(Counter::alive == 2);
    l.clear();
    LSTL_CHECK(Counter::alive == 0);
  }

  {
    const int da[] = {1, 2, 3};
    const int db[] = {10, 20};
    lstl::list<int> a = make_int_list(da, 3);
    lstl::list<int> b = make_int_list(db, 2);
    a.splice(a.end(), b);
    LSTL_CHECK(a.size() == 5);
    LSTL_CHECK(b.empty());
    LSTL_CHECK(a.back() == 20);
  }

  {
    const int data[] = {1, 2, 3, 4};
    lstl::list<int> l = make_int_list(data, 4);
    LSTL_CHECK(lstl::distance(l.begin(), l.end()) == 4);
    LSTL_CHECK(*l.rbegin() == 4);
  }

  {
    const int data[] = {5, 1, 4, 2, 3};
    lstl::list<int> l = make_int_list(data, 5);
    l.sort();
    LSTL_CHECK(l.front() == 1);
    LSTL_CHECK(l.back() == 5);
    int expect = 1;
    for (lstl::list<int>::iterator it = l.begin(); it != l.end(); ++it, ++expect) {
      LSTL_CHECK(*it == expect);
    }
  }

  {
    const int data[] = {1, 2, 3};
    lstl::list<int> l = make_int_list(data, 3);
    l.reverse();
    LSTL_CHECK(l.front() == 3);
    LSTL_CHECK(l.back() == 1);
  }

  {
    const int da[] = {1, 3, 5};
    const int db[] = {2, 4, 6};
    lstl::list<int> a = make_int_list(da, 3);
    lstl::list<int> b = make_int_list(db, 3);
    a.merge(b);
    LSTL_CHECK(b.empty());
    LSTL_CHECK(a.size() == 6);
    int expect = 1;
    for (lstl::list<int>::iterator it = a.begin(); it != a.end(); ++it, ++expect) {
      LSTL_CHECK(*it == expect);
    }
    lstl::list<int> self = make_int_list(da, 3);
    const size_t old_size = self.size();
    self.merge(self);
    LSTL_CHECK(self.size() == old_size);
  }

  {
    const int data[] = {1, 2, 3};
    lstl::list<int> l = make_int_list(data, 3);
    const size_t old_size = l.size();
    l.splice(l.begin(), l);
    LSTL_CHECK(l.size() == old_size);
  }

  std::printf("PASS test_list\n");
  return 0;
}
