#include "test_common.h"

#include "lemo/config/property_loader.h"

#include <map>
#include <string>

int main() {
  const std::string content =
      "# comment\n"
      "log.level = INFO\n"
      "log.file.path=/tmp/x.log\n";
  std::map<std::string, std::string> kv;
  LEMO_CHECK(lemo::config::PropertyLoader::LoadString(content, &kv));
  LEMO_CHECK(kv["log.level"] == "INFO");
  LEMO_CHECK(kv["log.file.path"] == "/tmp/x.log");
  std::printf("PASS test_config_property\n");
  return 0;
}
