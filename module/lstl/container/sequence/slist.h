#ifndef LSTL_SLIST_H
#define LSTL_SLIST_H

#include <cstddef>

#include "detail/slist_node.h"
#include "iterator/slist_iterator.h"
#include "memory.h"

namespace lstl {

template <typename T, typename Alloc = allocator<T> >
class slist {
 public:
  typedef T value_type;
  typedef Alloc allocator_type;
  typedef typename allocator_traits<Alloc>::size_type size_type;
  typedef typename allocator_traits<Alloc>::difference_type difference_type;
  typedef slist_iterator<T, T&, T*> iterator;
  typedef slist_iterator<T, const T&, const T*> const_iterator;
  typedef T& reference;
  typedef const T& const_reference;
  typedef T* pointer;
  typedef const T* const_pointer;

  typedef detail::slist_node<T> node;
  typedef detail::slist_node_base base_node;
  typedef typename allocator_traits<Alloc>::template rebind_alloc<node>::other
      node_allocator_type;

 protected:
  base_node head_;
  base_node* tail_;
  size_type size_;
  node_allocator_type node_alloc_;

 public:
  slist() : tail_(&head_), size_(0), node_alloc_() { empty_initialize(); }

  explicit slist(const allocator_type& a) throw() : tail_(&head_), size_(0), node_alloc_(a) {
    empty_initialize();
  }

  explicit slist(size_type n, const T& value = T(), const allocator_type& a = allocator_type())
      : tail_(&head_), size_(0), node_alloc_(a) {
    empty_initialize();
    insert(end(), n, value);
  }

  template <typename InputIterator>
  slist(InputIterator first, InputIterator last, const allocator_type& a = allocator_type())
      : tail_(&head_), size_(0), node_alloc_(a) {
    empty_initialize();
    initialize_dispatch(first, last, typename __is_integer<InputIterator>::_Integral());
  }

  slist(const slist& other)
      : tail_(&head_),
        size_(0),
        node_alloc_(allocator_traits<Alloc>::select_on_container_copy_construction(
            other.node_alloc_)) {
    empty_initialize();
    insert(end(), other.begin(), other.end());
  }

  slist(slist&& other) throw() : tail_(other.tail_), size_(other.size_), node_alloc_(other.node_alloc_) {
    head_.next = other.head_.next;
    other.empty_initialize();
    other.size_ = 0;
  }

  ~slist() { clear(); }

  slist& operator=(const slist& other) {
    if (this != &other) {
      assign(other.begin(), other.end());
    }
    return *this;
  }

  slist& operator=(slist&& other) throw() {
    if (this != &other) {
      clear();
      head_.next = other.head_.next;
      tail_ = other.tail_;
      size_ = other.size_;
      node_alloc_ = other.node_alloc_;
      other.empty_initialize();
      other.size_ = 0;
    }
    return *this;
  }

  iterator begin() throw() { return iterator(head_.next); }
  const_iterator begin() const throw() { return const_iterator(head_.next); }
  const_iterator cbegin() const throw() { return const_iterator(head_.next); }

  iterator end() throw() { return iterator(0); }
  const_iterator end() const throw() { return const_iterator(0); }
  const_iterator cend() const throw() { return end(); }

  bool empty() const throw() { return size_ == 0; }
  size_type size() const throw() { return size_; }
  size_type max_size() const throw() { return node_alloc_.max_size(); }

  reference front() { return *begin(); }
  const_reference front() const { return *begin(); }

  reference back() { return static_cast<node*>(tail_)->data; }
  const_reference back() const { return static_cast<const node*>(tail_)->data; }

  void push_front(const T& value) { insert_after(before_begin(), value); }

  void push_front(T&& value) { insert_after(before_begin(), move(value)); }

  template <typename... Args>
  void emplace_front(Args&&... args) {
    insert_after(before_begin(), T(static_cast<Args&&>(args)...));
  }

  void push_back(const T& value) { insert(end(), value); }

  void push_back(T&& value) { insert(end(), move(value)); }

  template <typename... Args>
  void emplace_back(Args&&... args) {
    insert(end(), T(static_cast<Args&&>(args)...));
  }

  void pop_front() {
    if (!empty()) {
      erase_after(before_begin());
    }
  }

  void pop_back() {
    if (empty()) {
      return;
    }
    if (size_ == 1) {
      pop_front();
      return;
    }
    erase_after(iterator(prev_node(tail_)));
  }

  iterator before_begin() throw() { return iterator(&head_); }
  const_iterator before_begin() const throw() { return const_iterator(const_cast<base_node*>(&head_)); }

