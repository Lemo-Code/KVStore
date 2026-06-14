#include "test_common.h"

#include "lemo/log/level.h"

#include <string>

int main() {
  LEMO_CHECK(std::string(lemo::log::LogLevel::ToString(lemo::log::LogLevel::DEBUG)) ==
             "DEBUG");
  LEMO_CHECK(lemo::log::LogLevel::FromString("info") == lemo::log::LogLevel::INFO);
  LEMO_CHECK(lemo::log::LogLevel::FromString("off") == lemo::log::LogLevel::OFF);
  std::printf("PASS test_log_level\n");
  return 0;
}
