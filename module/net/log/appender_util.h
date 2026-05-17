#ifndef NET_LOG_APPENDER_UTIL_H
#define NET_LOG_APPENDER_UTIL_H

#include "log/appender.h"
#include "log/async_sink.h"
#include "log/event.h"
#include "log/logger.h"

#include <ctime>
#include <memory>
#include <string>

namespace net {
namespace appender_util {

inline bool ShouldEmit(LogLevel::Level level, LogLevel::Level threshold) {
  return level >= threshold;
}

inline time_t EventTime(LogEvent::ptr event) {
  return static_cast<time_t>(event->getTime());
}

inline LogFormatter::ptr ResolveFormatter(LogAppender& app,
                                          const std::shared_ptr<Logger>& logger) {
  LogFormatter::ptr fmt = app.getFormatter();
  return fmt ? fmt : logger->getFormatter();
}

inline std::string FormatLine(LogAppender& app,
                              const std::shared_ptr<Logger>& logger,
                              LogLevel::Level level, LogEvent::ptr event) {
  return ResolveFormatter(app, logger)->format(logger, level, event);
}

/** 异步路径在 rename/truncate 前：排空队列并刷缓冲，再 reopen fd */
inline void AsyncPrepareFileMutation(const std::string& path) {
  AsyncLogMgr::GetInstance()->flushFile(path);
  AsyncLogMgr::GetInstance()->reopenFile(path);
}

}  // namespace appender_util
}  // namespace net

#endif  // NET_LOG_APPENDER_UTIL_H
