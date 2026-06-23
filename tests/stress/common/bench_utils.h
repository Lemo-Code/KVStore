#pragma once

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <cstdlib>
#include <limits.h>
#include <unistd.h>

namespace stress {

inline int64_t nowUs() {
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

inline double elapsedSec(int64_t start_us) {
    return (nowUs() - start_us) / 1e6;
}

/** 吞吐格式化: 1104069 -> "1.10M", 738248 -> "738.25K", 512 -> "512" */
inline std::string formatUnit(double v) {
    char buf[32];
    if (v >= 1e9)
        snprintf(buf, sizeof(buf), "%.2fG", v / 1e9);
    else if (v >= 1e6)
        snprintf(buf, sizeof(buf), "%.2fM", v / 1e6);
    else if (v >= 1e3)
        snprintf(buf, sizeof(buf), "%.2fK", v / 1e3);
    else
        snprintf(buf, sizeof(buf), "%.0f", v);
    return buf;
}

/** 秒数固定小数，避免科学计数法 */
inline std::string formatSec(double sec) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.4f", sec);
    return buf;
}

struct ProcessMem {
    long rss_kb = 0;
    long vm_kb = 0;
};

inline ProcessMem readProcessMem(pid_t pid = getpid()) {
    ProcessMem m;
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE* f = fopen(path, "r");
    if (!f) return m;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0)
            sscanf(line + 6, "%ld", &m.rss_kb);
        else if (strncmp(line, "VmSize:", 7) == 0)
            sscanf(line + 7, "%ld", &m.vm_kb);
    }
    fclose(f);
    return m;
}

inline std::string readLoadAvg() {
    std::ifstream f("/proc/loadavg");
    std::string s;
    std::getline(f, s);
    return s;
}

inline void ensureDir(const std::string& dir) {
    if (dir.empty() || dir == ".") return;
    std::string path;
    size_t i = 0;
    if (!dir.empty() && dir[0] == '/') {
        path = "/";
        i = 1;
    }
    while (i <= dir.size()) {
        size_t j = dir.find('/', i);
        if (j == std::string::npos) j = dir.size();
        if (j > i) {
            path += dir.substr(i, j - i);
            mkdir(path.c_str(), 0755);
        }
        if (j >= dir.size()) break;
        path += '/';
        i = j + 1;
    }
}

/** 项目根目录: KVSTORE_ROOT > 可执行文件所在 bin/ 的上级 > 向上查找标记文件 */
inline std::string projectRoot() {
    const char* env = std::getenv("KVSTORE_ROOT");
    if (env && env[0]) return env;

    char exe[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (len > 0) {
        exe[len] = '\0';
        std::string path(exe);
        auto pos = path.find_last_of('/');
        if (pos != std::string::npos) {
            path = path.substr(0, pos); // .../bin
            pos = path.find_last_of('/');
            if (pos != std::string::npos) {
                std::string root = path.substr(0, pos);
                if (access((root + "/CMakeLists.txt").c_str(), F_OK) == 0 &&
                    access((root + "/src/zero").c_str(), F_OK) == 0)
                    return root;
            }
        }
    }

    char cwd_buf[PATH_MAX];
    if (!getcwd(cwd_buf, sizeof(cwd_buf))) return ".";
    std::string path(cwd_buf);
    while (!path.empty()) {
        if (access((path + "/shell/run_bench_all.sh").c_str(), F_OK) == 0)
            return path;
        if (access((path + "/CMakeLists.txt").c_str(), F_OK) == 0 &&
            access((path + "/src/zero").c_str(), F_OK) == 0)
            return path;
        if (path == "/") break;
        auto pos = path.find_last_of('/');
        path = (pos == 0) ? "/" : path.substr(0, pos);
    }
    return cwd_buf;
}

/** 默认压测输出: <项目根>/benchInfo/perf_compare */
inline std::string defaultBenchOutDir() {
    return projectRoot() + "/benchInfo/perf_compare";
}

/** 解析输出目录: 有参数用参数, 否则项目目录下 benchInfo/perf_compare */
inline std::string resolveBenchOutDir(int argc, char** argv, int arg_idx = 1) {
    if (argc > arg_idx && argv[arg_idx] && argv[arg_idx][0])
        return argv[arg_idx];
    return defaultBenchOutDir();
}

inline void appendCsvRow(const std::string& path, const std::string& header,
                         const std::string& row) {
    ensureDir(path.substr(0, path.find_last_of('/')));
    bool need_header = !std::ifstream(path).good();
    std::ofstream out(path, std::ios::app);
    if (need_header) out << header << "\n";
    out << row << "\n";
}

inline void writeText(const std::string& path, const std::string& content) {
    ensureDir(path.substr(0, path.find_last_of('/')));
    std::ofstream out(path);
    out << content;
}

struct BenchResult {
    std::string category;
    std::string name;
    int threads = 1;
    std::string mode;
    uint64_t ops = 0;
    double seconds = 0;
    double qps = 0;   // 内部原始值; CSV 输出 formatUnit(qps)
    long rss_kb = 0;
    std::string extra;

    void save(const std::string& csv_path) const {
        appendCsvRow(csv_path,
            "category,name,threads,mode,ops,seconds,throughput,rss_kb,extra",
            csvRowRps());
    }

    std::string csvRowRps() const {
        std::ostringstream ss;
        ss << category << "," << name << "," << threads << "," << mode << ","
           << ops << "," << formatSec(seconds) << "," << formatUnit(qps) << ","
           << rss_kb << "," << extra;
        return ss.str();
    }

    void saveLog(const std::string& csv_path, int msgs_per_thread,
                 double lines_per_sec_per_thread) const {
        appendCsvRow(csv_path,
            "category,name,threads,mode,msgs_per_thread,lines,seconds,"
            "total_throughput,throughput_per_thread,rss_kb,extra",
            csvRowLog(msgs_per_thread, lines_per_sec_per_thread));
    }

    std::string csvRowLog(int msgs_per_thread, double lines_per_sec_per_thread) const {
        std::ostringstream ss;
        ss << category << "," << name << "," << threads << "," << mode << ","
           << msgs_per_thread << "," << ops << "," << formatSec(seconds) << ","
           << formatUnit(qps) << "," << formatUnit(lines_per_sec_per_thread) << ","
           << rss_kb << "," << extra;
        return ss.str();
    }
};

} // namespace stress
