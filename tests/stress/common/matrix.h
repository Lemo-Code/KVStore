/**
 * matrix.h — 并发矩阵压测框架
 *
 * 横轴: 线程数 (默认 1 → 2×CPU 核心, 按 2 的幂递增)
 * 纵轴: 各测试自定义对比项
 * 输出: 终端矩阵 + 规范 .log 文件 (单位 K/M/G)
 */

#pragma once

#include "bench_utils.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace stress {

// ============================================================
// CPU / 线程轴
// ============================================================
inline int coreCount() {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0) ? static_cast<int>(n) : 4;
}

/** 1, 2, 4, … 直到 2×cores (含 2×cores) */
inline std::vector<int> threadAxis1To2Core() {
    int max_t = coreCount() * 2;
    std::vector<int> axis;
    for (int t = 1; t <= max_t; t *= 2)
        axis.push_back(t);
    if (axis.empty() || axis.back() != max_t)
        axis.push_back(max_t);
    std::sort(axis.begin(), axis.end());
    axis.erase(std::unique(axis.begin(), axis.end()), axis.end());
    return axis;
}

/** cores, … 直到 2×cores (Ledis/Redis 高并发对比) */
inline std::vector<int> threadAxisCoreTo2Core() {
    int cores = coreCount();
    int max_t = cores * 2;
    std::set<int> s;
    s.insert(cores);
    for (int t = 1; t <= max_t; t *= 2) {
        if (t >= cores && t <= max_t)
            s.insert(t);
    }
    s.insert(max_t);
    return std::vector<int>(s.begin(), s.end());
}

inline std::string threadAxisStr(const std::vector<int>& axis) {
    std::ostringstream ss;
    for (size_t i = 0; i < axis.size(); ++i) {
        if (i) ss << ", ";
        ss << axis[i];
    }
    return ss.str();
}

