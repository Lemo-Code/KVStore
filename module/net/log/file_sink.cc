/**
 * @file file_sink.cc
 * @brief 文件输出与轮转辅助实现。
 */
#include "log/file_sink.h"

#include <cstdio>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace net {
namespace file_sink {

bool OpenAppend(std::ofstream& out, const std::string& path) {
  out.open(path, std::ios::app | std::ios::out);
  return static_cast<bool>(out);
}

bool OpenTruncate(std::ofstream& out, const std::string& path) {
  out.open(path, std::ios::out | std::ios::trunc);
  return static_cast<bool>(out);
}

std::string SlotPath(const std::string& base, uint32_t index) {
  return base + "." + std::to_string(index);
}

std::string DatedPath(const std::string& base, RollInterval interval, time_t now) {
  return base + "." + MakeTimeSuffix(interval, now);
}

uint64_t FileSize(const std::string& path) {
  std::ifstream in(path, std::ios::ate | std::ios::binary);
  if (!in) {
    return 0;
  }
  return static_cast<uint64_t>(in.tellg());
}

bool ReopenAppend(std::ofstream& out, const std::string& path, uint64_t* out_size) {
  if (out.is_open()) {
    out.close();
  }
  if (!OpenAppend(out, path)) {
    return false;
  }
  if (out_size) {
    *out_size = FileSize(path);
  }
  return true;
}

bool RollBySize(const std::string& path, uint32_t max_files) {
  if (max_files == 0) {
    return false;
  }
  const std::string oldest = path + "." + std::to_string(max_files);
  std::remove(oldest.c_str());

  for (uint32_t i = max_files; i >= 1; --i) {
    const std::string to = path + "." + std::to_string(i);
    if (i == 1) {
      if (std::rename(path.c_str(), to.c_str()) != 0) {
        return false;
      }
    } else {
      const std::string from = path + "." + std::to_string(i - 1);
      std::rename(from.c_str(), to.c_str());
    }
  }
  return true;
}

bool RollByTimeSuffix(const std::string& path, const std::string& suffix) {
  const std::string dest = path + "." + suffix;
  if (std::rename(path.c_str(), dest.c_str()) != 0) {
    return false;
  }
  return true;
}

bool ShouldRollByTime(RollInterval interval, time_t now, time_t last_roll) {
  if (interval == RollInterval::NONE) {
    return false;
  }
  struct tm t_now;
  struct tm t_last;
  localtime_r(&now, &t_now);
  localtime_r(&last_roll, &t_last);

  if (interval == RollInterval::DAY) {
    return t_now.tm_year != t_last.tm_year || t_now.tm_yday != t_last.tm_yday;
  }
  if (interval == RollInterval::HOUR) {
    return t_now.tm_year != t_last.tm_year || t_now.tm_yday != t_last.tm_yday ||
           t_now.tm_hour != t_last.tm_hour;
  }
  return false;
}

std::string MakeTimeSuffix(RollInterval interval, time_t now) {
  struct tm t;
  localtime_r(&now, &t);
  std::ostringstream ss;
  ss << std::put_time(&t, "%Y-%m-%d");
  if (interval == RollInterval::HOUR) {
    ss << "-" << std::setfill('0') << std::setw(2) << t.tm_hour;
  }
  return ss.str();
}

}  // namespace file_sink
}  // namespace net
