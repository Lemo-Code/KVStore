/**
 * @file test_snapshot.cc
 * @brief RDB-lite 快照 save/load 往返
 */
#include "../test_common.h"

#include "ledis/store/db_manager.h"
#include "ledis/store/object.h"
#include "ledis/store/snapshot.h"
#include "ledis/types.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {

int64_t nowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
      .count();
}

ledis::String makeTempDir() {
  char path[] = "/tmp/ledis_snap_XXXXXX";
  LEDIS_CHECK(mkdtemp(path) != nullptr);
  return ledis::String(path);
}

void fillDb(ledis::DBManager* db, int64_t now) {
  db->keyspace(0).set(ledis::Sds("s"), ledis::LedisObject::makeString(ledis::Sds("v")),
                      now + 3600 * 1000);

  ledis::LedisObject hash = ledis::LedisObject::makeHash();
  hash.hashSet(ledis::Sds("f"), ledis::Sds("h"));
  db->keyspace(0).set(ledis::Sds("h"), Move(hash));

  ledis::LedisObject list = ledis::LedisObject::makeList();
  list.listPushBack(ledis::Sds("a"));
  list.listPushBack(ledis::Sds("b"));
  db->keyspace(1).set(ledis::Sds("l"), Move(list));

  ledis::LedisObject set = ledis::LedisObject::makeSet();
  set.setAdd(ledis::Sds("x"));
  set.setAdd(ledis::Sds("y"));
  db->keyspace(2).set(ledis::Sds("z"), Move(set));

  ledis::LedisObject zset = ledis::LedisObject::makeZset();
  zset.zsetAdd(ledis::Sds("low"), 1.0);
  zset.zsetAdd(ledis::Sds("high"), 2.0);
  db->keyspace(3).set(ledis::Sds("zs"), Move(zset));
}

void verifyDb(ledis::DBManager& db) {
  ledis::LedisObject obj;
  ledis::Sds value;

  LEDIS_CHECK(db.keyspace(0).get(ledis::Sds("s"), &obj));
  LEDIS_CHECK(obj.asString(&value));
  LEDIS_CHECK(value.str() == "v");

  LEDIS_CHECK(db.keyspace(0).get(ledis::Sds("h"), &obj));
  LEDIS_CHECK(obj.isHash());
  LEDIS_CHECK(obj.hashLen() == 1);
  LEDIS_CHECK(obj.hashGet(ledis::Sds("f"), &value));
  LEDIS_CHECK(value.str() == "h");

  LEDIS_CHECK(db.keyspace(1).get(ledis::Sds("l"), &obj));
  LEDIS_CHECK(obj.isList());
  LEDIS_CHECK(obj.listLen() == 2);

  LEDIS_CHECK(db.keyspace(2).get(ledis::Sds("z"), &obj));
  LEDIS_CHECK(obj.isSet());
  LEDIS_CHECK(obj.setLen() == 2);
  LEDIS_CHECK(obj.setIsMember(ledis::Sds("x")));

  LEDIS_CHECK(db.keyspace(3).get(ledis::Sds("zs"), &obj));
  LEDIS_CHECK(obj.isZset());
  LEDIS_CHECK(obj.zsetLen() == 2);
  double score = 0;
  LEDIS_CHECK(obj.zsetScore(ledis::Sds("high"), &score));
  LEDIS_CHECK(score == 2.0);
}

void test_save_load_roundtrip() {
  const ledis::String dir = makeTempDir();
  const ledis::String path = ledis::makeSnapshotPath(dir, "dump.ledis");
  const int64_t now = nowMs();

  ledis::DBManager src;
  fillDb(&src, now);
  LEDIS_CHECK(ledis::saveSnapshot(src, path, now));

  ledis::DBManager dst;
  LEDIS_CHECK(ledis::loadSnapshot(path, &dst));
  verifyDb(dst);
}

void test_skip_expired_keys() {
  const ledis::String dir = makeTempDir();
  const ledis::String path = ledis::makeSnapshotPath(dir, "exp.ledis");
  const int64_t now = nowMs();

  ledis::DBManager src;
  src.keyspace(0).set(ledis::Sds("live"),
                      ledis::LedisObject::makeString(ledis::Sds("yes")), 0);
  src.keyspace(0).set(ledis::Sds("dead"),
                      ledis::LedisObject::makeString(ledis::Sds("no")), now - 1);
  LEDIS_CHECK(ledis::saveSnapshot(src, path, now));

  ledis::DBManager dst;
  LEDIS_CHECK(ledis::loadSnapshot(path, &dst));
  LEDIS_CHECK(dst.keyspace(0).exists(ledis::Sds("live")));
  LEDIS_CHECK(!dst.keyspace(0).exists(ledis::Sds("dead")));
}

}  // namespace

int main() {
  test_save_load_roundtrip();
  test_skip_expired_keys();
  std::printf("test_snapshot: OK\n");
  return 0;
}