inline std::string nowTimestamp() {
    char buf[32];
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

// ============================================================
// 格子 / 行
// ============================================================
struct MatrixCell {
    uint64_t ops        = 0;
    double   seconds    = 0;
    double   throughput = 0;
    long     rss_kb     = 0;
};

using BenchFunc = std::function<void(int threads, uint64_t& total_ops, double& elapsed_sec)>;

class MatrixRunner {
public:
    MatrixRunner(const std::string& title, const std::string& unit,
                 const std::vector<int>& thread_axis = threadAxis1To2Core())
        : title_(title), unit_(unit), thread_axis_(thread_axis) {}

    /** 兼容旧调用: 仅传 title */
    explicit MatrixRunner(const std::string& title)
        : MatrixRunner(title, "ops/s (QPS)") {}

    void addRow(const std::string& label, BenchFunc fn) {
        rows_.push_back({label, fn, {}});
    }

    void run() {
        for (auto& row : rows_) {
            row.cells.clear();
            for (int th : thread_axis_) {
                uint64_t ops = 0;
                double sec = 0;
                row.fn(th, ops, sec);
                MatrixCell cell;
                cell.ops        = ops;
                cell.seconds    = sec;
                cell.throughput = (sec > 0) ? static_cast<double>(ops) / sec : 0;
                cell.rss_kb     = readProcessMem().rss_kb;
                row.cells[th]   = cell;
            }
        }
    }

    void printMatrix() const { printMatrix(stdout); }

    void printMatrix(FILE* out) const {
        fprintf(out, "\n");
        fprintf(out, "================================================================\n");
        fprintf(out, "[%s] %s\n", nowTimestamp().c_str(), title_.c_str());
        fprintf(out, "单位: %s | CPU 核心: %d | 线程轴: %s\n",
                unit_.c_str(), coreCount(), threadAxisStr(thread_axis_).c_str());
        fprintf(out, "----------------------------------------------------------------\n");

        fprintf(out, "%-24s |", "对比项 \\ 线程数");
        for (int th : thread_axis_)
            fprintf(out, " %10s |", (std::string("t=") + std::to_string(th)).c_str());
        fprintf(out, "\n");

        fprintf(out, "%-24s |", "------------------------");
        for (size_t i = 0; i < thread_axis_.size(); ++i)
            fprintf(out, " %10s |", "----------");
        fprintf(out, "\n");

        for (auto& row : rows_) {
            fprintf(out, "%-24s |", row.label.c_str());
            for (int th : thread_axis_) {
                auto it = row.cells.find(th);
                if (it != row.cells.end())
                    fprintf(out, " %10s |", formatUnit(it->second.throughput).c_str());
                else
                    fprintf(out, " %10s |", "-");
            }
            fprintf(out, "\n");
        }

        fprintf(out, "================================================================\n");

        fprintf(out, "\n可扩展性 (相对 t=%d, 倍数):\n", thread_axis_.front());
        fprintf(out, "%-24s |", "");
        for (int th : thread_axis_)
            fprintf(out, " %10s |", (std::string("t=") + std::to_string(th)).c_str());
        fprintf(out, "\n");

        for (auto& row : rows_) {
            fprintf(out, "%-24s |", row.label.c_str());
            auto base_it = row.cells.find(thread_axis_.front());
            double base = (base_it != row.cells.end() && base_it->second.throughput > 0)
                ? base_it->second.throughput : 1.0;
            for (int th : thread_axis_) {
                auto it = row.cells.find(th);
                if (it != row.cells.end())
                    fprintf(out, " %9.2fx |", it->second.throughput / base);
                else
                    fprintf(out, " %10s |", "-");
            }
            fprintf(out, "\n");
        }
        fprintf(out, "\n");
    }

    void saveCsv(const std::string& path) const {
        ensureDir(path.substr(0, path.find_last_of('/')));
        bool need_header = !std::ifstream(path).good();
        std::ofstream out(path, std::ios::app);
        if (need_header)
            out << "benchmark,row,threads,ops,seconds,throughput,unit,rss_kb\n";
        for (auto& row : rows_) {
            for (int th : thread_axis_) {
                auto it = row.cells.find(th);
                if (it == row.cells.end()) continue;
                auto& c = it->second;
                out << title_ << "," << row.label << "," << th << ","
                    << c.ops << "," << formatSec(c.seconds) << ","
                    << formatUnit(c.throughput) << "," << unit_ << ","
                    << c.rss_kb << "\n";
            }
        }
    }

    void saveLog(const std::string& path) const {
        ensureDir(path.substr(0, path.find_last_of('/')));
        FILE* f = fopen(path.c_str(), "w");
        if (!f) return;
        printMatrix(f);
        fprintf(f, "明细 (raw):\n");
        for (auto& row : rows_) {
            for (int th : thread_axis_) {
                auto it = row.cells.find(th);
                if (it == row.cells.end()) continue;
                auto& c = it->second;
                fprintf(f, "  [%s] threads=%d ops=%lu sec=%s qps=%s (%s) rss=%ldKB\n",
                        row.label.c_str(), th,
                        (unsigned long)c.ops,
                        formatSec(c.seconds).c_str(),
                        formatUnit(c.throughput).c_str(),
                        unit_.c_str(), c.rss_kb);
            }
        }
        fclose(f);
    }

    void saveMd(const std::string& path) const {
        std::ostringstream md;
        md << "# " << title_ << "\n\n";
        md << "单位: " << unit_ << " | CPU: " << coreCount()
           << " | 线程轴: " << threadAxisStr(thread_axis_) << "\n\n";
        md << "| 对比项 |";
        for (int th : thread_axis_) md << " t=" << th << " |";
        md << "\n|---|";
        for (size_t i = 0; i < thread_axis_.size(); ++i) md << "---:|";
        md << "\n";
        for (auto& row : rows_) {
            md << "| " << row.label << " |";
            for (int th : thread_axis_) {
                auto it = row.cells.find(th);
                if (it != row.cells.end())
                    md << " " << formatUnit(it->second.throughput) << " |";
                else
                    md << " - |";
            }
            md << "\n";
        }
        writeText(path, md.str());
    }

private:
    struct Row {
        std::string label;
        BenchFunc fn;
        std::map<int, MatrixCell> cells;
    };

    std::string title_;
    std::string unit_;
    std::vector<int> thread_axis_;
    std::vector<Row> rows_;
};

template<typename WorkerFn>
inline void runMultiThread(int threads, WorkerFn worker,
                           uint64_t& total_ops, double& elapsed_sec) {
    std::atomic<uint64_t> ops{0};
    std::atomic<bool> go{false};
    std::vector<std::thread> thrs;
    thrs.reserve(threads);

    for (int t = 0; t < threads; ++t) {
        thrs.emplace_back([&, t]() {
            while (!go.load(std::memory_order_acquire)) {}
            worker(t, ops);
        });
    }

    auto t0 = nowUs();
    go.store(true, std::memory_order_release);
    for (auto& t : thrs) t.join();
    elapsed_sec = elapsedSec(t0);
    total_ops = ops.load();
}

} // namespace stress
