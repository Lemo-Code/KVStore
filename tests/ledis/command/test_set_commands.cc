/**
 * @file test_set_commands.cc
 * @brief Phase 5.3：SADD / SREM / SMEMBERS / SCARD / SISMEMBER + WRONGTYPE
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

void test_set_commands() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  LEDIS_CHECK(dispatch(registry, ctx, db, "SADD", {"s", "a", "b", "a"}).value.integer == 2);
  LEDIS_CHECK(dispatch(registry, ctx, db, "SCARD", {"s"}).value.integer == 2);
  LEDIS_CHECK(dispatch(registry, ctx, db, "SISMEMBER", {"s", "a"}).value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "SISMEMBER", {"s", "z"}).value.integer == 0);

  const auto members = dispatch(registry, ctx, db, "SMEMBERS", {"s"}).value;
  LEDIS_CHECK(members.type == ledis::RespType::kArray);
  LEDIS_CHECK(members.array.size() == 2);

  LEDIS_CHECK(dispatch(registry, ctx, db, "SREM", {"s", "a"}).value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "SCARD", {"s"}).value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "SREM", {"s", "b"}).value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "SCARD", {"s"}).value.integer == 0);
}

void test_set_ops() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  dispatch(registry, ctx, db, "SADD", {"s1", "a", "b", "c"});
  dispatch(registry, ctx, db, "SADD", {"s2", "b", "c", "d"});
  dispatch(registry, ctx, db, "SADD", {"s3", "c", "d", "e"});

  const auto inter = dispatch(registry, ctx, db, "SINTER", {"s1", "s2"}).value;
  LEDIS_CHECK(inter.type == ledis::RespType::kArray);
  LEDIS_CHECK(inter.array.size() == 2);

  const auto uni = dispatch(registry, ctx, db, "SUNION", {"s1", "s2"}).value;
  LEDIS_CHECK(uni.array.size() == 4);

  const auto diff = dispatch(registry, ctx, db, "SDIFF", {"s1", "s2"}).value;
  LEDIS_CHECK(diff.array.size() == 1);
  LEDIS_CHECK(diff.array[0].bulk.str() == "a");

  LEDIS_CHECK(dispatch(registry, ctx, db, "SINTER", {"s1", "missing"}).value.array.size() == 0);

  LEDIS_CHECK(dispatch(registry, ctx, db, "SINTERSTORE", {"out", "s1", "s2"}).value.integer == 2);
  LEDIS_CHECK(dispatch(registry, ctx, db, "SCARD", {"out"}).value.integer == 2);
  LEDIS_CHECK(dispatch(registry, ctx, db, "SUNIONSTORE", {"out2", "s1", "s3"}).value.integer == 5);
  LEDIS_CHECK(dispatch(registry, ctx, db, "SDIFFSTORE", {"out3", "s3", "s2"}).value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "SISMEMBER", {"out3", "e"}).value.integer == 1);
}

void test_set_wrongtype() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  LEDIS_CHECK(dispatch(registry, ctx, db, "SET", {"k", "string"}).value.bulk.str() == "OK");
  const auto sadd = dispatch(registry, ctx, db, "SADD", {"k", "x"});
  LEDIS_CHECK(sadd.value.type == ledis::RespType::kError);
  LEDIS_CHECK(sadd.value.bulk.str().find("wrong type") != std::string::npos);

  LEDIS_CHECK(dispatch(registry, ctx, db, "SADD", {"k2", "v"}).value.integer == 1);
  const auto get = dispatch(registry, ctx, db, "GET", {"k2"});
  LEDIS_CHECK(get.value.type == ledis::RespType::kError);
}

}  // namespace

int main() {
  test_set_commands();
  test_set_ops();
  test_set_wrongtype();
  return 0;
}
