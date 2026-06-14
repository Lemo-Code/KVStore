#include "lemo/log/rolling_file_appender.h"

#include "lemo/log/log_paths.h"

#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace lemo {
namespace log {

RollingFileAppender::RollingFileAppender(const std::string& path,
                                         uint64_t max_bytes, uint32_t max_files,
                                         RollInterval interval)
    : path_(path),
      max_bytes_(max_bytes > 0 ? max_bytes : 100 * 1024 * 1024),
      max_files_(max_files > 0 ? max_files : 10),
      interval_(interval),
      current_bytes_(0),
      last_roll_sec_(0) {
  OpenCurrentUnlocked();
}

RollingFileAppender::ptr RollingFileAppender::ForLogger(
    const std::string& logger_name, const std::string& base_path,
    uint64_t max_bytes, uint32_t max_files, RollInterval interval) {
  return ptr(new RollingFileAppender(
      ResolveLoggerFilePath(logger_name, base_path), max_bytes, max_files,
      interval));
}

void RollingFileAppender::OpenCurrentUnlocked() {
  if (stream_.is_open()) stream_.close();
  stream_.open(path_.c_str(), std::ios::app | std::ios::out | std::ios::binary);
  if (stream_) {
    stream_.seekp(0, std::ios::end);
    current_bytes_ = static_cast<uint64_t>(stream_.tellp());
  }
  last_roll_sec_ = static_cast<uint64_t>(time(NULL));
}

void RollingFileAppender::OpenCurrent() {
  std::lock_guard<std::mutex> lock(mutex_);
  OpenCurrentUnlocked();
}

void RollingFileAppender::RollBySizeUnlocked() {
  if (stream_) stream_.flush();
  stream_.close();
  for (uint32_t i = max_files_; i >= 1; --i) {
    const std::string from =
        (i == 1) ? path_ : (path_ + "." + std::to_string(i - 1));
    const std::string to = path_ + "." + std::to_string(i);
    if (i == 1) {
      std::rename(path_.c_str(), to.c_str());
    } else {
      std::rename(from.c_str(), to.c_str());
    }
  }
  OpenCurrentUnlocked();
}

void RollingFileAppender::RollByTimeUnlocked() {
  const time_t now = time(NULL);
  struct tm t;
  localtime_r(&now, &t);
  std::ostringstream ss;
  ss << std::put_time(&t, "%Y-%m-%d");
  if (interval_ == RollInterval::kHour) {
    ss << "-" << std::setfill('0') << std::setw(2) << t.tm_hour;
  }
  const std::string dest = path_ + "." + ss.str();
  if (stream_) stream_.flush();
  stream_.close();
  std::rename(path_.c_str(), dest.c_str());
  OpenCurrentUnlocked();
}

void RollingFileAppender::RollBySize() {
  std::lock_guard<std::mutex> lock(mutex_);
  RollBySizeUnlocked();
}

void RollingFileAppender::RollByTime() {
  std::lock_guard<std::mutex> lock(mutex_);
  RollByTimeUnlocked();
}

void RollingFileAppender::Append(const LogRecord& record) {
  if (!PassesThreshold(record.level)) return;
  const std::string content = Format(record);
  const uint64_t add = content.size();

  bool time_roll = false;
  if (interval_ != RollInterval::kNone) {
    const time_t now = time(NULL);
    struct tm t_now;
    struct tm t_last;
    localtime_r(&now, &t_now);
    time_t last_ts = static_cast<time_t>(last_roll_sec_);
    localtime_r(&last_ts, &t_last);
    if (interval_ == RollInterval::kDay) {
      time_roll =
          (t_now.tm_year != t_last.tm_year || t_now.tm_yday != t_last.tm_yday);
    } else {
      time_roll = (t_now.tm_year != t_last.tm_year ||
                   t_now.tm_yday != t_last.tm_yday ||
                   t_now.tm_hour != t_last.tm_hour);
    }
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (time_roll || (max_bytes_ > 0 && current_bytes_ + add >= max_bytes_)) {
    if (time_roll) {
      RollByTimeUnlocked();
    } else {
      RollBySizeUnlocked();
    }
  }
  if (stream_) {
    stream_ << content;
    current_bytes_ += add;
  }
}

void RollingFileAppender::Flush() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (stream_) stream_.flush();
}

const char* RollingFileAppender::Type() const { return "rolling_file"; }

}  // namespace log
}  // namespace lemo
