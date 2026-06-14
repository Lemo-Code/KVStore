/**
 * @file bench_log_perf.cc
 * @brief lemo 日志性能基准：同步 vs 异步（file appender，单线程）
 */
#include "lemo/log/async_appender.h"
#include "lemo/log/file_appender.h"
#include "lemo/log/log.h"
#include "lemo/log/pattern_layout.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

using Clock = std::chrono::high_resolution_clock;
using Us = std::chrono::microseconds;

enum Mode { kSync, kAsync };

struct Result {
  double msgs_per_sec;
  double us_per_msg;
  size_t total_msgs;
};

lemo::log::Logger::ptr MakeLogger(Mode mode, const std::string& path) {
  lemo::log::Logger::ptr logger =
      lemo::log::LoggerRepository::Instance().GetLogger("bench");
  logger->ClearAppenders();
  logger->SetAdditive(false);
  logger->SetLevel(lemo::log::LogLevel::INFO);
  logger->SetLayout(lemo::log::Layout::ptr(new lemo::log::PatternLayout("%m")));

  lemo::log::Appender::ptr file(new lemo::log::FileAppender(path));
  if (mode == kAsync) {
    logger->AddAppender(lemo::log::MakeAsync(file));
  } else {
    logger->AddAppender(file);
  }
  return logger;
}

Result Bench(Mode mode, int msgs, int payload_bytes, const std::string& path) {
  std::remove(path.c_str());

  lemo::log::Logger::ptr logger = MakeLogger(mode, path);
  std::string payload(static_cast<size_t>(payload_bytes), 'x');

  auto t0 = Clock::now();
  for (int i = 0; i < msgs; ++i) {
    LEMO_LOG_INFO(logger) << payload;
  }
  logger->Flush();
  auto t1 = Clock::now();

  auto us = std::chrono::duration_cast<Us>(t1 - t0).count();
  size_t total = static_cast<size_t>(msgs);

  Result r;
  r.total_msgs = total;
  r.msgs_per_sec = static_cast<double>(total) * 1e6 / static_cast<double>(us);
  r.us_per_msg = static_cast<double>(us) / static_cast<double>(total);
  return r;
}

void PrintResult(const char* name, const Result& r) {
  std::printf("  %-18s: %10.0f msg/s  %6.2f us/msg  (total=%zu)\n", name,
              r.msgs_per_sec, r.us_per_msg, r.total_msgs);
}

void RunCase(const char* label, int msgs, int payload_bytes) {
  const std::string sync_path = "/tmp/lemo_bench_sync.log";
  const std::string async_path = "/tmp/lemo_bench_async.log";

  std::printf("[%s] msgs=%d payload=%dB\n", label, msgs, payload_bytes);
  std::fflush(stdout);

  auto sync = Bench(kSync, msgs, payload_bytes, sync_path);
  PrintResult("sync/file", sync);

  auto async = Bench(kAsync, msgs, payload_bytes, async_path);
  PrintResult("async/file", async);

  double speedup = async.msgs_per_sec / sync.msgs_per_sec;
  std::printf("  speedup (async/sync): %.2fx\n\n", speedup);
  std::fflush(stdout);

  std::remove(sync_path.c_str());
  std::remove(async_path.c_str());
}

}  // namespace

int main() {
  std::printf("=== Lemo Log Benchmark (sync vs async, single-thread) ===\n\n");
  std::fflush(stdout);

  RunCase("50B", 10000, 50);
  RunCase("500B", 5000, 500);

  std::printf("==========================================\n");
  return 0;
}
