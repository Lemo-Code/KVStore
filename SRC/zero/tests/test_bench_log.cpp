// test_bench_log.cpp — Performance benchmarks for the logging subsystem
//
// Measures: QPS (queries/messages per second), throughput (MB/s),
// latency (us/op) at p50/p99/p999, for both single-threaded and
// multi-threaded scenarios. Covers null-sink overhead, file-sink
// throughput, async vs sync comparison, level-filtered path cost,
// message size impact, formatter overhead, and mixed-level workloads.
//
// All timing uses std::chrono::high_resolution_clock.
// Results printed with std::cout using [BENCH] prefix.

#include <gtest/gtest.h>
#include "zero/zero.h"
#include "zero/log/log.h"

#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <iostream>
#include <iomanip>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <fstream>
#include <unistd.h>

using namespace zero;
using namespace std::chrono;

// ============================================================
// Helper: strip newline for clean output
// ============================================================

static std::string strip_newline(const std::string& s) {
    auto r = s;
    while (!r.empty() && (r.back() == '\n' || r.back() == '\r'))
        r.pop_back();
    return r;
}

// ============================================================
// Helper: format large numbers with commas
// ============================================================

static std::string fmt_num(long long n) {
    if (n == 0) return "0";
    bool neg = n < 0;
    if (neg) n = -n;
    std::string s;
    int cnt = 0;
    while (n > 0) {
        if (cnt && cnt % 3 == 0) s = "," + s;
        s = static_cast<char>('0' + (n % 10)) + s;
        n /= 10;
        ++cnt;
    }
    return neg ? "-" + s : s;
}

// ============================================================
// Single-thread QPS benchmarks
// ============================================================

TEST(BenchLog, SingleThreadQPS) {
    Logger* log = Logger::get("bench.qps");
    log->set_level(LogLevel::INFO);
    log->set_inherit_appenders(false);
    log->clear_appenders();

    const int N = 1000000; // 1M messages
    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        log->info(__FILE__, __LINE__, "benchmark message " + std::to_string(i));
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    double qps = (double)N / (us / 1000000.0);
    std::cout << "[BENCH] Log SingleThread QPS: " << std::fixed << std::setprecision(0)
              << qps << " msgs/sec (" << fmt_num(N) << " msgs in "
              << us / 1000 << " ms)" << std::endl;
    EXPECT_GT(qps, 0);
}

TEST(BenchLog, SingleThreadDebugQPS) {
    Logger* log = Logger::get("bench.debug");
    log->set_level(LogLevel::DEBUG);
    log->set_inherit_appenders(false);
    log->clear_appenders();

    const int N = 1000000;
    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        log->debug(__FILE__, __LINE__, "debug benchmark " + std::to_string(i));
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    double qps = (double)N / (us / 1000000.0);
    std::cout << "[BENCH] Log DEBUG QPS: " << std::fixed << std::setprecision(0)
              << qps << " msgs/sec (" << fmt_num(N) << " msgs in "
              << us / 1000 << " ms)" << std::endl;
    EXPECT_GT(qps, 0);
}

TEST(BenchLog, SingleThreadTraceQPS) {
    Logger* log = Logger::get("bench.trace");
    log->set_level(LogLevel::TRACE);
    log->set_inherit_appenders(false);
    log->clear_appenders();

    const int N = 1000000;
    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        log->trace(__FILE__, __LINE__, "trace benchmark " + std::to_string(i));
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    double qps = (double)N / (us / 1000000.0);
    std::cout << "[BENCH] Log TRACE QPS: " << std::fixed << std::setprecision(0)
              << qps << " msgs/sec (" << fmt_num(N) << " msgs in "
              << us / 1000 << " ms)" << std::endl;
    EXPECT_GT(qps, 0);
}

// ============================================================
// Multi-thread QPS benchmark
// ============================================================

