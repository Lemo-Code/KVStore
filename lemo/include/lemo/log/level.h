#pragma once

#include <string>

namespace lemo {
namespace log {

class LogLevel {
 public:
  enum Level {
    UNKNOWN = 0,
    TRACE = 1,
    DEBUG = 2,
    INFO = 3,
    WARN = 4,
    ERROR = 5,
    FATAL = 6,
    OFF = 100,
  };

  static const char* ToString(Level level);
  static Level FromString(const std::string& str);
};

}  // namespace log
}  // namespace lemo
