#include "lemo/log/async_appender.h"

#include "lemo/log/log_runtime.h"

namespace lemo {
namespace log {
namespace {

void FlushUniqueTargets(const std::deque<Dispatcher::Task>& batch) {
  std::vector<Appender::ptr> targets;
  for (size_t i = 0; i < batch.size(); ++i) {
    if (!batch[i].target) continue;
    bool seen = false;
    for (size_t j = 0; j < targets.size(); ++j) {
      if (targets[j].get() == batch[i].target.get()) {
        seen = true;
        break;
      }
    }
    if (!seen) targets.push_back(batch[i].target);
  }
  for (size_t j = 0; j < targets.size(); ++j) {
    targets[j]->Flush();
  }
}

class TlsSubmitBuffer {
 public:
  ~TlsSubmitBuffer() { Flush(); }

  void Push(Dispatcher* dispatcher, Appender::ptr target, LogRecord record) {
    tasks_.push_back(Dispatcher::Task{target, std::move(record)});
    if (tasks_.size() >= Dispatcher::kTlsBatchSize) {
      Flush(dispatcher);
    }
  }

  void Flush(Dispatcher* dispatcher = NULL) {
    if (tasks_.empty()) return;
    if (!dispatcher) dispatcher = &Dispatcher::Instance();
    dispatcher->SubmitBatch(tasks_);
    tasks_.clear();
    tasks_.reserve(Dispatcher::kTlsBatchSize);
  }

 private:
  std::vector<Dispatcher::Task> tasks_;
};

TlsSubmitBuffer& GetTlsBuffer() {
  thread_local TlsSubmitBuffer buffer;
  return buffer;
}

}  // namespace

Dispatcher::Dispatcher() : running_(true), worker_busy_(false) {
  worker_.reset(new thread::Thread([this]() { Run(); },
                                   LogRuntime::AsyncWorkerName()));
}

Dispatcher::~Dispatcher() {
  running_ = false;
  cv_.notify_all();
  if (worker_) worker_->join();
  FlushAll();
}

Dispatcher& Dispatcher::Instance() {
  static Dispatcher d;
  return d;
}

void Dispatcher::SubmitBatch(std::vector<Task>& batch) {
  if (batch.empty()) return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < batch.size(); ++i) {
      queue_.push_back(std::move(batch[i]));
    }
  }
  cv_.notify_one();
}

void Dispatcher::Submit(Appender::ptr target, LogRecord record) {
  if (!target) return;
  GetTlsBuffer().Push(this, target, std::move(record));
}

void Dispatcher::FlushCurrentThread() { GetTlsBuffer().Flush(this); }

void Dispatcher::WaitIdle(Appender::ptr target) {
  (void)target;
  FlushCurrentThread();
  std::unique_lock<std::mutex> lock(mutex_);
  idle_cv_.wait(lock, [this] { return queue_.empty() && !worker_busy_.load(); });
}

void Dispatcher::FlushAll() {
  std::deque<Task> batch;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    batch.swap(queue_);
  }
  for (size_t i = 0; i < batch.size(); ++i) {
    if (batch[i].target) batch[i].target->Append(batch[i].record);
  }
  FlushUniqueTargets(batch);
}

void Dispatcher::Run() {
  while (true) {
    std::deque<Task> batch;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return !queue_.empty() || !running_; });
      if (!running_ && queue_.empty()) break;
      worker_busy_ = true;
      batch.swap(queue_);
    }

    for (size_t i = 0; i < batch.size(); ++i) {
      if (batch[i].target) batch[i].target->Append(batch[i].record);
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      worker_busy_ = false;
      if (queue_.empty()) idle_cv_.notify_all();
    }
  }
}

AsyncAppender::AsyncAppender(Appender::ptr delegate) : delegate_(delegate) {
  SyncToDelegate();
}

void AsyncAppender::SetLayout(Layout::ptr layout) {
  Appender::SetLayout(layout);
  SyncToDelegate();
}

void AsyncAppender::SyncToDelegate() {
  if (!delegate_) return;
  if (HasLayout()) delegate_->SetLayout(GetLayout());
  delegate_->SetThreshold(GetThreshold());
}

void AsyncAppender::Append(const LogRecord& record) {
  if (!PassesThreshold(record.level) || !delegate_) return;
  Dispatcher::Instance().Submit(delegate_, record);
}

void AsyncAppender::Flush() {
  if (!delegate_) return;
  Dispatcher::Instance().WaitIdle(delegate_);
  delegate_->Flush();
}

const char* AsyncAppender::Type() const { return "async"; }

Appender::ptr MakeAsync(Appender::ptr appender) {
  return Appender::ptr(new AsyncAppender(appender));
}

}  // namespace log
}  // namespace lemo
