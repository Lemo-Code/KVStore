#include "lstl_test_common.h"

#include <cstdio>
#include <cstring>

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

struct SmallStr {
  char buf[16];

  SmallStr() { buf[0] = '\0'; }
  explicit SmallStr(const char* s) {
    std::strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
  }
  SmallStr(const SmallStr& o) { std::strncpy(buf, o.buf, sizeof(buf)); }
  SmallStr(SmallStr&& o) throw() {
    std::strncpy(buf, o.buf, sizeof(buf));
    o.buf[0] = '\0';
  }

  SmallStr& operator=(const SmallStr& o) {
    if (this != &o) {
      std::strncpy(buf, o.buf, sizeof(buf));
    }
    return *this;
  }

  SmallStr& operator=(SmallStr&& o) throw() {
    if (this != &o) {
      std::strncpy(buf, o.buf, sizeof(buf));
      o.buf[0] = '\0';
    }
    return *this;
  }

  bool operator==(const SmallStr& o) const { return std::strcmp(buf, o.buf) == 0; }
};

lstl::vector<int> make_int_vec(const int* data, size_t n) {
  return lstl::vector<int>(data, data + n);
}

}  // namespace

int main() {
  {
    lstl::vector<int> v;
    LSTL_CHECK(v.empty());
    LSTL_CHECK(v.size() == 0);
    LSTL_CHECK(v.capacity() == 0);
  }

  {
    lstl::vector<int> v(5, 42);
    LSTL_CHECK(v.size() == 5);
    LSTL_CHECK(v.capacity() >= 5);
    for (size_t i = 0; i < v.size(); ++i) {
      LSTL_CHECK(v[i] == 42);
    }
  }

  {
    const int data[] = {1, 2, 3, 4, 5};
    lstl::vector<int> v = make_int_vec(data, 5);
    LSTL_CHECK(v.size() == 5);
    LSTL_CHECK(v.front() == 1);
    LSTL_CHECK(v.back() == 5);
    LSTL_CHECK(v[2] == 3);
    LSTL_CHECK(v.at(4) == 5);
  }

  {
    int arr[] = {10, 20, 30};
    lstl::vector<int> v(arr, arr + 3);
    LSTL_CHECK(v.size() == 3);
    LSTL_CHECK(v[1] == 20);
  }

  {
    const int data[] = {1, 2, 3};
    lstl::vector<int> a = make_int_vec(data, 3);
    lstl::vector<int> b(a);
    LSTL_CHECK(b == a);
    lstl::vector<int> c;
    c = a;
    LSTL_CHECK(c == a);
  }

  {
    lstl::vector<SmallStr> a;
    a.push_back(SmallStr("hello"));
    a.push_back(SmallStr("world"));
    lstl::vector<SmallStr> b(lstl::move(a));
    LSTL_CHECK(b.size() == 2);
    LSTL_CHECK(b[0] == SmallStr("hello"));
    LSTL_CHECK(a.empty());
  }

  {
    lstl::vector<int> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    LSTL_CHECK(v.size() == 3);
    const size_t cap = v.capacity();
    v.reserve(cap + 10);
    LSTL_CHECK(v.capacity() >= cap + 10);
    LSTL_CHECK(v.size() == 3);
    v.pop_back();
    LSTL_CHECK(v.size() == 2);
    LSTL_CHECK(v.back() == 2);
  }

  {
    const int data[] = {1, 2, 3, 4, 5};
    lstl::vector<int> v = make_int_vec(data, 5);
    v.insert(v.begin() + 2, 99);
    LSTL_CHECK(v.size() == 6);
    LSTL_CHECK(v[2] == 99);
    v.insert(v.end(), 3, 7);
    LSTL_CHECK(v.size() == 9);
    LSTL_CHECK(v[6] == 7 && v[8] == 7);
    v.erase(v.begin() + 1);
    LSTL_CHECK(v.size() == 8);
    LSTL_CHECK(v[1] == 99);
    v.erase(v.begin(), v.begin() + 2);
    LSTL_CHECK(v.size() == 6);
  }

  {
    const int data[] = {1, 2, 3};
    lstl::vector<int> v = make_int_vec(data, 3);
    v.resize(5, 0);
    LSTL_CHECK(v.size() == 5);
    LSTL_CHECK(v[4] == 0);
    v.resize(2);
    LSTL_CHECK(v.size() == 2);
    v.clear();
    LSTL_CHECK(v.empty());
  }

  {
    const int data[] = {1, 2, 3};
    lstl::vector<int> v = make_int_vec(data, 3);
    v.assign(4, 8);
    LSTL_CHECK(v.size() == 4);
    LSTL_CHECK(v[3] == 8);
    const int arr[] = {5, 6};
    v.assign(arr, arr + 2);
    LSTL_CHECK(v.size() == 2);
    LSTL_CHECK(v[0] == 5);
  }

  {
    const int da[] = {1, 2};
    const int db[] = {3, 4};
    lstl::vector<int> a = make_int_vec(da, 2);
    lstl::vector<int> b = make_int_vec(db, 2);
    a.swap(b);
    LSTL_CHECK(a[0] == 3);
    LSTL_CHECK(b[0] == 1);
    lstl::swap(a, b);
    LSTL_CHECK(a[0] == 1);
  }

  {
    lstl::vector<Counter> v;
    v.emplace_back(1);
    v.emplace_back(2);
    LSTL_CHECK(v.size() == 2);
    LSTL_CHECK(Counter::alive == 2);
    v.clear();
    LSTL_CHECK(Counter::alive == 0);
  }

  {
    const int data[] = {1, 2, 3, 4};
    lstl::vector<int> v = make_int_vec(data, 4);
    LSTL_CHECK(lstl::distance(v.begin(), v.end()) == 4);
    LSTL_CHECK(*v.rbegin() == 4);
    LSTL_CHECK(v.data() == &v[0]);
  }

  {
    bool threw = false;
    lstl::vector<int> v(3, 0);
    try {
      (void)v.at(10);
    } catch (const lstl::out_of_range&) {
      threw = true;
    }
    LSTL_CHECK(threw);
  }

  {
    const int data[] = {5, 1, 4, 2, 3};
    lstl::vector<int> v = make_int_vec(data, 5);
    v.sort();
    for (int i = 0; i < 5; ++i) {
      LSTL_CHECK(v[i] == i + 1);
    }
  }

  {
    const int data[] = {1, 2, 3};
    lstl::vector<int> v = make_int_vec(data, 3);
    v.reverse();
    LSTL_CHECK(v[0] == 3);
    LSTL_CHECK(v[2] == 1);
  }

  {
    const int da[] = {1, 3, 5};
    const int db[] = {2, 4, 6};
    lstl::vector<int> a = make_int_vec(da, 3);
    lstl::vector<int> b = make_int_vec(db, 3);
    a.merge(b);
    LSTL_CHECK(b.empty());
    LSTL_CHECK(a.size() == 6);
    for (int i = 0; i < 6; ++i) {
      LSTL_CHECK(a[i] == i + 1);
    }
    lstl::vector<int> self = make_int_vec(da, 3);
    const size_t old_size = self.size();
    self.merge(self);
    LSTL_CHECK(self.size() == old_size);
  }

  std::printf("PASS test_vector\n");
  return 0;
}
