#ifndef NET_LOG_WRAP_H
#define NET_LOG_WRAP_H

#include "log/event.h"

#include <memory>

namespace net {

/**
 * @brief LogEvent 的 RAII 包装器。
 *
 * 构造时持有事件，析构时自动调用 Logger::log 输出。
 */
class LogEventWrap {
 public:
  explicit LogEventWrap(LogEvent::ptr event);
  ~LogEventWrap();

  LogMessageStream stream() { return event_->stream(); }
  LogEvent::ptr getEvent() const { return event_; }

 private:
  LogEvent::ptr event_;
};

}  // namespace net

#endif  // NET_LOG_WRAP_H
