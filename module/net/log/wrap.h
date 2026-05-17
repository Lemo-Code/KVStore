#ifndef NET_LOG_WRAP_H
#define NET_LOG_WRAP_H

#include "log/event.h"

#include <memory>
#include <sstream>

namespace net {

/**
 * @brief LogEvent 的 RAII 包装器。
 *
 * 构造时持有事件，析构时自动调用 Logger::log 输出。
 * 保证即使中途 return/异常，已构造的日志也能刷出（宏 NET_LOG_* 的核心机制）。
 */
class LogEventWrap {
 public:
  // 初始化，持有事件指针
  explicit LogEventWrap(LogEvent::ptr event);

  // 析构的时候日志自动输出
  ~LogEventWrap();

  // 把 message 输出到 event 的 getSS()
  std::stringstream& getSS();

  // 获得 event（供 NET_LOG_FMT_* 宏使用）
  LogEvent::ptr getEvent() const { return event_; }

 private:
  LogEvent::ptr event_;  // 日志事件
};

}  // namespace net

#endif  // NET_LOG_WRAP_H
