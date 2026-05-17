#include "../memory/test_common.h"

#include <cstring>
#include <string>

#include "enginer.h"

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

}  // namespace

int main() {
  {
    enginer::vector<int> v;
    LSTL_CHECK(v.empty());
    LSTL_CHECK(v.size() == 0);
    LSTL_CHECK(v.capacity() == 0);
  }

  {
    enginer::vector<int> v(5, 42);
    LSTL_CHECK(v.size() == 5);
    LSTL_CHECK(v.capacity() >= 5);
    for (size_t i = 0; i < v.size(); ++i) {
      LSTL_CHECK(v[i] == 42);
    }
  }

  {
    enginer::vector<int> v = {1, 2, 3, 4, 5};
    LSTL_CHECK(v.size() == 5);
    LSTL_CHECK(v.front() == 1);
    LSTL_CHECK(v.back() == 5);
    LSTL_CHECK(v[2] == 3);
    LSTL_CHECK(v.at(4) == 5);
  }

  {
    int arr[] = {10, 20, 30};
    enginer::vector<int> v(arr, arr + 3);
    LSTL_CHECK(v.size() == 3);
    LSTL_CHECK(v[1] == 20);
  }

  {
    enginer::vector<int> a = {1, 2, 3};
    enginer::vector<int> b(a);
    LSTL_CHECK(b == a);
    enginer::vector<int> c;
    c = a;
    LSTL_CHECK(c == a);
  }

  {
    enginer::vector<std::string> a;
    a.push_back("hello");
    a.push_back("world");
    enginer::vector<std::string> b(std::move(a));
    LSTL_CHECK(b.size() == 2);
    LSTL_CHECK(b[0] == "hello");
    LSTL_CHECK(a.empty());
  }

  {
    enginer::vector<int> v;
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
    enginer::vector<int> v = {1, 2, 3, 4, 5};
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
    enginer::vector<int> v = {1, 2, 3};
    v.resize(5, 0);
    LSTL_CHECK(v.size() == 5);
    LSTL_CHECK(v[4] == 0);
    v.resize(2);
    LSTL_CHECK(v.size() == 2);
    v.clear();
    LSTL_CHECK(v.empty());
  }

  {
    enginer::vector<int> v = {1, 2, 3};
    v.assign(4, 8);
    LSTL_CHECK(v.size() == 4);
    LSTL_CHECK(v[3] == 8);
    const int arr[] = {5, 6};
    v.assign(arr, arr + 2);
    LSTL_CHECK(v.size() == 2);
    LSTL_CHECK(v[0] == 5);
  }

  {
    enginer::vector<int> a = {1, 2};
    enginer::vector<int> b = {3, 4};
    a.swap(b);
    LSTL_CHECK(a[0] == 3);
    LSTL_CHECK(b[0] == 1);
    swap(a, b);
    LSTL_CHECK(a[0] == 1);
  }

  {
    enginer::vector<Counter> v;
    v.emplace_back(1);
    v.emplace_back(2);
    LSTL_CHECK(v.size() == 2);
    LSTL_CHECK(Counter::alive == 2);
    v.clear();
    LSTL_CHECK(Counter::alive == 0);
  }

  {
    enginer::vector<int> v = {1, 2, 3, 4};
    LSTL_CHECK(std::distance(v.begin(), v.end()) == 4);
    LSTL_CHECK(*v.rbegin() == 4);
    LSTL_CHECK(v.data() == &v[0]);
  }

  {
    bool threw = false;
    enginer::vector<int> v(3, 0);
    try {
      (void)v.at(10);
    } catch (const std::out_of_range&) {
      threw = true;
    }
    LSTL_CHECK(threw);
  }

  std::printf("PASS test_vector\n");
  return 0;
}
