#pragma once

#include "lemo/log/appender.h"
#include "lemo/log/event.h"
#include "lemo/log/layout.h"
#include "lemo/log/level.h"

#include <list>
#include <memory>
#include <mutex>
#include <string>

namespace lemo {
namespace log {

class Logger : public std::enable_shared_from_this<Logger> {
 public:
  typedef std::shared_ptr<Logger> ptr;

  explicit Logger(const std::string& name = "root");

  void Log(LogLevel::Level level, LogEvent::ptr event);

  void AddAppender(Appender::ptr appender);
  void ClearAppenders();
  void Flush();

  void SetLayout(Layout::ptr layout);
  void SetLayout(const std::string& pattern);
  Layout::ptr GetLayout() const { return layout_; }

  LogLevel::Level GetLevel() const { return level_; }
  LogLevel::Level GetEffectiveLevel() const;
  void SetLevel(LogLevel::Level level);
  bool IsLevelSet() const { return level_set_; }

  const std::string& GetName() const { return name_; }
  Logger::ptr GetParent() const { return parent_; }

  void SetAdditive(bool additive) { additive_ = additive; }
  bool IsAdditive() const { return additive_; }

  const std::list<Appender::ptr>& GetAppenders() const { return appenders_; }

 private:
  friend class LoggerRepository;

  void AppendToSelf(const LogRecord& record);
  void AppendToAncestors(const LogRecord& record);
  void InheritLayoutTo(Appender::ptr appender);

  std::string name_;
  LogLevel::Level level_;
  bool level_set_;
  bool additive_;
  Layout::ptr layout_;
  std::list<Appender::ptr> appenders_;
  Logger::ptr parent_;
  Logger::ptr root_;
  std::mutex mutex_;
};

}  // namespace log
}  // namespace lemo
