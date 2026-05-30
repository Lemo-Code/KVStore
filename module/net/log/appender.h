#ifndef NET_LOG_APPENDER_H
#define NET_LOG_APPENDER_H

#include "log/config/build_config.h"
#include "log/event.h"
#include "log/file_sink.h"
#include "log/formatter.h"
#include "log/level.h"

#include "thread/mutex.h"

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace net {

class Logger;

/**
 * @brief 日志输出地抽象基类。
 *
 * 方案组合见 log/sink.h。同步在持锁内完成轮转/切日/换槽；异步入队前定路径。
 */
class LogAppender {
  friend class Logger;

 public:
  typedef Spinlock MutexType;
  typedef std::shared_ptr<LogAppender> ptr;

  LogAppender();
  virtual ~LogAppender() {}

  virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level,
                   LogEvent::ptr event, bool async_mode) = 0;

  void setFormatter(LogFormatter::ptr formatter);
  LogFormatter::ptr getFormatter();

  LogLevel::Level getLevel() const { return level_; }
  void setLevel(LogLevel::Level level) { level_ = level; }
  bool hasFormatter() const { return has_formatter_; }

 protected:
  LogLevel::Level level_;
  LogFormatter::ptr formatter_;
  bool has_formatter_;
  MutexType mutex_;
};

class StdoutLogAppender : public LogAppender {
 public:
  typedef std::shared_ptr<StdoutLogAppender> ptr;

  explicit StdoutLogAppender(const std::string& /*name*/ = "");

  void log(std::shared_ptr<Logger> logger, LogLevel::Level level,
           LogEvent::ptr event, bool async_mode) override;
};

class FileLogAppender : public LogAppender {
 public:
  typedef std::shared_ptr<FileLogAppender> ptr;

  explicit FileLogAppender(const std::string& filename);
  void log(std::shared_ptr<Logger> logger, LogLevel::Level level,
           LogEvent::ptr event, bool async_mode) override;
  bool reopen();
  const std::string& getFilename() const { return filename_; }

 private:
  std::string filename_;
  std::ofstream filestream_;
  uint64_t last_reopen_sec_;
};

class RollingFileLogAppender : public LogAppender {
 public:
  typedef std::shared_ptr<RollingFileLogAppender> ptr;

  RollingFileLogAppender(const std::string& filepath,
                         uint64_t max_file_size = NET_LOG_ROLL_DEFAULT_MAX_BYTES,
                         uint32_t max_files = NET_LOG_ROLL_DEFAULT_MAX_FILES,
                         file_sink::RollInterval roll_interval =
                             file_sink::RollInterval::NONE);

  void log(std::shared_ptr<Logger> logger, LogLevel::Level level,
           LogEvent::ptr event, bool async_mode) override;
  const std::string& getFilepath() const { return filepath_; }

 private:
  enum class RollOp { SIZE, TIME };

  void openCurrent();
  void rollLocked(RollOp op, const std::string& time_suffix, bool async_mode,
                  time_t event_time);
  void writeLineLocked(const std::string& line);

  std::string filepath_;
  uint64_t max_file_size_;
  uint32_t max_files_;
  file_sink::RollInterval roll_interval_;
  std::ofstream filestream_;
  uint64_t current_size_;
  time_t last_roll_time_;
};

class TimeRotateFileLogAppender : public LogAppender {
 public:
  typedef std::shared_ptr<TimeRotateFileLogAppender> ptr;

  TimeRotateFileLogAppender(const std::string& base_path,
                            file_sink::RollInterval interval);

  void log(std::shared_ptr<Logger> logger, LogLevel::Level level,
           LogEvent::ptr event, bool async_mode) override;
  const std::string& getCurrentPath() const { return current_path_; }

 private:
  void openDatedLocked(time_t event_time);

  std::string base_path_;
  file_sink::RollInterval roll_interval_;
  std::string current_path_;
  std::ofstream filestream_;
  time_t last_period_;
};

class CircularFileLogAppender : public LogAppender {
 public:
  typedef std::shared_ptr<CircularFileLogAppender> ptr;

  CircularFileLogAppender(const std::string& base_path, uint32_t slot_count,
                          uint64_t max_bytes_per_slot,
                          const std::vector<std::string>& paths = {});

  void log(std::shared_ptr<Logger> logger, LogLevel::Level level,
           LogEvent::ptr event, bool async_mode) override;
  uint32_t getCurrentSlot() const { return current_slot_; }

 private:
  void openSlotLocked(uint32_t slot, bool truncate_slot, bool async_mode);
  void advanceSlotLocked(bool async_mode);

  std::vector<std::string> slot_paths_;
  uint32_t slot_count_;
  uint64_t max_bytes_per_slot_;
  uint32_t current_slot_;
  uint64_t current_size_;
  std::ofstream filestream_;
};

}  // namespace net

#endif  // NET_LOG_APPENDER_H
