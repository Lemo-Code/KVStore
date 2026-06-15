#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <lstl/container/vector.h>
#include <fcntl.h>
#include <unistd.h>

#include "ledis/protocol/resp_types.h"

namespace ledis {

// ============================================================
// AofWriter — AOF 持久化 (append-only file)
// ============================================================
//
// 双缓冲 + 后台 fsync 线程:
//   - 存储线程: append() → active_buf_ (纯 memcpy, ns 级)
//   - 持久化线程: 每 1s 或 buffer 满 → swap → write(2) → fsync(2)
//
// 三种 fsync 策略:
//   ALWAYS    每条命令立即 fsync (最安全, 最慢)
//   EVERYSEC  每秒 fsync 一次 (推荐, 最多丢 1 秒数据)
//   NO        操作系统控制刷盘 (最快, 可能丢数据)
//
class AofWriter {
public:
    enum FsyncMode { ALWAYS = 0, EVERYSEC = 1, NO = 2 };

    AofWriter(const std::string& path, FsyncMode mode = EVERYSEC)
        : path_(path), fsync_mode_(mode)
    {
        fd_ = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        active_buf_.reserve(1024 * 1024);  // 1MB 预分配
    }

    ~AofWriter() {
        stop();
        if (fd_ >= 0) { close(fd_); fd_ = -1; }
    }

    // ---- 生命周期 ----

    void start() {
        running_ = true;
        thread_ = std::thread(&AofWriter::run, this);
    }

    void stop() {
        running_ = false;
        cv_.notify_one();
        if (thread_.joinable()) thread_.join();
    }

    // ---- 存储线程调用 ----

    // 追加一条命令到 AOF 缓冲区 (纯 memcpy, ns 级)
    void append(const std::string& resp_cmd) {
        std::lock_guard<std::mutex> lock(mutex_);
        active_buf_.append(resp_cmd);
        if (fsync_mode_ == ALWAYS) {
            // 立即写盘
            if (fd_ >= 0) {
                ::write(fd_, active_buf_.data(), active_buf_.size());
                ::fsync(fd_);
            }
            active_buf_.clear();
        }
    }

    // 将 args 转为 RESP 并追加
    void appendArgs(const lstl::vector<std::string_view>& args) {
        append(argsToResp(args));
    }

    // ---- 查询 ----
    const std::string& path() const { return path_; }
    uint64_t totalWritten() const { return total_written_.load(); }

private:
    // 持久化线程主循环
    void run() {
        while (running_) {
            // 等待 1 秒或 buffer 满
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait_for(lock, std::chrono::seconds(1), [this] {
                    return !running_ || active_buf_.size() >= 512 * 1024; // 512KB threshold
                });
                if (!running_) break;

                // 交换缓冲区
                if (!active_buf_.empty()) {
                    flush_buf_.swap(active_buf_);
                } else {
                    continue;
                }
            }

            // 写盘 (不持有锁)
            if (fd_ >= 0 && !flush_buf_.empty()) {
                ssize_t n = ::write(fd_, flush_buf_.data(), flush_buf_.size());
                if (n > 0) total_written_ += static_cast<uint64_t>(n);
                if (fsync_mode_ == EVERYSEC) {
                    ::fsync(fd_);
                }
            }
            flush_buf_.clear();
        }

        // 停止前最后的 flush
        std::lock_guard<std::mutex> lock(mutex_);
        if (fd_ >= 0 && !active_buf_.empty()) {
            ::write(fd_, active_buf_.data(), active_buf_.size());
            if (fsync_mode_ != NO) ::fsync(fd_);
        }
    }

    // 将参数列表转换回 RESP 格式 (用于 AOF 记录)
    static std::string argsToResp(const lstl::vector<std::string_view>& args) {
        std::string out;
        // Array header
        out += '*';
        out += std::to_string(args.size());
        out += "\r\n";
        // Bulk strings
        for (auto& a : args) {
            out += '$';
            out += std::to_string(a.size());
            out += "\r\n";
            out.append(a.data(), a.size());
            out += "\r\n";
        }
        return out;
    }

    // ---- 成员 ----
    std::string path_;
    int fd_ = -1;
    FsyncMode fsync_mode_ = EVERYSEC;

    std::string active_buf_;       // 存储线程写入
    std::string flush_buf_;        // 持久化线程写盘
    std::mutex mutex_;
    std::condition_variable cv_;

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> total_written_{0};
};

} // namespace ledis
