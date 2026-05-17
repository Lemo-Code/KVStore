/**
 * @file async_sink.cc
 * @brief MPSC 入队 + ByteBuffer 聚合 + 批量 write。
 */
#include "log/async_sink.h"

#include <chrono>

namespace net {

namespace {

std::string MakeChannelKey(AsyncSinkType type, const std::string& dst) {
  return std::to_string(static_cast<int>(type)) + "|" + dst;
}

}  // namespace

AsyncLogChannel::AsyncLogChannel(AsyncSinkType type,
                                 const std::string& destination)
    : type_(type),
      destination_(destination),
      key_(MakeChannelKey(type, destination)) {}

void AsyncLogChannel::enqueue(AsyncLogRecord&& record) {
  if (!detail::ShouldEnqueueAsyncLog()) {
    return;
  }
  queue_.push(std::move(record));
  detail::OnAsyncLogEnqueued();
}

size_t AsyncLogChannel::drainTo(AsyncLogManager& manager) {
  size_t count = 0;
  while (true) {
    AsyncLogRecord item;
    if (!queue_.try_pop(item)) {
      break;
    }
    manager.ingestRecord(type_, destination_, std::move(item.payload));
    ++count;
  }
  if (count > 0) {
    detail::OnAsyncLogDrained(count);
  }
  return count;
}

AsyncLogManager::AsyncLogManager()
    : stdout_writer_(NET_LOG_ASYNC_BUF_BYTES, NET_LOG_ASYNC_FLUSH_THRESHOLD),
      sink_mtx_(),
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
  flushAllSinkBuffers();

  {
    std::lock_guard<MutexType> lock(sink_mtx_);
    for (auto& kv : file_writers_) {
      kv.second->close();
    }
    file_writers_.clear();
  }

  std::lock_guard<MutexType> lock(ch_mtx_);
  channels_.clear();
}

AsyncLogChannel::ptr AsyncLogManager::emplaceChannel(
    AsyncSinkType type, const std::string& destination) {
  const std::string k = MakeChannelKey(type, destination);
  std::lock_guard<MutexType> lock(ch_mtx_);
  const auto it = channels_.find(k);
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

FileWriter& AsyncLogManager::fileWriterFor(const std::string& path) {
  const auto it = file_writers_.find(path);
  if (it != file_writers_.end()) {
    return *it->second;
  }
  std::unique_ptr<FileWriter> w(
      new FileWriter(buf_capacity_, flush_threshold_));
  w->open(path);
  FileWriter& ref = *w;
  file_writers_.emplace(path, std::move(w));
  return ref;
}

void AsyncLogManager::ingestRecord(AsyncSinkType type,
                                   const std::string& destination,
                                   std::string&& payload) {
  if (payload.empty()) {
    return;
  }
  std::lock_guard<MutexType> lock(sink_mtx_);
  if (type == AsyncSinkType::STDOUT) {
    stdout_writer_.append(payload);
    return;
  }
  FileWriter& writer = fileWriterFor(destination);
  if (writer.path() != destination) {
    writer.open(destination);
  }
  writer.append(payload);
}

void AsyncLogManager::flushAllSinkBuffers() {
  std::lock_guard<MutexType> lock(sink_mtx_);
  for (auto& kv : file_writers_) {
    kv.second->flush_buffer();
  }
  stdout_writer_.flush_buffer();
}

void AsyncLogManager::reopenFile(const std::string& destination) {
  std::lock_guard<MutexType> lock(sink_mtx_);
  const auto it = file_writers_.find(destination);
  if (it != file_writers_.end()) {
    it->second->close();
    it->second->open(destination);
  }
}

void AsyncLogManager::flushFile(const std::string& destination) {
  AsyncLogChannel::ptr ch;
  {
    std::lock_guard<MutexType> lock(ch_mtx_);
    const auto it =
        channels_.find(MakeChannelKey(AsyncSinkType::FILE, destination));
    if (it == channels_.end()) {
      std::lock_guard<MutexType> sink_lock(sink_mtx_);
      const auto wit = file_writers_.find(destination);
      if (wit != file_writers_.end()) {
        wit->second->flush_buffer();
      }
      return;
    }
    ch = it->second;
  }
  ch->drainTo(*this);
  std::lock_guard<MutexType> sink_lock(sink_mtx_);
  const auto wit = file_writers_.find(destination);
  if (wit != file_writers_.end()) {
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
    flushAllSinkBuffers();
  }
}

}  // namespace net
