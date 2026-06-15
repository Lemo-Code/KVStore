#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <mutex>

#include "zero/log/ring_buffer.h"
#include "zero/base/noncopyable.h"

namespace zero {

// ============ LogEntry (预格式化日志条目) ============
struct LogEntry {
    uint64_t timestamp_ms;      // 毫秒时间戳
    uint32_t thread_id;
    uint32_t fiber_id;
    uint8_t  level;             // LogLevel::Level
    char     message[240];      // 预格式化内容 (含文件:行号)
    // 总计: 8+4+4+1+240 = 257 bytes, cache-line 友好

    void format(uint64_t ts, uint32_t tid, uint32_t fid, uint8_t lvl,
                const char* file, int line, const char* msg, size_t msg_len);
};

// ============ AsyncLogChannel (每线程独立环形缓冲) ============
class AsyncLogChannel : public Noncopyable {
public:
    using ptr = std::shared_ptr<AsyncLogChannel>;

    static constexpr size_t kRingSize = 1 << 20;  // 1M 条目

    AsyncLogChannel();
    ~AsyncLogChannel();

    // 生产者: 写入日志 (无锁)
    bool write(uint64_t ts, uint32_t tid, uint32_t fid, uint8_t lvl,
               const char* file, int line, const char* msg, size_t len);

    // 消费者: 批量读取 (由 writer thread 调用)
    size_t readBatch(LogEntry*& out, size_t max_count);
    void   commitRead(size_t count);

    bool empty() const { return buffer_.empty(); }

private:
    RingBuffer<LogEntry, kRingSize> buffer_;
};

// ============ AsyncLogWriter (后台写线程) ============
class AsyncLogAppender {
public:
    using ptr = std::shared_ptr<AsyncLogAppender>;
    virtual ~AsyncLogAppender() = default;
    virtual void write(const LogEntry& entry) = 0;
    virtual void flush() = 0;
};

// 文件输出 (支持轮转)
class AsyncFileAppender : public AsyncLogAppender {
public:
    using ptr = std::shared_ptr<AsyncFileAppender>;

    AsyncFileAppender(const std::string& filepath,
                      uint64_t max_size = 100 * 1024 * 1024,
                      uint32_t max_files = 10,
                      bool compress = false);
    ~AsyncFileAppender() override;

    void write(const LogEntry& entry) override;
    void flush() override;

private:
    void rotate();
    void openFile();

    std::string filepath_;
    uint64_t max_size_;
    uint32_t max_files_;
    bool compress_;
    int fd_ = -1;
    uint64_t current_size_ = 0;
    char write_buf_[64 * 1024];  // 64KB write buffer
    size_t buf_pos_ = 0;
};

// 标准输出
class AsyncStdoutAppender : public AsyncLogAppender {
public:
    using ptr = std::shared_ptr<AsyncStdoutAppender>;
    void write(const LogEntry& entry) override;
    void flush() override;
};

class AsyncLogWriter : public Noncopyable {
public:
    static AsyncLogWriter& GetInstance();

    void start();
    void stop();

    // 注册/注销 channel
    void registerChannel(AsyncLogChannel::ptr channel);
    void unregisterChannel(AsyncLogChannel* channel);

    // 日志宏中获取当前线程的 channel
    static AsyncLogChannel* GetCurrentChannel();

    // 设置输出目标
    void addAppender(AsyncLogAppender::ptr app);
    void clearAppenders();

private:
    AsyncLogWriter();
    ~AsyncLogWriter();

    void writerLoop();

    std::atomic<bool> running_{false};
    std::thread writer_thread_;

    std::mutex mutex_;
    std::vector<AsyncLogChannel::ptr> channels_;
    std::vector<AsyncLogAppender::ptr> appenders_;

    // thread-local: 每线程独立的 channel
    static thread_local AsyncLogChannel* t_channel;
};

// ============ 便捷宏 ============
#define ZERO_ASYNC_LOG(channel, level, fmt, ...) \
    do { \
        if (channel) { \
            char _log_buf[256]; \
            int _log_len = snprintf(_log_buf, sizeof(_log_buf), fmt, ##__VA_ARGS__); \
            if (_log_len > 0) { \
                channel->write(zero::GetCurrentMS(), zero::GetThreadId(), \
                    zero::GetFiberId(), (uint8_t)(level), \
                    __FILE__, __LINE__, _log_buf, (size_t)_log_len); \
            } \
        } \
    } while(0)

} // namespace zero
