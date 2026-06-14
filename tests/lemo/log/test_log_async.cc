#include "test_common.h"

#include "lemo/log/async_appender.h"
#include "lemo/log/file_appender.h"
#include "lemo/log/log.h"
#include "lemo/log/logger_repository.h"
#include "lemo/log/pattern_layout.h"

#include <cstdio>
#include <fstream>
#include <string>

int main() {
  const std::string path = "/tmp/lemo_test_async.log";
  std::remove(path.c_str());

  lemo::log::Logger::ptr logger =
      lemo::log::LoggerRepository::Instance().GetLogger("async_test");
  logger->ClearAppenders();
  logger->SetAdditive(false);
  logger->SetLevel(lemo::log::LogLevel::INFO);
  logger->SetLayout(lemo::log::Layout::ptr(new lemo::log::PatternLayout("%m")));

  lemo::log::Appender::ptr file(new lemo::log::FileAppender(path));
  lemo::log::Appender::ptr async = lemo::log::MakeAsync(file);
  logger->AddAppender(async);

  for (int i = 0; i < 100; ++i) {
    LEMO_LOG_INFO(logger) << "msg";
  }
  logger->Flush();

  std::ifstream in(path.c_str());
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
  in.close();

  size_t count = 0;
  for (size_t pos = 0; (pos = content.find("msg", pos)) != std::string::npos;
       ++count, ++pos) {
  }
  LEMO_CHECK(count == 100);

  std::remove(path.c_str());
  std::printf("PASS test_log_async\n");
  return 0;
}
