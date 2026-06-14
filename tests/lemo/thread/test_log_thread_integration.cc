#include "test_common.h"

#include "lemo/log/async_appender.h"
#include "lemo/log/file_appender.h"
#include "lemo/log/log.h"
#include "lemo/log/log_paths.h"
#include "lemo/log/logger_repository.h"
#include "lemo/log/pattern_layout.h"
#include "lemo/thread/module.h"

#include <fstream>
#include <string>

namespace {

void log_from_named_thread(const std::string& name, const std::string& tag,
                           lemo::log::Logger::ptr logger) {
  lemo::thread::Thread::ptr t(new lemo::thread::Thread(
      [logger, tag]() { LEMO_LOG_INFO(logger) << tag; }, name));
  t->join();
}

}  // namespace

int main() {
  const std::string base_path = "/tmp/lemo_test_log_thread_integration.log";
  const std::string path =
      lemo::log::ResolveLoggerFilePath("thread_int", base_path);
  std::remove(path.c_str());

  lemo::thread::Thread::SetName("main");

  lemo::log::Logger::ptr logger =
      lemo::log::LoggerRepository::Instance().GetLogger("thread_int");
  logger->ClearAppenders();
  logger->SetAdditive(false);
  logger->SetLevel(lemo::log::LogLevel::DEBUG);
  logger->SetLayout(
      lemo::log::Layout::ptr(new lemo::log::PatternLayout("[%t][%N] %m")));

  lemo::log::Appender::ptr file =
      lemo::log::FileAppender::ForLogger("thread_int", base_path);
  lemo::log::Appender::ptr async = lemo::log::MakeAsync(file);
  logger->AddAppender(async);

  const uint32_t main_tid = lemo::log::GetThreadId();
  LEMO_LOG_INFO(logger) << "from_main";

  log_from_named_thread("biz_worker", "from_worker", logger);

  logger->Flush();

  std::ifstream in(path.c_str());
  LEMO_CHECK(in.good());
  const std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());

  LEMO_CHECK(content.find("from_main") != std::string::npos);
  LEMO_CHECK(content.find("from_worker") != std::string::npos);
  LEMO_CHECK(content.find("main") != std::string::npos);
  LEMO_CHECK(content.find("biz_worker") != std::string::npos);

  const std::string main_marker = "[" + std::to_string(main_tid) + "]";
  LEMO_CHECK(content.find(main_marker) != std::string::npos);

  std::remove(path.c_str());
  std::printf("PASS test_log_thread_integration\n");
  return 0;
}
