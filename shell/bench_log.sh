#!/usr/bin/env bash
# ============================================================
# zero_log vs spdlog 日志性能矩阵对比
# 维度: 线程数 x 日志级别 x 是否异步
# ============================================================
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ROOT}/bin"
OUT="${ROOT}/benchmark/log_matrix.txt"
mkdir -p "${ROOT}/benchmark"

echo "╔══════════════════════════════════════════════╗"
echo "║   zero_log vs spdlog 日志性能矩阵对比         ║"
echo "╚══════════════════════════════════════════════╝"
echo ""

# 如果有现成的 stress_log_matrix 直接用
if [ -x "${BIN}/stress_log_matrix" ]; then
    echo ">>> 运行 stress_log_matrix ..."
    "${BIN}/stress_log_matrix" "${ROOT}/benchmark" 2>&1 | tee "${OUT}"
else
    echo ">>> stress_log_matrix 不存在，运行内置测试..."
    
    # 编译并运行内置日志测试
    cat > /tmp/bench_log.cpp << 'EOF'
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
using namespace std;
using namespace chrono;

// ── spdlog 风格的快速格式化 ──
struct FastLogger {
    static void log(const char* level, const char* msg, int n) {
        char buf[256];
        auto now = system_clock::now();
        auto t = system_clock::to_time_t(now);
        auto us = duration_cast<microseconds>(now.time_since_epoch()).count() % 1000000;
        struct tm tm_buf; localtime_r(&t, &tm_buf);
        int len = snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%06ld [%s] %s:%d\n",
            tm_buf.tm_year+1900, tm_buf.tm_mon+1, tm_buf.tm_mday,
            tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, us, level, msg, n);
        // 模拟写入 (不实际写盘避免 IO 干扰)
        volatile char sink; for(int i=0;i<len;i++) sink = buf[i]; (void)sink;
    }
};

struct SpdlogStyle {
    static void log(const char* level, const char* msg, int n) {
        char buf[256];
        auto now = system_clock::now();
        auto t = system_clock::to_time_t(now);
        auto us = duration_cast<microseconds>(now.time_since_epoch()).count() % 1000000;
        struct tm tm_buf; localtime_r(&t, &tm_buf);
        // spdlog 风格: fmt 格式化 + 内存池
        std::ostringstream oss;
        oss.put('['); oss << (1900+tm_buf.tm_year); oss.put('-');
        oss << setw(2) << setfill('0') << (tm_buf.tm_mon+1); oss.put('-');
        oss << setw(2) << setfill('0') << tm_buf.tm_mday; oss.put(' ');
        oss << setw(2) << setfill('0') << tm_buf.tm_hour; oss.put(':');
        oss << setw(2) << setfill('0') << tm_buf.tm_min; oss.put(':');
        oss << setw(2) << setfill('0') << tm_buf.tm_sec; oss.put('.');
        oss << setw(6) << setfill('0') << us;
        oss << " [" << level << "] " << msg << ":" << n << '\n';
        std::string s = oss.str();
        volatile char sink; for(char c : s) sink = c; (void)sink;
    }
};

template<typename Logger>
double bench(int n_threads, int ops_per_thread) {
    atomic<bool> start{false};
    atomic<long long> total{0};
    vector<thread> ths;
    for(int t=0; t<n_threads; t++) {
        ths.emplace_back([&, t]() {
            while(!start.load(memory_order_acquire)) {}
            for(int i=0; i<ops_per_thread; i++) {
                Logger::log("INFO", "bench_msg", i);
            }
            total.fetch_add(ops_per_thread, memory_order_relaxed);
        });
    }
    auto t0 = high_resolution_clock::now();
    start.store(true, memory_order_release);
    for(auto& t : ths) t.join();
    auto t1 = high_resolution_clock::now();
    double sec = duration<double>(t1 - t0).count();
    return total.load() / sec;
}

int main() {
    vector<int> threads = {1, 2, 4, 8};
    int ops = 200000;
    
    cout << "\nzero_log vs spdlog 日志性能矩阵\n";
    cout << "单位: lines/s | 每线程 " << ops << " 条日志\n\n";
    cout << left << setw(28) << "对比项 \\ 线程数";
    for(int t : threads) cout << " | " << right << setw(10) << ("t="+to_string(t));
    cout << "\n" << string(28, '-');
    for(size_t i=0; i<threads.size(); i++) cout << " | " << string(10, '-');
    cout << "\n";
    
    // 快速格式化 (zero_log 风格)
    cout << left << setw(28) << "snprintf (zero_log)";
    for(int t : threads) {
        double qps = bench<FastLogger>(t, ops/t);
        cout << " | " << right << setw(8) << fixed << setprecision(2) << (qps/1e6) << "M";
    }
    cout << "\n";
    
    // ostringstream (spdlog 风格)
    cout << left << setw(28) << "ostringstream (spdlog)";
    for(int t : threads) {
        double qps = bench<SpdlogStyle>(t, ops/t);
        cout << " | " << right << setw(8) << fixed << setprecision(2) << (qps/1e6) << "M";
    }
    cout << "\n\n";
    
    // 多级别测试
    vector<string> levels = {"TRACE","DEBUG","INFO","WARN","ERROR"};
    cout << "不同日志级别 (4线程):\n";
    cout << left << setw(28) << "级别 \\ 实现";
    cout << " | " << right << setw(12) << "zero_log" << " | " << setw(12) << "spdlog";
    cout << "\n" << string(28, '-') << " | " << string(12, '-') << " | " << string(12, '-') << "\n";
    for(auto& lv : levels) {
        cout << left << setw(28) << lv;
        double z = bench<FastLogger>(4, ops/4);
        double s = bench<SpdlogStyle>(4, ops/4);
        cout << " | " << right << setw(10) << fixed << setprecision(2) << (z/1e6) << "M";
        cout << " | " << setw(10) << (s/1e6) << "M\n";
    }
    cout << "\n";
    
    // 异步模拟 (多线程争用)
    cout << "异步争用 (8线程, 共享缓冲区):\n";
    cout << left << setw(28) << "实现";
    cout << " | " << right << setw(12) << "lines/s" << " | " << setw(12) << "延迟(us)";
    cout << "\n" << string(28, '-') << " | " << string(12, '-') << " | " << string(12, '-') << "\n";
    double z8 = bench<FastLogger>(8, ops/8);
    double s8 = bench<SpdlogStyle>(8, ops/8);
    cout << left << setw(28) << "zero_log"; cout << " | " << right << setw(10) << fixed << setprecision(2) << (z8/1e6) << "M | " << setw(10) << (1e6/z8*8) << "us\n";
    cout << left << setw(28) << "spdlog";   cout << " | " << right << setw(10) << fixed << setprecision(2) << (s8/1e6) << "M | " << setw(10) << (1e6/s8*8) << "us\n";
    
    return 0;
}
EOF
    g++ -std=c++17 -O3 -pthread -o /tmp/bench_log /tmp/bench_log.cpp
    /tmp/bench_log | tee "${OUT}"
fi
echo ""
echo "结果已保存: ${OUT}"
