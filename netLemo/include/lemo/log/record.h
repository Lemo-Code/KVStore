#pragma once

#include "lemo/log/level.h"

#include <cstdint>
#include <map>
#include <string>

namespace lemo {
namespace log {

// 传递给 Appender 的不可变记录
struct LogRecord {
  LogLevel::Level level = LogLevel::INFO;
  std::string logger_name;
  std::string message;
  const char* file = "";
  int32_t line = 0;
  uint32_t elapse = 0;
  uint32_t thread_id = 0;
  uint32_t fiber_id = 0;
  uint64_t timestamp = 0;
  std::string thread_name;
  std::map<std::string, std::string> mdc;
};

}  // namespace log
}  // namespace lemo
