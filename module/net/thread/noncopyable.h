#ifndef NET_THREAD_NONCOPYABLE_H
#define NET_THREAD_NONCOPYABLE_H

namespace net {

class Noncopyable {
 public:
  Noncopyable() {}
  ~Noncopyable() {}

  Noncopyable(const Noncopyable&) = delete;
  Noncopyable(Noncopyable&&) = delete;
  Noncopyable& operator=(const Noncopyable&) = delete;
  Noncopyable& operator=(Noncopyable&&) = delete;
};

}  // namespace net

#endif  // NET_THREAD_NONCOPYABLE_H
