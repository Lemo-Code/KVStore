#include "sylar/asyc_log.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <utility>

namespace sylar {

static inline std::string make_key(AsyncSinkType type, const std::string& dst) {
    return std::to_string(static_cast<int>(type)) + "|" + dst;
}

AsyncLogChannel::AsyncLogChannel(AsyncSinkType type, const std::string& destination, size_t initialBufferSize)
    : type_(type)
    , destination_(destination)
    , key_(make_key(type, destination))
    , writeBuf_(initialBufferSize)
    , drainBuf_(initialBufferSize) {
}

void AsyncLogChannel::enqueue(const std::string& message) {
    std::lock_guard<std::mutex> lock(mtx_);
    const uint32_t msgLen = static_cast<uint32_t>(message.size());
    writeBuf_.ensureWritableBytes(sizeof(uint32_t) + msgLen);
    writeBuf_.append(&msgLen, sizeof(uint32_t));
    writeBuf_.append(message.data(), msgLen);
    cond_.notify_one();
}

bool AsyncLogChannel::hasData() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return writeBuf_.readableBytes() > 0;
}

void AsyncLogChannel::notifyWriters() {
    cond_.notify_all();
}

std::string AsyncLogChannel::drainBuffer(Ringbuffer& buf) {
    if (buf.readableBytes() == 0) {
        return std::string();
    }
    std::string out;
    out.reserve(buf.readableBytes());
    out.assign(buf.peek(), buf.readableBytes());
    buf.retrieveAll();
    return out;
}

bool AsyncLogChannel::ensureFileOpen() {
    if (type_ != AsyncSinkType::FILE) {
        return false;
    }
    if (fileStream_ && fileStream_->is_open()) {
        return true;
    }
    if (!fileStream_) {
        fileStream_.reset(new std::ofstream(
            destination_, std::ios::app | std::ios::out | std::ios::binary));
    } else {
        fileStream_->open(destination_, std::ios::app | std::ios::out | std::ios::binary);
    }
    if (!fileStream_ || !*fileStream_) {
        std::cerr << "async log open file failed: " << destination_ << std::endl;
        return false;
    }
    return true;
}

void AsyncLogChannel::closeFile() {
    if (fileStream_ && fileStream_->is_open()) {
        fileStream_->close();
    }
}

void AsyncLogChannel::flush() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (writeBuf_.readableBytes() == 0) {
            return;
        }
        writeBuf_.swap(drainBuf_);
    }
    std::string raw = drainBuffer(drainBuf_);
    if (raw.empty()) {
        return;
    }
    writeParsedToSink(raw.data(), raw.size());
}

void AsyncLogChannel::writeParsedToSink(const char* p, size_t remain) {
    if (type_ == AsyncSinkType::STDOUT) {
        while (remain >= sizeof(uint32_t)) {
            uint32_t len = 0;
            std::memcpy(&len, p, sizeof(uint32_t));
            p += sizeof(uint32_t);
            remain -= sizeof(uint32_t);
            if (remain < len) break;
            std::cout.write(p, len);
            p += len;
            remain -= len;
        }
        std::cout.flush();
        return;
    }
    if (type_ == AsyncSinkType::FILE) {
        if (!ensureFileOpen()) {
            return;
        }
        while (remain >= sizeof(uint32_t)) {
            uint32_t len = 0;
            std::memcpy(&len, p, sizeof(uint32_t));
            p += sizeof(uint32_t);
            remain -= sizeof(uint32_t);
            if (remain < len) break;
            fileStream_->write(p, len);
            p += len;
            remain -= len;
        }
        fileStream_->flush();
        if (!*fileStream_) {
            closeFile();
        }
        return;
    }
    if (type_ == AsyncSinkType::STDERR) {
        while (remain >= sizeof(uint32_t)) {
            uint32_t len = 0;
            std::memcpy(&len, p, sizeof(uint32_t));
            p += sizeof(uint32_t);
            remain -= sizeof(uint32_t);
            if (remain < len) break;
            std::cerr.write(p, len);
            p += len;
            remain -= len;
        }
        std::cerr.flush();
    }
}

AsyncLogManager::AsyncLogManager() {
    running_.store(true, std::memory_order_release);
    worker_ = std::thread(&AsyncLogManager::run, this);
    // 与老实现一致，detach 可避免进程退出阻塞；析构里仍做 joinable 判断
    worker_.detach();
}

AsyncLogManager::~AsyncLogManager() {
    running_.store(false, std::memory_order_release);
    notify();
    if (worker_.joinable()) {
        worker_.join();
    }
    // 结束前尽量刷新
    {
        std::lock_guard<MutexType> lock(chMtx_);
        for (auto& kv : channels_) {
            kv.second->flush();
        }
        channels_.clear();
    }
}

AsyncLogChannel::ptr AsyncLogManager::emplaceChannel(AsyncSinkType type, const std::string& destination) {
    const std::string k = make_key(type, destination);
    std::lock_guard<MutexType> lock(chMtx_);
    auto it = channels_.find(k);
    if (it != channels_.end()) {
        return it->second;
    }
    auto ch = std::make_shared<AsyncLogChannel>(type, destination);
    channels_.emplace(k, ch);
    return ch;
}

void AsyncLogManager::notify() {
    cv_.notify_one();
}

void AsyncLogManager::run() {
    while (running_.load(std::memory_order_acquire)) {
        {
            std::unique_lock<MutexType> lk(cvMtx_);
            cv_.wait_for(lk, std::chrono::milliseconds(flushIntervalMs_));
        }
        std::vector<AsyncLogChannel::ptr> snapshot;
        {
            std::lock_guard<MutexType> lock(chMtx_);
            snapshot.reserve(channels_.size());
            for (auto& kv : channels_) {
                snapshot.push_back(kv.second);
            }
        }
        for (auto& ch : snapshot) {
            ch->flush();
        }
    }
}

} // namespace sylar


