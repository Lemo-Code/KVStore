/**
 * @file test_phase3.cc
 * @brief Phase 3 命令：SELECT / EXPIRE / MGET / INCR
 */
#include "../test_common.h"

#include "ledis/command/registry.h"
#include "ledis/session/session_context.h"
#include "ledis/store/db_manager.h"

namespace {

void test_select_and_dbsize() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  ledis::Command set0;
  set0.name = ledis::Sds("SET");
  set0.args.push_back(ledis::Sds("k"));
  set0.args.push_back(ledis::Sds("v0"));
  LEDIS_CHECK(registry.dispatch(ctx, db, set0).value.bulk.str() == "OK");

  ledis::Command select1;
  select1.name = ledis::Sds("SELECT");
  select1.args.push_back(ledis::Sds("1"));
  LEDIS_CHECK(registry.dispatch(ctx, db, select1).value.bulk.str() == "OK");
  LEDIS_CHECK(ctx.db_index == 1);

  ledis::Command dbsize;
  dbsize.name = ledis::Sds("DBSIZE");
  LEDIS_CHECK(registry.dispatch(ctx, db, dbsize).value.integer == 0);
}

void test_set_ex_and_ttl() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  ledis::Command set;
  set.name = ledis::Sds("SET");
  set.args.push_back(ledis::Sds("key1"));
  set.args.push_back(ledis::Sds("val"));
  set.args.push_back(ledis::Sds("EX"));
  set.args.push_back(ledis::Sds("60"));
  LEDIS_CHECK(registry.dispatch(ctx, db, set).value.bulk.str() == "OK");

  ledis::Command ttl;
  ttl.name = ledis::Sds("TTL");
  ttl.args.push_back(ledis::Sds("key1"));
  const auto r = registry.dispatch(ctx, db, ttl);
  LEDIS_CHECK(r.value.type == ledis::RespType::kInteger);
  LEDIS_CHECK(r.value.integer > 0 && r.value.integer <= 60);
}

void test_mget_mset_incr() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  ledis::Command mset;
  mset.name = ledis::Sds("MSET");
  mset.args.push_back(ledis::Sds("a"));
  mset.args.push_back(ledis::Sds("1"));
  mset.args.push_back(ledis::Sds("b"));
  mset.args.push_back(ledis::Sds("2"));
  LEDIS_CHECK(registry.dispatch(ctx, db, mset).value.bulk.str() == "OK");

  ledis::Command mget;
  mget.name = ledis::Sds("MGET");
  mget.args.push_back(ledis::Sds("a"));
  mget.args.push_back(ledis::Sds("b"));
  mget.args.push_back(ledis::Sds("missing"));
  const auto arr = registry.dispatch(ctx, db, mget).value;
  LEDIS_CHECK(arr.type == ledis::RespType::kArray);
  LEDIS_CHECK(arr.array.size() == 3);
  LEDIS_CHECK(arr.array[0].bulk.str() == "1");
  LEDIS_CHECK(arr.array[1].bulk.str() == "2");
  LEDIS_CHECK(arr.array[2].isNullBulk());

  ledis::Command incr;
  incr.name = ledis::Sds("INCR");
  incr.args.push_back(ledis::Sds("a"));
  LEDIS_CHECK(registry.dispatch(ctx, db, incr).value.integer == 2);
}

}  // namespace

int main() {
  test_select_and_dbsize();
  test_set_ex_and_ttl();
  test_mget_mset_incr();
  std::printf("test_phase3: OK\n");
  return 0;
}
