#ifndef NET_LOG_EVENT_H
#define NET_LOG_EVENT_H

#include "log/level.h"

#include <cstdarg>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>

namespace net {

class Logger;  // 前向声明

/**
 * @brief 单条日志事件，承载上下文元数据与用户消息。
 *
 * 由宏 NET_LOG_* 构造，经 LogEventWrap 析构时提交给 Logger 输出。
 */
class LogEvent {
 public:
  typedef std::shared_ptr<LogEvent> ptr;

  // 参数的初始化
  LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
           const char* file, int32_t line, uint32_t elapse,
           uint32_t thread_id, uint32_t fiber_id, uint64_t time,
           const std::string& thread_name);

  // 获得日志输出的文件名（代码）
  const char* getFile() const { return file_; }

  // 获得日志输出的行号（代码）
  int32_t getLine() const { return line_; }

  // 获得服务器启动的累计毫秒
  uint32_t getElapse() const { return elapse_; }

  // 获得日志输出的线程号
  uint32_t getThreadId() const { return thread_id_; }

  // 获得日志输出的协程号
  uint32_t getFiberId() const { return fiber_id_; }

  // 获得日志产生的时间（Unix 秒）
  uint64_t getTime() const { return time_; }

  // 获得 event 所属的 logger
  std::shared_ptr<Logger> getLogger() const { return logger_; }

  // 获得本条日志的级别
  LogLevel::Level getLevel() const { return level_; }

  // 用户消息流：log << "hello" 即 ss_ << "hello"
  std::stringstream& getSS() { return ss_; }

  // 获得日志输出的线程名
  std::string getThreadName() const { return thread_name_; }

  // 可变参形式给 event 传值（NET_LOG_FMT_* 宏使用）
  void format(const char* fmt, ...);

  // format(...) 调用的底层 va_list 方法
  void format(const char* fmt, va_list al);

 private:
  const char* file_;                // 文件名（代码文件）
  int32_t line_;                    // 行号（代码行号）
  uint32_t elapse_;                 // 服务器启动累计毫秒
  uint32_t thread_id_;              // 线程号
  uint32_t fiber_id_;               // 协程号
  uint64_t time_;                   // 当前时间戳
  std::string thread_name_;         // 线程名
  std::stringstream ss_;            // 日志输出的内容（message）
  std::shared_ptr<Logger> logger_;  // logger（方便获得 logger 成员）
  LogLevel::Level level_;           // 日志的级别
};

}  // namespace net

#endif  // NET_LOG_EVENT_H
