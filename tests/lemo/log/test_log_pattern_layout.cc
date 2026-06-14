#include "test_common.h"

#include "lemo/log/log_event.h"
#include "lemo/log/pattern_layout.h"

int main() {
  lemo::log::LogEvent event;
  event.level = lemo::log::Level::Info;
  event.logger_name = "com.example";
  event.message = "hello lemo";
  event.thread_id = 12345;
  event.timestamp_ms = 0;
  event.mdc["trace_id"] = "abc";

  lemo::log::PatternLayout layout("%p %c %m trace=%X{trace_id}");
  const std::string out = layout.Format(event);

  LEMO_CHECK(out.find("INFO") != std::string::npos);
  LEMO_CHECK(out.find("com.example") != std::string::npos);
  LEMO_CHECK(out.find("hello lemo") != std::string::npos);
  LEMO_CHECK(out.find("trace=abc") != std::string::npos);

  lemo::log::PatternLayout layout2("%m%n");
  LEMO_CHECK(layout2.Format(event) == "hello lemo\n");

  std::printf("PASS test_log_pattern_layout\n");
  return 0;
}
