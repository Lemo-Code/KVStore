#ifndef NET_LOG_MANAGER_H
#define NET_LOG_MANAGER_H

#include "log/appender.h"
#include "log/logger.h"
#include "singleton.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace net {

/**
 * @brief 同步日志器管理器。
 *
 * 维护 name -> Logger 映射，root Logger 默认挂载 StdoutLogAppender。
 */
class LoggerManager {
 public:
  typedef std::mutex MutexType;
  typedef std::map<std::string, Logger::ptr> LoggerMap;

  LoggerManager();

  Logger::ptr getLogger(const std::string& name);
  Logger::ptr getRoot();

 private:
  LoggerMap loggers_;
  Logger::ptr root_;
  MutexType mutex_;
};

/**
 * @brief 异步日志器管理器。
 */
class AsyncLoggerManager {
 public:
  typedef std::mutex MutexType;
  typedef std::map<std::string, Logger::ptr> LoggerMap;

  AsyncLoggerManager();

  Logger::ptr getLogger(const std::string& name);
  Logger::ptr getRoot();

 private:
  LoggerMap loggers_;
  Logger::ptr root_;
  MutexType mutex_;
};

// 进程级单例（与 Sylar LoggerMgr / AsycLoggerMgr 对应）
typedef Singleton<LoggerManager> LoggerMgr;
typedef Singleton<AsyncLoggerManager> AsyncLoggerMgr;

}  // namespace net

#endif  // NET_LOG_MANAGER_H
