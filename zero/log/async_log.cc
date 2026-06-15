#include "zero/log/async_log.h"
#include "zero/log/log.h"
#include "zero/base/macro.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>

namespace zero {

// ====================================================================
// LogEntry
// ====================================================================
void LogEntry::format(uint64_t ts, uint32_t tid, uint32_t fid, uint8_t lvl,
                      const char* file, int line, const char* msg, size_t msg_len) {
    timestamp_ms = ts;
    thread_id = tid;
    fiber_id = fid;
    level = lvl;

    // 格式化: "2026-06-15 12:00:00.123 [INFO] [tid:fid] file:line] msg"
    time_t sec = ts / 1000;
    int ms = ts % 1000;
    struct tm tm_buf;
    localtime_r(&sec, &tm_buf);

    // 提取文件名 (去掉路径前缀)
    const char* basename = strrchr(file, '/');
    basename = basename ? basename + 1 : file;

    int off = snprintf(message, sizeof(message),
        "%04d-%02d-%02d %02d:%02d:%02d.%03d [%s] [%u:%u] [%s:%d] ",
        tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
        tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, ms,
        LogLevel::ToString(static_cast<LogLevel::Level>(lvl)),
        tid, fid, basename, line);

    size_t remain = sizeof(message) - off;
    size_t copy = std::min(msg_len, remain - 1);
    memcpy(message + off, msg, copy);
    message[off + copy] = '\0';
}

// ====================================================================
// AsyncLogChannel
// ====================================================================
thread_local AsyncLogChannel* AsyncLogWriter::t_channel = nullptr;

AsyncLogChannel::AsyncLogChannel() = default;
AsyncLogChannel::~AsyncLogChannel() = default;

bool AsyncLogChannel::write(uint64_t ts, uint32_t tid, uint32_t fid, uint8_t lvl,
                             const char* file, int line, const char* msg, size_t len) {
    size_t index;
    LogEntry* entry = buffer_.tryClaim(index);
    if (!entry) return false;  // ring full

    entry->format(ts, tid, fid, lvl, file, line, msg, len);
    buffer_.commit(index);
    return true;
}

size_t AsyncLogChannel::readBatch(LogEntry*& out, size_t max_count) {
    return buffer_.tryReadBatch(out, max_count);
}

void AsyncLogChannel::commitRead(size_t count) {
    buffer_.commitRead(count);
}

AsyncLogChannel* AsyncLogWriter::GetCurrentChannel() {
    return t_channel;
}

// ====================================================================
// AsyncFileAppender
// ====================================================================
AsyncFileAppender::AsyncFileAppender(const std::string& filepath,
                                       uint64_t max_size, uint32_t max_files,
                                       bool compress)
    : filepath_(filepath), max_size_(max_size),
      max_files_(max_files), compress_(compress) {
    openFile();
}

AsyncFileAppender::~AsyncFileAppender() {
    flush();
    if (fd_ >= 0) close(fd_);
}

void AsyncFileAppender::openFile() {
    if (fd_ >= 0) close(fd_);

    fd_ = open(filepath_.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (fd_ >= 0) {
        struct stat st;
        if (fstat(fd_, &st) == 0) {
            current_size_ = st.st_size;
        }
    }
    buf_pos_ = 0;
}

void AsyncFileAppender::rotate() {
    flush();
    if (fd_ >= 0) { close(fd_); fd_ = -1; }

    // 轮转: file.log → file.log.1, file.log.1 → file.log.2, ...
    for (uint32_t i = max_files_; i > 0; --i) {
        std::string old_name = filepath_ + "." + std::to_string(i - 1);
        std::string new_name = filepath_ + "." + std::to_string(i);
        rename(old_name.c_str(), new_name.c_str());
    }
    std::string backup = filepath_ + ".1";
    rename(filepath_.c_str(), backup.c_str());

    openFile();
}

void AsyncFileAppender::write(const LogEntry& entry) {
    std::string line(entry.message);
    line += '\n';

    size_t len = line.size();
    if (buf_pos_ + len > sizeof(write_buf_)) {
        flush();
    }
    if (len <= sizeof(write_buf_)) {
        memcpy(write_buf_ + buf_pos_, line.data(), len);
        buf_pos_ += len;
    } else {
        // 长行直接写
        ::write(fd_, line.data(), len);
    }

    current_size_ += len;

    // 检查是否需要轮转
    if (current_size_ >= max_size_) {
        rotate();
    }
}

void AsyncFileAppender::flush() {
    if (buf_pos_ > 0 && fd_ >= 0) {
        ::write(fd_, write_buf_, buf_pos_);
        buf_pos_ = 0;
    }
}

// ====================================================================
// AsyncStdoutAppender
// ====================================================================
void AsyncStdoutAppender::write(const LogEntry& entry) {
    // 直接写 stdout (带缓冲)
    fwrite(entry.message, 1, strlen(entry.message), stdout);
    fputc('\n', stdout);
}

void AsyncStdoutAppender::flush() {
    fflush(stdout);
}

// ====================================================================
// AsyncLogWriter
// ====================================================================
AsyncLogWriter& AsyncLogWriter::GetInstance() {
    static AsyncLogWriter instance;
    return instance;
}

AsyncLogWriter::AsyncLogWriter() = default;

AsyncLogWriter::~AsyncLogWriter() {
    stop();
}

void AsyncLogWriter::start() {
    if (running_.exchange(true)) return;
    writer_thread_ = std::thread(&AsyncLogWriter::writerLoop, this);
}

void AsyncLogWriter::stop() {
    running_.store(false);
    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }
    // 最终 flush
    for (auto& app : appenders_) app->flush();
}

void AsyncLogWriter::registerChannel(AsyncLogChannel::ptr channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    channels_.push_back(std::move(channel));
    // 设置为当前线程的 channel
    t_channel = channels_.back().get();
}

void AsyncLogWriter::unregisterChannel(AsyncLogChannel* channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(channels_.begin(), channels_.end(),
        [channel](const AsyncLogChannel::ptr& c) { return c.get() == channel; });
    if (it != channels_.end()) channels_.erase(it);
}

void AsyncLogWriter::addAppender(AsyncLogAppender::ptr app) {
    std::lock_guard<std::mutex> lock(mutex_);
    appenders_.push_back(std::move(app));
}

void AsyncLogWriter::clearAppenders() {
    std::lock_guard<std::mutex> lock(mutex_);
    appenders_.clear();
}

void AsyncLogWriter::writerLoop() {
    std::vector<AsyncLogChannel::ptr> local_channels;

    while (running_.load(std::memory_order_relaxed)) {
        // 获取当前注册的 channels 快照
        {
            std::lock_guard<std::mutex> lock(mutex_);
            local_channels = channels_;
        }

        bool any_work = false;
        for (auto& ch : local_channels) {
            LogEntry* entries;
            size_t count = ch->readBatch(entries, 256);
            if (count > 0) {
                any_work = true;
                // 批量写出
                for (auto& app : appenders_) {
                    for (size_t i = 0; i < count; ++i) {
                        app->write(entries[i]);
                    }
                }
                ch->commitRead(count);
            }
        }

        if (!any_work) {
            // 无日志时短暂休眠
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}

} // namespace zero
