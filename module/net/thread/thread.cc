#include "thread/thread.h"

#include "common/util.h"

#include <pthread.h>
#include <stdexcept>

namespace net {

namespace {

thread_local Thread* t_thread = nullptr;
thread_local std::string t_thread_name = "UNKNOWN";

void apply_thread_name(const std::string& name) {
  std::string trimmed = name.empty() ? "UNKNOWN" : name;
  if (trimmed.size() > 15) {
    trimmed.resize(15);
  }
  pthread_setname_np(pthread_self(), trimmed.c_str());
}

}  // namespace

Thread* Thread::GetThis() { return t_thread; }

const std::string& Thread::GetName() { return t_thread_name; }

void Thread::SetName(const std::string& name) {
  t_thread_name = name.empty() ? "UNKNOWN" : name;
  apply_thread_name(t_thread_name);
}

Thread::Thread(std::function<void()> cb, const std::string& name)
    : id_(0), thread_(0), cb_(std::move(cb)), name_(name), semaphore_(0) {
  if (name_.empty()) {
    name_ = "UNKNOWN";
  }
  const int rt = pthread_create(&thread_, nullptr, &Thread::run, this);
  if (rt != 0) {
    throw std::logic_error("pthread_create error");
  }
  // 等待子线程完成 TLS 与 id 初始化后再返回
  semaphore_.wait();
}

Thread::~Thread() {
  if (thread_) {
    pthread_detach(thread_);
  }
}

void Thread::join() {
  if (!thread_) {
    return;
  }
  const int rt = pthread_join(thread_, nullptr);
  if (rt != 0) {
    throw std::logic_error("pthread_join error");
  }
  thread_ = 0;
}

void* Thread::run(void* arg) {
  Thread* thread = static_cast<Thread*>(arg);
  t_thread = thread;
  thread->id_ = GetThreadId();
  t_thread_name = thread->name_;
  apply_thread_name(t_thread_name);

  std::function<void()> cb;
  cb.swap(thread->cb_);

  thread->semaphore_.notify();
  cb();
  return nullptr;
}

}  // namespace net
