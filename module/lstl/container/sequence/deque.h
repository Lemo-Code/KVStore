#ifndef LSTL_DEQUE_H
#define LSTL_DEQUE_H

#include <cstddef>

#include "iterator/deque_iterator.h"
#include "iterator/reverse_iterator.h"
#include "memory.h"

namespace lstl {

template <typename T, typename Alloc = allocator<T> >
class deque {
 public:
  typedef T value_type;
  typedef Alloc allocator_type;
  typedef typename allocator_traits<Alloc>::size_type size_type;
  typedef typename allocator_traits<Alloc>::difference_type difference_type;
  typedef deque_iterator<T, T&, T*> iterator;
  typedef deque_iterator<T, const T&, const T*> const_iterator;
  typedef detail::reverse_iterator<iterator> reverse_iterator;
  typedef detail::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef T& reference;
  typedef const T& const_reference;
  typedef T* pointer;
  typedef const T* const_pointer;

  typedef T** map_pointer;
  typedef typename allocator_traits<Alloc>::template rebind_alloc<T*>::other map_allocator_type;

 protected:
  iterator start_;
  iterator finish_;
  map_pointer map_;
  size_type map_size_;
  allocator_type alloc_;
  map_allocator_type map_alloc_;

  enum { initial_map_size = 8 };

 public:
  deque() : map_(0), map_size_(0), alloc_(), map_alloc_() { create_map_and_buffers(); }

  explicit deque(const allocator_type& a) throw()
      : map_(0), map_size_(0), alloc_(a), map_alloc_(a) {
    create_map_and_buffers();
  }

  explicit deque(size_type n, const T& value = T(), const allocator_type& a = allocator_type())
      : map_(0), map_size_(0), alloc_(a), map_alloc_(a) {
    create_map_and_buffers();
    initialize_dispatch(n, value, __true_type());
  }

  template <typename InputIterator>
  deque(InputIterator first, InputIterator last, const allocator_type& a = allocator_type())
      : map_(0), map_size_(0), alloc_(a), map_alloc_(a) {
    create_map_and_buffers();
    initialize_dispatch(first, last, typename __is_integer<InputIterator>::_Integral());
  }

  deque(const deque& other)
      : map_(0),
        map_size_(0),
        alloc_(allocator_traits<Alloc>::select_on_container_copy_construction(
            other.alloc_)),
        map_alloc_(other.map_alloc_) {
    create_map_and_buffers();
    insert(end(), other.begin(), other.end());
  }

  deque(deque&& other) throw()
      : start_(other.start_),
        finish_(other.finish_),
        map_(other.map_),
        map_size_(other.map_size_),
        alloc_(other.alloc_),
        map_alloc_(other.map_alloc_) {
    other.map_ = 0;
    other.map_size_ = 0;
    other.create_map_and_buffers();
  }

  ~deque() { destroy_all(); }

  deque& operator=(const deque& other) {
    if (this != &other) {
      assign(other.begin(), other.end());
    }
    return *this;
  }

  deque& operator=(deque&& other) throw() {
    if (this != &other) {
      destroy_all();
      start_ = other.start_;
      finish_ = other.finish_;
      map_ = other.map_;
      map_size_ = other.map_size_;
      alloc_ = other.alloc_;
      map_alloc_ = other.map_alloc_;
      other.map_ = 0;
      other.map_size_ = 0;
      other.create_map_and_buffers();
    }
    return *this;
  }

  iterator begin() throw() { return start_; }
  const_iterator begin() const throw() { return start_; }
  const_iterator cbegin() const throw() { return start_; }

  iterator end() throw() { return finish_; }
  const_iterator end() const throw() { return finish_; }
  const_iterator cend() const throw() { return finish_; }

  reverse_iterator rbegin() throw() { return reverse_iterator(finish_); }
  const_reverse_iterator rbegin() const throw() { return const_reverse_iterator(finish_); }
  reverse_iterator rend() throw() { return reverse_iterator(start_); }
  const_reverse_iterator rend() const throw() { return const_reverse_iterator(start_); }

