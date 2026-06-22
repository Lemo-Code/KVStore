#pragma once

#include <atomic>
#include <string>
#include <string_view>
#include <thread>
#include <lstl/container/vector.h>
#include <fcntl.h>
#include <unistd.h>

#include "kv_ledis/protocol/resp_writer.h"

namespace ledis {

// ============================================================
// AofWriter — Append-Only File 写入器
// ============================================================
//
// 支持三种 fsync 模式:
//   - ALWAYS:   每条命令 fsync (最安全)
//   - EVERYSEC: 每秒 fsync (默认)
//   - NO:       由 OS 决定 (最快)
//
class AofWriter {
public:
    enum FsyncMode { ALWAYS = 0, EVERYSEC = 1, NO = 2 };

    AofWriter() = default;
    AofWriter(const std::string& path, FsyncMode mode)
        : path_(path), mode_(mode) {}
    ~AofWriter() { stop(); }

    const std::string& path() const { return path_; }
    int fd() const { return fd_; }
    void setPath(const std::string& p) { path_ = p; }
    void setMode(FsyncMode m) { mode_ = m; }

    bool start() {
        if (path_.empty()) return true;
        fd_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd_ < 0) return false;
        running_ = true;
        if (mode_ == EVERYSEC)
            fsync_thread_ = std::thread(&AofWriter::fsyncLoop, this);
        return true;
    }

    void stop() {
        running_ = false;
        if (fsync_thread_.joinable()) fsync_thread_.join();
        if (fd_ >= 0) { ::fsync(fd_); ::close(fd_); fd_ = -1; }
    }

    void appendArgs(const lstl::vector<std::string_view>& args) {
        if (fd_ < 0) return;
        std::string buf;
        RespWriter::writeArrayHeader(buf, static_cast<int64_t>(args.size()));
        for (auto& arg : args)
            RespWriter::writeBulkString(buf, arg);
        ::write(fd_, buf.data(), buf.size());
        if (mode_ == ALWAYS) ::fsync(fd_);
    }

private:
    void fsyncLoop() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (fd_ >= 0) ::fsync(fd_);
        }
    }

    std::string path_;
    FsyncMode mode_ = EVERYSEC;
    int fd_ = -1;
    std::atomic<bool> running_{false};
    std::thread fsync_thread_;
};

} // namespace ledis
