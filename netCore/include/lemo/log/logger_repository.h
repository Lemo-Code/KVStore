#pragma once

#include "lemo/log/logger.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace lemo {
namespace log {

class LoggerRepository {
 public:
  static LoggerRepository& Instance();

  Logger::ptr GetLogger(const std::string& name);
  Logger::ptr GetRoot();

  LoggerRepository(const LoggerRepository&) = delete;
  LoggerRepository& operator=(const LoggerRepository&) = delete;

 private:
  LoggerRepository();
  Logger::ptr CreateUnlocked(const std::string& name);

  std::mutex mutex_;
  std::map<std::string, Logger::ptr> loggers_;
  Logger::ptr root_;
};

inline LoggerRepository& GetLoggerManager() { return LoggerRepository::Instance(); }

// 兼容旧名：异步由 AsyncAppender 装饰器实现，不再单独 Manager
inline LoggerRepository& GetAsyncLoggerManager() {
  return LoggerRepository::Instance();
}

}  // namespace log
}  // namespace lemo
