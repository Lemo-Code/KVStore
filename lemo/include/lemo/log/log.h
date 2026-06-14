#pragma once

#include "lemo/log/event.h"
#include "lemo/log/logger_repository.h"
#include "lemo/fiber/fiber_id.h"
#include "lemo/utils/thread_util.h"
#include "lemo/utils/time_util.h"

#include <cstdint>
#include <string>

namespace lemo {
namespace log {

inline uint32_t GetThreadId() { return utils::GetThreadId(); }
inline uint32_t GetFiberId() { return fiber::GetCurrentFiberId(); }
inline uint32_t GetElapse() { return utils::GetElapse(); }
inline const std::string& GetThreadName() { return utils::GetThreadName(); }

}  // namespace log
}  // namespace lemo

#define LEMO_LOG_LEVEL(logger, level)                                         \
  if ((logger)->GetEffectiveLevel() <= (level))                               \
    lemo::log::LogEventWrap(lemo::log::LogEvent::ptr(new lemo::log::LogEvent(  \
        (logger), (level), __FILE__, __LINE__, lemo::log::GetElapse(),        \
        lemo::log::GetThreadId(), lemo::log::GetFiberId(), time(0),           \
        lemo::log::GetThreadName()))).GetSS()

#define LEMO_LOG_TRACE(logger) LEMO_LOG_LEVEL(logger, lemo::log::LogLevel::TRACE)
#define LEMO_LOG_DEBUG(logger) LEMO_LOG_LEVEL(logger, lemo::log::LogLevel::DEBUG)
#define LEMO_LOG_INFO(logger) LEMO_LOG_LEVEL(logger, lemo::log::LogLevel::INFO)
#define LEMO_LOG_WARN(logger) LEMO_LOG_LEVEL(logger, lemo::log::LogLevel::WARN)
#define LEMO_LOG_ERROR(logger) LEMO_LOG_LEVEL(logger, lemo::log::LogLevel::ERROR)
#define LEMO_LOG_FATAL(logger) LEMO_LOG_LEVEL(logger, lemo::log::LogLevel::FATAL)

#define LEMO_LOG_FMT_LEVEL(logger, level, fmt, ...)                           \
  if ((logger)->GetEffectiveLevel() <= (level))                               \
    lemo::log::LogEventWrap(lemo::log::LogEvent::ptr(new lemo::log::LogEvent(  \
        (logger), (level), __FILE__, __LINE__, lemo::log::GetElapse(),        \
        lemo::log::GetThreadId(), lemo::log::GetFiberId(), time(0),           \
        lemo::log::GetThreadName())))                                         \
        .GetEvent()                                                           \
        ->Format(fmt, ##__VA_ARGS__)

#define LEMO_LOG_FMT_DEBUG(logger, fmt, ...) \
  LEMO_LOG_FMT_LEVEL(logger, lemo::log::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LEMO_LOG_FMT_INFO(logger, fmt, ...) \
  LEMO_LOG_FMT_LEVEL(logger, lemo::log::LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LEMO_LOG_FMT_WARN(logger, fmt, ...) \
  LEMO_LOG_FMT_LEVEL(logger, lemo::log::LogLevel::WARN, fmt, ##__VA_ARGS__)
#define LEMO_LOG_FMT_ERROR(logger, fmt, ...) \
  LEMO_LOG_FMT_LEVEL(logger, lemo::log::LogLevel::ERROR, fmt, ##__VA_ARGS__)
#define LEMO_LOG_FMT_FATAL(logger, fmt, ...) \
  LEMO_LOG_FMT_LEVEL(logger, lemo::log::LogLevel::FATAL, fmt, ##__VA_ARGS__)

#define LEMO_LOG_ROOT() lemo::log::LoggerRepository::Instance().GetRoot()
#define LEMO_LOG_NAME(name) lemo::log::LoggerRepository::Instance().GetLogger(name)

#define LEMO_ASYNC_LOG_ROOT() LEMO_LOG_ROOT()
#define LEMO_ASYNC_LOG_NAME(name) LEMO_LOG_NAME(name)
