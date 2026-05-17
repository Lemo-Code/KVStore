#ifndef NET_LOG_ASYNC_RECORD_H
#define NET_LOG_ASYNC_RECORD_H

#include <string>

namespace net {

/**
 * @brief 异步入队单元：目标在入队前由 Appender 解析，payload 为已格式化整行。
 */
struct AsyncLogRecord {
  std::string destination;
  std::string payload;

  AsyncLogRecord() = default;
  AsyncLogRecord(std::string dest, std::string body)
      : destination(std::move(dest)), payload(std::move(body)) {}
};

}  // namespace net

#endif  // NET_LOG_ASYNC_RECORD_H
