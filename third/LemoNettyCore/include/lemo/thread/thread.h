#pragma once

#include "lemo/thread/semaphore.h"
#include "lemo/utils/noncopyable.h"

#include <functional>
#include <memory>
#include <pthread.h>
#include <string>

namespace lemo {
namespace thread {

class Thread : public utils::NonCopyable {
 public:
  typedef std::shared_ptr<Thread> ptr;

  Thread(std::function<void()> cb, const std::string& name);
  ~Thread();

  uint32_t getId() const { return id_; }
  const std::string& getName() const { return name_; }

  void join();

  static Thread* GetThis();
  static const std::string& GetName();
  static void SetName(const std::string& name);

 private:
  static void* run(void* arg);

  uint32_t id_;
  pthread_t thread_;
  std::function<void()> cb_;
  std::string name_;
  Semaphore semaphore_;
};

}  // namespace thread
}  // namespace lemo
