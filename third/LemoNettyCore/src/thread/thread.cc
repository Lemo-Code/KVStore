#include "lemo/thread/thread.h"

#include "lemo/utils/thread_util.h"

#include <pthread.h>
#include <stdexcept>

namespace lemo {
namespace thread {

namespace {

thread_local Thread* t_thread = nullptr;

void apply_pthread_name(const std::string& name) {
  std::string trimmed = name.empty() ? "unknown" : name;
  if (trimmed.size() > 15) {
    trimmed.resize(15);
  }
#if defined(__linux__) || defined(__APPLE__)
  pthread_setname_np(pthread_self(), trimmed.c_str());
#else
  (void)trimmed;
#endif
}

}  // namespace

Thread* Thread::GetThis() { return t_thread; }

const std::string& Thread::GetName() { return utils::GetThreadName(); }

void Thread::SetName(const std::string& name) {
  utils::SetThreadName(name);
  apply_pthread_name(utils::GetThreadName());
}

Thread::Thread(std::function<void()> cb, const std::string& name)
    : id_(0), thread_(0), cb_(std::move(cb)), name_(name), semaphore_(0) {
  if (name_.empty()) {
    name_ = "unknown";
  }
  const int rt = pthread_create(&thread_, nullptr, &Thread::run, this);
  if (rt != 0) {
    throw std::logic_error("pthread_create error");
  }
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
  thread->id_ = utils::GetThreadId();
  utils::SetThreadName(thread->name_);
  apply_pthread_name(thread->name_);

  std::function<void()> cb;
  cb.swap(thread->cb_);

  thread->semaphore_.notify();
  cb();
  return nullptr;
}

}  // namespace thread
}  // namespace lemo
