#ifndef LSTL_CONTAINER_HEAP_H
#define LSTL_CONTAINER_HEAP_H

#include "heap.h"
#include "sequence/vector.h"

namespace lstl {

// heap：底层数据结构，内部以 vector 存储，通过迭代器维护堆性质
template <typename T, typename Compare = less<T> >
class heap {
 public:
  typedef T value_type;
  typedef vector<T> container_type;
  typedef typename container_type::allocator_type allocator_type;
  typedef typename container_type::size_type size_type;
  typedef typename container_type::difference_type difference_type;
  typedef typename container_type::iterator iterator;
  typedef typename container_type::const_iterator const_iterator;
  typedef typename container_type::reference reference;
  typedef typename container_type::const_reference const_reference;
  typedef Compare value_compare;

 protected:
  container_type c_;
  Compare comp_;

 public:
  heap() : c_(), comp_() {}

  explicit heap(const Compare& comp) : c_(), comp_(comp) {}

  explicit heap(const container_type& seq, const Compare& comp = Compare())
      : c_(seq), comp_(comp) {
    make_heap(begin(), end(), comp_);
  }

  template <typename InputIterator>
  heap(InputIterator first, InputIterator last, const Compare& comp = Compare())
      : c_(first, last), comp_(comp) {
    make_heap(begin(), end(), comp_);
  }

  iterator begin() throw() { return c_.begin(); }
  const_iterator begin() const throw() { return c_.begin(); }
  const_iterator cbegin() const throw() { return c_.cbegin(); }

  iterator end() throw() { return c_.end(); }
  const_iterator end() const throw() { return c_.end(); }
  const_iterator cend() const throw() { return c_.cend(); }

  bool empty() const throw() { return c_.empty(); }
  size_type size() const throw() { return c_.size(); }

  const_reference top() const { return *begin(); }

  value_compare value_comp() const { return comp_; }
  container_type& container() { return c_; }
  const container_type& container() const { return c_; }

  void push(const value_type& x) {
    c_.push_back(x);
    push_heap(begin(), end(), comp_);
  }

  void push(value_type&& x) {
    c_.push_back(lstl::move(x));
    push_heap(begin(), end(), comp_);
  }

  template <typename... Args>
  void emplace(Args&&... args) {
    c_.emplace_back(lstl::forward<Args>(args)...);
    push_heap(begin(), end(), comp_);
  }

  void pop() {
    pop_heap(begin(), end(), comp_);
    c_.pop_back();
  }

  void make() { make_heap(begin(), end(), comp_); }

  void sort() { sort_heap(begin(), end(), comp_); }

  void clear() throw() { c_.clear(); }

  void swap(heap& other) throw() {
    c_.swap(other.c_);
    Compare tmp = comp_;
    comp_ = other.comp_;
    other.comp_ = tmp;
  }
};

template <typename T, typename Compare>
inline void swap(heap<T, Compare>& a, heap<T, Compare>& b) {
  a.swap(b);
}

}  // namespace lstl

#endif  // LSTL_CONTAINER_HEAP_H
