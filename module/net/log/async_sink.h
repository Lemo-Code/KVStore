#ifndef NET_LOG_ASYNC_SINK_H
#define NET_LOG_ASYNC_SINK_H

#include "log/config/build_config.h"
#include "log/async_record.h"
#include "log/file_writer.h"
#include "log/lockfree_mpsc_queue.h"
#include "log/config/log_config.h"
#include "common/singleton.h"
#include "thread/mutex.h"
#include "thread/thread.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace net {

namespace detail {

inline std::atomic<size_t>& PendingCount() {
  static std::atomic<size_t> n(0);
  return n;
}

inline bool AllowEnqueue() {
  return LogConfig::instance().allowAsyncEnqueue(
      detail::PendingCount().load(std::memory_order_relaxed));
}

}  // namespace detail

/**
 * @brief 全局 MPSC → 按路径 FileWriter 聚合 → 批量 write。
 *
 * 单队列承载全部异步记录，worker drain 后按 destination 分桶写入 ByteBuffer，
 * 避免按日切文件时 channel map 无限增长。
 */
class AsyncLogManager {
 public:
  typedef Spinlock MutexType;

  AsyncLogManager();
  ~AsyncLogManager();

  void enqueue(AsyncSinkType type, const std::string& destination,
               std::string payload);

  void notify();
  void flush();
  void flushFile(const std::string& destination);
  void reopenFile(const std::string& destination);

 private:
  void ingest(AsyncSinkType type, const std::string& destination,
              std::string&& payload);
  void drainGlobalQueue();
  void drainGlobalQueueForPath(const std::string& destination);
  void flushAllBuffers();
  FileWriter& writerFor(const std::string& path);
  void run();

  LockFreeMpscQueue<AsyncLogRecord> queue_;

  std::unordered_map<std::string, std::unique_ptr<FileWriter>> writers_;
  StdoutWriter stdout_;
  MutexType sink_mtx_;
  MutexType drain_mtx_;  ///< 串行化 MPSC 消费（仅允许单消费者）

  std::atomic<bool> running_;
  std::atomic<bool> wake_;
  Thread::ptr worker_;
  uint32_t flush_interval_ms_;
  size_t buf_capacity_;
  size_t flush_threshold_;
};

typedef Singleton<AsyncLogManager> AsyncLogMgr;

inline void AsyncEnqueueFile(const std::string& path, std::string line) {
  AsyncLogManager* mgr = AsyncLogMgr::GetInstance();
  mgr->enqueue(AsyncSinkType::FILE, path, std::move(line));
  mgr->notify();
}

inline void AsyncEnqueueStdout(std::string line) {
  AsyncLogManager* mgr = AsyncLogMgr::GetInstance();
  mgr->enqueue(AsyncSinkType::STDOUT, StdoutDestination(), std::move(line));
  mgr->notify();
}

}  // namespace net

#endif  // NET_LOG_ASYNC_SINK_H
