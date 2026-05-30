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

namespace detail {

inline std::string& FormatTlsBuffer() {
  static thread_local std::string buf;
  return buf;
}

}  // namespace detail

/** 保证每条日志独占一行，避免多 Logger 共写同一文件时粘成一行。 */
inline void EnsureTrailingNewline(std::string& line) {
  if (!line.empty() && line.back() != '\n') {
    line.push_back('\n');
  }
}

/** 格式化日志行：thread_local 复用 capacity，再拷贝独立 payload 入队。 */
inline std::string FormatLine(LogAppender& app,
                              const std::shared_ptr<Logger>& logger,
                              LogLevel::Level level, LogEvent::ptr event) {
  std::string& buf = detail::FormatTlsBuffer();
  buf.clear();
  ResolveFormatter(app, logger)->formatTo(buf, logger, level, event);
  std::string line(buf.data(), buf.size());
  EnsureTrailingNewline(line);
  return line;
}

/** 异步路径在 rename/truncate 前：排空队列并刷缓冲，再 reopen fd */
inline void AsyncPrepareFileMutation(const std::string& path) {
  AsyncLogMgr::GetInstance()->flushFile(path);
  AsyncLogMgr::GetInstance()->reopenFile(path);
}

}  // namespace appender_util
}  // namespace net

#endif  // NET_LOG_APPENDER_UTIL_H
