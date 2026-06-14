#include "test_common.h"

#include "lemo/log/event.h"
#include "lemo/log/logger_repository.h"
#include "lemo/log/mdc.h"
#include "lemo/log/pattern_layout.h"

int main() {
  lemo::log::MDC::Put("trace_id", "t-1");
  lemo::log::Logger::ptr logger =
      lemo::log::LoggerRepository::Instance().GetLogger("demo");
  lemo::log::LogEvent::ptr event(new lemo::log::LogEvent(
      logger, lemo::log::LogLevel::INFO, "file.cc", 10, 100, 1, 0, time(0),
      "main"));
  event->GetSS() << "hello";

  lemo::log::PatternLayout fmt("%p %c %m trace=%X{trace_id}");
  const std::string out = fmt.Format(event->ToRecord());
  LEMO_CHECK(out.find("INFO") != std::string::npos);
  LEMO_CHECK(out.find("demo") != std::string::npos);
  LEMO_CHECK(out.find("hello") != std::string::npos);
  LEMO_CHECK(out.find("trace=t-1") != std::string::npos);
  std::printf("PASS test_log_formatter\n");
  return 0;
}
