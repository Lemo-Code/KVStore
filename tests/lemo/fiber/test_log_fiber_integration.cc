/**
 * @file test_log_fiber_integration.cc
 * @brief 协程 + 日志：%F/%t/%N 在调度任务中正确输出并落盘
 */
#include "test_common.h"

#include "lemo/fiber/fiber_id.h"
#include "lemo/fiber/module.h"
#include "lemo/log/file_appender.h"
#include "lemo/log/log.h"
#include "lemo/log/log_paths.h"
#include "lemo/log/logger_repository.h"
#include "lemo/log/pattern_layout.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <string>
#include <thread>

namespace {

std::string ReadAll(const std::string& path) {
  std::ifstream in(path.c_str());
  LEMO_CHECK(in.good());
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

void wait_until(const std::function<bool()>& pred, int max_iters = 200) {
  for (int i = 0; i < max_iters && !pred(); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

}  // namespace

int main() {
  const std::string base_path = "/tmp/lemo_test_log_fiber_integration.log";
  const std::string path =
      lemo::log::ResolveLoggerFilePath("fiber_log", base_path);
  std::remove(path.c_str());

  lemo::log::Logger::ptr logger =
      lemo::log::LoggerRepository::Instance().GetLogger("fiber_log");
  logger->ClearAppenders();
  logger->SetAdditive(false);
  logger->SetLevel(lemo::log::LogLevel::INFO);
  logger->SetLayout(lemo::log::Layout::ptr(
      new lemo::log::PatternLayout("[%F][%t][%N] %m")));

  logger->AddAppender(
      lemo::log::FileAppender::ForLogger("fiber_log", base_path));

  lemo::fiber::Scheduler sch(2, true, "fiber_log_sch");
  sch.start();

  std::atomic<uint32_t> task_fiber_id{0};
  std::atomic<bool> done{false};

  sch.schedule([&logger, &task_fiber_id, &done]() {
    task_fiber_id.store(lemo::fiber::GetCurrentFiberId());
    LEMO_LOG_INFO(logger) << "from_fiber_task";
    done.store(true);
  });

  wait_until([&done]() { return done.load(); });
  LEMO_CHECK(task_fiber_id.load() > 0);

  logger->Flush();

  const std::string content = ReadAll(path);
  LEMO_CHECK(content.find("from_fiber_task") != std::string::npos);
  LEMO_CHECK(content.find("[" + std::to_string(task_fiber_id.load()) + "]") !=
             std::string::npos);
  LEMO_CHECK(content.find("fiber_log_sch") != std::string::npos);

  sch.stop();
  std::remove(path.c_str());
  std::printf("PASS test_log_fiber_integration\n");
  return 0;
}
