/**
 * @file log_define.cc
 */
#include "log/config/log_define.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <iostream>

namespace net {

namespace {

std::string Lower(const std::string& s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

LogAppenderDefine ParseAppenderNode(const YAML::Node& a) {
  LogAppenderDefine lad;
  if (!a["type"].IsDefined()) {
    return lad;
  }

  const std::string type = a["type"].as<std::string>();
  if (type == "FileLogAppender") {
    lad.type = 1;
    if (!a["file"].IsDefined()) {
      std::cerr << "log config: FileLogAppender missing file\n";
      return LogAppenderDefine();
    }
    lad.file = a["file"].as<std::string>();
  } else if (type == "StdoutLogAppender" || type == "StdoutAppender") {
    lad.type = 2;
  } else if (type == "RollingFileLogAppender" || type == "RollingFileAppender") {
    lad.type = 4;
    if (!a["file"].IsDefined()) {
      std::cerr << "log config: RollingFileLogAppender missing file\n";
      return LogAppenderDefine();
    }
    lad.file = a["file"].as<std::string>();
    if (a["max_file_size"].IsDefined()) {
      lad.roll_max_size = static_cast<uint64_t>(a["max_file_size"].as<int64_t>());
    }
    if (a["max_files"].IsDefined()) {
      lad.roll_max_files = static_cast<uint32_t>(a["max_files"].as<int>());
    }
    if (a["roll_interval"].IsDefined()) {
      lad.roll_interval = a["roll_interval"].as<std::string>();
    }
  } else if (type == "TimeRotateFileLogAppender" ||
             type == "TimeRotateLogAppender") {
    lad.type = 6;
    if (!a["file"].IsDefined() && !a["path"].IsDefined()) {
      std::cerr << "log config: TimeRotateFileLogAppender missing file/path\n";
      return LogAppenderDefine();
    }
    lad.file = a["file"].IsDefined() ? a["file"].as<std::string>()
                                     : a["path"].as<std::string>();
    if (a["roll_interval"].IsDefined()) {
      lad.roll_interval = a["roll_interval"].as<std::string>();
    }
  } else if (type == "CircularFileLogAppender" ||
             type == "CircularLogAppender") {
    lad.type = 7;
    if (!a["file"].IsDefined() && !a["path"].IsDefined()) {
      std::cerr << "log config: CircularFileLogAppender missing file/path\n";
      return LogAppenderDefine();
    }
    lad.file = a["file"].IsDefined() ? a["file"].as<std::string>()
                                     : a["path"].as<std::string>();
    if (a["slot_count"].IsDefined()) {
      lad.slot_count = static_cast<uint32_t>(a["slot_count"].as<int>());
    }
    if (a["max_bytes_per_slot"].IsDefined()) {
      lad.max_bytes_per_slot =
          static_cast<uint64_t>(a["max_bytes_per_slot"].as<int64_t>());
    }
  } else {
    std::cerr << "log config: unknown appender type " << type << '\n';
    return LogAppenderDefine();
  }

  if (a["level"].IsDefined()) {
    lad.level = LogLevel::FromString(a["level"].as<std::string>());
  }
  if (a["formatter"].IsDefined()) {
    lad.formatter = a["formatter"].as<std::string>();
  }
  return lad;
}

}  // namespace

bool LogAppenderDefine::operator==(const LogAppenderDefine& o) const {
  return type == o.type && level == o.level && formatter == o.formatter &&
         file == o.file && roll_max_size == o.roll_max_size &&
         roll_max_files == o.roll_max_files &&
         roll_interval == o.roll_interval && slot_count == o.slot_count &&
         max_bytes_per_slot == o.max_bytes_per_slot;
}

bool LogDefine::operator==(const LogDefine& o) const {
  return name == o.name && level == o.level && formatter == o.formatter &&
         async == o.async && appenders == o.appenders;
}

bool LogDefine::operator<(const LogDefine& o) const { return name < o.name; }

file_sink::RollInterval ParseRollInterval(const std::string& s) {
  const std::string k = Lower(s);
  if (k == "hour") {
    return file_sink::RollInterval::HOUR;
  }
  if (k == "day") {
    return file_sink::RollInterval::DAY;
  }
  return file_sink::RollInterval::NONE;
}

std::string LogDefineSetToString(const std::set<LogDefine>& val) {
  YAML::Node node;
  for (const auto& i : val) {
    YAML::Node n;
    n["name"] = i.name;
    if (i.level != LogLevel::UNKNOWN) {
      n["level"] = LogLevel::ToString(i.level);
    }
    n["async"] = i.async;
    if (!i.formatter.empty()) {
      n["formatter"] = i.formatter;
    }
    for (const auto& a : i.appenders) {
      YAML::Node na;
      if (a.type == 1) {
        na["type"] = "FileLogAppender";
        na["file"] = a.file;
      } else if (a.type == 2) {
        na["type"] = "StdoutLogAppender";
      } else if (a.type == 4) {
        na["type"] = "RollingFileLogAppender";
        na["file"] = a.file;
        na["max_file_size"] = static_cast<int64_t>(a.roll_max_size);
        na["max_files"] = a.roll_max_files;
        if (!a.roll_interval.empty()) {
          na["roll_interval"] = a.roll_interval;
        }
      } else if (a.type == 6) {
        na["type"] = "TimeRotateFileLogAppender";
        na["file"] = a.file;
        if (!a.roll_interval.empty()) {
          na["roll_interval"] = a.roll_interval;
        }
      } else if (a.type == 7) {
        na["type"] = "CircularFileLogAppender";
        na["file"] = a.file;
        na["slot_count"] = static_cast<int>(a.slot_count);
        na["max_bytes_per_slot"] = static_cast<int64_t>(a.max_bytes_per_slot);
      }
      if (a.level != LogLevel::UNKNOWN) {
        na["level"] = LogLevel::ToString(a.level);
      }
      if (!a.formatter.empty()) {
        na["formatter"] = a.formatter;
      }
      n["appenders"].push_back(na);
    }
    node.push_back(n);
  }
  YAML::Emitter emitter;
  emitter << node;
  return emitter.c_str();
}

std::set<LogDefine> ParseLogDefineSetFromString(const std::string& val) {
  std::set<LogDefine> result;
  if (val.empty()) {
    return result;
  }

  YAML::Node node = YAML::Load(val);
  if (!node.IsSequence()) {
    return result;
  }

  const uint64_t default_roll =
      static_cast<uint64_t>(NET_LOG_ROLL_DEFAULT_MAX_BYTES);
  const uint32_t default_files = NET_LOG_ROLL_DEFAULT_MAX_FILES;

  for (size_t i = 0; i < node.size(); ++i) {
    const YAML::Node n = node[i];
    if (!n["name"].IsDefined()) {
      std::cerr << "log config: logger name is null\n";
      continue;
    }

    LogDefine ld;
    ld.name = n["name"].as<std::string>();
    if (n["level"].IsDefined()) {
      ld.level = LogLevel::FromString(n["level"].as<std::string>());
    }
    if (n["async"].IsDefined()) {
      ld.async = n["async"].as<bool>();
    } else if (n["isAsyc"].IsDefined()) {
      ld.async = n["isAsyc"].as<bool>();
    }
    if (n["formatter"].IsDefined()) {
      ld.formatter = n["formatter"].as<std::string>();
    }

    if (n["appenders"].IsDefined()) {
      for (size_t x = 0; x < n["appenders"].size(); ++x) {
        LogAppenderDefine lad = ParseAppenderNode(n["appenders"][x]);
        if (lad.type == 0) {
          continue;
        }
        if (lad.type == 4 && lad.roll_max_size == 0) {
          lad.roll_max_size = default_roll;
        }
        if (lad.type == 4 && lad.roll_max_files == 0) {
          lad.roll_max_files = default_files;
        }
        ld.appenders.push_back(lad);
      }
    }
    result.insert(ld);
  }
  return result;
}

}  // namespace net