TEST(BenchLog, MultiThreadQPS) {
    Logger* log = Logger::get("bench.mt");
    log->set_level(LogLevel::INFO);
    log->set_inherit_appenders(false);
    log->clear_appenders();

    const int threads_count = 8;
    const int per_thread = 100000;
    std::atomic<bool> start_flag{false};
    std::atomic<long long> total{0};

    auto worker = [&]() {
        while (!start_flag.load(std::memory_order_acquire))
            std::this_thread::yield();
        for (int i = 0; i < per_thread; ++i) {
            log->info(__FILE__, __LINE__, "mt benchmark " + std::to_string(i));
        }
        total.fetch_add(per_thread);
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < threads_count; ++t)
        threads.emplace_back(worker);

    auto t1 = high_resolution_clock::now();
    start_flag.store(true, std::memory_order_release);
    for (auto& t : threads) t.join();
    auto t2 = high_resolution_clock::now();

    auto us = duration_cast<microseconds>(t2 - t1).count();
    long long total_msgs = total.load();
    double qps = (double)total_msgs / (us / 1000000.0);
    std::cout << "[BENCH] Log " << threads_count << "-Thread QPS: "
              << std::fixed << std::setprecision(0)
              << qps << " msgs/sec (" << fmt_num(total_msgs)
              << " msgs in " << us / 1000 << " ms)" << std::endl;
    EXPECT_GT(qps, 0);
}

TEST(BenchLog, MultiThreadQPS_16threads) {
    Logger* log = Logger::get("bench.mt16");
    log->set_level(LogLevel::INFO);
    log->set_inherit_appenders(false);
    log->clear_appenders();

    const int threads_count = 16;
    const int per_thread = 50000;
    std::atomic<bool> start_flag{false};
    std::atomic<long long> total{0};

    auto worker = [&]() {
        while (!start_flag.load(std::memory_order_acquire))
            std::this_thread::yield();
        for (int i = 0; i < per_thread; ++i) {
            log->info(__FILE__, __LINE__, "mt16 " + std::to_string(i));
        }
        total.fetch_add(per_thread);
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < threads_count; ++t)
        threads.emplace_back(worker);

    auto t1 = high_resolution_clock::now();
    start_flag.store(true, std::memory_order_release);
    for (auto& t : threads) t.join();
    auto t2 = high_resolution_clock::now();

    auto us = duration_cast<microseconds>(t2 - t1).count();
    long long total_msgs = total.load();
    double qps = (double)total_msgs / (us / 1000000.0);
    std::cout << "[BENCH] Log " << threads_count << "-Thread QPS: "
              << std::fixed << std::setprecision(0)
              << qps << " msgs/sec (" << fmt_num(total_msgs)
              << " msgs in " << us / 1000 << " ms)" << std::endl;
    EXPECT_GT(qps, 0);
}

// ============================================================
// Null-sink overhead (no appenders)
// ============================================================

TEST(BenchLog, NullSinkQPS) {
    Logger* log = Logger::get("bench.null");
    log->set_level(LogLevel::INFO);
    log->set_inherit_appenders(false);
    log->clear_appenders();

    const int N = 5000000;
    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        log->info(__FILE__, __LINE__, "x");
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    double qps = (double)N / (us / 1000000.0);
    double ns_per_op = (double)us * 1000.0 / N;
    std::cout << "[BENCH] Log NullSink QPS: " << std::fixed << std::setprecision(0)
              << qps << " msgs/sec, " << std::setprecision(1)
              << ns_per_op << " ns/op (" << fmt_num(N) << " msgs in "
              << us / 1000 << " ms)" << std::endl;
    EXPECT_GT(qps, 0);
}

// ============================================================
// File-sink QPS benchmark
// ============================================================

