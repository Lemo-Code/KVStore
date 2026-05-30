#include "lstl_test_common.h"

#include <cstdio>

#include "associative/bset.h"
#include "associative/bmap.h"

namespace {

template <typename Iterator, typename T>
void check_sorted_range(Iterator first, Iterator last, const T* expected, size_t n) {
  size_t i = 0;
  for (Iterator it = first; it != last; ++it, ++i) {
    LSTL_CHECK(i < n);
    LSTL_CHECK(*it == expected[i]);
  }
  LSTL_CHECK(i == n);
}

template <typename Map>
void check_map_keys_sorted(const Map& m, const int* keys, size_t n) {
  size_t i = 0;
  for (typename Map::const_iterator it = m.begin(); it != m.end(); ++it, ++i) {
    LSTL_CHECK(i < n);
    LSTL_CHECK(it->first == keys[i]);
  }
  LSTL_CHECK(i == n);
}

}  // namespace

int main() {
  {
    lstl::bset<int> s;
    LSTL_CHECK(s.empty());
    LSTL_CHECK(s.insert(3).second);
    LSTL_CHECK(s.insert(1).second);
    LSTL_CHECK(s.insert(4).second);
    LSTL_CHECK(!s.insert(1).second);
    LSTL_CHECK(s.size() == 3);

    const int sorted[] = {1, 3, 4};
    check_sorted_range(s.begin(), s.end(), sorted, 3);

    LSTL_CHECK(s.find(3) != s.end());
    LSTL_CHECK(s.find(99) == s.end());
    LSTL_CHECK(s.count(3) == 1);
    LSTL_CHECK(s.count(99) == 0);

    lstl::bset<int>::iterator lb = s.lower_bound(3);
    LSTL_CHECK(lb != s.end() && *lb == 3);
    lstl::bset<int>::iterator ub = s.upper_bound(3);
    LSTL_CHECK(ub != s.end() && *ub == 4);

    lstl::pair<lstl::bset<int>::iterator, lstl::bset<int>::iterator> er = s.equal_range(3);
    LSTL_CHECK(er.first != er.second);
    LSTL_CHECK(distance(er.first, er.second) == 1);

    LSTL_CHECK(s.erase(3) == 1);
    LSTL_CHECK(s.size() == 2);

    const int data[] = {10, 20, 30};
    s.insert(data, data + 3);
    LSTL_CHECK(s.size() == 5);

    lstl::bset<int> cp(s);
    LSTL_CHECK(cp == s);
    lstl::bset<int> mv(lstl::move(cp));
    LSTL_CHECK(mv.size() == 5);
    cp.clear();
    cp = mv;
    LSTL_CHECK(cp == mv);

    lstl::bset<int> other;
    other.insert(100);
    s.swap(other);
    LSTL_CHECK(s.size() == 1);
    LSTL_CHECK(other.size() == 5);
    s.clear();
  }

  {
    lstl::bmap<int, int> m;
    m[1] = 10;
    m[2] = 20;
    m[3] = 30;
    LSTL_CHECK(m.size() == 3);
    LSTL_CHECK(m.find(1)->second == 10);
    LSTL_CHECK(!m.insert(lstl::pair<const int, int>(1, 99)).second);
    LSTL_CHECK(m.find(1)->second == 10);

    m[1] = 11;
    LSTL_CHECK(m.find(1)->second == 11);

    const int keys[] = {1, 2, 3};
    check_map_keys_sorted(m, keys, 3);

    LSTL_CHECK(m.lower_bound(2)->first == 2);
    LSTL_CHECK(m.upper_bound(2)->first == 3);
    LSTL_CHECK(m.count(2) == 1);

    LSTL_CHECK(m.erase(2) == 1);
    LSTL_CHECK(m.size() == 2);
    LSTL_CHECK(m.find(2) == m.end());

    const lstl::pair<const int, int> data[] = {
        lstl::pair<const int, int>(4, 40),
        lstl::pair<const int, int>(5, 50),
    };
    m.insert(data, data + 2);
    LSTL_CHECK(m.size() == 4);

    lstl::bmap<int, int> cp(m);
    LSTL_CHECK(cp.size() == m.size());
    LSTL_CHECK(cp.find(5)->second == 50);
    m.clear();
    LSTL_CHECK(m.empty());
  }

  {
    lstl::bset<int> large;
    for (int i = 0; i < 128; ++i) {
      LSTL_CHECK(large.insert(i).second);
    }
    LSTL_CHECK(large.size() == 128);
    for (int i = 0; i < 128; i += 2) {
      LSTL_CHECK(large.erase(i) == 1);
    }
    LSTL_CHECK(large.size() == 64);
    int expect = 1;
    for (lstl::bset<int>::const_iterator it = large.begin(); it != large.end();
         ++it, expect += 2) {
      LSTL_CHECK(*it == expect);
    }
  }

  std::printf("PASS test_b_associative\n");
  return 0;
}
