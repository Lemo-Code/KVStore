#include "lemo/log/event.h"
#include "lemo/log/logger.h"
#include "lemo/log/mdc.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace lemo {
namespace log {

LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
                   const char* file, int32_t line, uint32_t elapse,
                   uint32_t thread_id, uint32_t fiber_id, uint64_t time,
                   const std::string& thread_name)
    : file_(file),
      line_(line),
      elapse_(elapse),
      thread_id_(thread_id),
      fiber_id_(fiber_id),
      time_(time),
      thread_name_(thread_name),
      logger_(logger),
      level_(level),
      mdc_(MDC::GetCopy()) {}

void LogEvent::Format(const char* fmt, ...) {
  va_list al;
  va_start(al, fmt);
  char* buf = NULL;
  const int len = vasprintf(&buf, fmt, al);
  va_end(al);
  if (len != -1 && buf) {
    ss_ << std::string(buf, static_cast<size_t>(len));
    free(buf);
  }
}

LogRecord LogEvent::ToRecord() const {
  LogRecord record;
  record.level = level_;
  record.logger_name = logger_ ? logger_->GetName() : "";
  record.message = ss_.str();
  record.file = file_ ? file_ : "";
  record.line = line_;
  record.elapse = elapse_;
  record.thread_id = thread_id_;
  record.fiber_id = fiber_id_;
  record.timestamp = time_;
  record.thread_name = thread_name_;
  record.mdc = mdc_;
  return record;
}

LogEventWrap::LogEventWrap(LogEvent::ptr event) : event_(event) {}

LogEventWrap::~LogEventWrap() {
  event_->GetLogger()->Log(event_->GetLevel(), event_);
}

std::stringstream& LogEventWrap::GetSS() { return event_->GetSS(); }

}  // namespace log
}  // namespace lemo
