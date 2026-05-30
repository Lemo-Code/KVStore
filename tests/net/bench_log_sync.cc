/**
 * @file bench_log_sync.cc
 * @brief 同步日志性能基准测试（对比用）。
 */
#include "log/log.h"
#include "log/log_paths.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::high_resolution_clock;
using Us = std::chrono::microseconds;

struct Result {
  double msgs_per_sec;
  double us_per_msg;
  size_t total_msgs;
};

Result BenchSync(int threads, int msgs_per_thread, int payload_bytes) {
  const std::string path = net::ResolveLogPath("bench_sync.log");
  std::string payload(payload_bytes, 'x');

  auto logger = net::LoggerMgr::GetInstance()->getLogger("bench_sync");
  logger->clearAppenders();
  logger->setFormatter("%m");
  logger->addAppender(
      net::LogAppender::ptr(new net::FileLogAppender(path)));

  auto t0 = Clock::now();

  std::vector<std::thread> workers;
  workers.reserve(threads);
  for (int t = 0; t < threads; ++t) {
    workers.emplace_back([&, t]() {
      for (int i = 0; i < msgs_per_thread; ++i) {
        NET_LOG_INFO(logger) << payload;
      }
    });
  }
  for (auto& w : workers) {
    w.join();
  }

  auto t1 = Clock::now();
  auto us = std::chrono::duration_cast<Us>(t1 - t0).count();
  size_t total = static_cast<size_t>(threads) * msgs_per_thread;

  Result r;
  r.total_msgs = total;
  r.msgs_per_sec = static_cast<double>(total) * 1e6 / static_cast<double>(us);
  r.us_per_msg = static_cast<double>(us) / static_cast<double>(total);
  return r;
}

void PrintResult(const char* name, const Result& r) {
  std::printf("%-20s: %10.0f msg/s  %6.2f us/msg  (total=%zu)\n",
              name, r.msgs_per_sec, r.us_per_msg, r.total_msgs);
}

}  // namespace

int main() {
  std::printf("=== Sync Log Benchmark ===\n");

  auto r1 = BenchSync(1, 10000, 50);
  PrintResult("1thread/50B", r1);

  auto r2 = BenchSync(4, 2500, 50);
  PrintResult("4thread/50B", r2);

  auto r3 = BenchSync(1, 5000, 500);
  PrintResult("1thread/500B", r3);

  std::printf("==========================\n");
  return 0;
}
