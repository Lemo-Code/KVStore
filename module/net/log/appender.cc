/**
 * @file appender.cc
 * @brief LogAppender / StdoutLogAppender / FileLogAppender 实现。
 */
#include "log/appender.h"
#include "log/file_sink.h"
#include "log/logger.h"
#include "config.h"

#include <ctime>
#include <iostream>

namespace net {

namespace {

/** 日志事件时间（秒），用于切日/切小时边界，不用刷盘时刻的 time(nullptr) */
time_t EventUnixTime(LogEvent::ptr event) {
  return static_cast<time_t>(event->getTime());
}

}  // namespace

/** 基类构造：级别默认 DEBUG，格式器为空 */
LogAppender::LogAppender(const std::string& /*name*/)
    : level_(LogLevel::DEBUG),
      formatter_(nullptr),
      has_formatter_(false),
      async_(nullptr) {}

/** 设置 Appender 级格式器，并更新 has_formatter_ 标记 */
void LogAppender::setFormatter(LogFormatter::ptr formatter) {
  std::lock_guard<MutexType> lock(mutex_);
  formatter_ = formatter;
  has_formatter_ = static_cast<bool>(formatter_);
}

/** 线程安全地返回格式器指针 */
LogFormatter::ptr LogAppender::getFormatter() {
  std::lock_guard<MutexType> lock(mutex_);
  return formatter_;
}

StdoutLogAppender::StdoutLogAppender(const std::string& /*name*/) {}

/**
 * 输出到标准输出。
 * 异步模式：格式化后入队并由 AsyncLogManager 后台刷出；
 * 同步模式：直接 cout，由 mutex_ 保证多线程不乱序。
 */
void StdoutLogAppender::log(std::shared_ptr<Logger> logger,
                            LogLevel::Level level, LogEvent::ptr event,
                            bool async_mode) {
  if (level < level_) {
    return;
  }

  LogFormatter::ptr fmt;
  {
    std::lock_guard<MutexType> lock(mutex_);
    fmt = formatter_;
  }
  if (!fmt) {
    fmt = logger->getFormatter();
  }

  std::string line = fmt->format(logger, level, event);
  if (async_mode) {
    AsyncEnqueueStdout(std::move(line));
    return;
  }

  std::lock_guard<MutexType> lock(mutex_);
  std::cout << line;
}

/** 以追加模式打开目标文件，并注册 FILE 类型异步通道 */
FileLogAppender::FileLogAppender(const std::string& filename)
    : filename_(filename), last_reopen_sec_(0) {
  filestream_.open(filename_, std::ios::app);
}

/**
 * 输出到文件。
 * 同步模式每秒 reopen 一次，防止日志文件被外部删除后无法感知；
 * 异步模式：固定 path，入队即确定目标。
 */
void FileLogAppender::log(std::shared_ptr<Logger> logger,
                          LogLevel::Level level, LogEvent::ptr event,
                          bool async_mode) {
  if (level < level_) {
    return;
  }

  LogFormatter::ptr fmt;
  {
    std::lock_guard<MutexType> lock(mutex_);
    fmt = formatter_;
  }
  if (!fmt) {
    fmt = logger->getFormatter();
  }

  std::string line = fmt->format(logger, level, event);
  if (async_mode) {
    AsyncEnqueueFile(filename_, std::move(line));
    return;
  }

  const uint64_t now = static_cast<uint64_t>(time(nullptr));
  if (now - last_reopen_sec_ >= static_cast<uint64_t>(NET_LOG_FILE_REOPEN_SEC)) {
    reopen();
    last_reopen_sec_ = now;
  }

  std::lock_guard<MutexType> lock(mutex_);
  filestream_ << line;
  filestream_.flush();
}

/** 关闭后重新以 ios::app 打开，返回是否成功 */
bool FileLogAppender::reopen() {
  std::lock_guard<MutexType> lock(mutex_);
  uint64_t size = 0;
  return file_sink::ReopenAppend(filestream_, filename_, &size);
}

RollingFileLogAppender::RollingFileLogAppender(const std::string& filepath,
                                               uint64_t max_file_size,
                                               uint32_t max_files,
                                               file_sink::RollInterval roll_interval)
    : filepath_(filepath),
      max_file_size_(max_file_size > 0 ? max_file_size
                                       : NET_LOG_ROLL_DEFAULT_MAX_BYTES),
      max_files_(max_files > 0 ? max_files : NET_LOG_ROLL_DEFAULT_MAX_FILES),
      roll_interval_(roll_interval),
      current_size_(0),
      last_roll_time_(0) {
  openCurrent();
}

void RollingFileLogAppender::openCurrent() {
  std::lock_guard<MutexType> lock(mutex_);
  file_sink::ReopenAppend(filestream_, filepath_, &current_size_);
  last_roll_time_ = time(nullptr);
}

void RollingFileLogAppender::rollBySizeLocked(bool async_mode) {
  if (async_mode) {
    AsyncLogMgr::GetInstance()->flushFile(filepath_);
  }
  filestream_.close();
  file_sink::RollBySize(filepath_, max_files_);
  if (async_mode) {
    AsyncLogMgr::GetInstance()->reopenFile(filepath_);
  }
  file_sink::ReopenAppend(filestream_, filepath_, &current_size_);
}

void RollingFileLogAppender::rollByTimeLocked(const std::string& suffix,
                                              bool async_mode,
                                              time_t event_time) {
  if (async_mode) {
    AsyncLogMgr::GetInstance()->flushFile(filepath_);
  }
  filestream_.close();
  file_sink::RollByTimeSuffix(filepath_, suffix);
  if (async_mode) {
    AsyncLogMgr::GetInstance()->reopenFile(filepath_);
  }
  file_sink::ReopenAppend(filestream_, filepath_, &current_size_);
  last_roll_time_ = event_time;
}

void RollingFileLogAppender::writeLineLocked(const std::string& line) {
  if (!filestream_) {
    return;
  }
  filestream_ << line;
  filestream_.flush();
  current_size_ += line.size();
}

void RollingFileLogAppender::log(std::shared_ptr<Logger> logger,
                                 LogLevel::Level level, LogEvent::ptr event,
                                 bool async_mode) {
  if (level < level_) {
    return;
  }

  LogFormatter::ptr fmt;
  {
    std::lock_guard<MutexType> lock(mutex_);
    fmt = formatter_;
  }
  if (!fmt) {
    fmt = logger->getFormatter();
  }

  std::string line = fmt->format(logger, level, event);
  const time_t event_time = EventUnixTime(event);
  const uint64_t add = line.size();

  if (async_mode) {
    std::string dest;
    {
      std::lock_guard<MutexType> lock(mutex_);
      const bool time_roll = file_sink::ShouldRollByTime(
          roll_interval_, event_time, last_roll_time_);
      if (time_roll) {
        rollByTimeLocked(file_sink::MakeTimeSuffix(roll_interval_, event_time),
                         true, event_time);
      } else if (max_file_size_ > 0 && current_size_ + add >= max_file_size_) {
        rollBySizeLocked(true);
      }
      dest = filepath_;
      current_size_ += add;
    }
    AsyncEnqueueFile(dest, std::move(line));
    return;
  }

  std::lock_guard<MutexType> lock(mutex_);
  const bool time_roll =
      file_sink::ShouldRollByTime(roll_interval_, event_time, last_roll_time_);

  if (time_roll) {
    rollByTimeLocked(file_sink::MakeTimeSuffix(roll_interval_, event_time),
                     false, event_time);
  } else if (max_file_size_ > 0 && current_size_ + add >= max_file_size_) {
    rollBySizeLocked(false);
  }

  writeLineLocked(line);
}

TimeRotateFileLogAppender::TimeRotateFileLogAppender(
    const std::string& base_path, file_sink::RollInterval interval)
    : base_path_(base_path),
      roll_interval_(interval),
      last_period_(0) {
  std::lock_guard<MutexType> lock(mutex_);
  openDatedLocked(time(nullptr));
}

void TimeRotateFileLogAppender::openDatedLocked(time_t event_time) {
  current_path_ = file_sink::DatedPath(base_path_, roll_interval_, event_time);
  file_sink::ReopenAppend(filestream_, current_path_, nullptr);
  last_period_ = event_time;
}

void TimeRotateFileLogAppender::log(std::shared_ptr<Logger> logger,
                                    LogLevel::Level level,
                                    LogEvent::ptr event, bool async_mode) {
  if (level < level_) {
    return;
  }

  LogFormatter::ptr fmt;
  {
    std::lock_guard<MutexType> lock(mutex_);
    fmt = formatter_;
  }
  if (!fmt) {
    fmt = logger->getFormatter();
  }
  std::string line = fmt->format(logger, level, event);

  const time_t event_time = EventUnixTime(event);
  const std::string dest =
      file_sink::DatedPath(base_path_, roll_interval_, event_time);

  if (async_mode) {
    AsyncEnqueueFile(dest, std::move(line));
    return;
  }

  std::lock_guard<MutexType> lock(mutex_);
  if (file_sink::ShouldRollByTime(roll_interval_, event_time, last_period_)) {
    openDatedLocked(event_time);
  }
  if (filestream_) {
    filestream_ << line;
    filestream_.flush();
  }
}

CircularFileLogAppender::CircularFileLogAppender(
    const std::string& base_path, uint32_t slot_count,
    uint64_t max_bytes_per_slot, const std::vector<std::string>& paths)
    : slot_count_(slot_count > 0 ? slot_count : 1),
      max_bytes_per_slot_(max_bytes_per_slot > 0 ? max_bytes_per_slot : 4096),
      current_slot_(0),
      current_size_(0) {
  if (!paths.empty()) {
    slot_paths_ = paths;
    slot_count_ = static_cast<uint32_t>(slot_paths_.size());
  } else {
    slot_paths_.reserve(slot_count_);
    for (uint32_t i = 0; i < slot_count_; ++i) {
      slot_paths_.push_back(file_sink::SlotPath(base_path, i));
    }
  }
  std::lock_guard<MutexType> lock(mutex_);
  openSlotLocked(0, false, false);
}

void CircularFileLogAppender::openSlotLocked(uint32_t slot, bool truncate_slot,
                                             bool async_mode) {
  current_slot_ = slot % slot_count_;
  const std::string& path = slot_paths_[current_slot_];
  if (truncate_slot && async_mode) {
    AsyncLogMgr::GetInstance()->flushFile(path);
    AsyncLogMgr::GetInstance()->reopenFile(path);
  }
  if (truncate_slot) {
    file_sink::OpenTruncate(filestream_, path);
    current_size_ = 0;
  } else {
    file_sink::ReopenAppend(filestream_, path, &current_size_);
  }
}

void CircularFileLogAppender::advanceSlotLocked(bool async_mode) {
  const uint32_t next = (current_slot_ + 1) % slot_count_;
  filestream_.close();
  openSlotLocked(next, true, async_mode);
}

void CircularFileLogAppender::log(std::shared_ptr<Logger> logger,
                                  LogLevel::Level level, LogEvent::ptr event,
                                  bool async_mode) {
  if (level < level_) {
    return;
  }

  LogFormatter::ptr fmt;
  {
    std::lock_guard<MutexType> lock(mutex_);
    fmt = formatter_;
  }
  if (!fmt) {
    fmt = logger->getFormatter();
  }
  std::string line = fmt->format(logger, level, event);
  const uint64_t add = line.size();

  if (async_mode) {
    std::string dest;
    {
      std::lock_guard<MutexType> lock(mutex_);
      if (max_bytes_per_slot_ > 0 && current_size_ + add >= max_bytes_per_slot_) {
        advanceSlotLocked(true);
      }
      dest = slot_paths_[current_slot_];
      current_size_ += add;
    }
    AsyncEnqueueFile(dest, std::move(line));
    return;
  }

  std::lock_guard<MutexType> lock(mutex_);
  if (max_bytes_per_slot_ > 0 && current_size_ + add >= max_bytes_per_slot_) {
    advanceSlotLocked(false);
  }
  if (filestream_) {
    filestream_ << line;
    filestream_.flush();
    current_size_ += add;
  }
}

}  // namespace net
