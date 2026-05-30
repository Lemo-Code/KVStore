/**
 * @file event.cc
 * @brief LogEvent 构造、格式化与重置实现。
 */
#include "log/event.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace net {

/** 初始化事件全部上下文字段 */
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
      level_(level) {}

/** printf 风格格式化入口，转发到 va_list 版本 */
void LogEvent::format(const char* fmt, ...) {
  va_list al;
  va_start(al, fmt);
  format(fmt, al);
  va_end(al);
}

/** 使用 vasprintf 格式化并追加到消息流 */
void LogEvent::format(const char* fmt, va_list al) {
  char* buf = nullptr;
  const int len = vasprintf(&buf, fmt, al);
  if (len != -1) {
    message_.append(buf, static_cast<size_t>(len));
    free(buf);
  }
}

}  // namespace net
