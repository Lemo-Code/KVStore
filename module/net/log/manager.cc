/**
 * @file manager.cc
 * @brief LoggerManager / AsyncLoggerManager 实现。
 */
#include "log/manager.h"

namespace net {

LoggerManager::LoggerManager() {
  root_.reset(new Logger("root", false));
  root_->addAppender(LogAppender::ptr(new StdoutLogAppender("root")));
  loggers_[root_->getName()] = root_;
}

Logger::ptr LoggerManager::getLogger(const std::string& name) {
  std::lock_guard<MutexType> lock(mutex_);
  const auto it = loggers_.find(name);
  if (it != loggers_.end()) {
    return it->second;
  }
  Logger::ptr logger(new Logger(name, false));
  logger->root_ = root_;
  loggers_[name] = logger;
  return logger;
}

Logger::ptr LoggerManager::getRoot() {
  return root_;
}

AsyncLoggerManager::AsyncLoggerManager() {
  root_.reset(new Logger("root", true));
  root_->addAppender(LogAppender::ptr(new StdoutLogAppender("root")));
  loggers_[root_->getName()] = root_;
}

Logger::ptr AsyncLoggerManager::getLogger(const std::string& name) {
  std::lock_guard<MutexType> lock(mutex_);
  const auto it = loggers_.find(name);
  if (it != loggers_.end()) {
    return it->second;
  }
  Logger::ptr logger(new Logger(name, true));
  logger->root_ = root_;
  loggers_[name] = logger;
  return logger;
}

Logger::ptr AsyncLoggerManager::getRoot() {
  return root_;
}

}  // namespace net
