#include "test_common.h"

#include "log/log.h"

#include <memory>
#include <string>

int main() {
  auto logger = net::LoggerMgr::GetInstance()->getLogger("fmt_test");
  logger->clearAppenders();

  net::LogFormatter::ptr fmt(new net::LogFormatter("[%p] %m"));
  NET_CHECK(fmt->isValid());

  auto event = net::LogEvent::ptr(new net::LogEvent(
      logger, net::LogLevel::INFO, __FILE__, __LINE__, net::GetElapseMs(),
      net::GetThreadId(), net::GetFiberId(),
      static_cast<uint64_t>(time(nullptr)), net::GetThreadName()));
  event->stream() << "hello";

  const std::string out = fmt->format(logger, net::LogLevel::INFO, event);
  NET_CHECK(out.find("[INFO]") != std::string::npos);
  NET_CHECK(out.find("hello") != std::string::npos);

  net::LogFormatter::ptr bad(new net::LogFormatter("%z"));
  NET_CHECK(bad->hasError());

  std::printf("PASS test_log_formatter\n");
  return 0;
}
