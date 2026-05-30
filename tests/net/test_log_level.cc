#include "test_common.h"

#include "log/log.h"

#include <string>

int main() {
  NET_CHECK(std::string(net::LogLevel::ToString(net::LogLevel::DEBUG)) ==
            "DEBUG");
  NET_CHECK(std::string(net::LogLevel::ToString(net::LogLevel::INFO)) == "INFO");
  NET_CHECK(std::string(net::LogLevel::ToString(net::LogLevel::WARN)) == "WARN");
  NET_CHECK(std::string(net::LogLevel::ToString(net::LogLevel::ERROR)) ==
            "ERROR");
  NET_CHECK(std::string(net::LogLevel::ToString(net::LogLevel::FATAL)) ==
            "FATAL");
  NET_CHECK(std::string(net::LogLevel::ToString(net::LogLevel::UNKNOWN)) ==
            "UNKNOWN");

  NET_CHECK(net::LogLevel::FromString("DEBUG") == net::LogLevel::DEBUG);
  NET_CHECK(net::LogLevel::FromString("info") == net::LogLevel::INFO);
  NET_CHECK(net::LogLevel::FromString("warn") == net::LogLevel::WARN);
  NET_CHECK(net::LogLevel::FromString("error") == net::LogLevel::ERROR);
  NET_CHECK(net::LogLevel::FromString("fatal") == net::LogLevel::FATAL);
  NET_CHECK(net::LogLevel::FromString("invalid") == net::LogLevel::UNKNOWN);

  std::printf("PASS test_log_level\n");
  return 0;
}
