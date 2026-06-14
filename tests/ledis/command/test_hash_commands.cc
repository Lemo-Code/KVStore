/**
 * @file test_hash.cc
 * @brief Phase 5.1：HSET / HGET / HDEL / HGETALL / HLEN + WRONGTYPE
 */
#include "../test_common.h"

#include "ledis/command/registry.h"
#include "ledis/session/session_context.h"
#include "ledis/store/db_manager.h"

namespace {

ledis::CommandResult dispatch(ledis::CommandRegistry& registry, ledis::SessionContext& ctx,
                              ledis::DBManager& db, const char* name,
                              std::initializer_list<const char*> args) {
  ledis::Command cmd;
  cmd.name = ledis::Sds(name);
  for (const char* arg : args) {
    cmd.args.push_back(ledis::Sds(arg));
  }
  return registry.dispatch(ctx, db, cmd);
}

void test_hash_commands() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  LEDIS_CHECK(dispatch(registry, ctx, db, "HSET", {"user", "name", "alice"}).value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "HSET", {"user", "age", "30"}).value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "HSET", {"user", "name", "bob"}).value.integer == 0);

  LEDIS_CHECK(dispatch(registry, ctx, db, "HGET", {"user", "name"}).value.bulk.str() == "bob");
  LEDIS_CHECK(dispatch(registry, ctx, db, "HGET", {"user", "missing"}).value.type ==
              ledis::RespType::kNull);
  LEDIS_CHECK(dispatch(registry, ctx, db, "HLEN", {"user"}).value.integer == 2);

  const auto all = dispatch(registry, ctx, db, "HGETALL", {"user"}).value;
  LEDIS_CHECK(all.type == ledis::RespType::kArray);
  LEDIS_CHECK(all.array.size() == 4);

  LEDIS_CHECK(dispatch(registry, ctx, db, "HDEL", {"user", "age"}).value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "HLEN", {"user"}).value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "HDEL", {"user", "name"}).value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "HLEN", {"user"}).value.integer == 0);
}

void test_hash_extended() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  dispatch(registry, ctx, db, "HSET", {"stats", "hits", "10"});
  dispatch(registry, ctx, db, "HSET", {"stats", "miss", "2"});

  LEDIS_CHECK(dispatch(registry, ctx, db, "HEXISTS", {"stats", "hits"}).value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "HEXISTS", {"stats", "gone"}).value.integer == 0);
  LEDIS_CHECK(dispatch(registry, ctx, db, "HEXISTS", {"missing", "f"}).value.integer == 0);

  const auto hmget = dispatch(registry, ctx, db, "HMGET", {"stats", "hits", "gone", "miss"})
                         .value;
  LEDIS_CHECK(hmget.array.size() == 3);
  LEDIS_CHECK(hmget.array[0].bulk.str() == "10");
  LEDIS_CHECK(hmget.array[1].type == ledis::RespType::kNull);
  LEDIS_CHECK(hmget.array[2].bulk.str() == "2");

  const auto hkeys = dispatch(registry, ctx, db, "HKEYS", {"stats"}).value;
  LEDIS_CHECK(hkeys.array.size() == 2);

  const auto hvals = dispatch(registry, ctx, db, "HVALS", {"stats"}).value;
  LEDIS_CHECK(hvals.array.size() == 2);

  LEDIS_CHECK(dispatch(registry, ctx, db, "HINCRBY", {"stats", "hits", "5"}).value.integer == 15);
  LEDIS_CHECK(dispatch(registry, ctx, db, "HGET", {"stats", "hits"}).value.bulk.str() == "15");
  LEDIS_CHECK(dispatch(registry, ctx, db, "HINCRBY", {"stats", "new", "3"}).value.integer == 3);
}

void test_hash_wrongtype() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  LEDIS_CHECK(dispatch(registry, ctx, db, "SET", {"k", "string"}).value.bulk.str() == "OK");

  const auto hget = dispatch(registry, ctx, db, "HGET", {"k", "f"});
  LEDIS_CHECK(hget.value.type == ledis::RespType::kError);
  LEDIS_CHECK(hget.value.bulk.str().find("wrong type") != std::string::npos);

  LEDIS_CHECK(dispatch(registry, ctx, db, "HSET", {"k2", "f", "v"}).value.integer == 1);
  const auto set = dispatch(registry, ctx, db, "SET", {"k2", "x"});
  LEDIS_CHECK(set.value.type == ledis::RespType::kError);
  LEDIS_CHECK(set.value.bulk.str().find("wrong type") != std::string::npos);

  const auto get = dispatch(registry, ctx, db, "GET", {"k2"});
  LEDIS_CHECK(get.value.type == ledis::RespType::kError);
}

}  // namespace

int main() {
  test_hash_commands();
  test_hash_extended();
  test_hash_wrongtype();
  return 0;
}
