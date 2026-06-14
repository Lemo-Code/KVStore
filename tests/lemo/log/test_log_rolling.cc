#include "test_common.h"

#include "lemo/log/rolling_file_appender.h"
#include "lemo/log/pattern_layout.h"

#include <fstream>
#include <string>

int main() {
  const std::string path = "/tmp/lemo_roll_test.log";
  std::remove(path.c_str());
  for (int i = 0; i < 10; ++i) {
    std::string p = path + "." + std::to_string(i);
    std::remove(p.c_str());
  }

  lemo::log::RollingFileAppender appender(path, 64, 3,
                                          lemo::log::RollInterval::kNone);
  lemo::log::Layout::ptr layout(new lemo::log::PatternLayout("%m"));
  appender.SetLayout(layout);

  lemo::log::LogRecord record;
  record.level = lemo::log::LogLevel::INFO;
  record.message = std::string(40, 'x');
  for (int i = 0; i < 4; ++i) {
    appender.Append(record);
  }
  appender.Flush();

  std::ifstream in(path.c_str());
  LEMO_CHECK(in.good());
  std::printf("PASS test_log_rolling\n");
  std::remove(path.c_str());
  return 0;
}