  size_type size() const throw() { return static_cast<size_type>(finish_ - start_); }
  size_type max_size() const throw() { return alloc_.max_size(); }
  bool empty() const throw() { return start_ == finish_; }

  reference operator[](size_type n) { return start_[static_cast<difference_type>(n)]; }
  const_reference operator[](size_type n) const {
    return start_[static_cast<difference_type>(n)];
  }

  reference front() { return *start_; }
  const_reference front() const { return *start_; }
  reference back() {
    iterator tmp = finish_;
    return *--tmp;
  }
  const_reference back() const {
    const_iterator tmp = finish_;
    return *--tmp;
  }

  void push_front(const T& value) {
    if (start_.cur != start_.first) {
      construct(--start_.cur, value);
    } else {
      reserve_map_at_front();
      T* new_buffer = allocate_buffer();
      *(--start_.node) = new_buffer;
      start_.set(new_buffer + buffer_size() - 1, new_buffer, new_buffer + buffer_size(),
                 start_.node);
      construct(start_.cur, value);
    }
  }

  void push_front(T&& value) {
    if (start_.cur != start_.first) {
      construct(--start_.cur, move(value));
    } else {
      reserve_map_at_front();
      T* new_buffer = allocate_buffer();
      *(--start_.node) = new_buffer;
      start_.set(new_buffer + buffer_size() - 1, new_buffer, new_buffer + buffer_size(),
                 start_.node);
      construct(start_.cur, move(value));
    }
  }

  void push_back(const T& value) {
    if (finish_.cur != finish_.last) {
      construct(finish_.cur, value);
      ++finish_.cur;
    } else {
      reserve_map_at_back();
      T* new_buffer = allocate_buffer();
      *(finish_.node + 1) = new_buffer;
      finish_.set(new_buffer, new_buffer, new_buffer + buffer_size(), finish_.node + 1);
      construct(finish_.cur, value);
      ++finish_.cur;
    }
  }

  void push_back(T&& value) {
    if (finish_.cur != finish_.last) {
      construct(finish_.cur, move(value));
      ++finish_.cur;
    } else {
      reserve_map_at_back();
      T* new_buffer = allocate_buffer();
      *(finish_.node + 1) = new_buffer;
      finish_.set(new_buffer, new_buffer, new_buffer + buffer_size(), finish_.node + 1);
      construct(finish_.cur, move(value));
      ++finish_.cur;
    }
  }

  template <typename... Args>
  void emplace_front(Args&&... args) {
    push_front(T(static_cast<Args&&>(args)...));
  }

  template <typename... Args>
  void emplace_back(Args&&... args) {
    push_back(T(static_cast<Args&&>(args)...));
  }

  void pop_front() {
    destroy(start_.cur);
    if (++start_.cur == start_.last) {
      deallocate_buffer(*start_.node);
      ++start_.node;
      T* buf = *start_.node;
      start_.set(buf, buf, buf + buffer_size(), start_.node);
    }
  }

  void pop_back() {
    if (finish_.cur == finish_.first) {
      destroy(finish_.last - 1);
      deallocate_buffer(*finish_.node);
      finish_.set(*(finish_.node - 1) + buffer_size() - 1, *(finish_.node - 1),
                  *(finish_.node - 1) + buffer_size(), finish_.node - 1);
    } else {
      destroy(--finish_.cur);
    }
  }

  iterator insert(iterator position, const T& value) {
    if (position == begin()) {
      push_front(value);
      return begin();
    }
    if (position == end()) {
      push_back(value);
      iterator tmp = finish_;
      return --tmp;
    }
    return insert_in_middle(position, value);
  }

