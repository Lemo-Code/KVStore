/**
 * @file test_env.cc
 * @brief Env / Application 启动参数解析
 */
#include "../test_common.h"

#include "ledis/app/env.h"
#include "ledis/config/ledis_settings.h"

namespace {

void test_env_short_options() {
  const char* argv[] = {"ledis-server", "-s", "-c", "conf", "--async"};
  ledis::Env env;
  LEDIS_CHECK(env.init(5, const_cast<char**>(argv)));
  LEDIS_CHECK(env.has("s"));
  LEDIS_CHECK(!env.has("d"));
  LEDIS_CHECK(env.get("c") == "conf");
}

void test_env_skips_long_options() {
  const char* argv[] = {"ledis-server", "--config", "a.yaml", "-d"};
  ledis::Env env;
  LEDIS_CHECK(env.init(4, const_cast<char**>(argv)));
  LEDIS_CHECK(env.has("d"));
  LEDIS_CHECK(!env.has("s"));
}

void test_apply_cli_after_yaml_base() {
  const char* argv[] = {"ledis-server", "-c", "conf", "--async", "10.1.1.1",
                        "17001"};
  ledis::LedisSettings settings;
  settings.host = "yaml-host";
  settings.port = 1111;
  settings.single_thread_mode = true;
  ledis::ApplyLedisCliOverrides(6, const_cast<char**>(argv), &settings);
  LEDIS_CHECK(!settings.single_thread_mode);
  LEDIS_CHECK(settings.host == "10.1.1.1");
  LEDIS_CHECK(settings.port == 17001);
}

}  // namespace

int main() {
  test_env_short_options();
  test_env_skips_long_options();
  test_apply_cli_after_yaml_base();
  std::printf("test_env: OK\n");
  return 0;
}
