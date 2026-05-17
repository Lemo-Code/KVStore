#ifndef NET_LOG_ASYNC_SINK_H
#define NET_LOG_ASYNC_SINK_H

#include "config.h"
#include "log/async_record.h"
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

inline std::atomic<size_t>& AsyncLogPendingCount() {
  static std::atomic<size_t> count(0);
  return count;
}

inline bool ShouldEnqueueAsyncLog() {
#if NET_LOG_DEGRADE_MODE == 1
  const size_t pending =
      AsyncLogPendingCount().load(std::memory_order_relaxed);
  if (pending >= static_cast<size_t>(NET_LOG_ASYNC_SOFT_CAP)) {
    return false;
  }
#endif
  return true;
}

inline void OnAsyncLogEnqueued() {
  AsyncLogPendingCount().fetch_add(1, std::memory_order_relaxed);
}

inline void OnAsyncLogDrained(size_t drained_count) {
  if (drained_count == 0) {
    return;
  }
  AsyncLogPendingCount().fetch_sub(drained_count, std::memory_order_relaxed);
}

}  // namespace detail

enum class AsyncSinkType {
  STDOUT = 0,
  FILE = 1,
};

/**
 * @brief 单路 MPSC 队列；元素为已格式化记录。
 *
 * flush 时由 AsyncLogManager 将 payload 追加到对应 FileWriter/StdoutWriter 的
 * 固定 ByteBuffer，达到阈值后批量 write，而非逐条写文件。
 */
class AsyncLogChannel {
 public:
  typedef std::shared_ptr<AsyncLogChannel> ptr;

  AsyncLogChannel(AsyncSinkType type, const std::string& destination);

  void enqueue(AsyncLogRecord&& record);

  /** 弹出全部记录交给 manager 聚合（仅后台线程） */
  size_t drainTo(AsyncLogManager& manager);

  std::string key() const { return key_; }
  AsyncSinkType type() const { return type_; }
  const std::string& destination() const { return destination_; }

 private:
  friend class AsyncLogManager;

  AsyncSinkType type_;
  std::string destination_;
  std::string key_;
  LockFreeMpscQueue<AsyncLogRecord> queue_;
};

/**
 * @brief 异步日志管理器：MPSC → ByteBuffer 聚合 → 批量 write。
 */
class AsyncLogManager {
 public:
  typedef std::mutex MutexType;

  AsyncLogManager();
  ~AsyncLogManager();

  AsyncLogChannel::ptr emplaceChannel(AsyncSinkType type,
                                      const std::string& destination);

  void notify();

  /** 轮转/truncate 前：排空该路径队列并刷缓冲 */
  void flushFile(const std::string& destination);

  /** rename 后：关闭旧 fd 并重新 open(O_APPEND) 新文件 */
  void reopenFile(const std::string& destination);

  /** 由 AsyncLogChannel::drainTo 调用（仅后台线程） */
  void ingestRecord(AsyncSinkType type, const std::string& destination,
                    std::string&& payload);

 private:
  friend class AsyncLogChannel;

  void run();
  void flushAllSinkBuffers();
  FileWriter& fileWriterFor(const std::string& path);

  std::map<std::string, AsyncLogChannel::ptr> channels_;
  MutexType ch_mtx_;

  std::unordered_map<std::string, std::unique_ptr<FileWriter>> file_writers_;
  StdoutWriter stdout_writer_;
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

inline void AsyncEnqueue(AsyncSinkType type, const std::string& destination,
                         std::string payload) {
  AsyncLogMgr::GetInstance()
      ->emplaceChannel(type, destination)
      ->enqueue(AsyncLogRecord(destination, std::move(payload)));
  AsyncLogMgr::GetInstance()->notify();
}

inline void AsyncEnqueueFile(const std::string& destination, std::string line) {
  AsyncEnqueue(AsyncSinkType::FILE, destination, std::move(line));
}

inline void AsyncEnqueueStdout(std::string line) {
  static const std::string kStdoutDest = "@stdout";
  AsyncEnqueue(AsyncSinkType::STDOUT, kStdoutDest, std::move(line));
}

}  // namespace net

#endif  // NET_LOG_ASYNC_SINK_H
