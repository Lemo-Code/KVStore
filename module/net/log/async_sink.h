#ifndef NET_LOG_ASYNC_SINK_H
#define NET_LOG_ASYNC_SINK_H

#include "config.h"
#include "log/file_writer.h"
#include "log/lockfree_mpsc_queue.h"
#include "singleton.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace net {

class AsyncLogManager;

namespace detail {

inline std::atomic<size_t>& PendingCount() {
  static std::atomic<size_t> n(0);
  return n;
}

inline bool AllowEnqueue() {
#if NET_LOG_DEGRADE_MODE == 1
  if (PendingCount().load(std::memory_order_relaxed) >=
      static_cast<size_t>(NET_LOG_ASYNC_SOFT_CAP)) {
    return false;
  }
#endif
  return true;
}

}  // namespace detail

enum class AsyncSinkType { STDOUT = 0, FILE = 1 };

/** 标准输出通道固定键 */
inline const char* StdoutDestination() { return "@stdout"; }

/**
 * @brief 单路 MPSC；队列元素仅为已格式化文本，目标路径由 channel 持有。
 */
class AsyncLogChannel {
 public:
  typedef std::shared_ptr<AsyncLogChannel> ptr;

  AsyncLogChannel(AsyncSinkType type, std::string destination);

  void enqueue(std::string payload);
  size_t drainTo(AsyncLogManager& manager);

  AsyncSinkType type() const { return type_; }
  const std::string& destination() const { return destination_; }

 private:
  friend class AsyncLogManager;

  AsyncSinkType type_;
  std::string destination_;
  std::string key_;
  LockFreeMpscQueue<std::string> queue_;
};

/** MPSC → 按路径 ByteBuffer 聚合 → 批量 write */
class AsyncLogManager {
 public:
  typedef std::mutex MutexType;

  AsyncLogManager();
  ~AsyncLogManager();

  void enqueue(AsyncSinkType type, const std::string& destination,
               std::string payload);

  void notify();
  void flushFile(const std::string& destination);
  void reopenFile(const std::string& destination);

 private:
  friend class AsyncLogChannel;

  AsyncLogChannel::ptr channelFor(AsyncSinkType type,
                                  const std::string& destination);
  void ingest(AsyncSinkType type, const std::string& destination,
              std::string&& payload);
  void flushAllBuffers();
  FileWriter& writerFor(const std::string& path);
  void run();

  std::map<std::string, AsyncLogChannel::ptr> channels_;
  MutexType ch_mtx_;

  std::unordered_map<std::string, std::unique_ptr<FileWriter>> writers_;
  StdoutWriter stdout_;
  MutexType sink_mtx_;

  std::atomic<bool> running_;
  std::condition_variable cv_;
  MutexType cv_mtx_;
  std::thread worker_;
  uint32_t flush_interval_ms_;
  size_t buf_capacity_;
  size_t flush_threshold_;
};

typedef Singleton<AsyncLogManager> AsyncLogMgr;

inline void AsyncEnqueueFile(const std::string& path, std::string line) {
  AsyncLogMgr::GetInstance()->enqueue(AsyncSinkType::FILE, path, std::move(line));
}

inline void AsyncEnqueueStdout(std::string line) {
  AsyncLogMgr::GetInstance()->enqueue(AsyncSinkType::STDOUT, StdoutDestination(),
                                      std::move(line));
}

}  // namespace net

#endif  // NET_LOG_ASYNC_SINK_H
