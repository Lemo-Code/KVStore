#include "lemo/log/logger_repository.h"

#include "lemo/log/appender_registry.h"
#include "lemo/log/console_appender.h"

namespace lemo {
namespace log {

LoggerRepository::LoggerRepository() {
  RegisterBuiltInAppenders();
  root_.reset(new Logger("root"));
  root_->AddAppender(Appender::ptr(new ConsoleAppender()));
  loggers_["root"] = root_;
}

LoggerRepository& LoggerRepository::Instance() {
  static LoggerRepository repo;
  return repo;
}

Logger::ptr LoggerRepository::GetRoot() { return root_; }

Logger::ptr LoggerRepository::GetLogger(const std::string& name) {
  if (name.empty() || name == "root") return root_;
  std::lock_guard<std::mutex> lock(mutex_);
  std::map<std::string, Logger::ptr>::iterator it = loggers_.find(name);
  if (it != loggers_.end()) return it->second;
  return CreateUnlocked(name);
}

Logger::ptr LoggerRepository::CreateUnlocked(const std::string& name) {
  Logger::ptr parent = root_;
  const std::string::size_type pos = name.find_last_of('.');
  if (pos != std::string::npos) {
    const std::string parent_name = name.substr(0, pos);
    std::map<std::string, Logger::ptr>::iterator it = loggers_.find(parent_name);
    if (it != loggers_.end()) {
      parent = it->second;
    } else {
      parent = CreateUnlocked(parent_name);
    }
  }
  Logger::ptr logger(new Logger(name));
  logger->parent_ = parent;
  logger->root_ = root_;
  loggers_[name] = logger;
  return logger;
}

}  // namespace log
}  // namespace lemo
