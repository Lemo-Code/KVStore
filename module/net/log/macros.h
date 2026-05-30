#ifndef NET_LOG_MACROS_H
#define NET_LOG_MACROS_H

#include "log/event.h"
#include "log/manager.h"
#include "common/util.h"
#include "log/wrap.h"

#include <ctime>
#include <memory>

/**
 * @file macros.h
 * @brief 用户侧日志宏（命名与用法参考 Sylar，实现独立）。
 *
 * 用法示例：
 *   NET_LOG_INFO(NET_LOG_ROOT()) << "hello";
 *   NET_LOG_FMT_WARN(NET_LOG_NAME("net"), "port=%d", 8080);
 */

/** 构造 LogEvent 的公共参数块 */
#define NET_LOG_EVENT(logger, level)                                         \
  net::LogEventWrap(net::LogEvent::ptr(new net::LogEvent(                   \
      (logger), (level), __FILE__, __LINE__, net::GetElapseMs(),             \
      net::GetThreadId(), net::GetFiberId(),                                 \
      static_cast<uint64_t>(time(nullptr)), net::GetThreadName())))

/** 按级别输出（流式 <<） */
#define NET_LOG_LEVEL(logger, level)                                         \
  if ((logger)->getLevel() <= (level)) NET_LOG_EVENT(logger, level).stream()

#define NET_LOG_DEBUG(logger) NET_LOG_LEVEL(logger, net::LogLevel::DEBUG)
#define NET_LOG_INFO(logger) NET_LOG_LEVEL(logger, net::LogLevel::INFO)
#define NET_LOG_WARN(logger) NET_LOG_LEVEL(logger, net::LogLevel::WARN)
#define NET_LOG_ERROR(logger) NET_LOG_LEVEL(logger, net::LogLevel::ERROR)
#define NET_LOG_FATAL(logger) NET_LOG_LEVEL(logger, net::LogLevel::FATAL)

/** 按级别输出（printf 风格） */
#define NET_LOG_FMT_LEVEL(logger, level, fmt, ...)                           \
  if ((logger)->getLevel() <= (level))                                       \
    NET_LOG_EVENT(logger, level).getEvent()->format(fmt, __VA_ARGS__)

#define NET_LOG_FMT_DEBUG(logger, fmt, ...)                                  \
  NET_LOG_FMT_LEVEL(logger, net::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define NET_LOG_FMT_INFO(logger, fmt, ...)                                   \
  NET_LOG_FMT_LEVEL(logger, net::LogLevel::INFO, fmt, __VA_ARGS__)
#define NET_LOG_FMT_WARN(logger, fmt, ...)                                   \
  NET_LOG_FMT_LEVEL(logger, net::LogLevel::WARN, fmt, __VA_ARGS__)
#define NET_LOG_FMT_ERROR(logger, fmt, ...)                                  \
  NET_LOG_FMT_LEVEL(logger, net::LogLevel::ERROR, fmt, __VA_ARGS__)
#define NET_LOG_FMT_FATAL(logger, fmt, ...)                                  \
  NET_LOG_FMT_LEVEL(logger, net::LogLevel::FATAL, fmt, __VA_ARGS__)

/** 同步管理器快捷宏 */
#define NET_LOG_ROOT() net::LoggerMgr::GetInstance()->getRoot()
#define NET_LOG_NAME(name) net::LoggerMgr::GetInstance()->getLogger(name)

/** 异步管理器快捷宏 */
#define NET_ASYNC_LOG_ROOT() net::AsyncLoggerMgr::GetInstance()->getRoot()
#define NET_ASYNC_LOG_NAME(name) \
  net::AsyncLoggerMgr::GetInstance()->getLogger(name)

#endif  // NET_LOG_MACROS_H
