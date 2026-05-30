#ifndef NET_LOG_LOGGER_H
#define NET_LOG_LOGGER_H

#include "log/appender.h"
#include "log/event.h"
#include "log/formatter.h"
#include "log/level.h"
#include "log/config/build_config.h"

#include "thread/mutex.h"

#include <list>
#include <memory>
#include <string>

namespace net {

class Logger;

namespace detail {
void AttachRoot(const std::shared_ptr<Logger>& child,
                const std::shared_ptr<Logger>& root);
}

/**
 * @brief 日志器，聚合格式器与多个 Appender。
 *
 * 若本 Logger 未配置 Appender，则委托 root Logger 输出（由 LoggerManager 注入）。
 * async_mode_ 为 true 时，所有 Appender 走异步通道。
 */
class Logger : public std::enable_shared_from_this<Logger> {
  friend class LoggerManager;
  friend class AsyncLoggerManager;
  friend void detail::AttachRoot(const std::shared_ptr<Logger>&,
                                 const std::shared_ptr<Logger>&);

 public:
  typedef Spinlock MutexType;
  typedef std::shared_ptr<Logger> ptr;

  // 参数的初始化
  explicit Logger(const std::string& name = "root", bool async_mode = false);

  // 输出到不同的目的地（核心入口）
  void log(LogLevel::Level level, LogEvent::ptr event);

  // 输出 debug 级别的日志
  void debug(LogEvent::ptr event);

  // 输出 info 级别的日志
  void info(LogEvent::ptr event);

  // 输出 warn 级别的日志
  void warn(LogEvent::ptr event);

  // 输出 error 级别的日志
  void error(LogEvent::ptr event);

  // 输出 fatal 级别的日志
  void fatal(LogEvent::ptr event);

  // 添加日志输出地
  void addAppender(LogAppender::ptr appender);

  // 删除日志输出地
  void delAppender(LogAppender::ptr appender);

  // 清空日志输出地
  void clearAppenders();

  // 获得 appenders 容器的大小
  size_t getAppendersSize() const { return appenders_.size(); }

  // 指定的 appender 是否存在
  bool isAppenderExists(LogAppender::ptr appender) const;

  // 设置日志的格式器（智能指针版本）
  void setFormatter(LogFormatter::ptr formatter);

  // 设置日志的格式（模式字符串版本）
  void setFormatter(const std::string& pattern);

  // 获得日志的格式字符串
  std::string getFormatterStr() const;

  // 获得日志的格式器
  LogFormatter::ptr getFormatter() const;

  // 获得日志器级别阈值
  LogLevel::Level getLevel() const { return level_; }

  // 设置日志器级别阈值
  void setLevel(LogLevel::Level level) { level_ = level; }

  // 获取日志的名称
  const std::string& getName() const { return name_; }

  // 设置日志名称
  void setName(const std::string& name) { name_ = name; }

  // 设置异步输出模式
  void setAsync(bool async_mode) { async_mode_ = async_mode; }

  // 获得异步方式
  bool getAsync() const { return async_mode_; }

 private:
  bool async_mode_;                        // 是否异步输出
  std::string name_;                       // 日志名称
  LogLevel::Level level_;                  // 日志级别阈值
  LogFormatter::ptr formatter_;            // 日志格式器
  std::list<LogAppender::ptr> appenders_;  // 日志输出地列表
  Logger::ptr root_;                       // 默认 root 日志器（回退链）
  mutable MutexType mutex_;                // 互斥锁
};

}  // namespace net

#endif  // NET_LOG_LOGGER_H