  iterator insert_after(iterator position, const T& value) {
    node* tmp = create_node(value);
    base_node::insert_after(position.cur, tmp);
    if (position.cur == tail_) {
      tail_ = tmp;
    }
    ++size_;
    return iterator(tmp);
  }

  iterator insert_after(iterator position, T&& value) {
    node* tmp = create_node(move(value));
    base_node::insert_after(position.cur, tmp);
    if (position.cur == tail_) {
      tail_ = tmp;
    }
    ++size_;
    return iterator(tmp);
  }

  iterator insert_after(iterator position, size_type n, const T& value) {
    iterator result = end();
    for (size_type i = 0; i < n; ++i) {
      result = insert_after(position, value);
      position = result;
    }
    return result;
  }

  template <typename InputIterator>
  iterator insert_after(iterator position, InputIterator first, InputIterator last) {
    return insert_after_dispatch(position, first, last,
                                 typename __is_integer<InputIterator>::_Integral());
  }

  iterator erase_after(iterator position) {
    base_node* next = position.cur->next;
    if (!next) {
      return end();
    }
    base_node::unlink_after(position.cur);
    if (next == tail_) {
      tail_ = position.cur;
    }
    destroy_node(static_cast<node*>(next));
    --size_;
    return iterator(position.cur->next);
  }

  iterator insert(iterator position, const T& value) {
    if (position == begin()) {
      return insert_after(before_begin(), value);
    }
    return insert_after(iterator(prev_node(position.cur)), value);
  }

  iterator insert(iterator position, T&& value) {
    if (position == begin()) {
      return insert_after(before_begin(), move(value));
    }
    return insert_after(iterator(prev_node(position.cur)), move(value));
  }

  iterator insert(iterator position, size_type n, const T& value) {
    if (position == end()) {
      return insert_after(iterator(tail_), n, value);
    }
    iterator result = position;
    for (size_type i = 0; i < n; ++i) {
      result = insert(position, value);
    }
    return result;
  }

  template <typename InputIterator>
  iterator insert(iterator position, InputIterator first, InputIterator last) {
    return insert_dispatch(position, first, last,
                           typename __is_integer<InputIterator>::_Integral());
  }

  iterator erase(iterator position) {
    if (position == begin()) {
      return erase_after(before_begin());
    }
    return erase_after(iterator(prev_node(position.cur)));
  }

  iterator erase(iterator first, iterator last) {
    while (first != last) {
      first = erase(first);
    }
    return last;
  }

  void resize(size_type new_size, const T& value = T()) {
    if (new_size < size_) {
      while (size_ > new_size) {
        pop_back();
      }
    } else if (new_size > size_) {
      insert(end(), new_size - size_, value);
    }
  }

  void clear() throw() {
    base_node* cur = head_.next;
    while (cur) {
      base_node* next = cur->next;
      destroy_node(static_cast<node*>(cur));
      cur = next;
    }
    empty_initialize();
    size_ = 0;
  }

  void assign(size_type n, const T& value) {
    clear();
    insert(end(), n, value);
  }

  template <typename InputIterator>
  void assign(InputIterator first, InputIterator last) {
    assign_dispatch(first, last, typename __is_integer<InputIterator>::_Integral());
  }

  void remove(const T& value) {
    iterator prev = before_begin();
    iterator cur = begin();
    while (cur != end()) {
      if (*cur == value) {
        cur = erase_after(prev);
      } else {
        prev = cur;
        ++cur;
      }
    }
  }

  template <typename Predicate>
  void remove_if(Predicate pred) {
    iterator prev = before_begin();
    iterator cur = begin();
    while (cur != end()) {
      if (pred(*cur)) {
        cur = erase_after(prev);
      } else {
        prev = cur;
        ++cur;
      }
    }
  }

  void reverse() {
    if (size_ < 2) {
      return;
    }
    base_node* prev = 0;
    base_node* cur = head_.next;
    base_node* const old_first = head_.next;
    while (cur) {
      base_node* const next = cur->next;
      cur->next = prev;
      prev = cur;
      cur = next;
    }
    tail_ = old_first;
    head_.next = prev;
  }

  void sort() { sort(less<T>()); }

  template <typename Compare>
  void sort(Compare comp) {
    if (size_ < 2) {
      return;
    }
    iterator cur = begin();
    ++cur;
    while (cur != end()) {
      iterator next = cur;
      ++next;
      iterator insert_pos = begin();
      while (insert_pos != cur && !comp(*cur, *insert_pos)) {
        ++insert_pos;
      }
      if (insert_pos != cur) {
        relink_before(insert_pos, cur);
      }
      cur = next;
    }
  }

