#include "test_common.h"

#include "lemo/log/appender_registry.h"

int main() {
  lemo::log::AppenderRegistry& reg = lemo::log::AppenderRegistry::Instance();
  LEMO_CHECK(reg.Has("console"));
  LEMO_CHECK(reg.Has("file"));
  LEMO_CHECK(reg.Has("rolling_file"));
  LEMO_CHECK(reg.Has("async"));

  lemo::log::AppenderConfig cfg;
  cfg.properties["path"] = "/tmp/lemo_registry_test.log";
  lemo::log::Appender::ptr file = reg.Create("file", cfg);
  LEMO_CHECK(file.get() != NULL);
  LEMO_CHECK(std::string(file->Type()) == "file");
  std::printf("PASS test_log_registry\n");
  return 0;
}
