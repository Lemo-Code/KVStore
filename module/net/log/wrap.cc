/**
 * @file wrap.cc
 * @brief LogEventWrap RAII 包装器实现。
 */
#include "log/wrap.h"
#include "log/logger.h"

namespace net {

/** 持有事件指针，不立即输出 */
LogEventWrap::LogEventWrap(LogEvent::ptr event) : event_(event) {}

/**
 * 析构时自动提交日志。
 * 保证宏写法 NET_LOG_INFO(logger) << msg 在语句结束时刷出。
 */
LogEventWrap::~LogEventWrap() {
  event_->getLogger()->log(event_->getLevel(), event_);
}

/** 返回底层消息流，供 operator<< 链式写入 */
std::stringstream& LogEventWrap::getSS() {
  return event_->getSS();
}

}  // namespace net
