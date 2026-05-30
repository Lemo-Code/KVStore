/**
 * @file bench_log_mt.cc
 * @brief 多线程日志吞吐压测（sync / async），可配合 Helgrind / ThreadSanitizer 做死锁检测。
 *
 * 性能运行：
 *   bin/net/bench_log_mt
 *   bin/net/bench_log_mt --threads 8 --msgs 50000
 *
 * 死锁检测（Helgrind，推荐）：
 *   tests/net/run_bench_log_mt_helgrind.sh
 * 或：
 *   NET_BENCH_QUICK=1 valgrind --tool=helgrind --error-exitcode=1 \\
 *       bin/net/bench_log_mt --quick
 *
 * 死锁 / 数据竞争（ThreadSanitizer）：
 *   cmake -DNET_LOG_BENCH_TSAN=ON .. && make bench_log_mt_tsan
 *   bin/net/bench_log_mt_tsan --quick
 */
#include "log/log.h"
#include "log/log_paths.h"
#include "thread/module.h"

#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::high_resolution_clock;
using Us = std::chrono::microseconds;

struct BenchConfig {
  int threads = 8;
  int msgs_per_thread = 25000;
  int payload_bytes = 64;
  bool quick = false;
  bool run_sync = true;
  bool run_async = true;
};

struct Result {
  const char* mode;
  int threads;
  size_t total_msgs;
  double msgs_per_sec;
  double us_per_msg;
};

BenchConfig ParseConfig(int argc, char** argv) {
  BenchConfig cfg;
  if (std::getenv("NET_BENCH_QUICK") != nullptr) {
    cfg.quick = true;
  }
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--quick") == 0) {
      cfg.quick = true;
    } else if (std::strcmp(argv[i], "--sync-only") == 0) {
      cfg.run_async = false;
    } else if (std::strcmp(argv[i], "--async-only") == 0) {
      cfg.run_sync = false;
    } else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
      cfg.threads = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--msgs") == 0 && i + 1 < argc) {
      cfg.msgs_per_thread = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--payload") == 0 && i + 1 < argc) {
      cfg.payload_bytes = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      std::printf(
          "Usage: %s [--quick] [--sync-only|--async-only]\n"
          "       [--threads N] [--msgs N] [--payload B]\n",
          argv[0]);
      std::exit(0);
    }
  }
  if (cfg.quick) {
    cfg.threads = 4;
    cfg.msgs_per_thread = 2000;
    cfg.payload_bytes = 48;
  }
  if (cfg.threads < 1) {
    cfg.threads = 1;
  }
  if (cfg.msgs_per_thread < 1) {
    cfg.msgs_per_thread = 1;
  }
  if (cfg.payload_bytes < 1) {
    cfg.payload_bytes = 1;
  }
  return cfg;
}

void Worker(net::Logger::ptr logger, int msgs, const std::string& payload) {
  for (int i = 0; i < msgs; ++i) {
    NET_LOG_INFO(logger) << payload;
  }
}

Result Bench(bool async, const BenchConfig& cfg) {
  const std::string path = net::ResolveLogPath(async ? "bench_mt_async.log"
                                 : "bench_mt_sync.log");
  const std::string payload(static_cast<size_t>(cfg.payload_bytes), 'L');

  net::Logger::ptr logger;
  if (async) {
    logger = net::AsyncLoggerMgr::GetInstance()->getLogger("bench_mt_async");
  } else {
    logger = net::LoggerMgr::GetInstance()->getLogger("bench_mt_sync");
  }
  logger->clearAppenders();
  logger->setFormatter("[%t][%N] %m");
  logger->addAppender(
      net::LogAppender::ptr(new net::FileLogAppender(path)));

  // 预热，触发 async worker 等初始化路径
  for (int i = 0; i < 50; ++i) {
    NET_LOG_DEBUG(logger) << "warmup";
  }
  if (async) {
    net::AsyncLogMgr::GetInstance()->flush();
  }

  const auto t0 = Clock::now();

  std::vector<net::Thread::ptr> workers;
  workers.reserve(static_cast<size_t>(cfg.threads));
  for (int t = 0; t < cfg.threads; ++t) {
    const std::string name =
        std::string(async ? "alog_" : "slog_") + std::to_string(t);
    workers.push_back(net::Thread::ptr(new net::Thread(
        [logger, cfg, &payload]() { Worker(logger, cfg.msgs_per_thread, payload); },
        name)));
  }
  for (auto& w : workers) {
    w->join();
  }

  if (async) {
    net::AsyncLogMgr::GetInstance()->flush();
  }

  const auto t1 = Clock::now();
  const auto us = std::chrono::duration_cast<Us>(t1 - t0).count();
  const size_t total =
      static_cast<size_t>(cfg.threads) * static_cast<size_t>(cfg.msgs_per_thread);

  Result r;
  r.mode = async ? "async" : "sync";
  r.threads = cfg.threads;
  r.total_msgs = total;
  r.msgs_per_sec =
      us > 0 ? static_cast<double>(total) * 1e6 / static_cast<double>(us) : 0.0;
  r.us_per_msg =
      total > 0 ? static_cast<double>(us) / static_cast<double>(total) : 0.0;
  return r;
}

void PrintResult(const Result& r) {
  std::printf(
      "[%s] threads=%d  total=%zu  throughput=%10.0f msg/s  latency=%6.2f us/msg\n",
      r.mode, r.threads, r.total_msgs, r.msgs_per_sec, r.us_per_msg);
}

void PrintHeader(const BenchConfig& cfg) {
  std::printf("=== Multi-thread Log Benchmark ===\n");
  std::printf("threads=%d  msgs/thread=%d  payload=%dB  quick=%s\n",
              cfg.threads, cfg.msgs_per_thread, cfg.payload_bytes,
              cfg.quick ? "yes" : "no");
  if (cfg.quick) {
    std::printf("(quick mode: for Helgrind/TSan deadlock checks)\n");
  }
}

}  // namespace

int main(int argc, char** argv) {
  const BenchConfig cfg = ParseConfig(argc, argv);
  net::Thread::SetName("bench_main");
  PrintHeader(cfg);

  if (cfg.run_sync) {
    PrintResult(Bench(false, cfg));
  }
  if (cfg.run_async) {
    PrintResult(Bench(true, cfg));
  }

  std::printf("==================================\n");
  return 0;
}
