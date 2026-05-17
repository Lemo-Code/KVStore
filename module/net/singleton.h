#ifndef NET_SINGLETON_H
#define NET_SINGLETON_H

#include <memory>

namespace net {

/**
 * @brief 进程内唯一实例（指针版），用法参考 Sylar Singleton。
 *
 * 通过函数内 static 局部变量保证 C++11 线程安全的一次性初始化。
 * 模板参数 X、N 保留与 Sylar 相同签名，便于同一翻译单元内多实例特化。
 */
template <class T, class X = void, int N = 0>
class Singleton {
 public:
  // 获取唯一实例指针
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

/**
 * @brief 进程内唯一实例（shared_ptr 版）。
 *
 * 适用于需要 shared_from_this 或统一以智能指针传递的管理器。
 */
template <class T, class X = void, int N = 0>
class SingletonPtr {
 public:
  // 获取唯一实例的 shared_ptr
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

#endif  // NET_SINGLETON_H
