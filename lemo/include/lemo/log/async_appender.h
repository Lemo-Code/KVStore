#pragma once

#include "lemo/log/appender.h"
#include "lemo/thread/module.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace lemo {
namespace log {

// 后台刷盘调度器（单 worker，caller 侧 TLS 批量入队）
class Dispatcher {
 public:
  static const size_t kTlsBatchSize = 32;

  struct Task {
    Appender::ptr target;
    LogRecord record;
  };

  static Dispatcher& Instance();

  void Submit(Appender::ptr target, LogRecord record);
  void FlushCurrentThread();
  void WaitIdle(Appender::ptr target);
  void FlushAll();
  void SubmitBatch(std::vector<Task>& batch);

  Dispatcher(const Dispatcher&) = delete;
  Dispatcher& operator=(const Dispatcher&) = delete;

 private:
  Dispatcher();
  ~Dispatcher();

  void Run();

  std::mutex mutex_;
  std::condition_variable cv_;
  std::condition_variable idle_cv_;
  std::deque<Task> queue_;
  thread::Thread::ptr worker_;
  std::atomic<bool> running_;
  std::atomic<bool> worker_busy_;
};

// log4j AsyncAppender：装饰任意 Appender
class AsyncAppender : public Appender {
 public:
  typedef std::shared_ptr<AsyncAppender> ptr;

  explicit AsyncAppender(Appender::ptr delegate);

  void SetLayout(Layout::ptr layout) override;
  void Append(const LogRecord& record) override;
  void Flush() override;
  const char* Type() const override;

  Appender::ptr GetDelegate() const { return delegate_; }

 private:
  void SyncToDelegate();

  Appender::ptr delegate_;
};

Appender::ptr MakeAsync(Appender::ptr appender);

}  // namespace log
}  // namespace lemo
