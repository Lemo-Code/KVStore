#include "lstl_test_common.h"

#include <cstdio>

#include "container.h"

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

}  // namespace

int main() {
  {
    lstl::skip_set<int> s;
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
    LSTL_CHECK(s.erase(3) == 1);
    LSTL_CHECK(s.size() == 2);
  }

  {
    lstl::skip_map<int, int> m;
    m[1] = 10;
    m[2] = 20;
    LSTL_CHECK(m.size() == 2);
    LSTL_CHECK(m.find(1)->second == 10);
    m[1] = 11;
    LSTL_CHECK(m.find(1)->second == 11);
    LSTL_CHECK(m.erase(2) == 1);
    LSTL_CHECK(m.size() == 1);
  }

  {
    lstl::skip_multiset<int> ms;
    ms.insert(1);
    ms.insert(1);
    ms.insert(2);
    LSTL_CHECK(ms.size() == 3);
    LSTL_CHECK(ms.count(1) == 2);
    LSTL_CHECK(ms.equal_range(1).first != ms.equal_range(1).second);
  }

  std::printf("PASS %s\n", __FILE__);
  return 0;
}
