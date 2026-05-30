#include "lstl_test_common.h"

#include <cstdio>

#include "lsmtree.h"

namespace {

void check_scan(const lstl::lsm::LsmTree<int, int>& db, const int* keys, const int* vals, size_t n) {
  size_t i = 0;
  for (lstl::lsm::LsmTree<int, int>::const_iterator it = db.begin(); it != db.end(); ++it, ++i) {
    LSTL_CHECK(i < n);
    LSTL_CHECK(it->first == keys[i]);
    LSTL_CHECK(it->second == vals[i]);
  }
  LSTL_CHECK(i == n);
  LSTL_CHECK(db.live_size() == n);
}

}  // namespace

int main() {
  lstl::lsm::LsmTree<int, int> db(4, 2);

  db.put(1, 100);
  db.put(2, 200);
  db.put(3, 300);

  int v = 0;
  LSTL_CHECK(db.get(2, &v));
  LSTL_CHECK(v == 200);

  db.put(2, 222);
  LSTL_CHECK(db.get(2, &v));
  LSTL_CHECK(v == 222);

  db.erase(3);
  LSTL_CHECK(!db.get(3, &v));

  db.put(4, 400);
  db.put(5, 500);
  db.put(6, 600);
  db.flush();

  LSTL_CHECK(db.get(1, &v));
  LSTL_CHECK(v == 100);
  LSTL_CHECK(db.get(4, &v));
  LSTL_CHECK(v == 400);

  db.put(1, 111);
  LSTL_CHECK(db.get(1, &v));
  LSTL_CHECK(v == 111);

  db.erase(4);
  LSTL_CHECK(!db.get(4, &v));

  db.put(7, 700);
  db.put(8, 800);
  db.put(9, 900);
  db.put(10, 1000);

  LSTL_CHECK(db.get(7, &v));
  LSTL_CHECK(v == 700);
  LSTL_CHECK(!db.contains(4));

  {
    const int keys[] = {1, 2, 5, 6, 7, 8, 9, 10};
    const int vals[] = {111, 222, 500, 600, 700, 800, 900, 1000};
    check_scan(db, keys, vals, 8);
  }

  db.put(11, 1100);
  db.put(12, 1200);
  db.put(13, 1300);
  db.put(14, 1400);
  db.flush();

  LSTL_CHECK(db.l0_file_count() >= 1);

  db.compact_l0();
  LSTL_CHECK(db.level_count() >= 2);
  LSTL_CHECK(db.level_file_count(1) >= 1);

  LSTL_CHECK(db.get(11, &v));
  LSTL_CHECK(v == 1100);
  LSTL_CHECK(db.get(1, &v));
  LSTL_CHECK(v == 111);

  {
    lstl::skip_map<int, lstl::lsm::Record<int> > src;
    typedef lstl::skip_map<int, lstl::lsm::Record<int> >::value_type entry_type;
    src.insert(entry_type(1, lstl::lsm::Record<int>(1)));
    src.insert(entry_type(3, lstl::lsm::Record<int>(3)));
    lstl::lsm::SortedTable<int, int> table(src);
    LSTL_CHECK(table.get(3, &v));
    LSTL_CHECK(v == 3);
    LSTL_CHECK(!table.get(2, &v));
  }

  std::printf("PASS %s\n", __FILE__);
  return 0;
}
