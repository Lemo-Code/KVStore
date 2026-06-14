#include "lemo/log/level.h"

namespace lemo {
namespace log {

const char* LogLevel::ToString(Level level) {
  switch (level) {
    case TRACE:
      return "TRACE";
    case DEBUG:
      return "DEBUG";
    case INFO:
      return "INFO";
    case WARN:
      return "WARN";
    case ERROR:
      return "ERROR";
    case FATAL:
      return "FATAL";
    case OFF:
      return "OFF";
    default:
      return "UNKNOWN";
  }
}

LogLevel::Level LogLevel::FromString(const std::string& str) {
  if (str == "TRACE" || str == "trace") return TRACE;
  if (str == "DEBUG" || str == "debug") return DEBUG;
  if (str == "INFO" || str == "info") return INFO;
  if (str == "WARN" || str == "warn") return WARN;
  if (str == "ERROR" || str == "error") return ERROR;
  if (str == "FATAL" || str == "fatal") return FATAL;
  if (str == "OFF" || str == "off") return OFF;
  return UNKNOWN;
}

}  // namespace log
}  // namespace lemo