  void swap(slist& other) throw() {
    if (&other != this) {
      base_node* tmp_next = head_.next;
      head_.next = other.head_.next;
      other.head_.next = tmp_next;

      base_node* tmp_tail = tail_;
      tail_ = other.tail_;
      other.tail_ = tmp_tail;

      const size_type tmp_size = size_;
      size_ = other.size_;
      other.size_ = tmp_size;

      node_allocator_type tmp_alloc = node_alloc_;
      node_alloc_ = other.node_alloc_;
      other.node_alloc_ = tmp_alloc;

      fix_tail_after_swap();
      other.fix_tail_after_swap();
    }
  }

  allocator_type get_allocator() const throw() { return allocator_type(); }

 protected:
  void empty_initialize() {
    head_.next = 0;
    tail_ = &head_;
  }

  void fix_tail_after_swap() {
    if (!head_.next) {
      tail_ = &head_;
      return;
    }
    tail_ = head_.next;
    while (tail_->next) {
      tail_ = tail_->next;
    }
  }

  base_node* prev_node(base_node* pos) const {
    base_node* prev = const_cast<base_node*>(&head_);
    while (prev->next != pos) {
      prev = prev->next;
    }
    return prev;
  }

  void unlink_node(base_node* node_ptr) {
    base_node* prev = prev_node(node_ptr);
    prev->next = node_ptr->next;
    if (tail_ == node_ptr) {
      tail_ = prev;
    }
  }

  void relink_before(iterator position, iterator node_it) {
    base_node* const node_ptr = node_it.cur;
    unlink_node(node_ptr);
    if (position == begin()) {
      node_ptr->next = head_.next;
      head_.next = node_ptr;
    } else {
      base_node* const prev = prev_node(position.cur);
      node_ptr->next = position.cur;
      prev->next = node_ptr;
    }
  }

  template <typename Integer>
  void initialize_dispatch(Integer n, Integer value, __true_type) {
    insert(end(), static_cast<size_type>(n), static_cast<const T&>(value));
  }

  template <typename InputIterator>
  void initialize_dispatch(InputIterator first, InputIterator last, __false_type) {
    for (; first != last; ++first) {
      insert(end(), *first);
    }
  }

  template <typename Integer>
  iterator insert_dispatch(iterator position, Integer n, Integer value, __true_type) {
    return insert(position, static_cast<size_type>(n), static_cast<const T&>(value));
  }

  template <typename InputIterator>
  iterator insert_dispatch(iterator position, InputIterator first, InputIterator last,
                           __false_type) {
    if (position == end()) {
      iterator result = end();
      iterator pos = iterator(tail_);
      for (bool first_elem = true; first != last; ++first) {
        result = insert_after(pos, *first);
        pos = result;
        first_elem = false;
        (void)first_elem;
      }
      return result;
    }
    iterator result = position;
    for (bool first_elem = true; first != last; ++first) {
      iterator tmp = insert(position, *first);
      if (first_elem) {
        result = tmp;
        first_elem = false;
      }
    }
    return result;
  }

  template <typename Integer>
  iterator insert_after_dispatch(iterator position, Integer n, Integer value, __true_type) {
    return insert_after(position, static_cast<size_type>(n), static_cast<const T&>(value));
  }

  template <typename InputIterator>
  iterator insert_after_dispatch(iterator position, InputIterator first, InputIterator last,
                                 __false_type) {
    iterator result = end();
    for (bool first_elem = true; first != last; ++first) {
      result = insert_after(position, *first);
      position = result;
      first_elem = false;
      (void)first_elem;
    }
    return result;
  }

  template <typename Integer>
  void assign_dispatch(Integer n, const T& value, __true_type) {
    assign(static_cast<size_type>(n), value);
  }

  template <typename InputIterator>
  void assign_dispatch(InputIterator first, InputIterator last, __false_type) {
    clear();
    insert(end(), first, last);
  }

  node* create_node(const T& value) {
    node* p = node_alloc_.allocate(1);
    construct(&p->data, value);
    return p;
  }

  node* create_node(T&& value) {
    node* p = node_alloc_.allocate(1);
    construct(&p->data, move(value));
    return p;
  }

  void destroy_node(node* p) {
    destroy(&p->data);
    node_alloc_.deallocate(p, 1);
  }
};

template <typename T, typename Alloc>
inline bool operator==(const slist<T, Alloc>& a, const slist<T, Alloc>& b) {
  return a.size() == b.size() && equal(a.begin(), a.end(), b.begin());
}

template <typename T, typename Alloc>
inline bool operator!=(const slist<T, Alloc>& a, const slist<T, Alloc>& b) {
  return !(a == b);
}

template <typename T, typename Alloc>
inline void swap(slist<T, Alloc>& a, slist<T, Alloc>& b) {
  a.swap(b);
}

}  // namespace lstl

#endif  // LSTL_SLIST_H
