/**
 * @file test_phase6.cc
 * @brief Phase 6：AUTH / NOAUTH / INFO
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

void test_auth_no_password_configured() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  const auto r = dispatch(registry, ctx, db, "AUTH", {"secret"});
  LEDIS_CHECK(r.value.type == ledis::RespType::kError);
  LEDIS_CHECK(r.value.bulk.str().find("no password is set") != std::string::npos);
}

void test_auth_and_noauth() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();
  registry.setRequirePass("ledis-pass");

  const auto noauth = dispatch(registry, ctx, db, "GET", {"k"});
  LEDIS_CHECK(noauth.value.type == ledis::RespType::kError);
  LEDIS_CHECK(noauth.value.bulk.str().find("NOAUTH") != std::string::npos);

  LEDIS_CHECK(dispatch(registry, ctx, db, "PING", {}).value.bulk.str() == "PONG");

  const auto bad = dispatch(registry, ctx, db, "AUTH", {"wrong"});
  LEDIS_CHECK(bad.value.type == ledis::RespType::kError);
  LEDIS_CHECK(bad.value.bulk.str().find("invalid password") != std::string::npos);
  LEDIS_CHECK(!ctx.authenticated);

  LEDIS_CHECK(dispatch(registry, ctx, db, "AUTH", {"ledis-pass"}).value.bulk.str() == "OK");
  LEDIS_CHECK(ctx.authenticated);

  LEDIS_CHECK(dispatch(registry, ctx, db, "SET", {"k", "v"}).value.bulk.str() == "OK");
}

void test_info() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  dispatch(registry, ctx, db, "SET", {"a", "1"});

  const auto all = dispatch(registry, ctx, db, "INFO", {}).value;
  LEDIS_CHECK(all.type == ledis::RespType::kBulkString);
  LEDIS_CHECK(all.bulk.str().find("# Server") != std::string::npos);
  LEDIS_CHECK(all.bulk.str().find("# Memory") != std::string::npos);
  LEDIS_CHECK(all.bulk.str().find("db_keys:") != std::string::npos);

  const auto server = dispatch(registry, ctx, db, "INFO", {"server"}).value;
  LEDIS_CHECK(server.bulk.str().find("redis_version:") != std::string::npos);
  LEDIS_CHECK(server.bulk.str().find("# Memory") == std::string::npos);

  const auto memory = dispatch(registry, ctx, db, "INFO", {"memory"}).value;
  LEDIS_CHECK(memory.bulk.str().find("# Memory") != std::string::npos);

  const auto bad = dispatch(registry, ctx, db, "INFO", {"nosuch"}).value;
  LEDIS_CHECK(bad.type == ledis::RespType::kError);
}

void test_config_get() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();
  registry.setServerConfig(16379, 5000, 0);

  const auto port = dispatch(registry, ctx, db, "CONFIG", {"GET", "port"}).value;
  LEDIS_CHECK(port.type == ledis::RespType::kArray);
  LEDIS_CHECK(port.array.size() == 2);
  LEDIS_CHECK(port.array[0].bulk.str() == "port");
  LEDIS_CHECK(port.array[1].bulk.str() == "16379");

  const auto dbs = dispatch(registry, ctx, db, "CONFIG", {"GET", "databases"}).value;
  LEDIS_CHECK(dbs.array[1].bulk.str() == "16");

  const auto mc = dispatch(registry, ctx, db, "CONFIG", {"GET", "maxclients"}).value;
  LEDIS_CHECK(mc.array[1].bulk.str() == "5000");

  const auto mem = dispatch(registry, ctx, db, "CONFIG", {"GET", "maxmemory"}).value;
  LEDIS_CHECK(mem.array[1].bulk.str() == "0");

  const auto bad = dispatch(registry, ctx, db, "CONFIG", {"GET", "nosuch"}).value;
  LEDIS_CHECK(bad.type == ledis::RespType::kError);
}

void test_config_set() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  LEDIS_CHECK(dispatch(registry, ctx, db, "CONFIG", {"SET", "dir", "/tmp/ledis"}).value.bulk.str() ==
              "OK");
  LEDIS_CHECK(dispatch(registry, ctx, db, "CONFIG", {"GET", "dir"}).value.array[1].bulk.str() ==
              "/tmp/ledis");

  LEDIS_CHECK(dispatch(registry, ctx, db, "CONFIG", {"SET", "maxclients", "128"}).value.bulk.str() ==
              "OK");
  LEDIS_CHECK(dispatch(registry, ctx, db, "CONFIG", {"GET", "maxclients"}).value.array[1].bulk.str() ==
              "128");

  LEDIS_CHECK(dispatch(registry, ctx, db, "CONFIG", {"SET", "requirepass", "secret"}).value.bulk.str() ==
              "OK");
  ctx.authenticated = false;
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {"k"}).value.type == ledis::RespType::kError);

  const auto ro = dispatch(registry, ctx, db, "CONFIG", {"SET", "port", "6380"}).value;
  LEDIS_CHECK(ro.type == ledis::RespType::kError);

  const auto bad_aof =
      dispatch(registry, ctx, db, "CONFIG", {"SET", "appendonly", "maybe"}).value;
  LEDIS_CHECK(bad_aof.type == ledis::RespType::kError);
}

}  // namespace

int main() {
  test_auth_no_password_configured();
  test_auth_and_noauth();
  test_info();
  test_config_get();
  test_config_set();
  std::printf("test_phase6: OK\n");
  return 0;
}
