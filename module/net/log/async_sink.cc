/**
 * @file async_sink.cc
 */
#include "log/async_sink.h"

#include <chrono>

namespace net {

namespace {

std::string ChannelKey(AsyncSinkType type, const std::string& dst) {
  return std::to_string(static_cast<int>(type)) + "|" + dst;
}

}  // namespace

AsyncLogChannel::AsyncLogChannel(AsyncSinkType type, std::string destination)
    : type_(type),
      destination_(std::move(destination)),
      key_(ChannelKey(type_, destination_)) {}

void AsyncLogChannel::enqueue(std::string payload) {
  if (!detail::AllowEnqueue() || payload.empty()) {
    return;
  }
  queue_.push(std::move(payload));
  detail::PendingCount().fetch_add(1, std::memory_order_relaxed);
}

size_t AsyncLogChannel::drainTo(AsyncLogManager& manager) {
  size_t n = 0;
  std::string line;
  while (queue_.try_pop(line)) {
    manager.ingest(type_, destination_, std::move(line));
    ++n;
  }
  if (n > 0) {
    detail::PendingCount().fetch_sub(n, std::memory_order_relaxed);
  }
  return n;
}

AsyncLogManager::AsyncLogManager()
    : stdout_(NET_LOG_ASYNC_BUF_BYTES, NET_LOG_ASYNC_FLUSH_THRESHOLD),
      running_(true),
      flush_interval_ms_(NET_LOG_ASYNC_FLUSH_MS),
      buf_capacity_(NET_LOG_ASYNC_BUF_BYTES),
      flush_threshold_(NET_LOG_ASYNC_FLUSH_THRESHOLD) {
  worker_ = std::thread(&AsyncLogManager::run, this);
}

AsyncLogManager::~AsyncLogManager() {
  running_.store(false, std::memory_order_release);
  notify();
  if (worker_.joinable()) {
    worker_.join();
  }

  std::vector<AsyncLogChannel::ptr> snapshot;
  {
    std::lock_guard<MutexType> lock(ch_mtx_);
    snapshot.reserve(channels_.size());
    for (auto& kv : channels_) {
      snapshot.push_back(kv.second);
    }
  }
  for (const auto& ch : snapshot) {
    ch->drainTo(*this);
  }
  flushAllBuffers();

  std::lock_guard<MutexType> lock(sink_mtx_);
  for (auto& kv : writers_) {
    kv.second->close();
  }
  writers_.clear();

  std::lock_guard<MutexType> ch_lock(ch_mtx_);
  channels_.clear();
}

AsyncLogChannel::ptr AsyncLogManager::channelFor(AsyncSinkType type,
                                               const std::string& destination) {
  const std::string key = ChannelKey(type, destination);
  std::lock_guard<MutexType> lock(ch_mtx_);
  const auto it = channels_.find(key);
  if (it != channels_.end()) {
    return it->second;
  }
  auto ch = std::make_shared<AsyncLogChannel>(type, destination);
  channels_.emplace(key, ch);
  return ch;
}

void AsyncLogManager::enqueue(AsyncSinkType type, const std::string& destination,
                              std::string payload) {
  channelFor(type, destination)->enqueue(std::move(payload));
  notify();
}

void AsyncLogManager::notify() {
  cv_.notify_one();
}

FileWriter& AsyncLogManager::writerFor(const std::string& path) {
  const auto it = writers_.find(path);
  if (it != writers_.end()) {
    return *it->second;
  }
  std::unique_ptr<FileWriter> w(new FileWriter(buf_capacity_, flush_threshold_));
  w->open(path);
  FileWriter& ref = *w;
  writers_.emplace(path, std::move(w));
  return ref;
}

void AsyncLogManager::ingest(AsyncSinkType type, const std::string& destination,
                             std::string&& payload) {
  if (payload.empty()) {
    return;
  }
  std::lock_guard<MutexType> lock(sink_mtx_);
  if (type == AsyncSinkType::STDOUT) {
    stdout_.append(payload);
    return;
  }
  writerFor(destination).append(payload);
}

void AsyncLogManager::flushAllBuffers() {
  std::lock_guard<MutexType> lock(sink_mtx_);
  for (auto& kv : writers_) {
    kv.second->flush_buffer();
  }
  stdout_.flush_buffer();
}

void AsyncLogManager::reopenFile(const std::string& destination) {
  std::lock_guard<MutexType> lock(sink_mtx_);
  const auto it = writers_.find(destination);
  if (it != writers_.end()) {
    it->second->close();
    it->second->open(destination);
  }
}

void AsyncLogManager::flushFile(const std::string& destination) {
  AsyncLogChannel::ptr ch;
  {
    std::lock_guard<MutexType> lock(ch_mtx_);
    const auto it = channels_.find(ChannelKey(AsyncSinkType::FILE, destination));
    if (it != channels_.end()) {
      ch = it->second;
    }
  }
  if (ch) {
    ch->drainTo(*this);
  }
  std::lock_guard<MutexType> lock(sink_mtx_);
  const auto wit = writers_.find(destination);
  if (wit != writers_.end()) {
    wit->second->flush_buffer();
  }
}

void AsyncLogManager::run() {
  while (running_.load(std::memory_order_acquire)) {
    {
      std::unique_lock<MutexType> lk(cv_mtx_);
      cv_.wait_for(lk, std::chrono::milliseconds(flush_interval_ms_));
    }

    std::vector<AsyncLogChannel::ptr> snapshot;
    {
      std::lock_guard<MutexType> lock(ch_mtx_);
      snapshot.reserve(channels_.size());
      for (auto& kv : channels_) {
        snapshot.push_back(kv.second);
      }
    }
    for (const auto& ch : snapshot) {
      ch->drainTo(*this);
    }
    flushAllBuffers();
  }
}

}  // namespace net
