/**
 * @file bench_log_perf.cc
 * @brief 单线程 / 多线程日志吞吐压测，日志写入 log/ 目录。
 *
 * 两种布局（对照）：
 *   shared  — 多线程共用一个 Logger + 一个文件（测锁竞争）
 *   sharded — 每线程独立 Logger + 独立文件（减轻 Logger 锁，观察写盘/worker 瓶颈）
 *
 * 指标：
 *   aggregate msg/s  = 总条数 / 墙钟
 *   per-thread msg/s = 每线程条数 / 墙钟
 *   us/call          = 生产阶段墙钟 / 每线程条数
 *
 * 运行：
 *   bin/net/bench_log_perf
 *   bin/net/bench_log_perf --threads 1,4,8 --msgs 50000
 *   bin/net/bench_log_perf --sharded-only
 */
#include "log/log.h"
#include "log/log_paths.h"
#include "thread/module.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::high_resolution_clock;
using Us = std::chrono::microseconds;

enum class Layout { kShared, kSharded };

struct BenchConfig {
  std::vector<int> thread_counts{1, 4, 8};
  int msgs_per_thread = 50000;
  int payload_bytes = 64;
  bool run_sync = true;
  bool run_async = true;
  bool run_shared = true;
  bool run_sharded = true;
  bool quick = false;
};

struct Result {
  const char* mode;
  const char* layout;
  int threads;
  int msgs_per_thread;
  size_t total_msgs;
  size_t file_lines;
  double wall_ms;
  double aggregate_msg_s;
  double per_thread_msg_s;
  double us_per_call;
  long long flush_us;
  std::string log_path;
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
    } else if (std::strcmp(argv[i], "--shared-only") == 0) {
      cfg.run_sharded = false;
    } else if (std::strcmp(argv[i], "--sharded-only") == 0) {
      cfg.run_shared = false;
    } else if (std::strcmp(argv[i], "--msgs") == 0 && i + 1 < argc) {
      cfg.msgs_per_thread = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--payload") == 0 && i + 1 < argc) {
      cfg.payload_bytes = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
      cfg.thread_counts.clear();
      const char* p = argv[++i];
      const char* start = p;
      for (;;) {
        const char* comma = std::strchr(start, ',');
        const std::string token(start,
                                comma != nullptr ? comma : start + std::strlen(start));
        const int n = std::atoi(token.c_str());
        if (n > 0) {
          cfg.thread_counts.push_back(n);
        }
        if (comma == nullptr) {
          break;
        }
        start = comma + 1;
      }
    } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      std::printf(
          "Usage: %s [--quick] [--sync-only|--async-only]\n"
          "       [--shared-only|--sharded-only]\n"
          "       [--threads 1,4,8] [--msgs N] [--payload B]\n"
          "\n"
          "  shared  : one Logger + one file for all threads\n"
          "  sharded : one Logger + one file per thread\n"
          "\n"
          "Logs: %s (NET_LOG_DIR to override)\n",
          argv[0], net::GetLogDir().c_str());
      std::exit(0);
    }
  }
  if (cfg.quick) {
    cfg.thread_counts = {1, 4};
    cfg.msgs_per_thread = 5000;
    cfg.payload_bytes = 48;
  }
  if (cfg.thread_counts.empty()) {
    cfg.thread_counts.push_back(1);
  }
  if (cfg.msgs_per_thread < 1) {
    cfg.msgs_per_thread = 1;
  }
  if (cfg.payload_bytes < 1) {
    cfg.payload_bytes = 1;
  }
  return cfg;
}

const char* LayoutName(Layout layout) {
  return layout == Layout::kShared ? "shared" : "sharded";
}

std::string BenchLogPath(bool async, Layout layout, int threads, int thread_id) {
  const char* mode = async ? "async" : "sync";
  const char* lay = layout == Layout::kShared ? "shared" : "sharded";
  std::string name = std::string("bench_perf_") + lay + "_" + mode + "_" +
                     std::to_string(threads) + "t";
  if (layout == Layout::kSharded) {
    name += "_w" + std::to_string(thread_id);
  }
  name += ".log";
  return net::ResolveLogPath(name);
}

void RemoveLogFile(const std::string& path) {
  std::remove(path.c_str());
}

size_t CountFileLines(const std::string& path) {
  std::ifstream ifs(path.c_str());
  if (!ifs) {
    return 0;
  }
  size_t n = 0;
  std::string line;
  while (std::getline(ifs, line)) {
    ++n;
  }
  return n;
}

