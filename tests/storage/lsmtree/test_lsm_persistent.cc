#include "lstl_test_common.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "lsmtree_persistent.h"

namespace {

std::string make_temp_dir() {
  char tpl[] = "/tmp/kvstore_lsm_XXXXXX";
  char* dir = ::mkdtemp(tpl);
  LSTL_CHECK(dir != 0);
  return std::string(dir);
}

void remove_recursive(const std::string& dir) {
  std::string cmd = "rm -rf ";
  cmd += dir;
  ::system(cmd.c_str());
}

}  // namespace

int main() {
  {
    lstl::lsm::BloomFilter bloom;
    bloom.init(100);
    bloom.add(42);
    LSTL_CHECK(bloom.may_contain(42));
    LSTL_CHECK(!bloom.may_contain(99));
  }

  {
    lstl::skip_map<int, lstl::lsm::Record<int> > src;
    typedef lstl::skip_map<int, lstl::lsm::Record<int> >::value_type entry_type;
    src.insert(entry_type(1, lstl::lsm::Record<int>(10)));
    src.insert(entry_type(3, lstl::lsm::Record<int>(30)));
    src.insert(entry_type(5, lstl::lsm::Record<int>(50)));

    lstl::lsm::SortedTable<int, int> table(src);
    const std::string path = make_temp_dir() + "/test.sst";
    LSTL_CHECK((lstl::lsm::DiskSstable<int, int>::write_file(path, table)));

    lstl::lsm::DiskSstable<int, int> disk(path);
    LSTL_CHECK(disk.load());
    LSTL_CHECK(disk.may_contain(3));
    LSTL_CHECK(!disk.may_contain(4));

    int v = 0;
    LSTL_CHECK(disk.get(3, &v));
    LSTL_CHECK(v == 30);
    LSTL_CHECK(!disk.get(4, &v));

    remove_recursive(path.substr(0, path.find_last_of('/')));
  }

  {
    const std::string dir = make_temp_dir();
    int v = 0;

    {
      lstl::lsm::PersistentLsmTree<int, int> db(dir, 4, 4);
      LSTL_CHECK(db.open(true));
      db.put(1, 100);
      db.put(2, 200);
      db.put(3, 300);
      db.flush();
      LSTL_CHECK(db.get(2, &v));
      LSTL_CHECK(v == 200);
      db.close();
    }

    {
      lstl::lsm::PersistentLsmTree<int, int> db(dir, 4, 4);
      LSTL_CHECK(db.open(false));
      LSTL_CHECK(db.get(1, &v));
      LSTL_CHECK(v == 100);
      LSTL_CHECK(db.get(3, &v));
      LSTL_CHECK(v == 300);
      db.put(3, 333);
      LSTL_CHECK(db.get(3, &v));
      LSTL_CHECK(v == 333);
      db.close();
    }

    {
      lstl::lsm::PersistentLsmTree<int, int> db(dir, 4, 4);
      LSTL_CHECK(db.open(false));
      LSTL_CHECK(db.get(3, &v));
      LSTL_CHECK(v == 333);
      db.close();
    }

    remove_recursive(dir);
  }

  {
    const std::string dir = make_temp_dir();

    {
      lstl::lsm::PersistentLsmTree<int, int> db(dir, 64, 4);
      LSTL_CHECK(db.open(true));
      db.put(10, 1000);
      db.put(20, 2000);
      db.erase(10);
      db.close();
    }

    {
      lstl::lsm::PersistentLsmTree<int, int> db(dir, 64, 4);
      LSTL_CHECK(db.open(false));
      int v = 0;
      LSTL_CHECK(!db.get(10, &v));
      LSTL_CHECK(db.get(20, &v));
      LSTL_CHECK(v == 2000);
      db.close();
    }

    remove_recursive(dir);
  }

  std::printf("PASS %s\n", __FILE__);
  return 0;
}
