#ifndef LSTL_LIST_H
#define LSTL_LIST_H

#include <cstddef>

#include "detail/list_node.h"
#include "iterator/list_iterator.h"
#include "iterator/reverse_iterator.h"
#include "memory.h"

namespace lstl {

template <typename T, typename Alloc = allocator<T> >
class list {
 public:
  typedef T value_type;
  typedef Alloc allocator_type;
  typedef typename allocator_traits<Alloc>::size_type size_type;
  typedef typename allocator_traits<Alloc>::difference_type difference_type;
  typedef list_iterator<T, T&, T*> iterator;
  typedef list_iterator<T, const T&, const T*> const_iterator;
  typedef detail::reverse_iterator<iterator> reverse_iterator;
  typedef detail::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef T& reference;
  typedef const T& const_reference;
  typedef T* pointer;
  typedef const T* const_pointer;

  typedef detail::list_node<T> node;
  typedef detail::list_node_base base_node;
  typedef typename allocator_traits<Alloc>::template rebind_alloc<node>::other
      node_allocator_type;

 protected:
  base_node node_;
  size_type size_;
  node_allocator_type node_alloc_;

 public:
  list() : size_(0), node_alloc_() { empty_initialize(); }

  explicit list(const allocator_type& a) throw() : size_(0), node_alloc_(a) {
    empty_initialize();
  }

  explicit list(size_type n, const T& value = T(),
                const allocator_type& a = allocator_type())
      : size_(0), node_alloc_(a) {
    empty_initialize();
    insert(end(), n, value);
  }

  template <typename InputIterator>
  list(InputIterator first, InputIterator last, const allocator_type& a = allocator_type())
      : size_(0), node_alloc_(a) {
    empty_initialize();
    initialize_dispatch(first, last, typename __is_integer<InputIterator>::_Integral());
  }

  list(const list& other)
      : size_(0),
        node_alloc_(allocator_traits<Alloc>::select_on_container_copy_construction(
            other.node_alloc_)) {
    empty_initialize();
    insert(end(), other.begin(), other.end());
  }

  list(list&& other) throw() : size_(other.size_), node_alloc_(other.node_alloc_) {
    if (other.empty()) {
      empty_initialize();
    } else {
      node_.next = other.node_.next;
      node_.prev = other.node_.prev;
      node_.next->prev = node_.prev->next = &node_;
    }
    other.empty_initialize();
    other.size_ = 0;
  }

  ~list() { clear(); }

  list& operator=(const list& other) {
    if (this != &other) {
      assign(other.begin(), other.end());
    }
    return *this;
  }

  list& operator=(list&& other) throw() {
    if (this != &other) {
      clear();
      if (!other.empty()) {
        node_.next = other.node_.next;
        node_.prev = other.node_.prev;
        node_.next->prev = node_.prev->next = &node_;
      }
      size_ = other.size_;
      node_alloc_ = other.node_alloc_;
      other.empty_initialize();
      other.size_ = 0;
    }
    return *this;
  }

  iterator begin() throw() { return iterator(node_.next); }
  const_iterator begin() const throw() { return const_iterator(node_.next); }
  const_iterator cbegin() const throw() { return const_iterator(node_.next); }

  iterator end() throw() { return iterator(&node_); }
  const_iterator end() const throw() { return const_iterator(const_cast<base_node*>(&node_)); }
  const_iterator cend() const throw() { return end(); }

  reverse_iterator rbegin() throw() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const throw() { return const_reverse_iterator(end()); }
  reverse_iterator rend() throw() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const throw() { return const_reverse_iterator(begin()); }

  bool empty() const throw() { return size_ == 0; }
  size_type size() const throw() { return size_; }
  size_type max_size() const throw() { return node_alloc_.max_size(); }

  reference front() { return *begin(); }
  const_reference front() const { return *begin(); }
  reference back() {
    iterator tmp = end();
    return *--tmp;
  }
  const_reference back() const {
    const_iterator tmp = end();
    return *--tmp;
  }

  void push_front(const T& value) { insert(begin(), value); }
  void push_front(T&& value) { insert(begin(), move(value)); }

  void push_back(const T& value) { insert(end(), value); }
  void push_back(T&& value) { insert(end(), move(value)); }

  template <typename... Args>
  void emplace_front(Args&&... args) {
    insert(begin(), T(static_cast<Args&&>(args)...));
  }

  template <typename... Args>
  void emplace_back(Args&&... args) {
    insert(end(), T(static_cast<Args&&>(args)...));
  }

  void pop_front() { erase(begin()); }

  void pop_back() {
    iterator tmp = end();
    erase(--tmp);
  }

  iterator insert(iterator position, const T& value) {
    node* tmp = create_node(value);
    base_node::insert_before(position.cur, tmp);
    ++size_;
    return iterator(tmp);
  }

  iterator insert(iterator position, T&& value) {
    node* tmp = create_node(move(value));
    base_node::insert_before(position.cur, tmp);
    ++size_;
    return iterator(tmp);
  }

