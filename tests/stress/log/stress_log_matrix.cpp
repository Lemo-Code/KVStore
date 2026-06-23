/**
 * stress_log_matrix.cpp — zeroLog vs spdlog 异步日志 QPS 矩阵
 *
 * 仅对比异步模式, 避免同步刷盘干扰 QPS
 * 单位: lines/s (QPS)
 */
#include "bench_utils.h"
#include "matrix.h"
using namespace stress;

#include "zero/log/async_log.h"
#include "zero/log/log.h"
#include "zero/log/config.h"
#include "zero/thread/thread.h"

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdio>
#include <string>
#include <vector>
#include <sys/stat.h>

static const char* kPayload =
    "benchmark log message payload data xxxxxxxxxxxxxxxxxxxxxxxxxxxx";

static void benchZeroAsync(int threads, uint64_t& total_ops, double& elapsed_sec,
                           int msgs_per_thread, const std::string& logfile) {
    auto& writer = zero::AsyncLogWriter::GetInstance();
    writer.stop();
    writer.clearAppenders();
    writer.addAppender(std::make_shared<zero::AsyncFileAppender>(
        logfile, 1024ULL * 1024 * 1024, 5, false));
    writer.start();

    std::vector<zero::AsyncLogChannel::ptr> channels;
    channels.reserve(threads);
    for (int t = 0; t < threads; ++t) {
        auto ch = std::make_shared<zero::AsyncLogChannel>();
        writer.registerChannel(ch);
        channels.push_back(ch);
    }

    std::atomic<uint64_t> ops{0};
    std::atomic<bool> go{false};
    std::vector<zero::Thread::ptr> thrs;
    thrs.reserve(threads);

    for (int t = 0; t < threads; ++t) {
        auto ch = channels[t];
        thrs.push_back(std::make_shared<zero::Thread>([&, ch]() {
            while (!go.load(std::memory_order_acquire)) {}
            for (int i = 0; i < msgs_per_thread; ++i) {
                ch->write(zero::GetCurrentMS(), zero::GetThreadId(), 0,
                          zero::LogLevel::INFO, "stress_log", 1,
                          kPayload, (uint32_t)strlen(kPayload));
                ops.fetch_add(1, std::memory_order_relaxed);
            }
        }, "zero_async_" + std::to_string(t)));
    }

    auto t0 = stress::nowUs();
    go.store(true, std::memory_order_release);
    for (auto& t : thrs) t->join();
    for (int i = 0; i < 50; ++i) {
        bool empty = true;
        for (auto& ch : channels) {
            if (!ch->empty()) { empty = false; break; }
        }
        if (empty) break;
        usleep(10000);
    }
    writer.stop();
    for (auto& ch : channels) writer.unregisterChannel(ch.get());
    elapsed_sec = stress::elapsedSec(t0);
    total_ops = ops.load();
}

static void benchSpdlogAsync(int threads, uint64_t& total_ops, double& elapsed_sec,
                             int msgs_per_thread, const std::string& logfile) {
    std::string name = "spdlog_async_" + std::to_string(threads);
    spdlog::init_thread_pool(8192, 1);
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logfile, true);
    auto logger = std::make_shared<spdlog::async_logger>(
        name, sink, spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
    logger->set_level(spdlog::level::info);
    spdlog::register_logger(logger);

    std::atomic<uint64_t> ops{0};
    std::atomic<bool> go{false};
    std::vector<std::thread> thrs;
    thrs.reserve(threads);

    for (int t = 0; t < threads; ++t) {
        thrs.emplace_back([&]() {
            while (!go.load(std::memory_order_acquire)) {}
            for (int i = 0; i < msgs_per_thread; ++i) {
                logger->info(kPayload);
                ops.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    auto t0 = stress::nowUs();
    go.store(true, std::memory_order_release);
    for (auto& t : thrs) t.join();
    logger->flush();
    elapsed_sec = stress::elapsedSec(t0);
    total_ops = ops.load();
    spdlog::drop(name);
    spdlog::shutdown();
}

int main(int argc, char** argv) {
    zero::LogConfig::SetupBenchSilent();

    std::string out_dir = resolveBenchOutDir(argc, argv);
    ensureDir(out_dir);
    printf("输出目录: %s\n", out_dir.c_str());
    printf("模式: 仅异步日志 (async), 单位 lines/s\n\n");

    const int msgs_per_thread = 100000;
    std::string tmpdir = "/tmp/kvstore_stress_log";
    mkdir(tmpdir.c_str(), 0755);

    MatrixRunner runner("zeroLog vs spdlog 异步 QPS", "lines/s (QPS)");

    runner.addRow("zero   async", [&](int threads, uint64_t& ops, double& sec) {
        std::string fname = tmpdir + "/zero_async.log";
        remove(fname.c_str());
        benchZeroAsync(threads, ops, sec, msgs_per_thread, fname);
    });
    runner.addRow("spdlog async", [&](int threads, uint64_t& ops, double& sec) {
        std::string fname = tmpdir + "/spdlog_async.log";
        remove(fname.c_str());
        benchSpdlogAsync(threads, ops, sec, msgs_per_thread, fname);
    });

    runner.run();
    runner.printMatrix();
    runner.saveLog(out_dir + "/log_compare.log");
    return 0;
}