TEST(BenchLog, FileSinkQPS) {
    std::string path = "/tmp/zero_bench_log_" + std::to_string(getpid()) + ".log";
    Logger* log = Logger::get("bench.file");
    log->set_level(LogLevel::INFO);
    log->set_inherit_appenders(false);
    log->clear_appenders();
    log->add_appender(std::make_shared<FileAppender>(path));

    const int N = 500000;
    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        log->info(__FILE__, __LINE__,
                  "file benchmark message number " + std::to_string(i));
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    double qps = (double)N / (us / 1000000.0);

    // Get file size for throughput calculation
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    size_t file_size = in.tellg();
    in.close();

    double mbps = (file_size / (1024.0 * 1024.0)) / (us / 1000000.0);

    std::cout << "[BENCH] Log FileSink QPS: " << std::fixed << std::setprecision(0)
              << qps << " msgs/sec, throughput: " << std::setprecision(2)
              << mbps << " MB/s (" << fmt_num(N) << " msgs, "
              << file_size / 1024 << " KB in " << us / 1000 << " ms)" << std::endl;

    // flush and remove
    log->clear_appenders();
    std::remove(path.c_str());
    EXPECT_GT(qps, 0);
}

TEST(BenchLog, FileSinkSmallMessages) {
    std::string path = "/tmp/zero_bench_small_" + std::to_string(getpid()) + ".log";
    Logger* log = Logger::get("bench.file_small");
    log->set_level(LogLevel::INFO);
    log->set_inherit_appenders(false);
    log->clear_appenders();
    log->add_appender(std::make_shared<FileAppender>(path));

    const int N = 500000;
    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        log->info(__FILE__, __LINE__, "x");
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    double qps = (double)N / (us / 1000000.0);

    std::cout << "[BENCH] Log FileSink (1-char) QPS: " << std::fixed
              << std::setprecision(0) << qps << " msgs/sec ("
              << fmt_num(N) << " msgs in " << us / 1000 << " ms)" << std::endl;

    log->clear_appenders();
    std::remove(path.c_str());
    EXPECT_GT(qps, 0);
}

// ============================================================
// Level-filter overhead
// ============================================================

TEST(BenchLog, LevelFilterOverhead) {
    Logger* log = Logger::get("bench.filter");
    log->set_level(LogLevel::ERROR);
    log->set_inherit_appenders(false);
    log->clear_appenders();

    const int N = 5000000;
    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        ZERO_LOG_TRACE(log, "should be filtered " << i);
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    double qps = (double)N / (us / 1000000.0);
    double ns_per_op = (double)us * 1000.0 / N;
    std::cout << "[BENCH] Log Filtered TRACE->ERROR: " << std::fixed
              << std::setprecision(0) << qps << " checks/sec, "
              << std::setprecision(1) << ns_per_op << " ns/check ("
              << fmt_num(N) << " checks in " << us / 1000 << " ms)" << std::endl;
    EXPECT_GT(qps, 0);
}

TEST(BenchLog, MacroFilterOverhead) {
    // Measure the cost of the macro guard itself (ZERO_LOG checks level before
    // building the stream)
    Logger* log = Logger::get("bench.macrofilter");
    log->set_level(LogLevel::ERROR);

    const int N = 10000000;
    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        ZERO_LOG_DEBUG(log, "filtered debug " << i);
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    double qps = (double)N / (us / 1000000.0);
    double ns_per_op = (double)us * 1000.0 / N;
    std::cout << "[BENCH] Log MacroFilter DEBUG->ERROR: " << std::fixed
              << std::setprecision(0) << qps << " checks/sec, "
              << std::setprecision(1) << ns_per_op << " ns/check" << std::endl;
}

// ============================================================
// Latency distribution (p50, p99, p999, max)
// ============================================================

TEST(BenchLog, LatencyMicroseconds) {
    Logger* log = Logger::get("bench.latency");
    log->set_level(LogLevel::INFO);
    log->set_inherit_appenders(false);
    log->clear_appenders();

    const int N = 10000;
    std::vector<long long> latencies;
    latencies.reserve(N);

    for (int i = 0; i < N; ++i) {
        auto t1 = high_resolution_clock::now();
        log->info(__FILE__, __LINE__, "latency test");
        auto t2 = high_resolution_clock::now();
        latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
    }

    std::sort(latencies.begin(), latencies.end());
    long long p50  = latencies[N * 50 / 100];
    long long p90  = latencies[N * 90 / 100];
    long long p99  = latencies[N * 99 / 100];
    long long p999 = latencies[N * 999 / 1000];
    long long p100 = latencies.back();
    double avg = (double)std::accumulate(latencies.begin(), latencies.end(), 0LL) / N;

    std::cout << "[BENCH] Log Latency (ns): avg=" << std::fixed << std::setprecision(0)
              << avg << " p50=" << p50 << " p90=" << p90
              << " p99=" << p99 << " p999=" << p999 << " max=" << p100
              << std::endl;
    std::cout << "[BENCH] Log Latency (us): avg=" << std::setprecision(3)
              << avg / 1000.0 << " p50=" << p50 / 1000.0
              << " p90=" << p90 / 1000.0 << " p99=" << p99 / 1000.0
              << " max=" << p100 / 1000.0 << std::endl;

    EXPECT_GT(N, 0); // sanity
}

