/**
 * @file test_ledis_yaml_config.cc
 * @brief YAML 配置加载
 */
#include "../test_common.h"

#include "ledis/config/ledis_settings.h"
#include "ledis/config/ledis_yaml_config.h"

#include <cstdio>
#include <unistd.h>

namespace {

ledis::String writeTempYaml() {
  char path[] = "/tmp/ledis_yaml_XXXXXX";
  LEDIS_CHECK(mkdtemp(path) != nullptr);
  const ledis::String file = ledis::String(path) + "/ledis.yaml";
  FILE* fp = std::fopen(file.c_str(), "wb");
  LEDIS_CHECK(fp != nullptr);
  std::fprintf(fp,
               "server:\n"
               "  host: 10.0.0.2\n"
               "  port: 16380\n"
               "ledis:\n"
               "  single_thread_mode: false\n"
               "  maxclients: 500\n"
               "  maxmemory: 1048576\n"
               "  maxmemory_policy: volatile-lru\n"
               "  appendonly: true\n"
               "  appendfsync: no\n"
               "  requirepass: secret\n");
  std::fclose(fp);
  return file;
}

void test_load_yaml_file() {
  const ledis::String path = writeTempYaml();
  ledis::LedisSettings settings;
  LEDIS_CHECK(ledis::LoadLedisSettingsFromYamlFile(path, &settings));
  LEDIS_CHECK(settings.host == "10.0.0.2");
  LEDIS_CHECK(settings.port == 16380);
  LEDIS_CHECK(!settings.single_thread_mode);
  LEDIS_CHECK(settings.maxclients == 500);
  LEDIS_CHECK(settings.maxmemory == 1048576);
  LEDIS_CHECK(settings.maxmemory_policy == "volatile-lru");
  LEDIS_CHECK(settings.appendonly);
  LEDIS_CHECK(settings.appendfsync == "no");
  LEDIS_CHECK(settings.requirepass == "secret");
}

void test_cli_overrides_yaml() {
  const ledis::String path = writeTempYaml();
  const char* argv[] = {"ledis-server", "--config", path.c_str(), "192.168.1.1",
                        "17000"};
  const ledis::LedisSettings settings =
      ledis::ParseLedisSettingsFromArgs(5, const_cast<char**>(argv));
  LEDIS_CHECK(settings.host == "192.168.1.1");
  LEDIS_CHECK(settings.port == 17000);
  LEDIS_CHECK(settings.maxmemory == 1048576);
  LEDIS_CHECK(settings.maxmemory_policy == "volatile-lru");
}

}  // namespace

int main() {
  test_load_yaml_file();
  test_cli_overrides_yaml();
  std::printf("test_ledis_yaml_config: OK\n");
  return 0;
}
