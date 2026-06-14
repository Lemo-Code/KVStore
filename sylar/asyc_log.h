#ifndef __SYLAR_ASYC_LOG_H__
#define __SYLAR_ASYC_LOG_H__

#include "sylar/ringbuffer.h"
#include "sylar/singleton.h"

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <map>
#include <ostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace sylar {

// 目标类型：与 Buffer::Type 对齐，避免引入 buffer 依赖
enum class AsyncSinkType {
    STDOUT = 0,
    FILE = 1,
    REMOTE = 2,
    STDERR = 3
};

// 单个输出通道：双缓冲 + 输出目的地，生产者写 writeBuf_，flush 时 swap 后刷 drainBuf_
class AsyncLogChannel : public std::enable_shared_from_this<AsyncLogChannel> {
public:
    typedef std::shared_ptr<AsyncLogChannel> ptr;

    AsyncLogChannel(AsyncSinkType type, const std::string& destination, size_t initialBufferSize = 1 << 20);

    // 生产者写入（线程安全），仅持锁做 append，不参与拷贝输出
    void enqueue(const std::string& message);

    // 消费者刷盘（由管理器线程调用）：swap 后无锁拷贝与 I/O
    void flush();

    // 是否有可读数据（内部用）
    bool hasData() const;

    void notifyWriters();

    std::string key() const { return key_; }

private:
    // 将指定 buffer 内容取出（调用方在 swap 后对 drainBuf_ 调用，无需持锁）
    static std::string drainBuffer(Ringbuffer& buf);

private:
    bool ensureFileOpen();
    void closeFile();
    void writeParsedToSink(const char* p, size_t remain);

private:
    AsyncSinkType type_;
    std::string destination_;
    std::string key_;

    mutable std::mutex mtx_;
    std::condition_variable cond_;
    // 双缓冲：生产者只写 writeBuf_，flush 时 swap 后刷 drainBuf_，持锁时间极短
    Ringbuffer writeBuf_;
    Ringbuffer drainBuf_;

    std::unique_ptr<std::ofstream> fileStream_;
};

// 管理所有通道的单线程消费者
class AsyncLogManager {
public:
    typedef std::mutex MutexType;

    AsyncLogManager();
    ~AsyncLogManager();

    // 获取或创建通道
    AsyncLogChannel::ptr emplaceChannel(AsyncSinkType type, const std::string& destination);

    // 唤醒消费者线程
    void notify();

    // 设置异步刷盘间隔（毫秒），供配置整合使用
    void setFlushIntervalMs(uint32_t ms) { flushIntervalMs_ = ms; }
    uint32_t getFlushIntervalMs() const { return flushIntervalMs_; }

private:
    void run();

private:
    std::map<std::string, AsyncLogChannel::ptr> channels_;
    MutexType chMtx_;

    std::atomic<bool> running_{false};
    std::condition_variable cv_;
    MutexType cvMtx_;
    std::thread worker_;
    // 刷新间隔（毫秒）
    uint32_t flushIntervalMs_{800};
};

typedef Singleton<AsyncLogManager> AsyncLogMgr;

} // namespace sylar

#endif


