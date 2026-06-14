/**
 * @file bench_log_mt_perf.cc
 * @brief 多线程 sync vs async 性能对比（file appender）
 */
#include "lemo/log/async_appender.h"
#include "lemo/log/file_appender.h"
#include "lemo/log/log.h"
#include "lemo/log/pattern_layout.h"
#include "lemo/thread/module.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

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
      lemo::log::LoggerRepository::Instance().GetLogger("bench_cmp");
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

Result RunBench(Mode mode, int thread_count, int msgs_per_thread,
                int payload_bytes, const std::string& path) {
  std::remove(path.c_str());

  lemo::log::Logger::ptr logger = MakeLogger(mode, path);
  std::string payload(static_cast<size_t>(payload_bytes), 'x');
  std::atomic<int> done{0};

  auto t0 = Clock::now();
  std::vector<lemo::thread::Thread::ptr> threads;
  for (int t = 0; t < thread_count; ++t) {
    threads.push_back(lemo::thread::Thread::ptr(new lemo::thread::Thread(
        [logger, &payload, msgs_per_thread, &done]() {
          for (int i = 0; i < msgs_per_thread; ++i) {
            LEMO_LOG_INFO(logger) << payload;
          }
          done.fetch_add(1);
        },
        "bench_" + std::to_string(t))));
  }
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i]->join();
  }
  logger->Flush();
  auto t1 = Clock::now();

  const size_t total =
      static_cast<size_t>(thread_count) * static_cast<size_t>(msgs_per_thread);
  const auto us = std::chrono::duration_cast<Us>(t1 - t0).count();

  Result r;
  r.total_msgs = total;
  r.msgs_per_sec =
      static_cast<double>(total) * 1e6 / static_cast<double>(us);
  r.us_per_msg = static_cast<double>(us) / static_cast<double>(total);
  return r;
}

void PrintCompare(int threads, const Result& sync, const Result& async) {
  const double speedup = async.msgs_per_sec / sync.msgs_per_sec;
  const double sync_lat = sync.us_per_msg;
  const double async_lat = async.us_per_msg;
  const double lat_gain_pct = (sync_lat - async_lat) / sync_lat * 100.0;

  std::printf(
      "  thr=%-2d  sync %8.0f msg/s (%5.2f us/msg)  "
      "async %8.0f msg/s (%5.2f us/msg)  "
      "async/sync=%.2fx  latency -%.0f%%\n",
      threads, sync.msgs_per_sec, sync.us_per_msg, async.msgs_per_sec,
      async.us_per_msg, speedup, lat_gain_pct);
}

}  // namespace

int main() {
  const int kMsgsPerThread = 5000;
  const int kPayload = 64;
  const int kThreadCounts[] = {1, 2, 4, 8};

  std::printf("=== Lemo Sync vs Async (file, multi-thread) ===\n");
  std::printf("msgs_per_thread=%d  payload=%dB  pattern=%%m\n\n", kMsgsPerThread,
              kPayload);

  std::printf("--- 吞吐 & 延迟（含 Flush，端到端）---\n");
  for (size_t i = 0; i < sizeof(kThreadCounts) / sizeof(kThreadCounts[0]); ++i) {
    const int thr = kThreadCounts[i];
    const std::string sync_path =
        "/tmp/lemo_cmp_sync_" + std::to_string(thr) + ".log";
    const std::string async_path =
        "/tmp/lemo_cmp_async_" + std::to_string(thr) + ".log";

    const Result sync = RunBench(kSync, thr, kMsgsPerThread, kPayload, sync_path);
    const Result async =
        RunBench(kAsync, thr, kMsgsPerThread, kPayload, async_path);
    PrintCompare(thr, sync, async);

    std::remove(sync_path.c_str());
    std::remove(async_path.c_str());
  }

  std::printf("\n--- 仅测入队（async 不 Flush，sync 仍含写盘）---\n");
  for (size_t i = 0; i < sizeof(kThreadCounts) / sizeof(kThreadCounts[0]); ++i) {
    const int thr = kThreadCounts[i];
    if (thr == 1) continue;

    const std::string async_path =
        "/tmp/lemo_cmp_async_noflush_" + std::to_string(thr) + ".log";
    std::remove(async_path.c_str());

    lemo::log::Logger::ptr logger = MakeLogger(kAsync, async_path);
    std::string payload(static_cast<size_t>(kPayload), 'x');
    std::atomic<int> done{0};

    auto t0 = Clock::now();
    std::vector<lemo::thread::Thread::ptr> threads;
    for (int t = 0; t < thr; ++t) {
      threads.push_back(lemo::thread::Thread::ptr(new lemo::thread::Thread(
          [logger, &payload, &done]() {
            for (int i = 0; i < kMsgsPerThread; ++i) {
              LEMO_LOG_INFO(logger) << payload;
            }
            done.fetch_add(1);
          },
          "nf_" + std::to_string(t))));
    }
    for (size_t j = 0; j < threads.size(); ++j) {
      threads[j]->join();
    }
    auto t1 = Clock::now();

    const size_t total =
        static_cast<size_t>(thr) * static_cast<size_t>(kMsgsPerThread);
    const auto us = std::chrono::duration_cast<Us>(t1 - t0).count();
    const double enqueue_mps =
        static_cast<double>(total) * 1e6 / static_cast<double>(us);

    logger->Flush();

    std::printf("  thr=%-2d  async enqueue-only %8.0f msg/s (%5.2f us/msg)\n",
                thr, enqueue_mps,
                static_cast<double>(us) / static_cast<double>(total));
    std::remove(async_path.c_str());
  }

  std::printf("\n==========================================\n");
  std::printf("解读:\n");
  std::printf("  - 多线程下 async 优势来自: Logger 锁外入队 + TLS 批量 + worker 并行写盘\n");
  std::printf("  - 单线程本地快 IO 时 async 可能接近或略低于 sync（队列开销）\n");
  std::printf("  - 「入队-only」反映 caller 侧延迟，不含 Flush 等待\n");
  return 0;
}