  iterator insert(iterator position, size_type n, const T& value) {
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
    base_node* next_node = position.cur->next;
    base_node::unlink(position.cur);
    destroy_node(static_cast<node*>(position.cur));
    --size_;
    return iterator(next_node);
  }

  iterator erase(iterator first, iterator last) {
    while (first != last) {
      first = erase(first);
    }
    return last;
  }

  void resize(size_type new_size, const T& value = T()) {
    if (new_size < size_) {
      iterator it = begin();
      for (size_type i = 0; i < new_size; ++i) {
        ++it;
      }
      erase(it, end());
    } else if (new_size > size_) {
      insert(end(), new_size - size_, value);
    }
  }

  void clear() throw() {
    base_node* cur = node_.next;
    while (cur != &node_) {
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

  void reverse() {
    if (size_ <= 1) {
      return;
    }
    base_node* p = node_.next;
    while (p != &node_) {
      base_node* tmp = p->next;
      p->next = p->prev;
      p->prev = tmp;
      p = tmp;
    }
    base_node* tmp = node_.next;
    node_.next = node_.prev;
    node_.prev = tmp;
  }

  void sort() { sort(less<T>()); }

  template <typename Compare>
  void sort(Compare comp) {
    if (size_ < 2) {
      return;
    }
    list carry;
    list counter[64];
    int fill = 0;
    while (!empty()) {
      carry.splice(carry.begin(), *this, begin());
      int i = 0;
      while (i < fill && !counter[i].empty()) {
        counter[i].merge(carry, comp);
        carry.swap(counter[i++]);
      }
      carry.swap(counter[i]);
      if (i == fill) {
        ++fill;
      }
    }
    for (int i = 1; i < fill; ++i) {
      counter[i].merge(counter[i - 1], comp);
    }
    splice(begin(), counter[fill - 1]);
  }

  void merge(list& other) { merge(other, less<T>()); }

  template <typename Compare>
  void merge(list& other, Compare comp) {
    if (&other == this || other.empty()) {
      return;
    }
    iterator first1 = begin();
    iterator first2 = other.begin();
    while (first1 != end() && first2 != other.end()) {
      if (comp(*first2, *first1)) {
        iterator next = first2;
        ++next;
        transfer(first1, first2, next);
        first2 = next;
      } else {
        ++first1;
      }
    }
    if (first2 != other.end()) {
      transfer(end(), first2, other.end());
    }
    size_ += other.size_;
    other.size_ = 0;
  }

  void splice(iterator position, list& other) {
    if (&other == this || other.empty()) {
      return;
    }
    transfer(position, other.begin(), other.end());
    size_ += other.size_;
    other.size_ = 0;
  }

  void splice(iterator position, list& other, iterator i) {
    iterator j = i;
    ++j;
    if (position == i || position == j) {
      return;
    }
    if (&other != this) {
      size_ += 1;
      other.size_ -= 1;
    }
    transfer(position, i, j);
  }

  void splice(iterator position, list& other, iterator first, iterator last) {
    if (first != last) {
      if (&other != this) {
        size_ += static_cast<size_type>(distance(first, last));
        other.size_ -= static_cast<size_type>(distance(first, last));
      }
      transfer(position, first, last);
    }
  }

  void swap(list& other) throw() {
    if (&other != this) {
      swap_nodes(node_, other.node_);
      const size_type tmp_size = size_;
      size_ = other.size_;
      other.size_ = tmp_size;
      node_allocator_type tmp_alloc = node_alloc_;
      node_alloc_ = other.node_alloc_;
      other.node_alloc_ = tmp_alloc;
    }
  }

  allocator_type get_allocator() const throw() { return allocator_type(); }

 protected:
  void empty_initialize() {
    node_.next = &node_;
    node_.prev = &node_;
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

  void transfer(iterator position, iterator first, iterator last) {
    if (position != last) {
      base_node::transfer(first.cur, last.cur, position.cur);
    }
  }

  static void swap_nodes(base_node& a, base_node& b) {
    if (a.next == &a && b.next == &b) {
      return;
    }
    if (a.next == &a) {
      a.next = b.next;
      a.prev = b.prev;
      b.next->prev = b.prev->next = &a;
      b.next = b.prev = &b;
      return;
    }
    if (b.next == &b) {
      swap_nodes(b, a);
      return;
    }
    base_node* tmp_next = a.next;
    base_node* tmp_prev = a.prev;
    a.next = b.next;
    a.prev = b.prev;
    b.next = tmp_next;
    b.prev = tmp_prev;
    a.next->prev = a.prev->next = &a;
    b.next->prev = b.prev->next = &b;
  }
};

template <typename T, typename Alloc>
inline bool operator==(const list<T, Alloc>& a, const list<T, Alloc>& b) {
  return a.size() == b.size() && equal(a.begin(), a.end(), b.begin());
}

template <typename T, typename Alloc>
inline bool operator!=(const list<T, Alloc>& a, const list<T, Alloc>& b) {
  return !(a == b);
}

template <typename T, typename Alloc>
inline void swap(list<T, Alloc>& a, list<T, Alloc>& b) {
  a.swap(b);
}

}  // namespace lstl

#endif  // LSTL_LIST_H
