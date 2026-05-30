/**
 * @file appender.cc
 */
#include "log/appender.h"
#include "log/appender_util.h"
#include "log/async_sink.h"
#include "log/file_sink.h"
#include "log/config/log_config.h"
#include "log/config/build_config.h"

#include <ctime>
#include <iostream>

namespace net {

LogAppender::LogAppender()
    : level_(LogLevel::DEBUG),
      formatter_(nullptr),
      has_formatter_(false) {}

void LogAppender::setFormatter(LogFormatter::ptr formatter) {
  MutexType::Lock lock(mutex_);
  formatter_ = formatter;
  has_formatter_ = static_cast<bool>(formatter_);
}

LogFormatter::ptr LogAppender::getFormatter() {
  MutexType::Lock lock(mutex_);
  return formatter_;
}

StdoutLogAppender::StdoutLogAppender(const std::string& /*name*/) {}

void StdoutLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level,
                            LogEvent::ptr event, bool async_mode) {
  if (!appender_util::ShouldEmit(level, level_)) {
    return;
  }
  std::string line = appender_util::FormatLine(*this, logger, level, event);
  if (async_mode) {
    AsyncEnqueueStdout(std::move(line));
    return;
  }
  MutexType::Lock lock(mutex_);
  std::cout << line;
}

FileLogAppender::FileLogAppender(const std::string& filename)
    : filename_(filename), last_reopen_sec_(0) {
  filestream_.open(filename_, std::ios::app);
}

void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level,
                          LogEvent::ptr event, bool async_mode) {
  if (!appender_util::ShouldEmit(level, level_)) {
    return;
  }
  std::string line = appender_util::FormatLine(*this, logger, level, event);
  if (async_mode) {
    AsyncEnqueueFile(filename_, std::move(line));
    return;
  }
  const uint64_t now = static_cast<uint64_t>(time(nullptr));
  const int reopen_sec = LogConfig::instance().fileReopenSec();
  if (reopen_sec > 0 &&
      now - last_reopen_sec_ >= static_cast<uint64_t>(reopen_sec)) {
    reopen();
    last_reopen_sec_ = now;
  }
  MutexType::Lock lock(mutex_);
  filestream_ << line;
  filestream_.flush();
}

bool FileLogAppender::reopen() {
  MutexType::Lock lock(mutex_);
  uint64_t size = 0;
  return file_sink::ReopenAppend(filestream_, filename_, &size);
}

RollingFileLogAppender::RollingFileLogAppender(const std::string& filepath,
                                               uint64_t max_file_size,
                                               uint32_t max_files,
                                               file_sink::RollInterval roll_interval)
    : filepath_(filepath),
      max_file_size_(max_file_size > 0 ? max_file_size
                                       : LogConfig::instance().rollMaxBytes()),
      max_files_(max_files > 0 ? max_files
                                : LogConfig::instance().rollMaxFiles()),
      roll_interval_(roll_interval),
      current_size_(0),
      last_roll_time_(0) {
  openCurrent();
}

void RollingFileLogAppender::openCurrent() {
  MutexType::Lock lock(mutex_);
  file_sink::ReopenAppend(filestream_, filepath_, &current_size_);
  last_roll_time_ = time(nullptr);
}