TEST(BenchLog, FileSinkLatency) {
    std::string path = "/tmp/zero_bench_lat_" + std::to_string(getpid()) + ".log";
    Logger* log = Logger::get("bench.lat_file");
    log->set_level(LogLevel::INFO);
    log->set_inherit_appenders(false);
    log->clear_appenders();
    log->add_appender(std::make_shared<FileAppender>(path));

    const int N = 1000;
    std::vector<long long> latencies;
    latencies.reserve(N);

    for (int i = 0; i < N; ++i) {
        auto t1 = high_resolution_clock::now();
        log->info(__FILE__, __LINE__, "file latency test");
        auto t2 = high_resolution_clock::now();
        latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
    }

    std::sort(latencies.begin(), latencies.end());
    long long p50  = latencies[N * 50 / 100];
    long long p99  = latencies[N * 99 / 100];
    long long p100 = latencies.back();
    double avg = (double)std::accumulate(latencies.begin(), latencies.end(), 0LL) / N;

    std::cout << "[BENCH] Log FileSink Latency: avg=" << std::fixed
              << std::setprecision(3) << avg / 1000.0
              << " us, p50=" << p50 / 1000.0 << " us, p99="
              << p99 / 1000.0 << " us, max=" << p100 / 1000.0
              << " us" << std::endl;

    log->clear_appenders();
    std::remove(path.c_str());
}

// ============================================================
// Async vs Sync comparison
// ============================================================

TEST(BenchLog, AsyncVsSync) {
    std::string sync_path =
        "/tmp/zero_sync_" + std::to_string(getpid()) + ".log";
    std::string async_path =
        "/tmp/zero_async_" + std::to_string(getpid()) + ".log";

    Logger* log_sync = Logger::get("bench.sync");
    Logger* log_async = Logger::get("bench.async");

    log_sync->set_level(LogLevel::INFO);
    log_async->set_level(LogLevel::INFO);

    log_sync->set_inherit_appenders(false);
    log_async->set_inherit_appenders(false);

    log_sync->clear_appenders();
    log_async->clear_appenders();

    auto sync_app = std::make_shared<FileAppender>(sync_path);
    auto async_file_app = std::make_shared<FileAppender>(async_path);
    auto async_wrapper = std::make_shared<AsyncLogAppender>(async_file_app);

    log_sync->add_appender(sync_app);
    log_async->add_appender(async_wrapper);

    const int N = 100000;

    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        log_sync->info(__FILE__, __LINE__, "sync " + std::to_string(i));
    }
    auto t2 = high_resolution_clock::now();

    auto t3 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        log_async->info(__FILE__, __LINE__, "async " + std::to_string(i));
    }
    auto t4 = high_resolution_clock::now();

    auto sync_us = duration_cast<microseconds>(t2 - t1).count();
    auto async_us = duration_cast<microseconds>(t4 - t3).count();
    double sync_qps  = (double)N / (sync_us / 1000000.0);
    double async_qps = (double)N / (async_us / 1000000.0);

    // Wait for async flush before computing dropped count
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "[BENCH] Log " << fmt_num(N) << " msgs: "
              << "sync=" << sync_us / 1000 << " ms ("
              << std::fixed << std::setprecision(0) << sync_qps << " QPS), "
              << "async=" << async_us / 1000 << " ms ("
              << async_qps << " QPS), "
              << "speedup=" << std::setprecision(2)
              << (double)sync_us / async_us << "x"
              << ", dropped=" << async_wrapper->dropped()
              << std::endl;

    log_sync->clear_appenders();
    log_async->clear_appenders();
    std::remove(sync_path.c_str());
    std::remove(async_path.c_str());
}

