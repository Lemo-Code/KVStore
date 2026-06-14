#pragma once

#include "lemo/log/layout.h"
#include "lemo/log/level.h"
#include "lemo/log/record.h"

#include <memory>
#include <mutex>
#include <string>

namespace lemo {
namespace log {

class Logger;

// log4j Appender 抽象
class Appender {
 public:
  typedef std::shared_ptr<Appender> ptr;

  Appender();
  virtual ~Appender() {}

  virtual void Append(const LogRecord& record) = 0;
  virtual void Flush() = 0;
  virtual const char* Type() const = 0;

  virtual void SetLayout(Layout::ptr layout);
  Layout::ptr GetLayout() const;
  bool HasLayout() const;

  void SetThreshold(LogLevel::Level level);
  LogLevel::Level GetThreshold() const;

 protected:
  bool PassesThreshold(LogLevel::Level level) const;
  std::string Format(const LogRecord& record) const;

  Layout::ptr layout_;
  bool has_layout_;
  LogLevel::Level threshold_;
  mutable std::mutex mutex_;
};

}  // namespace log
}  // namespace lemo