void RollingFileLogAppender::rollLocked(RollOp op, const std::string& time_suffix,
                                        bool async_mode, time_t event_time) {
  if (async_mode) {
    AsyncLogMgr::GetInstance()->flushFile(filepath_);
  }
  filestream_.close();
  if (op == RollOp::SIZE) {
    file_sink::RollBySize(filepath_, max_files_);
  } else {
    file_sink::RollByTimeSuffix(filepath_, time_suffix);
    last_roll_time_ = event_time;
  }
  if (async_mode) {
    AsyncLogMgr::GetInstance()->reopenFile(filepath_);
  }
  file_sink::ReopenAppend(filestream_, filepath_, &current_size_);
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
  if (!appender_util::ShouldEmit(level, level_)) {
    return;
  }
  std::string line = appender_util::FormatLine(*this, logger, level, event);
  const time_t event_time = appender_util::EventTime(event);
  const uint64_t add = line.size();

  if (async_mode) {
    MutexType::Lock lock(mutex_);
    if (file_sink::ShouldRollByTime(roll_interval_, event_time, last_roll_time_)) {
      rollLocked(RollOp::TIME,
                 file_sink::MakeTimeSuffix(roll_interval_, event_time), true,
                 event_time);
    } else if (max_file_size_ > 0 && current_size_ + add >= max_file_size_) {
      rollLocked(RollOp::SIZE, "", true, event_time);
    }
    current_size_ += add;
    AsyncEnqueueFile(filepath_, std::move(line));
    return;
  }

  MutexType::Lock lock(mutex_);
  if (file_sink::ShouldRollByTime(roll_interval_, event_time, last_roll_time_)) {
    rollLocked(RollOp::TIME, file_sink::MakeTimeSuffix(roll_interval_, event_time),
               false, event_time);
  } else if (max_file_size_ > 0 && current_size_ + add >= max_file_size_) {
    rollLocked(RollOp::SIZE, "", false, event_time);
  }
  writeLineLocked(line);
}

TimeRotateFileLogAppender::TimeRotateFileLogAppender(
    const std::string& base_path, file_sink::RollInterval interval)
    : base_path_(base_path), roll_interval_(interval), last_period_(0) {
  MutexType::Lock lock(mutex_);
  openDatedLocked(time(nullptr));
}

void TimeRotateFileLogAppender::openDatedLocked(time_t event_time) {
  current_path_ = file_sink::DatedPath(base_path_, roll_interval_, event_time);
  file_sink::ReopenAppend(filestream_, current_path_, nullptr);
  last_period_ = event_time;
}

void TimeRotateFileLogAppender::log(std::shared_ptr<Logger> logger,
                                    LogLevel::Level level, LogEvent::ptr event,
                                    bool async_mode) {
  if (!appender_util::ShouldEmit(level, level_)) {
    return;
  }
  std::string line = appender_util::FormatLine(*this, logger, level, event);
  const time_t event_time = appender_util::EventTime(event);
  const std::string path =
      file_sink::DatedPath(base_path_, roll_interval_, event_time);

  if (async_mode) {
    AsyncEnqueueFile(path, std::move(line));
    return;
  }

  MutexType::Lock lock(mutex_);
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
  MutexType::Lock lock(mutex_);
  openSlotLocked(0, false, false);
}

void CircularFileLogAppender::openSlotLocked(uint32_t slot, bool truncate_slot,
                                             bool async_mode) {
  current_slot_ = slot % slot_count_;
  const std::string& path = slot_paths_[current_slot_];
  if (truncate_slot) {
    if (async_mode) {
      appender_util::AsyncPrepareFileMutation(path);
    }
    file_sink::OpenTruncate(filestream_, path);
    current_size_ = 0;
  } else {
    file_sink::ReopenAppend(filestream_, path, &current_size_);
  }
}

void CircularFileLogAppender::advanceSlotLocked(bool async_mode) {
  filestream_.close();
  openSlotLocked((current_slot_ + 1) % slot_count_, true, async_mode);
}

void CircularFileLogAppender::log(std::shared_ptr<Logger> logger,
                                  LogLevel::Level level, LogEvent::ptr event,
                                  bool async_mode) {
  if (!appender_util::ShouldEmit(level, level_)) {
    return;
  }
  std::string line = appender_util::FormatLine(*this, logger, level, event);
  const uint64_t add = line.size();

  if (async_mode) {
    std::string path;
    {
      MutexType::Lock lock(mutex_);
      if (max_bytes_per_slot_ > 0 && current_size_ + add >= max_bytes_per_slot_) {
        advanceSlotLocked(true);
      }
      path = slot_paths_[current_slot_];
      current_size_ += add;
    }
    AsyncEnqueueFile(path, std::move(line));
    return;
  }

  MutexType::Lock lock(mutex_);
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