// ============================================================
// Message size impact
// ============================================================

TEST(BenchLog, MessageSizeImpact) {
    Logger* log = Logger::get("bench.msgsize");
    log->set_level(LogLevel::INFO);
    log->set_inherit_appenders(false);
    log->clear_appenders();

    struct Case {
        const char* label;
        int         size;
        int         count;
    };
    std::vector<Case> cases = {
        {"10B",   10,   200000},
        {"100B",  100,  200000},
        {"1KB",   1000, 200000},
        {"4KB",   4000, 100000},
        {"16KB", 16000,  50000},
    };

    std::string pad_str(65536, 'X');

    std::cout << "[BENCH] Log Message Size Impact:" << std::endl;
    for (const auto& c : cases) {
        std::string msg = pad_str.substr(0, c.size);
        auto start = high_resolution_clock::now();
        for (int i = 0; i < c.count; ++i) {
            log->info(__FILE__, __LINE__, msg);
        }
        auto end = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(end - start).count();
        double qps = (double)c.count / (us / 1000000.0);
        double mbps = ((double)c.count * c.size / (1024.0 * 1024.0))
                      / (us / 1000000.0);
        std::cout << "  " << c.label << ": " << std::fixed << std::setprecision(0)
                  << qps << " QPS, " << std::setprecision(2) << mbps
                  << " MB/s (" << c.count << " msgs in " << us / 1000
                  << " ms)" << std::endl;
    }
}

// ============================================================
// Throughput in MB/s
// ============================================================

TEST(BenchLog, ThroughputMBps) {
    Logger* log = Logger::get("bench.tput");
    log->set_level(LogLevel::INFO);
    log->set_inherit_appenders(false);
    log->clear_appenders();

    const int msg_size = 200;
    const int N = 1000000;
    std::string msg(msg_size, 'B');

    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        log->info(__FILE__, __LINE__, msg);
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();

    size_t total_bytes = (size_t)N * msg_size;
    double mbps = (total_bytes / (1024.0 * 1024.0)) / (us / 1000000.0);

    std::cout << "[BENCH] Log Throughput: " << std::fixed << std::setprecision(2)
              << mbps << " MB/s (" << fmt_num(N) << " x " << msg_size
              << "B msgs in " << us / 1000 << " ms)" << std::endl;
    EXPECT_GT(mbps, 0);
}

// ============================================================
// Multi-logger QPS
// ============================================================