  iterator insert(iterator position, T&& value) {
    if (position == begin()) {
      push_front(move(value));
      return begin();
    }
    if (position == end()) {
      push_back(move(value));
      iterator tmp = finish_;
      return --tmp;
    }
    const T copy(move(value));
    return insert_in_middle(position, copy);
  }

  iterator insert(iterator position, size_type n, const T& value) {
    if (position == end()) {
      iterator result = end();
      for (size_type i = 0; i < n; ++i) {
        push_back(value);
        if (i == 0) {
          result = finish_;
          --result;
        }
      }
      return result;
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
    iterator next = position;
    ++next;
    while (next != finish_) {
      *position = move(*next);
      ++position;
      ++next;
    }
    pop_back();
    return position;
  }

  iterator erase(iterator first, iterator last) {
    if (first == last) {
      return last;
    }
    if (last == end()) {
      while (first != finish_) {
        pop_back();
      }
      return finish_;
    }
    deque tail;
    for (iterator it = last; it != end(); ++it) {
      tail.push_back(*it);
    }
    while (first != finish_) {
      pop_back();
    }
    insert(end(), tail.begin(), tail.end());
    return first;
  }

  void resize(size_type new_size, const T& value = T()) {
    if (new_size < size()) {
      while (size() > new_size) {
        pop_back();
      }
    } else {
      insert(end(), new_size - size(), value);
    }
  }

  void clear() throw() {
    while (!empty()) {
      pop_back();
    }
  }

  void assign(size_type n, const T& value) {
    clear();
    insert(end(), n, value);
  }

  template <typename InputIterator>
  void assign(InputIterator first, InputIterator last) {
    assign_dispatch(first, last, typename __is_integer<InputIterator>::_Integral());
  }

  void swap(deque& other) throw() {
    if (&other != this) {
      iterator tmp_start = start_;
      iterator tmp_finish = finish_;
      map_pointer tmp_map = map_;
      size_type tmp_map_size = map_size_;
      allocator_type tmp_alloc = alloc_;
      map_allocator_type tmp_map_alloc = map_alloc_;

      start_ = other.start_;
      finish_ = other.finish_;
      map_ = other.map_;
      map_size_ = other.map_size_;
      alloc_ = other.alloc_;
      map_alloc_ = other.map_alloc_;

      other.start_ = tmp_start;
      other.finish_ = tmp_finish;
      other.map_ = tmp_map;
      other.map_size_ = tmp_map_size;
      other.alloc_ = tmp_alloc;
      other.map_alloc_ = tmp_map_alloc;
    }
  }

  allocator_type get_allocator() const throw() { return alloc_; }

 protected:
  static size_type buffer_size() { return iterator::buffer_size(); }

  void create_map_and_buffers() {
    map_size_ = initial_map_size;
    map_ = map_alloc_.allocate(map_size_);
    for (size_type i = 0; i < map_size_; ++i) {
      map_[i] = 0;
    }
    T* buffer = allocate_buffer();
    const size_type center = map_size_ / 2;
    map_[center] = buffer;
    start_.set(buffer + buffer_size() / 2, buffer, buffer + buffer_size(), map_ + center);
    finish_ = start_;
  }

  T* allocate_buffer() { return alloc_.allocate(buffer_size()); }

  void deallocate_buffer(T* p) {
    if (p) {
      alloc_.deallocate(p, buffer_size());
    }
  }

  void destroy_all() {
    if (!map_) {
      return;
    }
    clear();
    for (size_type i = 0; i < map_size_; ++i) {
      if (map_[i]) {
        deallocate_buffer(map_[i]);
        map_[i] = 0;
      }
    }
    map_alloc_.deallocate(map_, map_size_);
    map_ = 0;
    map_size_ = 0;
  }

  void reserve_map_at_back() {
    if (finish_.node == map_ + map_size_ - 1) {
      reallocate_map(finish_.node - map_ + 1, true);
    }
  }

  void reserve_map_at_front() {
    if (start_.node == map_) {
      reallocate_map(start_.node - map_ + 1, false);
    }
  }

  void reallocate_map(size_type nodes_to_add, bool at_back) {
    const size_type old_map_size = map_size_;
    const size_type init_map = static_cast<size_type>(initial_map_size);
    const size_type add = nodes_to_add > init_map ? nodes_to_add : init_map;
    const size_type new_map_size = map_size_ + add;
    map_pointer new_map = map_alloc_.allocate(new_map_size);
    for (size_type i = 0; i < new_map_size; ++i) {
      new_map[i] = 0;
    }
    const size_type center = (new_map_size - old_map_size) / 2;
    for (size_type i = 0; i < old_map_size; ++i) {
      new_map[i + center] = map_[i];
    }
    map_alloc_.deallocate(map_, map_size_);
    map_ = new_map;
    map_size_ = new_map_size;
    start_.node += center;
    finish_.node += center;
    start_.first = *start_.node;
    start_.last = start_.first + buffer_size();
    finish_.first = *finish_.node;
    finish_.last = finish_.first + buffer_size();
  }

  template <typename Integer>
  void initialize_dispatch(Integer n, const T& value, __true_type) {
    insert(end(), static_cast<size_type>(n), value);
  }

  template <typename InputIterator>
  void initialize_dispatch(InputIterator first, InputIterator last, __false_type) {
    insert(end(), first, last);
  }

  template <typename Integer>
  iterator insert_dispatch(iterator position, Integer n, const T& value, __true_type) {
    return insert(position, static_cast<size_type>(n), value);
  }

  template <typename InputIterator>
  iterator insert_dispatch(iterator position, InputIterator first, InputIterator last,
                           __false_type) {
    if (position == end()) {
      iterator result = end();
      for (bool first_elem = true; first != last; ++first) {
        push_back(*first);
        if (first_elem) {
          result = finish_;
          --result;
          first_elem = false;
        }
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
  void assign_dispatch(Integer n, const T& value, __true_type) {
    assign(static_cast<size_type>(n), value);
  }

  template <typename InputIterator>
  void assign_dispatch(InputIterator first, InputIterator last, __false_type) {
    clear();
    insert(end(), first, last);
  }

  iterator insert_in_middle(iterator position, const T& value) {
    if (finish_.cur != finish_.last) {
      construct(finish_.cur, move(*(finish_.cur - 1)));
      ++finish_.cur;
      iterator it = finish_;
      --it;
      --it;
      while (it != position) {
        *it = move(*(it - 1));
        --it;
      }
      *position = value;
      return position;
    }
    push_back(back());
    iterator it = finish_;
    --it;
    --it;
    while (it != position) {
      *it = move(*(it - 1));
      --it;
    }
    *position = value;
    return position;
  }
};

template <typename T, typename Alloc>
inline bool operator==(const deque<T, Alloc>& a, const deque<T, Alloc>& b) {
  return a.size() == b.size() && equal(a.begin(), a.end(), b.begin());
}

template <typename T, typename Alloc>
inline bool operator!=(const deque<T, Alloc>& a, const deque<T, Alloc>& b) {
  return !(a == b);
}

template <typename T, typename Alloc>
inline bool operator<(const deque<T, Alloc>& a, const deque<T, Alloc>& b) {
  return lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
}

template <typename T, typename Alloc>
inline bool operator>(const deque<T, Alloc>& a, const deque<T, Alloc>& b) {
  return b < a;
}

template <typename T, typename Alloc>
inline bool operator<=(const deque<T, Alloc>& a, const deque<T, Alloc>& b) {
  return !(b < a);
}

template <typename T, typename Alloc>
inline bool operator>=(const deque<T, Alloc>& a, const deque<T, Alloc>& b) {
  return !(a < b);
}

template <typename T, typename Alloc>
inline void swap(deque<T, Alloc>& a, deque<T, Alloc>& b) {
  a.swap(b);
}

}  // namespace lstl

#endif  // LSTL_DEQUE_H