net::Logger::ptr MakeBenchLogger(bool async, Layout layout, int threads,
                                 int thread_id, const std::string& path) {
  const std::string name = std::string("bench_perf_") +
                           (async ? "async_" : "sync_") + LayoutName(layout) +
                           "_" + std::to_string(threads) + "t" +
                           (layout == Layout::kSharded
                                ? ("_w" + std::to_string(thread_id))
                                : "");

  net::Logger::ptr logger;
  if (async) {
    logger = net::AsyncLoggerMgr::GetInstance()->getLogger(name);
  } else {
    logger = net::LoggerMgr::GetInstance()->getLogger(name);
  }
  logger->clearAppenders();
  logger->setFormatter("%m");
  logger->addAppender(net::LogAppender::ptr(new net::FileLogAppender(path)));
  return logger;
}

void Worker(net::Logger::ptr logger, int msgs, const std::string& payload) {
  for (int i = 0; i < msgs; ++i) {
    NET_LOG_INFO(logger) << payload;
  }
}

Result RunBench(bool async, Layout layout, int threads, const BenchConfig& cfg) {
  const std::string payload(static_cast<size_t>(cfg.payload_bytes), 'P');
  std::vector<std::string> paths;
  std::vector<net::Logger::ptr> loggers;

  if (layout == Layout::kShared) {
    paths.push_back(BenchLogPath(async, layout, threads, 0));
    RemoveLogFile(paths.back());
    loggers.push_back(MakeBenchLogger(async, layout, threads, 0, paths.back()));
  } else {
    paths.reserve(static_cast<size_t>(threads));
    loggers.reserve(static_cast<size_t>(threads));
    for (int t = 0; t < threads; ++t) {
      paths.push_back(BenchLogPath(async, layout, threads, t));
      RemoveLogFile(paths.back());
      loggers.push_back(MakeBenchLogger(async, layout, threads, t, paths.back()));
    }
  }

  const auto t0 = Clock::now();

  if (threads == 1) {
    Worker(loggers[0], cfg.msgs_per_thread, payload);
  } else if (layout == Layout::kShared) {
    net::Logger::ptr logger = loggers[0];
    std::vector<net::Thread::ptr> workers;
    workers.reserve(static_cast<size_t>(threads));
    for (int t = 0; t < threads; ++t) {
      const std::string name =
          std::string(async ? "alog_" : "slog_") + std::to_string(t);
      workers.push_back(net::Thread::ptr(new net::Thread(
          [logger, cfg, &payload]() {
            Worker(logger, cfg.msgs_per_thread, payload);
          },
          name)));
    }
    for (auto& w : workers) {
      w->join();
    }
  } else {
    std::vector<net::Thread::ptr> workers;
    workers.reserve(static_cast<size_t>(threads));
    for (int t = 0; t < threads; ++t) {
      net::Logger::ptr logger = loggers[static_cast<size_t>(t)];
      const std::string name =
          std::string(async ? "alog_" : "slog_") + std::to_string(t);
      workers.push_back(net::Thread::ptr(new net::Thread(
          [logger, cfg, &payload]() {
            Worker(logger, cfg.msgs_per_thread, payload);
          },
          name)));
    }
    for (auto& w : workers) {
      w->join();
    }
  }

  const auto t1 = Clock::now();
  long long flush_us = 0;
  if (async) {
    const auto f0 = Clock::now();
    net::AsyncLogMgr::GetInstance()->flush();
    flush_us = std::chrono::duration_cast<Us>(Clock::now() - f0).count();
  }

  const auto t2 = Clock::now();

  const auto produce_us = std::chrono::duration_cast<Us>(t1 - t0).count();
  const auto total_us = std::chrono::duration_cast<Us>(t2 - t0).count();
  const size_t total =
      static_cast<size_t>(threads) * static_cast<size_t>(cfg.msgs_per_thread);

  size_t file_lines = 0;
  for (const auto& p : paths) {
    file_lines += CountFileLines(p);
  }

  Result r;
  r.mode = async ? "async" : "sync";
  r.layout = LayoutName(layout);
  r.threads = threads;
  r.msgs_per_thread = cfg.msgs_per_thread;
  r.total_msgs = total;
  r.file_lines = file_lines;
  r.wall_ms = static_cast<double>(total_us) / 1000.0;
  r.flush_us = flush_us;
  if (layout == Layout::kShared) {
    r.log_path = paths[0];
  } else {
    r.log_path = BenchLogPath(async, layout, threads, 0) + " .. _w" +
                 std::to_string(threads - 1) + ".log (" +
                 std::to_string(threads) + " files)";
  }
  r.aggregate_msg_s =
      total_us > 0 ? static_cast<double>(total) * 1e6 / static_cast<double>(total_us)
                   : 0.0;
  r.per_thread_msg_s =
      total_us > 0
          ? static_cast<double>(cfg.msgs_per_thread) * 1e6 / static_cast<double>(total_us)
          : 0.0;
  r.us_per_call =
      cfg.msgs_per_thread > 0
          ? static_cast<double>(produce_us) /
                static_cast<double>(cfg.msgs_per_thread)
          : 0.0;
  return r;
}

