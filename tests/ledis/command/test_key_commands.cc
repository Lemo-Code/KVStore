/**
 * @file test_key_commands.cc
 * @brief TYPE / FLUSHALL / SET NX PX / KEYS / SCAN
 */
#include "../test_common.h"

#include "ledis/command/registry.h"
#include "ledis/session/session_context.h"
#include "ledis/store/db_manager.h"

#include <chrono>

namespace {

ledis::CommandResult dispatch(ledis::CommandRegistry& registry,
                              ledis::SessionContext& ctx, ledis::DBManager& db,
                              const char* name, const ledis::SdsArgList& args) {
  ledis::Command cmd;
  cmd.name = ledis::Sds(name);
  cmd.args = args;
  return registry.dispatch(ctx, db, cmd);
}

void test_type_and_flushall() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  dispatch(registry, ctx, db, "SET", {ledis::Sds("s"), ledis::Sds("v")});
  dispatch(registry, ctx, db, "HSET",
           {ledis::Sds("h"), ledis::Sds("f"), ledis::Sds("x")});

  auto t1 = dispatch(registry, ctx, db, "TYPE", {ledis::Sds("s")}).value;
  LEDIS_CHECK(t1.bulk.str() == "string");
  auto t2 = dispatch(registry, ctx, db, "TYPE", {ledis::Sds("h")}).value;
  LEDIS_CHECK(t2.bulk.str() == "hash");
  auto t3 = dispatch(registry, ctx, db, "TYPE", {ledis::Sds("missing")}).value;
  LEDIS_CHECK(t3.bulk.str() == "none");

  dispatch(registry, ctx, db, "SELECT", {ledis::Sds("1")});
  dispatch(registry, ctx, db, "SET", {ledis::Sds("db1"), ledis::Sds("x")});
  dispatch(registry, ctx, db, "FLUSHALL", {});
  LEDIS_CHECK(dispatch(registry, ctx, db, "DBSIZE", {}).value.integer == 0);
  ctx.db_index = 0;
  LEDIS_CHECK(dispatch(registry, ctx, db, "DBSIZE", {}).value.integer == 0);
}

void test_set_nx_xx_px() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  auto ok = dispatch(registry, ctx, db, "SET",
                     {ledis::Sds("k"), ledis::Sds("v"), ledis::Sds("NX")});
  LEDIS_CHECK(ok.value.bulk.str() == "OK");

  auto dup = dispatch(registry, ctx, db, "SET",
                     {ledis::Sds("k"), ledis::Sds("v2"), ledis::Sds("NX")});
  LEDIS_CHECK(dup.value.type == ledis::RespType::kNull);

  auto miss = dispatch(registry, ctx, db, "SET",
                      {ledis::Sds("nope"), ledis::Sds("v"), ledis::Sds("XX")});
  LEDIS_CHECK(miss.value.type == ledis::RespType::kNull);

  auto hit = dispatch(registry, ctx, db, "SET",
                      {ledis::Sds("k"), ledis::Sds("v3"), ledis::Sds("XX")});
  LEDIS_CHECK(hit.value.bulk.str() == "OK");
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {ledis::Sds("k")}).value.bulk.str() ==
              "v3");

  dispatch(registry, ctx, db, "SET",
           {ledis::Sds("ttl"), ledis::Sds("1"), ledis::Sds("PX"),
            ledis::Sds("60000")});
  LEDIS_CHECK(dispatch(registry, ctx, db, "TTL", {ledis::Sds("ttl")}).value.integer >= 0);
}

void test_keys_and_scan() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  dispatch(registry, ctx, db, "SET", {ledis::Sds("user:1"), ledis::Sds("a")});
  dispatch(registry, ctx, db, "SET", {ledis::Sds("user:2"), ledis::Sds("b")});
  dispatch(registry, ctx, db, "SET", {ledis::Sds("other"), ledis::Sds("c")});

  auto keys = dispatch(registry, ctx, db, "KEYS", {ledis::Sds("user:*")}).value;
  LEDIS_CHECK(keys.type == ledis::RespType::kArray);
  LEDIS_CHECK(keys.array.size() == 2);

  auto scan0 = dispatch(registry, ctx, db, "SCAN",
                        {ledis::Sds("0"), ledis::Sds("MATCH"), ledis::Sds("user:*"),
                         ledis::Sds("COUNT"), ledis::Sds("10")})
                   .value;
  LEDIS_CHECK(scan0.type == ledis::RespType::kArray);
  LEDIS_CHECK(scan0.array.size() == 2);
  LEDIS_CHECK(scan0.array[1].type == ledis::RespType::kArray);
  LEDIS_CHECK(scan0.array[1].array.size() == 2);
}