TEST(BenchLog, MultiLoggerQPS) {
    const int num_loggers = 16;
    const int per_logger = 50000;
    std::vector<Logger*> loggers;

    for (int i = 0; i < num_loggers; ++i) {
        Logger* l =
            Logger::get("bench.multi." + std::to_string(i));
        l->set_level(LogLevel::INFO);
        l->set_inherit_appenders(false);
        l->clear_appenders();
        loggers.push_back(l);
    }

    auto start = high_resolution_clock::now();
    for (int i = 0; i < per_logger; ++i) {
        for (int j = 0; j < num_loggers; ++j) {
            loggers[j]->info(__FILE__, __LINE__,
                             "logger" + std::to_string(j) + "_" + std::to_string(i));
        }
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();

    long long total = (long long)num_loggers * per_logger;
    double qps = (double)total / (us / 1000000.0);
    std::cout << "[BENCH] Log " << num_loggers << " Loggers QPS: "
              << std::fixed << std::setprecision(0)
              << qps << " msgs/sec (" << fmt_num(total)
              << " msgs in " << us / 1000 << " ms)" << std::endl;
    EXPECT_GT(qps, 0);
}

// ============================================================
// Mixed-level workload benchmark
// ============================================================

TEST(BenchLog, MixedLevelWorkload) {
    Logger* log = Logger::get("bench.mixed");
    log->set_level(LogLevel::TRACE);
    log->set_inherit_appenders(false);
    log->clear_appenders();

    // Simulate real-world log mix: 50% DEBUG, 30% INFO, 15% WARN, 4% ERROR, 1% TRACE
    const int N = 500000;
    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        int bucket = i % 100;
        if (bucket < 1) {
            log->trace(__FILE__, __LINE__, "trace " + std::to_string(i));
        } else if (bucket < 5) {
            log->error(__FILE__, __LINE__, "error " + std::to_string(i));
        } else if (bucket < 20) {
            log->warn(__FILE__, __LINE__, "warn " + std::to_string(i));
        } else if (bucket < 50) {
            log->info(__FILE__, __LINE__, "info " + std::to_string(i));
        } else {
            log->debug(__FILE__, __LINE__, "debug " + std::to_string(i));
        }
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    double qps = (double)N / (us / 1000000.0);
    std::cout << "[BENCH] Log Mixed-Level QPS: " << std::fixed << std::setprecision(0)
              << qps << " msgs/sec (" << fmt_num(N) << " msgs in "
              << us / 1000 << " ms)" << std::endl;
    EXPECT_GT(qps, 0);
}

// ============================================================
// Formatter overhead comparison
// ============================================================

TEST(BenchLog, FormatterOverhead) {
    std::string path1 = "/tmp/zero_fmt_default_" + std::to_string(getpid()) + ".log";
    std::string path2 = "/tmp/zero_fmt_minimal_" + std::to_string(getpid()) + ".log";

    Logger* log_default = Logger::get("bench.fmt_default");
    Logger* log_minimal = Logger::get("bench.fmt_minimal");

    log_default->set_inherit_appenders(false);
    log_minimal->set_inherit_appenders(false);
    log_default->clear_appenders();
    log_minimal->clear_appenders();

    auto app1 = std::make_shared<FileAppender>(path1);
    auto app2 = std::make_shared<FileAppender>(path2);
    // Default pattern: "%t [%L] [%N] %f:%l - %m%n"
    app2->set_formatter(std::make_shared<LogFormatter>("%m%n")); // message only

    log_default->add_appender(app1);
    log_minimal->add_appender(app2);

    const int N = 200000;
    std::string msg = "formatter benchmark message with some extra text";

    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        log_default->info(__FILE__, __LINE__, msg);
    }
    auto t2 = high_resolution_clock::now();

    auto t3 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        log_minimal->info(__FILE__, __LINE__, msg);
    }
    auto t4 = high_resolution_clock::now();

    auto def_ms = duration_cast<milliseconds>(t2 - t1).count();
    auto min_ms = duration_cast<milliseconds>(t4 - t3).count();

    std::cout << "[BENCH] Log Formatter: default=" << def_ms
              << " ms, minimal=" << min_ms << " ms ("
              << std::fixed << std::setprecision(1)
              << ((double)def_ms / min_ms) << "x)" << std::endl;

    log_default->clear_appenders();
    log_minimal->clear_appenders();
    std::remove(path1.c_str());
    std::remove(path2.c_str());
}

// ============================================================
// ConsoleAppender benchmark (capped count)
// ============================================================

TEST(BenchLog, ConsoleSinkQPS) {
    Logger* log = Logger::get("bench.console");
    log->set_level(LogLevel::WARN); // Only WARN and above to limit output
    log->set_inherit_appenders(false);
    log->clear_appenders();
    log->add_appender(std::make_shared<ConsoleAppender>(false));

    // Use WARN to actually produce output
    const int N = 10000;
    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        log->warn(__FILE__, __LINE__, "console benchmark " + std::to_string(i));
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    double qps = (double)N / (us / 1000000.0);
    std::cout << "\n[BENCH] Log Console QPS: " << std::fixed << std::setprecision(0)
              << qps << " msgs/sec (" << N << " msgs in " << us / 1000
              << " ms)" << std::endl;
}
