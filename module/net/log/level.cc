/**
 * @file level.cc
 * @brief LogLevel 枚举与字符串互转实现。
 */
#include "log/level.h"

namespace net {

/** 级别枚举 -> 字符串映射表（X-Macro 展开） */
const char* LogLevel::ToString(LogLevel::Level level) {
  switch (level) {
#define XX(name)          \
  case LogLevel::name:    \
    return #name;         \
    break;
    XX(DEBUG);
    XX(INFO);
    XX(WARN);
    XX(ERROR);
    XX(FATAL);
#undef XX
    default:
      return "UNKNOWN";
  }
}

/** 字符串 -> 级别枚举，兼容大小写及全大写形式 */
LogLevel::Level LogLevel::FromString(const std::string& str) {
#define XX(level, name)       \
  if (str == #name) {         \
    return LogLevel::level;   \
  }
  XX(DEBUG, DEBUG)
  XX(INFO, INFO)
  XX(WARN, WARN)
  XX(ERROR, ERROR)
  XX(FATAL, FATAL)
  XX(DEBUG, debug)
  XX(INFO, info)
  XX(WARN, warn)
  XX(ERROR, error)
  XX(FATAL, fatal)
#undef XX
  return LogLevel::UNKNOWN;
}

}  // namespace net
