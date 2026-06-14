/**
 * @file test_maxmemory.cc
 * @brief maxmemory + allkeys-lru 淘汰
 */
#include "../test_common.h"

#include "ledis/command/registry.h"
#include "ledis/session/session_context.h"
#include "ledis/store/db_manager.h"
#include "ledis/store/eviction.h"
#include "ledis/store/memory.h"

namespace {

std::string payload(size_t n, char c) {
  return std::string(n, c);
}

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

void test_evict_on_set() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  const std::string value = payload(80, 'x');
  const size_t entry_bytes =
      ledis::estimateEntryMemory(ledis::Sds("k1"), ledis::LedisObject::makeString(ledis::Sds(value)));
  db.setMaxmemory(entry_bytes + entry_bytes / 2);

  LEDIS_CHECK(dispatch(registry, ctx, db, "SET", {"k1", value.c_str()}).value.bulk.str() == "OK");
  LEDIS_CHECK(dispatch(registry, ctx, db, "SET", {"k2", value.c_str()}).value.bulk.str() == "OK");

  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {"k1"}).value.type == ledis::RespType::kNull);
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {"k2"}).value.bulk.str() == value);
  LEDIS_CHECK(dispatch(registry, ctx, db, "DBSIZE", {}).value.integer == 1);
}

void test_lru_evicts_coldest() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  const std::string value = payload(80, 'y');
  const size_t entry_bytes =
      ledis::estimateEntryMemory(ledis::Sds("k1"), ledis::LedisObject::makeString(ledis::Sds(value)));
  db.setMaxmemory(entry_bytes * 2 + entry_bytes / 4);

  LEDIS_CHECK(dispatch(registry, ctx, db, "SET", {"k1", value.c_str()}).value.bulk.str() == "OK");
  LEDIS_CHECK(dispatch(registry, ctx, db, "SET", {"k2", value.c_str()}).value.bulk.str() == "OK");
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {"k1"}).value.bulk.str() == value);

  db.setMaxmemory(entry_bytes * 2 + entry_bytes / 5);
  LEDIS_CHECK(dispatch(registry, ctx, db, "SET", {"k3", value.c_str()}).value.bulk.str() == "OK");

  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {"k1"}).value.bulk.str() == value);
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {"k2"}).value.type == ledis::RespType::kNull);
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {"k3"}).value.bulk.str() == value);
}

void test_config_set_maxmemory() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();
  registry.setConfigApplyCallback([&](const ledis::String& param, const ledis::String& value) {
    if (param == "maxmemory") {
      db.setMaxmemory(static_cast<size_t>(std::stoull(value)));
    }
  });

  LEDIS_CHECK(dispatch(registry, ctx, db, "CONFIG", {"SET", "maxmemory", "4096"}).value.bulk.str() ==
              "OK");
  const auto get = dispatch(registry, ctx, db, "CONFIG", {"GET", "maxmemory"}).value;
  LEDIS_CHECK(get.array[1].bulk.str() == "4096");
  LEDIS_CHECK(db.maxmemory() == 4096);
}

void test_info_memory() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();
  db.setMaxmemory(8192);

  dispatch(registry, ctx, db, "SET", {"k", "v"});
  const auto info = dispatch(registry, ctx, db, "INFO", {"memory"}).value;
  LEDIS_CHECK(info.bulk.str().find("used_memory:") != std::string::npos);
  LEDIS_CHECK(info.bulk.str().find("maxmemory:8192") != std::string::npos);
  LEDIS_CHECK(info.bulk.str().find("maxmemory_policy:allkeys-lru") != std::string::npos);
}

void test_volatile_lru_skips_persistent_keys() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();
  db.setMaxmemoryPolicy(ledis::MaxmemoryPolicy::kVolatileLru);

  const std::string value = payload(80, 'p');
  const size_t entry_bytes =
      ledis::estimateEntryMemory(ledis::Sds("k1"), ledis::LedisObject::makeString(ledis::Sds(value)));
  db.setMaxmemory(entry_bytes * 2 + entry_bytes / 4);

  dispatch(registry, ctx, db, "SET", {"persist", value.c_str()});
  dispatch(registry, ctx, db, "SET", {"volatile", value.c_str()});
  dispatch(registry, ctx, db, "EXPIRE", {"volatile", "60"});

  db.setMaxmemory(entry_bytes + entry_bytes / 2);
  LEDIS_CHECK(dispatch(registry, ctx, db, "SET", {"new", value.c_str()}).value.bulk.str() == "OK");

  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {"persist"}).value.bulk.str() == value);
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {"volatile"}).value.type == ledis::RespType::kNull);
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {"new"}).value.bulk.str() == value);
}

void test_allkeys_lfu_evicts_infrequent() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();
  db.setMaxmemoryPolicy(ledis::MaxmemoryPolicy::kAllkeysLfu);

  const std::string value = payload(80, 'f');
  const size_t entry_bytes =
      ledis::estimateEntryMemory(ledis::Sds("hot"), ledis::LedisObject::makeString(ledis::Sds(value)));
  db.setMaxmemory(entry_bytes * 2 + entry_bytes / 8);

  dispatch(registry, ctx, db, "SET", {"cold", value.c_str()});
  dispatch(registry, ctx, db, "SET", {"hot", value.c_str()});
  for (int i = 0; i < 8; ++i) {
    dispatch(registry, ctx, db, "GET", {"hot"});
  }

  LEDIS_CHECK(dispatch(registry, ctx, db, "SET", {"new", value.c_str()}).value.bulk.str() == "OK");

  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {"hot"}).value.bulk.str() == value);
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {"cold"}).value.type == ledis::RespType::kNull);
}

}  // namespace

int main() {
  test_evict_on_set();
  test_lru_evicts_coldest();
  test_config_set_maxmemory();
  test_info_memory();
  test_volatile_lru_skips_persistent_keys();
  test_allkeys_lfu_evicts_infrequent();
  std::printf("test_maxmemory: OK\n");
  return 0;
}
