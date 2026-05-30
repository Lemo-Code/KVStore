#ifndef LSTL_STACK_H
#define LSTL_STACK_H

#include "sequence/deque.h"
#include "utility.h"

namespace lstl {

template <typename T, typename Sequence = deque<T> >
class stack {
 public:
  typedef typename Sequence::value_type value_type;
  typedef typename Sequence::size_type size_type;
  typedef Sequence container_type;

 protected:
  Sequence c_;

  // 友元声明，允许比较运算符访问 c_
  template <typename T1, typename Seq1>
  friend bool operator==(const stack<T1, Seq1>&, const stack<T1, Seq1>&);
  template <typename T1, typename Seq1>
  friend bool operator<(const stack<T1, Seq1>&, const stack<T1, Seq1>&);

 public:
  stack() : c_() {}
  explicit stack(const Sequence& seq) : c_(seq) {}

  bool empty() const { return c_.empty(); }
  size_type size() const { return c_.size(); }

  value_type& top() { return c_.back(); }
  const value_type& top() const { return c_.back(); }

  void push(const value_type& x) { c_.push_back(x); }
  void push(value_type&& x) { c_.push_back(lstl::move(x)); }

  template <typename... Args>
  void emplace(Args&&... args) {
    c_.emplace_back(lstl::forward<Args>(args)...);
  }

  void pop() { c_.pop_back(); }

  void swap(stack& other) { c_.swap(other.c_); }
};

template <typename T, typename Sequence>
inline bool operator==(const stack<T, Sequence>& a, const stack<T, Sequence>& b) {
  return a.c_ == b.c_;
}

template <typename T, typename Sequence>
inline bool operator!=(const stack<T, Sequence>& a, const stack<T, Sequence>& b) {
  return !(a == b);
}

template <typename T, typename Sequence>
inline bool operator<(const stack<T, Sequence>& a, const stack<T, Sequence>& b) {
  return a.c_ < b.c_;
}

template <typename T, typename Sequence>
inline bool operator>(const stack<T, Sequence>& a, const stack<T, Sequence>& b) {
  return b < a;
}

template <typename T, typename Sequence>
inline bool operator<=(const stack<T, Sequence>& a, const stack<T, Sequence>& b) {
  return !(b < a);
}

template <typename T, typename Sequence>
inline bool operator>=(const stack<T, Sequence>& a, const stack<T, Sequence>& b) {
  return !(a < b);
}

template <typename T, typename Sequence>
inline void swap(stack<T, Sequence>& a, stack<T, Sequence>& b) {
  a.swap(b);
}

}  // namespace lstl

#endif  // LSTL_STACK_H
