/**
 * @file test_aof_restart.cc
 * @brief AOF 启用时 Engine 重启恢复
 */
#include "../test_common.h"

#include "ledis/config/ledis_settings.h"
#include "ledis/session/ledis_engine.h"
#include "ledis/types.h"

#include <chrono>
#include <cstdlib>
#include <thread>
#include <unistd.h>

namespace {

ledis::Command makeCmd(const char* name) {
  ledis::Command cmd;
  cmd.name = ledis::Sds(name);
  return cmd;
}

ledis::String makeTempDir() {
  char path[] = "/tmp/ledis_aof_r_XXXXXX";
  LEDIS_CHECK(mkdtemp(path) != nullptr);
  return ledis::String(path);
}

void test_engine_aof_restart() {
  const ledis::String dir = makeTempDir();
  ledis::LedisSettings settings;
  settings.dir = dir;
  settings.appendonly = true;
  settings.appendfilename = "appendonly.aof";

  {
    ledis::LedisEngine engine(settings);
    ledis::SessionContext ctx;
    ledis::Command set = makeCmd("SET");
    set.args.push_back(ledis::Sds("persist_aof"));
    set.args.push_back(ledis::Sds("hello"));
    LEDIS_CHECK(engine.dispatchSync(ctx, set).value.bulk.str() == "OK");
  }

  ledis::LedisEngine engine2(settings);
  ledis::SessionContext ctx;
  ledis::Command get = makeCmd("GET");
  get.args.push_back(ledis::Sds("persist_aof"));
  LEDIS_CHECK(engine2.dispatchSync(ctx, get).value.bulk.str() == "hello");
}

void test_bgrewriteaof() {
  const ledis::String dir = makeTempDir();
  ledis::LedisSettings settings;
  settings.dir = dir;
  settings.appendonly = true;
  settings.appendfilename = "appendonly.aof";

  ledis::LedisEngine engine(settings);
  ledis::SessionContext ctx;
  ledis::Command set = makeCmd("SET");
  set.args.push_back(ledis::Sds("before"));
  set.args.push_back(ledis::Sds("1"));
  LEDIS_CHECK(engine.dispatchSync(ctx, set).value.bulk.str() == "OK");

  ledis::Command rewrite = makeCmd("BGREWRITEAOF");
  LEDIS_CHECK(engine.dispatchSync(ctx, rewrite).value.bulk.str() == "OK");

  set.args[0] = ledis::Sds("during");
  set.args[1] = ledis::Sds("2");
  LEDIS_CHECK(engine.dispatchSync(ctx, set).value.bulk.str() == "OK");

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  ledis::Command get = makeCmd("GET");
  get.args.push_back(ledis::Sds("before"));
  LEDIS_CHECK(engine.dispatchSync(ctx, get).value.bulk.str() == "1");
  get.args[0] = ledis::Sds("during");
  LEDIS_CHECK(engine.dispatchSync(ctx, get).value.bulk.str() == "2");

  ledis::LedisEngine engine2(settings);
  ledis::SessionContext ctx2;
  get.args[0] = ledis::Sds("before");
  LEDIS_CHECK(engine2.dispatchSync(ctx2, get).value.bulk.str() == "1");
  get.args[0] = ledis::Sds("during");
  LEDIS_CHECK(engine2.dispatchSync(ctx2, get).value.bulk.str() == "2");
}

}  // namespace

int main() {
  test_engine_aof_restart();
  test_bgrewriteaof();
  std::printf("test_aof_restart: OK\n");
  return 0;
}
