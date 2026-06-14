#pragma once

#include "lemo/log/appender.h"

#include <mutex>

namespace lemo {
namespace log {

enum class ConsoleTarget { kStdout, kStderr };

class ConsoleAppender : public Appender {
 public:
  typedef std::shared_ptr<ConsoleAppender> ptr;
  explicit ConsoleAppender(ConsoleTarget target = ConsoleTarget::kStdout);

  void Append(const LogRecord& record) override;
  void Flush() override;
  const char* Type() const override;

 private:
  ConsoleTarget target_;
  std::mutex io_mutex_;
};

}  // namespace log
}  // namespace lemo
