#pragma once

#include "lemo/log/level.h"
#include "lemo/log/record.h"

#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>

namespace lemo {
namespace log {

class Logger;

class LogEvent {
 public:
  typedef std::shared_ptr<LogEvent> ptr;

  LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
           const char* file, int32_t line, uint32_t elapse, uint32_t thread_id,
           uint32_t fiber_id, uint64_t time, const std::string& thread_name);

  const char* GetFile() const { return file_; }
  int32_t GetLine() const { return line_; }
  uint32_t GetElapse() const { return elapse_; }
  uint32_t GetThreadId() const { return thread_id_; }
  uint32_t GetFiberId() const { return fiber_id_; }
  uint64_t GetTime() const { return time_; }
  std::shared_ptr<Logger> GetLogger() const { return logger_; }
  LogLevel::Level GetLevel() const { return level_; }
  std::stringstream& GetSS() { return ss_; }
  std::string GetThreadName() const { return thread_name_; }
  const std::map<std::string, std::string>& GetMDC() const { return mdc_; }

  void Format(const char* fmt, ...);
  LogRecord ToRecord() const;

 private:
  const char* file_;
  int32_t line_;
  uint32_t elapse_;
  uint32_t thread_id_;
  uint32_t fiber_id_;
  uint64_t time_;
  std::string thread_name_;
  std::stringstream ss_;
  std::shared_ptr<Logger> logger_;
  LogLevel::Level level_;
  std::map<std::string, std::string> mdc_;
};

class LogEventWrap {
 public:
  explicit LogEventWrap(LogEvent::ptr event);
  ~LogEventWrap();
  std::stringstream& GetSS();
  LogEvent::ptr GetEvent() const { return event_; }

 private:
  LogEvent::ptr event_;
};

}  // namespace log
}  // namespace lemo
