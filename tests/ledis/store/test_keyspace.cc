/**
 * @file test_keyspace.cc
 */
#include "../test_common.h"

#include "ledis/session/session_context.h"
#include "ledis/store/db_manager.h"
#include "ledis/store/object.h"

#include <chrono>
#include <thread>

namespace {

int64_t nowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
      .count();
}

void test_set_get_del() {
  ledis::Keyspace ks;
  LEDIS_CHECK(ks.set(ledis::Sds("foo"), ledis::LedisObject::makeString(ledis::Sds("bar"))));
  LEDIS_CHECK(ks.exists(ledis::Sds("foo")));

  ledis::LedisObject obj;
  LEDIS_CHECK(ks.get(ledis::Sds("foo"), &obj));
  ledis::Sds value;
  LEDIS_CHECK(obj.asString(&value));
  LEDIS_CHECK(value.str() == "bar");

  LEDIS_CHECK(ks.del(ledis::Sds("foo")) == 1);
  LEDIS_CHECK(!ks.exists(ledis::Sds("foo")));
}

void test_db_select() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ctx.db_index = 0;
  db.keyspaceOf(ctx).set(ledis::Sds("k"), ledis::LedisObject::makeString(ledis::Sds("v0")));
  LEDIS_CHECK(db.select(&ctx, 1));
  db.keyspaceOf(ctx).set(ledis::Sds("k"), ledis::LedisObject::makeString(ledis::Sds("v1")));

  ledis::SessionContext ctx0;
  ctx0.db_index = 0;
  ledis::LedisObject obj;
  LEDIS_CHECK(db.keyspaceOf(ctx0).get(ledis::Sds("k"), &obj));
  ledis::Sds s;
  LEDIS_CHECK(obj.asString(&s));
  LEDIS_CHECK(s.str() == "v0");
}

void test_expire_lazy_delete() {
  ledis::Keyspace ks;
  const int64_t now = nowMs();
  ks.set(ledis::Sds("k"), ledis::LedisObject::makeString(ledis::Sds("v")),
        now + 50);
  LEDIS_CHECK(ks.ttlSeconds(ledis::Sds("k"), now) >= 0);
  LEDIS_CHECK(ks.exists(ledis::Sds("k")));

  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  LEDIS_CHECK(!ks.exists(ledis::Sds("k")));
  LEDIS_CHECK(ks.ttlSeconds(ledis::Sds("k"), nowMs()) == -2);
}

void test_active_expire_cycle() {
  ledis::Keyspace ks;
  const int64_t now = nowMs();
  ks.set(ledis::Sds("a"), ledis::LedisObject::makeString(ledis::Sds("1")),
        now - 1);
  ks.set(ledis::Sds("b"), ledis::LedisObject::makeString(ledis::Sds("2")));
  LEDIS_CHECK(ks.activeExpireCycle(10, now) >= 1);
  LEDIS_CHECK(ks.exists(ledis::Sds("b")));
}

}  // namespace

int main() {
  test_set_get_del();
  test_db_select();
  test_expire_lazy_delete();
  test_active_expire_cycle();
  std::printf("test_keyspace: OK\n");
  return 0;
}
