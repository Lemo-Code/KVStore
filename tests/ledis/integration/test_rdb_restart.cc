/**
 * @file test_rdb_restart.cc
 * @brief SAVE 后新 Engine 启动自动加载快照
 */
#include "../test_common.h"

#include "ledis/config/ledis_settings.h"
#include "ledis/session/ledis_engine.h"
#include "ledis/store/snapshot.h"
#include "ledis/types.h"

#include <chrono>
#include <cstdlib>
#include <thread>
#include <unistd.h>

namespace {

ledis::String makeTempDir() {
  char path[] = "/tmp/ledis_rdb_XXXXXX";
  LEDIS_CHECK(mkdtemp(path) != nullptr);
  return ledis::String(path);
}

ledis::Command makeCmd(const char* name) {
  ledis::Command cmd;
  cmd.name = ledis::Sds(name);
  return cmd;
}

void test_engine_restart_load() {
  const ledis::String dir = makeTempDir();
  ledis::LedisSettings settings;
  settings.dir = dir;
  settings.dbfilename = "dump.ledis";

  {
    ledis::LedisEngine engine(settings);
    ledis::SessionContext ctx;

    ledis::Command set = makeCmd("SET");
    set.args.push_back(ledis::Sds("persist_me"));
    set.args.push_back(ledis::Sds("hello"));
    LEDIS_CHECK(engine.dispatchSync(ctx, set).value.bulk.str() == "OK");

    ledis::Command sadd = makeCmd("SADD");
    sadd.args.push_back(ledis::Sds("tags"));
    sadd.args.push_back(ledis::Sds("a"));
    LEDIS_CHECK(engine.dispatchSync(ctx, sadd).value.integer == 1);

    LEDIS_CHECK(engine.saveSnapshotSync());
  }

  ledis::LedisEngine engine2(settings);
  ledis::SessionContext ctx;

  ledis::Command get = makeCmd("GET");
  get.args.push_back(ledis::Sds("persist_me"));
  LEDIS_CHECK(engine2.dispatchSync(ctx, get).value.bulk.str() == "hello");

  ledis::Command sismember = makeCmd("SISMEMBER");
  sismember.args.push_back(ledis::Sds("tags"));
  sismember.args.push_back(ledis::Sds("a"));
  LEDIS_CHECK(engine2.dispatchSync(ctx, sismember).value.integer == 1);
}

void test_bgsave_command() {
  const ledis::String dir = makeTempDir();
  ledis::LedisSettings settings;
  settings.dir = dir;
  settings.dbfilename = "dump.ledis";

  ledis::LedisEngine engine(settings);
  ledis::SessionContext ctx;

  ledis::Command set = makeCmd("SET");
  set.args.push_back(ledis::Sds("bg"));
  set.args.push_back(ledis::Sds("save"));
  LEDIS_CHECK(engine.dispatchSync(ctx, set).value.bulk.str() == "OK");

  ledis::Command bgsave = makeCmd("BGSAVE");
  LEDIS_CHECK(engine.dispatchSync(ctx, bgsave).value.bulk.str() == "OK");

  const ledis::String path = ledis::makeSnapshotPath(dir, settings.dbfilename);
  for (int i = 0; i < 200; ++i) {
    ledis::DBManager probe;
    if (ledis::loadSnapshot(path, &probe)) {
      ledis::LedisObject obj;
      if (probe.keyspace(0).get(ledis::Sds("bg"), &obj)) {
        ledis::Sds value;
        if (obj.asString(&value) && value.str() == "save") {
          return;
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  LEDIS_CHECK(false);
}

}  // namespace

int main() {
  test_engine_restart_load();
  test_bgsave_command();
  std::printf("test_rdb_restart: OK\n");
  return 0;
}
