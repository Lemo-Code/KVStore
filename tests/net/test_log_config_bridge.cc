#include "test_common.h"

#include "config/config_center.h"
#include "log/log.h"

#include <fstream>
#include <unistd.h>

namespace {

std::string TmpYaml() {
  return net_test::LogPath("bridge_" + std::to_string(getpid()) + ".yml");
}

}  // namespace

int main() {
  const std::string path = TmpYaml();
  std::ofstream ofs(path);
  ofs << R"(
log:
  level: 3
  pattern: "%m%n"
  async:
    flush_interval_ms: 600
    soft_cap: 3000
    degrade_mode: 1
  file:
    reopen_sec: 2
  roll:
    max_bytes: 1048576
    max_files: 5
logs:
  - name: bridge_test
    level: warn
    async: false
    appenders:
      - type: StdoutLogAppender
)";
  ofs.close();

  NET_CHECK(net::ConfigCenter::loadFromYamlFile(path));
  NET_CHECK(net::LogConfig::instance().defaultLevel() == 3);
  NET_CHECK(net::LogConfig::instance().defaultPattern() == "%m%n");
  NET_CHECK(net::LogConfig::instance().flushIntervalMs() == 600);
  NET_CHECK(net::LogConfig::instance().softCap() == 3000);
  NET_CHECK(net::LogConfig::instance().degradeMode() == 1);
  NET_CHECK(net::LogConfig::instance().fileReopenSec() == 2);
  NET_CHECK(net::LogConfig::instance().rollMaxBytes() == 1048576);
  NET_CHECK(net::LogConfig::instance().rollMaxFiles() == 5);

  net::Logger::ptr logger =
      net::LoggerMgr::GetInstance()->getLogger("bridge_test");
  NET_CHECK(logger->getLevel() == net::LogLevel::WARN);
  NET_CHECK(logger->getAppendersSize() >= 1);

  std::remove(path.c_str());
  std::printf("PASS test_log_config_bridge\n");
  return 0;
}
