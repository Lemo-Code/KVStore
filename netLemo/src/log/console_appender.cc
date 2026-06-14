#include "lemo/log/console_appender.h"

#include <iostream>

namespace lemo {
namespace log {

ConsoleAppender::ConsoleAppender(ConsoleTarget target) : target_(target) {}

void ConsoleAppender::Append(const LogRecord& record) {
  if (!PassesThreshold(record.level)) return;
  const std::string line = Format(record);
  std::lock_guard<std::mutex> lock(io_mutex_);
  if (target_ == ConsoleTarget::kStderr) {
    std::cerr << line;
  } else {
    std::cout << line;
  }
}

void ConsoleAppender::Flush() {
  std::lock_guard<std::mutex> lock(io_mutex_);
  if (target_ == ConsoleTarget::kStderr) {
    std::cerr.flush();
  } else {
    std::cout.flush();
  }
}

const char* ConsoleAppender::Type() const { return "console"; }

}  // namespace log
}  // namespace lemo