void PrintResult(const Result& r) {
  const char* line_ok = (r.file_lines == r.total_msgs) ? "ok" : "MISMATCH";
  std::printf(
      "  %-5s %-7s thr=%d  total=%zu  lines=%zu(%s)  wall=%.1fms",
      r.mode, r.layout, r.threads, r.total_msgs, r.file_lines, line_ok, r.wall_ms);
  if (r.flush_us > 0) {
    std::printf("  flush=%.1fms", static_cast<double>(r.flush_us) / 1000.0);
  }
  std::printf("\n");
  std::printf(
      "         aggregate=%10.0f msg/s   per-thread=%10.0f msg/s   "
      "us/call(produce)=%7.2f\n",
      r.aggregate_msg_s, r.per_thread_msg_s, r.us_per_call);
  std::printf("         -> %s\n", r.log_path.c_str());
}

void PrintCompareRow(const Result& shared, const Result& sharded) {
  const double agg_gain =
      shared.aggregate_msg_s > 0
          ? (sharded.aggregate_msg_s / shared.aggregate_msg_s - 1.0) * 100.0
          : 0.0;
  const double us_lower =
      shared.us_per_call > 0
          ? (shared.us_per_call - sharded.us_per_call) / shared.us_per_call *
                100.0
          : 0.0;
  std::printf(
      "  [compare %s thr=%d] sharded vs shared: aggregate %+.0f%%   "
      "us/call %.0f%% lower\n",
      shared.mode, shared.threads, agg_gain, us_lower);
}

void PrintHeader(const BenchConfig& cfg) {
  if (!net::EnsureLogDir()) {
    std::fprintf(stderr, "warn: cannot create log dir: %s\n",
                 net::GetLogDir().c_str());
  }
  std::printf("=== Log Performance Benchmark ===\n");
  std::printf("log_dir=%s\n", net::GetLogDir().c_str());
  std::printf("msgs/thread=%d  payload=%dB  quick=%s\n", cfg.msgs_per_thread,
              cfg.payload_bytes, cfg.quick ? "yes" : "no");
  std::printf("layouts:");
  if (cfg.run_shared) {
    std::printf(" shared");
  }
  if (cfg.run_sharded) {
    std::printf(" sharded");
  }
  std::printf("\nthread_counts:");
  for (int n : cfg.thread_counts) {
    std::printf(" %d", n);
  }
  std::printf("\n\n");
}

void RunMode(bool async, const BenchConfig& cfg) {
  std::printf("[%s]\n", async ? "async" : "sync");

  for (int n : cfg.thread_counts) {
    if (cfg.run_shared) {
      std::printf("--- shared (1 logger, 1 file) ---\n");
      const Result shared = RunBench(async, Layout::kShared, n, cfg);
      PrintResult(shared);

      if (cfg.run_sharded) {
        std::printf("--- sharded (1 logger + 1 file per thread) ---\n");
        const Result sharded = RunBench(async, Layout::kSharded, n, cfg);
        PrintResult(sharded);
        PrintCompareRow(shared, sharded);
      }
    } else if (cfg.run_sharded) {
      std::printf("--- sharded (1 logger + 1 file per thread) ---\n");
      PrintResult(RunBench(async, Layout::kSharded, n, cfg));
    }
    std::printf("\n");
  }
}

}  // namespace

int main(int argc, char** argv) {
  const BenchConfig cfg = ParseConfig(argc, argv);
  net::Thread::SetName("bench_main");
  PrintHeader(cfg);

  if (cfg.run_sync) {
    RunMode(false, cfg);
  }
  if (cfg.run_async) {
    RunMode(true, cfg);
  }

  std::printf("=================================\n");
  return 0;
}