void test_getset_rename_expireat() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  auto first = dispatch(registry, ctx, db, "GETSET", {ledis::Sds("k"), ledis::Sds("v1")});
  LEDIS_CHECK(first.value.type == ledis::RespType::kNull);
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {ledis::Sds("k")}).value.bulk.str() == "v1");

  auto second = dispatch(registry, ctx, db, "GETSET", {ledis::Sds("k"), ledis::Sds("v2")});
  LEDIS_CHECK(second.value.bulk.str() == "v1");
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {ledis::Sds("k")}).value.bulk.str() == "v2");

  dispatch(registry, ctx, db, "SET",
           {ledis::Sds("ttl"), ledis::Sds("x"), ledis::Sds("EX"), ledis::Sds("60")});
  dispatch(registry, ctx, db, "GETSET", {ledis::Sds("ttl"), ledis::Sds("y")});
  LEDIS_CHECK(dispatch(registry, ctx, db, "TTL", {ledis::Sds("ttl")}).value.integer >= 0);

  dispatch(registry, ctx, db, "SET", {ledis::Sds("src"), ledis::Sds("payload")});
  LEDIS_CHECK(dispatch(registry, ctx, db, "RENAME", {ledis::Sds("src"), ledis::Sds("dst")})
                  .value.bulk.str() == "OK");
  LEDIS_CHECK(!db.keyspace(0).exists(ledis::Sds("src")));
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {ledis::Sds("dst")}).value.bulk.str() ==
              "payload");

  dispatch(registry, ctx, db, "SET", {ledis::Sds("old"), ledis::Sds("1")});
  dispatch(registry, ctx, db, "SET", {ledis::Sds("new"), ledis::Sds("2")});
  LEDIS_CHECK(dispatch(registry, ctx, db, "RENAME", {ledis::Sds("old"), ledis::Sds("new")})
                  .value.bulk.str() == "OK");
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {ledis::Sds("new")}).value.bulk.str() == "1");

  const int64_t future =
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count() +
      3600;
  dispatch(registry, ctx, db, "SET", {ledis::Sds("at"), ledis::Sds("z")});
  LEDIS_CHECK(dispatch(registry, ctx, db, "EXPIREAT",
                       {ledis::Sds("at"), ledis::Sds(std::to_string(future))})
                  .value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "TTL", {ledis::Sds("at")}).value.integer > 0);

  const int64_t past = future - 7200;
  LEDIS_CHECK(dispatch(registry, ctx, db, "EXPIREAT",
                       {ledis::Sds("at"), ledis::Sds(std::to_string(past))})
                  .value.integer == 1);
  LEDIS_CHECK(!db.keyspace(0).exists(ledis::Sds("at")));
}

void test_echo_and_randomkey() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  LEDIS_CHECK(dispatch(registry, ctx, db, "ECHO", {ledis::Sds("ledis")}).value.bulk.str() ==
              "ledis");
  LEDIS_CHECK(dispatch(registry, ctx, db, "RANDOMKEY", {}).value.type == ledis::RespType::kNull);

  dispatch(registry, ctx, db, "SET", {ledis::Sds("only"), ledis::Sds("1")});
  LEDIS_CHECK(dispatch(registry, ctx, db, "RANDOMKEY", {}).value.bulk.str() == "only");
}

void test_exists_pttl_move_renamenx_touch() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  dispatch(registry, ctx, db, "SET", {ledis::Sds("a"), ledis::Sds("1")});
  dispatch(registry, ctx, db, "SET", {ledis::Sds("b"), ledis::Sds("2")});
  LEDIS_CHECK(dispatch(registry, ctx, db, "EXISTS", {ledis::Sds("a"), ledis::Sds("b"),
                                                     ledis::Sds("missing")})
                  .value.integer == 2);

  dispatch(registry, ctx, db, "SET",
           {ledis::Sds("px"), ledis::Sds("v"), ledis::Sds("PX"), ledis::Sds("120000")});
  LEDIS_CHECK(dispatch(registry, ctx, db, "PTTL", {ledis::Sds("px")}).value.integer > 0);

  LEDIS_CHECK(dispatch(registry, ctx, db, "RENAMENX", {ledis::Sds("b"), ledis::Sds("a")})
                  .value.integer == 0);
  LEDIS_CHECK(dispatch(registry, ctx, db, "RENAMENX", {ledis::Sds("b"), ledis::Sds("c")})
                  .value.integer == 1);
  LEDIS_CHECK(db.keyspace(0).exists(ledis::Sds("c")));

  dispatch(registry, ctx, db, "SET", {ledis::Sds("mv"), ledis::Sds("payload")});
  LEDIS_CHECK(
      dispatch(registry, ctx, db, "MOVE", {ledis::Sds("mv"), ledis::Sds("1")}).value.integer ==
      1);
  ctx.db_index = 1;
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {ledis::Sds("mv")}).value.bulk.str() ==
              "payload");
  ctx.db_index = 0;
  LEDIS_CHECK(!db.keyspace(0).exists(ledis::Sds("mv")));

  dispatch(registry, ctx, db, "SET", {ledis::Sds("touch_me"), ledis::Sds("x")});
  LEDIS_CHECK(dispatch(registry, ctx, db, "TOUCH", {ledis::Sds("touch_me"), ledis::Sds("nope")})
                  .value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "UNLINK", {ledis::Sds("touch_me")}).value.integer ==
              1);
  LEDIS_CHECK(!db.keyspace(0).exists(ledis::Sds("touch_me")));

  const int64_t future_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count() +
      3600000;
  dispatch(registry, ctx, db, "SET", {ledis::Sds("pat"), ledis::Sds("z")});
  LEDIS_CHECK(dispatch(registry, ctx, db, "PEXPIREAT",
                       {ledis::Sds("pat"), ledis::Sds(std::to_string(future_ms))})
                  .value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "PTTL", {ledis::Sds("pat")}).value.integer > 0);
}

}  // namespace

int main() {
  test_type_and_flushall();
  test_set_nx_xx_px();
  test_keys_and_scan();
  test_getset_rename_expireat();
  test_echo_and_randomkey();
  test_exists_pttl_move_renamenx_touch();
  std::printf("test_key_commands: OK\n");
  return 0;
}
