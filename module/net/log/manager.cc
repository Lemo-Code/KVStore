/**
 * @file manager.cc
 */
#include "log/manager.h"

namespace net {

namespace {

Logger::ptr CreateRoot(bool async) {
  Logger::ptr root(new Logger("root", async));
  root->addAppender(LogAppender::ptr(new StdoutLogAppender()));
  return root;
}

Logger::ptr FindOrCreateLogger(std::map<std::string, Logger::ptr>& loggers,
                               Logger::ptr root, const std::string& name,
                               bool async, Spinlock& mtx) {
  Spinlock::Lock lock(mtx);
  const auto it = loggers.find(name);
  if (it != loggers.end()) {
    return it->second;
  }
  Logger::ptr logger(new Logger(name, async));
  detail::AttachRoot(logger, root);
  loggers[name] = logger;
  return logger;
}

}  // namespace

LoggerManager::LoggerManager() : root_(CreateRoot(false)) {
  loggers_[root_->getName()] = root_;
}

Logger::ptr LoggerManager::getLogger(const std::string& name) {
  return FindOrCreateLogger(loggers_, root_, name, false, mutex_);
}

Logger::ptr LoggerManager::getRoot() {
  return root_;
}

AsyncLoggerManager::AsyncLoggerManager() : root_(CreateRoot(true)) {
  loggers_[root_->getName()] = root_;
}

Logger::ptr AsyncLoggerManager::getLogger(const std::string& name) {
  return FindOrCreateLogger(loggers_, root_, name, true, mutex_);
}

Logger::ptr AsyncLoggerManager::getRoot() {
  return root_;
}

}  // namespace net
