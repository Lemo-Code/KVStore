#include "lstl_test_common.h"

#include <cstdio>

#include "associative/set.h"
#include "associative/map.h"
#include "associative/multiset.h"
#include "associative/multimap.h"
#include "associative/unordered_set.h"
#include "associative/unordered_map.h"
#include "associative/unordered_multiset.h"
#include "associative/unordered_multimap.h"

namespace lstl {
template <typename Key, typename Hash = hash<Key>, typename Pred = equal_to<Key>,
          typename Alloc = allocator<Key> >
class hash_set : public unordered_set<Key, Hash, Pred, Alloc> {
 public:
  typedef unordered_set<Key, Hash, Pred, Alloc> base;
  hash_set() : base() {}
  explicit hash_set(typename base::size_type n, const Hash& hf = Hash(), const Pred& eq = Pred())
      : base(n, hf, eq) {}
  template <typename InputIterator>
  hash_set(InputIterator first, InputIterator last, typename base::size_type n = 0,
           const Hash& hf = Hash(), const Pred& eq = Pred())
      : base(first, last, n, hf, eq) {}
};

template <typename Key, typename T, typename Hash = hash<Key>, typename Pred = equal_to<Key>,
          typename Alloc = allocator<pair<const Key, T> > >
class hash_map : public unordered_map<Key, T, Hash, Pred, Alloc> {
 public:
  typedef unordered_map<Key, T, Hash, Pred, Alloc> base;
  hash_map() : base() {}
  explicit hash_map(typename base::size_type n, const Hash& hf = Hash(), const Pred& eq = Pred())
      : base(n, hf, eq) {}
  template <typename InputIterator>
  hash_map(InputIterator first, InputIterator last, typename base::size_type n = 0,
           const Hash& hf = Hash(), const Pred& eq = Pred())
      : base(first, last, n, hf, eq) {}
};
}  // namespace lstl

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
    lstl::set<int> s;
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

    lstl::set<int>::iterator lb = s.lower_bound(3);
    LSTL_CHECK(lb != s.end() && *lb == 3);
    lstl::set<int>::iterator ub = s.upper_bound(3);
    LSTL_CHECK(ub != s.end() && *ub == 4);

    lstl::pair<lstl::set<int>::iterator, lstl::set<int>::iterator> er = s.equal_range(3);
    LSTL_CHECK(er.first != er.second);
    LSTL_CHECK(distance(er.first, er.second) == 1);

    LSTL_CHECK(s.erase(3) == 1);
    LSTL_CHECK(s.size() == 2);

    const int data[] = {10, 20, 30};
    s.insert(data, data + 3);
    LSTL_CHECK(s.size() == 5);

    lstl::set<int> cp(s);
    LSTL_CHECK(cp == s);
    lstl::set<int> mv(lstl::move(cp));
    LSTL_CHECK(mv.size() == 5);
    cp.clear();
    cp = mv;
    LSTL_CHECK(cp == mv);

    lstl::set<int> other;
    other.insert(100);
    s.swap(other);
    LSTL_CHECK(s.size() == 1);
    LSTL_CHECK(other.size() == 5);
    s.clear();
  }

  {
    lstl::multiset<int> ms;
    ms.insert(2);
    ms.insert(2);
    ms.insert(1);
    ms.insert(4);
    ms.insert(2);
    LSTL_CHECK(ms.size() == 5);
    LSTL_CHECK(ms.count(2) == 3);
    LSTL_CHECK(ms.count(9) == 0);

    const int sorted[] = {1, 2, 2, 2, 4};
    check_sorted_range(ms.begin(), ms.end(), sorted, 5);

    lstl::pair<lstl::multiset<int>::iterator, lstl::multiset<int>::iterator> er =
        ms.equal_range(2);
    LSTL_CHECK(distance(er.first, er.second) == 3);

    LSTL_CHECK(ms.erase(2) == 3);
    LSTL_CHECK(ms.size() == 2);

    const int data[] = {5, 5};
    ms.insert(data, data + 2);
    LSTL_CHECK(ms.count(5) == 2);
    ms.clear();
    LSTL_CHECK(ms.empty());
  }

  {
    lstl::map<int, int> m;
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

    lstl::map<int, int> cp(m);
    LSTL_CHECK(cp.size() == m.size());
    LSTL_CHECK(cp.find(5)->second == 50);
    m.clear();
    LSTL_CHECK(m.empty());
  }

  {
    lstl::multimap<int, int> mm;
    mm.insert(lstl::pair<const int, int>(1, 10));
    mm.insert(lstl::pair<const int, int>(1, 20));
    mm.insert(lstl::pair<const int, int>(2, 30));
    LSTL_CHECK(mm.size() == 3);
    LSTL_CHECK(mm.count(1) == 2);

    int sum = 0;
    for (lstl::multimap<int, int>::iterator it = mm.lower_bound(1);
         it != mm.upper_bound(1); ++it) {
      sum += it->second;
    }
    LSTL_CHECK(sum == 30);

    lstl::pair<lstl::multimap<int, int>::iterator, lstl::multimap<int, int>::iterator> er =
        mm.equal_range(1);
    LSTL_CHECK(distance(er.first, er.second) == 2);
    mm.erase(1);
    LSTL_CHECK(mm.size() == 1);
    mm.clear();
  }

  {
    lstl::unordered_set<int> us;
    LSTL_CHECK(us.empty());
    LSTL_CHECK(us.insert(5).second);
    LSTL_CHECK(us.insert(3).second);
    LSTL_CHECK(!us.insert(5).second);
    LSTL_CHECK(us.size() == 2);
    LSTL_CHECK(us.find(5) != us.end());
    LSTL_CHECK(us.find(99) == us.end());
    LSTL_CHECK(us.count(5) == 1);

    const size_t old_buckets = us.bucket_count();
    us.rehash(old_buckets * 4);
    LSTL_CHECK(us.bucket_count() >= old_buckets);
    LSTL_CHECK(us.find(3) != us.end());

    LSTL_CHECK(us.erase(5) == 1);
    LSTL_CHECK(us.size() == 1);

    const int data[] = {1, 2, 3, 4};
    us.insert(data, data + 4);
    LSTL_CHECK(us.size() == 4);

    lstl::unordered_set<int> cp(us);
    LSTL_CHECK(cp.size() == us.size());
    LSTL_CHECK(cp.find(4) != cp.end());
    us.clear();
    LSTL_CHECK(us.empty());
  }

  {
    lstl::unordered_map<int, int> um;
    um[7] = 70;
    um[3] = 30;
    um[1] = 10;
    LSTL_CHECK(um.size() == 3);
    LSTL_CHECK(um.find(7)->second == 70);
    LSTL_CHECK(!um.insert(lstl::pair<const int, int>(7, 0)).second);
    LSTL_CHECK(um.find(7)->second == 70);

    um[7] = 77;
    LSTL_CHECK(um[7] == 77);
    LSTL_CHECK(um.erase(3) == 1);
    LSTL_CHECK(um.size() == 2);

    um.rehash(32);
    LSTL_CHECK(um.bucket_count() >= 32);
    LSTL_CHECK(um.find(1)->second == 10);

    const lstl::pair<const int, int> data[] = {
        lstl::pair<const int, int>(8, 80),
        lstl::pair<const int, int>(9, 90),
    };
    um.insert(data, data + 2);
    LSTL_CHECK(um.size() == 4);

    lstl::unordered_map<int, int> cp(um);
    LSTL_CHECK(cp.size() == um.size());
    LSTL_CHECK(cp[9] == 90);
    um.clear();
  }

  {
    lstl::hash_set<int> hs;
    hs.insert(42);
    hs.insert(7);
    LSTL_CHECK(hs.find(42) != hs.end());
    LSTL_CHECK(hs.size() == 2);

    lstl::hash_map<int, int> hm;
    hm[9] = 90;
    hm[1] = 10;
    LSTL_CHECK(hm[9] == 90);
    LSTL_CHECK(hm.size() == 2);
    LSTL_CHECK(hm.find(1)->second == 10);
    hm.erase(9);
    LSTL_CHECK(hm.size() == 1);
  }

  {
    lstl::unordered_multiset<int> ums;
    ums.insert(1);
    ums.insert(1);
    ums.insert(2);
    LSTL_CHECK(ums.size() == 3);
    LSTL_CHECK(ums.erase(1) >= 1);
    LSTL_CHECK(!ums.empty());

    const int data[] = {3, 3, 4};
    ums.insert(data, data + 3);
    LSTL_CHECK(ums.size() >= 3);
    ums.clear();
    LSTL_CHECK(ums.empty());
  }

  {
    lstl::unordered_multimap<int, int> umm;
    umm.insert(lstl::pair<const int, int>(2, 1));
    umm.insert(lstl::pair<const int, int>(2, 2));
    umm.insert(lstl::pair<const int, int>(3, 3));
    LSTL_CHECK(umm.size() == 3);
    LSTL_CHECK(umm.erase(2) >= 1);
    LSTL_CHECK(umm.find(3) != umm.end());

    const lstl::pair<const int, int> data[] = {
        lstl::pair<const int, int>(5, 50),
        lstl::pair<const int, int>(5, 51),
    };
    umm.insert(data, data + 2);
    LSTL_CHECK(umm.size() >= 2);
    umm.clear();
  }

  std::printf("PASS test_associative\n");
  return 0;
}
