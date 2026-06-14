/**
 * @file test_aof.cc
 * @brief AOF 写入与重放
 */
#include "../test_common.h"

#include "ledis/command/command.h"
#include "ledis/command/registry.h"
#include "ledis/session/session_context.h"
#include "ledis/store/aof.h"
#include "ledis/store/db_manager.h"
#include "ledis/types.h"

#include <cstdlib>
#include <unistd.h>

namespace {

ledis::Command makeCmd(const char* name) {
  ledis::Command cmd;
  cmd.name = ledis::Sds(name);
  return cmd;
}

ledis::String makeTempDir() {
  char path[] = "/tmp/ledis_aof_XXXXXX";
  LEDIS_CHECK(mkdtemp(path) != nullptr);
  return ledis::String(path);
}

void test_aof_append_and_load() {
  const ledis::String dir = makeTempDir();
  const ledis::String path = ledis::makeAofPath(dir, "appendonly.aof");

  {
    ledis::AofWriter writer;
    LEDIS_CHECK(writer.open(path, true));
    ledis::Command set = makeCmd("SET");
    set.args.push_back(ledis::Sds("aof_key"));
    set.args.push_back(ledis::Sds("aof_val"));
    LEDIS_CHECK(writer.append(set));
    LEDIS_CHECK(writer.flush());
  }

  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();
  LEDIS_CHECK(ledis::loadAof(path, &db, &registry, &ctx));

  ledis::LedisObject obj;
  LEDIS_CHECK(db.keyspace(0).get(ledis::Sds("aof_key"), &obj));
  ledis::Sds value;
  LEDIS_CHECK(obj.asString(&value));
  LEDIS_CHECK(value.str() == "aof_val");
}

void test_aof_skips_read_commands() {
  const ledis::String dir = makeTempDir();
  const ledis::String path = ledis::makeAofPath(dir, "mixed.aof");

  {
    ledis::AofWriter writer;
    LEDIS_CHECK(writer.open(path, true));
    ledis::Command set = makeCmd("SET");
    set.args.push_back(ledis::Sds("k"));
    set.args.push_back(ledis::Sds("v"));
    LEDIS_CHECK(writer.append(set));
    LEDIS_CHECK(isAofWriteCommand(set));
    ledis::Command get = makeCmd("GET");
    get.args.push_back(ledis::Sds("k"));
    LEDIS_CHECK(!isAofWriteCommand(get));
  }

  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();
  LEDIS_CHECK(ledis::loadAof(path, &db, &registry, &ctx));
  LEDIS_CHECK(db.keyspace(0).exists(ledis::Sds("k")));
}

void test_appendfsync_policy_parse() {
  bool ok = false;
  LEDIS_CHECK(ledis::parseAppendFsyncPolicy("always", &ok) ==
              ledis::AppendFsyncPolicy::kAlways);
  LEDIS_CHECK(ok);
  LEDIS_CHECK(ledis::parseAppendFsyncPolicy("everysec", &ok) ==
              ledis::AppendFsyncPolicy::kEverySec);
  LEDIS_CHECK(ok);
  LEDIS_CHECK(ledis::parseAppendFsyncPolicy("no", &ok) == ledis::AppendFsyncPolicy::kNo);
  LEDIS_CHECK(ok);
  (void)ledis::parseAppendFsyncPolicy("invalid", &ok);
  LEDIS_CHECK(!ok);
}

void test_rewrite_aof_from_db() {
  const ledis::String dir = makeTempDir();
  const ledis::String path = ledis::makeAofPath(dir, "rewrite.aof");

  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  ledis::Command set = makeCmd("SET");
  set.args.push_back(ledis::Sds("rewrite_key"));
  set.args.push_back(ledis::Sds("rewrite_val"));
  LEDIS_CHECK(registry.dispatch(ctx, db, set).value.bulk.str() == "OK");

  ledis::Command hset = makeCmd("HSET");
  hset.args.push_back(ledis::Sds("hash"));
  hset.args.push_back(ledis::Sds("f1"));
  hset.args.push_back(ledis::Sds("v1"));
  LEDIS_CHECK(registry.dispatch(ctx, db, hset).value.type == ledis::RespType::kInteger);

  LEDIS_CHECK(ledis::rewriteAofFromDb(db, path, 0));

  ledis::DBManager loaded;
  ledis::SessionContext load_ctx;
  LEDIS_CHECK(ledis::loadAof(path, &loaded, &registry, &load_ctx));
  LEDIS_CHECK(loaded.keyspace(0).exists(ledis::Sds("rewrite_key")));
  LEDIS_CHECK(loaded.keyspace(0).exists(ledis::Sds("hash")));
}

}  // namespace

int main() {
  test_aof_append_and_load();
  test_aof_skips_read_commands();
  test_appendfsync_policy_parse();
  test_rewrite_aof_from_db();
  std::printf("test_aof: OK\n");
  return 0;
}
