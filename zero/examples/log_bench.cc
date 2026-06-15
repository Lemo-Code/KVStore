// Async log benchmark — 验证 300w+ QPS
#include "zero/log/async_log.h"
#include "zero/log/log.h"
#include "zero/thread/thread.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <vector>

int main() {
    printf("=== Async Log Benchmark ===\n");

    // 启动异步 writer — 写入临时文件
    auto& writer = zero::AsyncLogWriter::GetInstance();
    auto file_app = std::make_shared<zero::AsyncFileAppender>(
        "/tmp/zero_log_bench.log", 1024ULL * 1024 * 1024, 5, false);
    writer.addAppender(file_app);
    writer.start();

    // 每线程独立 channel (SPSC 无锁)
    const int kThreads = 4;
    const int kMsgsPerThread = 500000;  // 50w msg/thread → 200w total
    std::atomic<uint64_t> total_written{0};
    std::atomic<uint64_t> total_dropped{0};
    std::atomic<bool> start_flag{false};

    std::vector<zero::Thread::ptr> threads;
    std::vector<zero::AsyncLogChannel::ptr> channels;
    threads.reserve(kThreads);
    channels.reserve(kThreads);

    for (int t = 0; t < kThreads; t++) {
        auto ch = std::make_shared<zero::AsyncLogChannel>();
        writer.registerChannel(ch);
        channels.push_back(ch);
    }

    auto start = std::chrono::steady_clock::now();

    for (int t = 0; t < kThreads; t++) {
        auto ch = channels[t];  // 每个线程独立 channel
        threads.push_back(std::make_shared<zero::Thread>(
            [&, t, ch]() {
                while (!start_flag.load(std::memory_order_acquire))
                    ;
                for (int i = 0; i < kMsgsPerThread; i++) {
                    bool ok = ch->write(
                        zero::GetCurrentMS(), zero::GetThreadId(),
                        0, zero::LogLevel::INFO,
                        "log_bench.cc", 42,
                        "benchmark log message payload data xxxxxxxxxxxxxxxxxxxxxx", 54);
                    if (ok) total_written.fetch_add(1);
                    else    total_dropped.fetch_add(1);
                }
            },
            "log_bench_" + std::to_string(t)));
    }

    // 开始!
    start_flag.store(true, std::memory_order_release);

    // 等待所有线程完成
    for (auto& thr : threads) thr->join();

    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // 等待 writer 消化完
    bool all_empty;
    do {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        all_empty = true;
        for (auto& ch : channels) {
            if (!ch->empty()) { all_empty = false; break; }
        }
    } while (!all_empty);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    uint64_t written = total_written.load();
    double seconds = elapsed / 1000.0;
    double qps = written / seconds;

    printf("Threads:        %d\n", kThreads);
    printf("Messages/thread:%d\n", kMsgsPerThread);
    printf("Total written:  %lu\n", written);
    printf("Total dropped:  %lu\n", total_dropped.load());
    printf("Time:           %.3f s\n", seconds);
    printf("QPS:            %.0f req/s\n", qps);
    printf("QPS per thread: %.0f req/s\n", qps / kThreads);

    if (qps > 3000000) {
        printf("=== PASS (>3M QPS) ===\n");
    } else {
        printf("=== FAIL (<3M QPS, got %.0f) ===\n", qps);
    }

    writer.stop();
    return qps >= 3000000 ? 0 : 1;
}
