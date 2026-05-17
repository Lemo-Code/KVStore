#ifndef NET_LOG_APPENDER_H
#define NET_LOG_APPENDER_H

#include "config.h"
#include "log/async_sink.h"
#include "log/event.h"
#include "log/file_sink.h"
#include "log/formatter.h"
#include "log/level.h"

#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace net {

class Logger;  // 前向声明，避免与 appender 头文件循环依赖

/**
 * @brief 日志输出地抽象基类。
 *
 * 具体「单文件 / 多文件 / 轮转 / 组合」见 log/sink.h 的 SinkKind、SinkSet。
 * 边界约定：
 *   - 同步：一条 format 结果作为原子单元；轮转/切日/换槽在写入前于持锁内完成。
 *   - 异步：入队前按 event->getTime() 与 Appender 状态解析目标路径并更新状态；
 *     后台线程只向该路径追加，不在 flush 时再做边界判断。
 */
class LogAppender {
  friend class Logger;

 public:
  typedef std::mutex MutexType;
  typedef std::shared_ptr<LogAppender> ptr;

  // 默认构造，级别阈值为 DEBUG
  explicit LogAppender(const std::string& name = "");

  // 虚析构，支持通过基类指针多态销毁
  virtual ~LogAppender() {}

  // 输出到指定目的地
  virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level,
                   LogEvent::ptr event, bool async_mode) = 0;

  // 设置日志的格式器
  void setFormatter(LogFormatter::ptr formatter);

  // 获得日志的格式器
  LogFormatter::ptr getFormatter();

  // 获得日志输出地的级别阈值
  LogLevel::Level getLevel() const { return level_; }

  // 设置日志输出地的级别阈值
  void setLevel(LogLevel::Level level) { level_ = level; }

  // 是否显式设置过格式器
  bool hasFormatter() const { return has_formatter_; }

 protected:
  LogLevel::Level level_;              // 日志输出地的级别阈值
  LogFormatter::ptr formatter_;        // 日志的格式器
  bool has_formatter_;                 // 是否有独立格式器
  MutexType mutex_;                    // 互斥锁
  AsyncLogChannel::ptr async_;         // 异步输出通道
};

/**
 * @brief 标准输出 Appender。
 */
class StdoutLogAppender : public LogAppender {
  friend class Logger;

 public:
  typedef std::shared_ptr<StdoutLogAppender> ptr;

  // 构造并注册 stdout 异步通道
  explicit StdoutLogAppender(const std::string& name = "");

  // 输出到终端
  void log(std::shared_ptr<Logger> logger, LogLevel::Level level,
           LogEvent::ptr event, bool async_mode) override;
};

/**
 * @brief 单文件输出（无轮转）。
 *
 * 同一 path 的多个 Appender / Logger 在异步模式下共用同一 AsyncLogChannel。
 */
class FileLogAppender : public LogAppender {
  friend class Logger;

 public:
  typedef std::shared_ptr<FileLogAppender> ptr;

  // 以追加模式打开目标文件
  explicit FileLogAppender(const std::string& filename);

  // 输出到文件
  void log(std::shared_ptr<Logger> logger, LogLevel::Level level,
           LogEvent::ptr event, bool async_mode) override;

  // 重新打开文件，成功返回 true
  bool reopen();

  // 获得目标文件路径
  const std::string& getFilename() const { return filename_; }

 private:
  std::string filename_;        // 日志输出文件名
  std::ofstream filestream_;    // 文件操作流（同步模式）
  uint64_t last_reopen_sec_;    // 上次 reopen 检测时刻（秒）
};

/**
 * @brief 轮转文件输出（最多保留 max_files 个历史切片）。
 *
 * 同步/异步均在输出前完成轮转决策；异步按 event 时间与入队时 filepath 路由。
 */
class RollingFileLogAppender : public LogAppender {
  friend class Logger;

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
  void openCurrent();
  void rollBySizeLocked(bool async_mode);
  void rollByTimeLocked(const std::string& suffix, bool async_mode,
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

/**
 * @brief 按日历时间顺序切到新文件（旧文件保留，不覆盖）。
 *
 * 例如 base=/var/log/app → /var/log/app.2025-05-18、app.2025-05-19 …
 * 异步：按 event->getTime() 选择 dated 路径入队，避免刷盘时跨日错写。
 */
class TimeRotateFileLogAppender : public LogAppender {
  friend class Logger;

 public:
  typedef std::shared_ptr<TimeRotateFileLogAppender> ptr;

  TimeRotateFileLogAppender(const std::string& base_path,
                          file_sink::RollInterval interval);

  void log(std::shared_ptr<Logger> logger, LogLevel::Level level,
           LogEvent::ptr event, bool async_mode) override;

  const std::string& getCurrentPath() const { return current_path_; }

 private:
  void openDatedLocked(time_t now);

  std::string base_path_;
  file_sink::RollInterval roll_interval_;
  std::string current_path_;
  std::ofstream filestream_;
  time_t last_period_;
};

/**
 * @brief 环形 N 文件：写满一个槽位后切到下一个，超过 N 回到 0 并丢弃原内容。
 *
 * 槽位路径为 base.0 … base.(N-1)，或用户显式传入 paths。
 * 回到槽位 k 时 truncate 覆盖；异步在入队前完成换槽/truncate，再按槽路径入队。
 */
class CircularFileLogAppender : public LogAppender {
  friend class Logger;

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
