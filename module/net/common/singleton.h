#ifndef NET_COMMON_SINGLETON_H
#define NET_COMMON_SINGLETON_H

#include <memory>

namespace net {

/**
 * @brief 进程内唯一实例（指针版），用法参考 Sylar Singleton。
 */
template <class T, class X = void, int N = 0>
class Singleton {
 public:
  static T* GetInstance() {
    static T instance;
    return &instance;
  }

  Singleton(const Singleton&) = delete;
  Singleton& operator=(const Singleton&) = delete;

 protected:
  Singleton() = default;
  ~Singleton() = default;
};

/** 进程内唯一实例（shared_ptr 版）。 */
template <class T, class X = void, int N = 0>
class SingletonPtr {
 public:
  static std::shared_ptr<T> GetInstance() {
    static std::shared_ptr<T> instance(new T());
    return instance;
  }

  SingletonPtr(const SingletonPtr&) = delete;
  SingletonPtr& operator=(const SingletonPtr&) = delete;

 protected:
  SingletonPtr() = default;
  ~SingletonPtr() = default;
};

}  // namespace net

#endif  // NET_COMMON_SINGLETON_H
