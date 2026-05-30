#ifndef NET_LOG_CONFIG_LOG_DEFINE_H
#define NET_LOG_CONFIG_LOG_DEFINE_H

#include "log/file_sink.h"
#include "log/level.h"
#include "build_config.h"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace net {

/**
 * @brief 日志输出地配置（与 Sylar LogAppenderDefine 对齐的子集）。
 *
 * type: 1=File 2=Stdout 4=RollingFile 6=TimeRotate 7=Circular
 */
struct LogAppenderDefine {
  int type = 0;
  LogLevel::Level level = LogLevel::UNKNOWN;
  std::string formatter;
  std::string file;
  uint64_t roll_max_size = 0;
  uint32_t roll_max_files = 0;
  std::string roll_interval;
  uint32_t slot_count = 0;
  uint64_t max_bytes_per_slot = 0;

  bool operator==(const LogAppenderDefine& o) const;
};

/**
 * @brief 单个 Logger 配置（对应 YAML logs 数组的一项）。
 */
struct LogDefine {
  std::string name;
  LogLevel::Level level = LogLevel::UNKNOWN;
  std::string formatter;
  bool async = false;
  std::vector<LogAppenderDefine> appenders;

  bool operator==(const LogDefine& o) const;
  bool operator<(const LogDefine& o) const;
};

file_sink::RollInterval ParseRollInterval(const std::string& s);

/** YAML 序列 <-> logs 配置项 */
std::set<LogDefine> ParseLogDefineSetFromString(const std::string& val);
std::string LogDefineSetToString(const std::set<LogDefine>& val);

}  // namespace net

#include "config/lexical_cast.h"

namespace net {

template <>
struct LexicalCast<std::set<LogDefine>, std::string> {
  std::string operator()(const std::set<LogDefine>& val) {
    return LogDefineSetToString(val);
  }
};

template <>
struct LexicalCast<std::string, std::set<LogDefine>> {
  std::set<LogDefine> operator()(const std::string& val) {
    return ParseLogDefineSetFromString(val);
  }
};

}  // namespace net

#endif  // NET_LOG_CONFIG_LOG_DEFINE_H
