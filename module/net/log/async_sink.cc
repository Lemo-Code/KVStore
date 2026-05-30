/**
 * @file async_sink.cc
 */
#include "log/async_sink.h"

#include <chrono>
#include <functional>
#include <thread>

namespace net {

AsyncLogManager::AsyncLogManager()
    : stdout_(LogConfig::instance().bufBytes(),
              LogConfig::instance().flushThreshold()),
      running_(true),
      wake_(false),
      flush_interval_ms_(LogConfig::instance().flushIntervalMs()),
      buf_capacity_(LogConfig::instance().bufBytes()),
      flush_threshold_(LogConfig::instance().flushThreshold()) {
  worker_.reset(new Thread(std::bind(&AsyncLogManager::run, this),
                           NET_LOG_ASYNC_WORKER_NAME));
}

AsyncLogManager::~AsyncLogManager() {
  running_.store(false, std::memory_order_release);
  notify();
  if (worker_) {
    worker_->join();
    worker_.reset();
  }
  drainGlobalQueue();
  flushAllBuffers();

  MutexType::Lock lock(sink_mtx_);
  for (auto& kv : writers_) {
    kv.second->close();
  }
  writers_.clear();
}

void AsyncLogManager::enqueue(AsyncSinkType type, const std::string& destination,
                              std::string payload) {
  if (!detail::AllowEnqueue() || payload.empty()) {
    return;
  }
  queue_.push(AsyncLogRecord(type, destination, std::move(payload)));
  detail::PendingCount().fetch_add(1, std::memory_order_relaxed);
  LogConfig::instance().recordEnqueueAccepted();
}

void AsyncLogManager::notify() {
  wake_.store(true, std::memory_order_release);
}

void AsyncLogManager::ingest(AsyncSinkType type, const std::string& destination,
                             std::string&& payload) {
  if (payload.empty()) {
    return;
  }
  MutexType::Lock lock(sink_mtx_);
  if (type == AsyncSinkType::STDOUT) {
    stdout_.append(payload);
  } else {
    writerFor(destination).append(payload);
  }
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

void AsyncLogManager::drainGlobalQueue() {
  MutexType::Lock lock(drain_mtx_);
  AsyncLogRecord rec;
  while (queue_.try_pop(rec)) {
    ingest(rec.type, rec.destination, std::move(rec.payload));
    detail::PendingCount().fetch_sub(1, std::memory_order_relaxed);
  }
}

void AsyncLogManager::drainGlobalQueueForPath(const std::string& destination) {
  MutexType::Lock lock(drain_mtx_);
  std::vector<AsyncLogRecord> deferred;
  deferred.reserve(64);

  AsyncLogRecord rec;
  while (queue_.try_pop(rec)) {
    detail::PendingCount().fetch_sub(1, std::memory_order_relaxed);
    if (rec.type == AsyncSinkType::FILE && rec.destination == destination) {
      ingest(rec.type, rec.destination, std::move(rec.payload));
    } else {
      deferred.push_back(std::move(rec));
      detail::PendingCount().fetch_add(1, std::memory_order_relaxed);
    }
  }

  for (auto& item : deferred) {
    queue_.push(std::move(item));
  }
}

void AsyncLogManager::flushAllBuffers() {
  MutexType::Lock lock(sink_mtx_);
  for (auto& kv : writers_) {
    kv.second->flush_buffer();
  }
  stdout_.flush_buffer();
}

void AsyncLogManager::flush() {
  drainGlobalQueue();
  flushAllBuffers();
}

void AsyncLogManager::reopenFile(const std::string& destination) {
  MutexType::Lock lock(sink_mtx_);
  const auto it = writers_.find(destination);
  if (it != writers_.end()) {
    it->second->close();
    it->second->open(destination);
  }
}

void AsyncLogManager::flushFile(const std::string& destination) {
  drainGlobalQueueForPath(destination);
  MutexType::Lock lock(sink_mtx_);
  const auto it = writers_.find(destination);
  if (it != writers_.end()) {
    it->second->flush_buffer();
  }
}

void AsyncLogManager::run() {
  using Clock = std::chrono::steady_clock;
  while (running_.load(std::memory_order_acquire)) {
    flush_interval_ms_ = LogConfig::instance().flushIntervalMs();
    const auto deadline =
        Clock::now() + std::chrono::milliseconds(flush_interval_ms_);
    while (running_.load(std::memory_order_acquire)) {
      if (wake_.exchange(false, std::memory_order_acq_rel)) {
        break;
      }
      if (Clock::now() >= deadline) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    drainGlobalQueue();
    flushAllBuffers();
  }
}

}  // namespace net
