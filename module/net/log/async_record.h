#ifndef NET_LOG_ASYNC_RECORD_H
#define NET_LOG_ASYNC_RECORD_H

#include <string>
#include <utility>

namespace net {

enum class AsyncSinkType { STDOUT = 0, FILE = 1 };

inline const char* StdoutDestination() { return "@stdout"; }

/** 异步入队单元：目标在 Appender 入队前已解析，payload 为已格式化整行 */
struct AsyncLogRecord {
  AsyncSinkType type;
  std::string destination;
  std::string payload;

  AsyncLogRecord() : type(AsyncSinkType::FILE) {}

  AsyncLogRecord(AsyncSinkType t, std::string dest, std::string body)
      : type(t), destination(std::move(dest)), payload(std::move(body)) {}
};

}  // namespace net

#endif  // NET_LOG_ASYNC_RECORD_H
