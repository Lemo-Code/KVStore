#ifndef LSTL_PRIORITY_QUEUE_H
#define LSTL_PRIORITY_QUEUE_H

#include "detail/heap.h"
#include "utility.h"

namespace lstl {

// priority_queue：容器适配器，通过 heap 的迭代器区间维护堆
template <typename T, typename Sequence = vector<T>, typename Compare = less<T> >
class priority_queue {
 public:
  typedef typename heap<T, Compare>::value_type value_type;
  typedef typename heap<T, Compare>::size_type size_type;
  typedef Sequence container_type;
  typedef Compare value_compare;

 protected:
  heap<T, Compare> h_;

 public:
  priority_queue() : h_() {}
  explicit priority_queue(const Compare& comp) : h_(comp) {}

  explicit priority_queue(const Sequence& seq) : h_(seq.begin(), seq.end()) {}

  priority_queue(const Compare& comp, const Sequence& seq) : h_(seq.begin(), seq.end(), comp) {}

  template <typename InputIterator>
  priority_queue(InputIterator first, InputIterator last) : h_(first, last) {}

  template <typename InputIterator>
  priority_queue(InputIterator first, InputIterator last, const Compare& comp) : h_(first, last, comp) {}

  bool empty() const { return h_.empty(); }
  size_type size() const { return h_.size(); }

  const value_type& top() const { return h_.top(); }

  void push(const value_type& x) { h_.push(x); }

  void push(value_type&& x) { h_.push(lstl::move(x)); }

  template <typename... Args>
  void emplace(Args&&... args) {
    h_.emplace(lstl::forward<Args>(args)...);
  }

  void pop() { h_.pop(); }

  void swap(priority_queue& other) { h_.swap(other.h_); }
};

template <typename T, typename Sequence, typename Compare>
inline void swap(priority_queue<T, Sequence, Compare>& a,
                 priority_queue<T, Sequence, Compare>& b) {
  a.swap(b);
}

}  // namespace lstl

#endif  // LSTL_PRIORITY_QUEUE_H
