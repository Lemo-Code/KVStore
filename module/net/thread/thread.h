#ifndef NET_THREAD_THREAD_H
#define NET_THREAD_THREAD_H

#include "noncopyable.h"
#include "semaphore.h"

#include <functional>
#include <memory>
#include <pthread.h>
#include <string>

namespace net {

/**
 * @brief pthread 线程封装：构造后等待 run() 完成 TLS 初始化，再执行回调。
 *
 * 提供 thread_local 当前线程指针与名称，供日志 util 等模块查询。
 */
class Thread : Noncopyable {
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

}  // namespace net

#endif  // NET_THREAD_THREAD_H
