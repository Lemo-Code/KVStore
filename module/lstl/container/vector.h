#ifndef LSTL_VECTOR_H
#define LSTL_VECTOR_H

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "allocator.h"
#include "allocator_traits.h"
#include "construct.h"
#include "uninitialized.h"

namespace lstl {

template <typename T, typename Alloc = allocator<T> >
class vector {
 public:
  typedef T value_type;
  typedef Alloc allocator_type;
  typedef typename allocator_traits<Alloc>::size_type size_type;
  typedef typename allocator_traits<Alloc>::difference_type difference_type;
  typedef T* iterator;
  typedef const T* const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef T& reference;
  typedef const T& const_reference;
  typedef T* pointer;
  typedef const T* const_pointer;

  vector() : start_(0), finish_(0), end_of_storage_(0), alloc_() {}

  explicit vector(const allocator_type& a) throw()
      : start_(0), finish_(0), end_of_storage_(0), alloc_(a) {}

  explicit vector(size_type n, const T& value = T(), const allocator_type& a = allocator_type())
      : start_(0), finish_(0), end_of_storage_(0), alloc_(a) {
    fill_initialize(n, value);
  }

  template <typename InputIterator,
            typename = typename std::enable_if<
                !std::is_integral<InputIterator>::value>::type>
  vector(InputIterator first, InputIterator last, const allocator_type& a = allocator_type())
      : start_(0), finish_(0), end_of_storage_(0), alloc_(a) {
    range_initialize(first, last);
  }

  vector(std::initializer_list<T> il, const allocator_type& a = allocator_type())
      : start_(0), finish_(0), end_of_storage_(0), alloc_(a) {
    range_initialize(il.begin(), il.end());
  }

  vector(const vector& other)
      : start_(0), finish_(0), end_of_storage_(0),
        alloc_(allocator_traits<Alloc>::select_on_container_copy_construction(
            other.alloc_)) {
    range_initialize(other.start_, other.finish_);
  }

  vector(vector&& other) throw()
      : start_(other.start_),
        finish_(other.finish_),
        end_of_storage_(other.end_of_storage_),
        alloc_(other.alloc_) {
    other.start_ = 0;
    other.finish_ = 0;
    other.end_of_storage_ = 0;
  }

  ~vector() { destroy_and_deallocate(); }

  vector& operator=(const vector& other) {
    if (this != &other) {
      assign(other.begin(), other.end());
    }
    return *this;
  }

  vector& operator=(vector&& other) throw() {
    if (this != &other) {
      destroy_and_deallocate();
      start_ = other.start_;
      finish_ = other.finish_;
      end_of_storage_ = other.end_of_storage_;
      alloc_ = other.alloc_;
      other.start_ = 0;
      other.finish_ = 0;
      other.end_of_storage_ = 0;
    }
    return *this;
  }

  vector& operator=(std::initializer_list<T> il) {
    assign(il.begin(), il.end());
    return *this;
  }

  iterator begin() throw() { return start_; }
  const_iterator begin() const throw() { return start_; }
  const_iterator cbegin() const throw() { return start_; }

  iterator end() throw() { return finish_; }
  const_iterator end() const throw() { return finish_; }
  const_iterator cend() const throw() { return finish_; }

  reverse_iterator rbegin() throw() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const throw() { return const_reverse_iterator(end()); }
  reverse_iterator rend() throw() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const throw() { return const_reverse_iterator(begin()); }

  size_type size() const throw() { return static_cast<size_type>(finish_ - start_); }
  size_type max_size() const throw() {
    return allocator_traits<Alloc>::max_size(alloc_);
  }
  size_type capacity() const throw() {
    return static_cast<size_type>(end_of_storage_ - start_);
  }
  bool empty() const throw() { return start_ == finish_; }

  reference operator[](size_type n) { return start_[n]; }
  const_reference operator[](size_type n) const { return start_[n]; }

  reference at(size_type n) {
    if (n >= size()) {
      throw std::out_of_range("lstl::vector::at");
    }
    return start_[n];
  }

  const_reference at(size_type n) const {
    if (n >= size()) {
      throw std::out_of_range("lstl::vector::at");
    }
    return start_[n];
  }

  reference front() { return *begin(); }
  const_reference front() const { return *begin(); }
  reference back() { return *(end() - 1); }
  const_reference back() const { return *(end() - 1); }

  pointer data() throw() { return start_; }
  const_pointer data() const throw() { return start_; }

  allocator_type get_allocator() const throw() { return alloc_; }

  void reserve(size_type n) {
    if (capacity() < n) {
      reallocate(n);
    }
  }

  void resize(size_type new_size, const T& value = T()) {
    if (new_size < size()) {
      erase(begin() + static_cast<difference_type>(new_size), end());
    } else if (new_size > size()) {
      insert(end(), new_size - size(), value);
    }
  }

  void push_back(const T& value) {
    if (finish_ != end_of_storage_) {
      construct(finish_, value);
      ++finish_;
    } else {
      insert_aux(end(), value);
    }
  }

  void push_back(T&& value) {
    if (finish_ != end_of_storage_) {
      construct(finish_, static_cast<T&&>(value));
      ++finish_;
    } else {
      insert_aux(end(), static_cast<T&&>(value));
    }
  }

  template <typename... Args>
  void emplace_back(Args&&... args) {
    if (finish_ != end_of_storage_) {
      construct(finish_, static_cast<Args&&>(args)...);
      ++finish_;
    } else {
      emplace_back_aux(static_cast<Args&&>(args)...);
    }
  }

  void pop_back() {
    if (!empty()) {
      --finish_;
      destroy(finish_);
    }
  }

  void clear() throw() {
    destroy(start_, finish_);
    finish_ = start_;
  }

  iterator insert(iterator position, const T& value) {
    const size_type offset = static_cast<size_type>(position - begin());
    if (finish_ != end_of_storage_ && position == end()) {
      construct(finish_, value);
      ++finish_;
    } else {
      insert_aux(position, value);
    }
    return begin() + static_cast<difference_type>(offset);
  }

  iterator insert(iterator position, T&& value) {
    const size_type offset = static_cast<size_type>(position - begin());
    if (finish_ != end_of_storage_ && position == end()) {
      construct(finish_, static_cast<T&&>(value));
      ++finish_;
    } else {
      insert_aux(position, static_cast<T&&>(value));
    }
    return begin() + static_cast<difference_type>(offset);
  }

  iterator insert(iterator position, size_type n, const T& value) {
    const size_type offset = static_cast<size_type>(position - begin());
    for (size_type i = 0; i < n; ++i) {
      position = insert(position, value);
      ++position;
    }
    return begin() + static_cast<difference_type>(offset);
  }

  template <typename InputIterator,
            typename = typename std::enable_if<
                !std::is_integral<InputIterator>::value>::type>
  iterator insert(iterator position, InputIterator first, InputIterator last) {
    const size_type offset = static_cast<size_type>(position - begin());
    for (; first != last; ++first) {
      position = insert(position, *first);
      ++position;
    }
    return begin() + static_cast<difference_type>(offset);
  }

  iterator erase(iterator position) {
    if (position + 1 != end()) {
      for (iterator next = position + 1; next != end(); ++position, ++next) {
        *position = *next;
      }
    }
    destroy(finish_ - 1);
    --finish_;
    return position;
  }

  iterator erase(iterator first, iterator last) {
    if (first != last) {
      if (last != end()) {
        iterator dest = first;
        for (iterator src = last; src != end(); ++dest, ++src) {
          *dest = *src;
        }
      }
      destroy(first + (end() - last), end());
      finish_ -= (last - first);
    }
    return first;
  }

  void assign(size_type n, const T& value) {
    clear();
    fill_initialize(n, value);
  }

  template <typename InputIterator,
            typename = typename std::enable_if<
                !std::is_integral<InputIterator>::value>::type>
  void assign(InputIterator first, InputIterator last) {
    clear();
    range_initialize(first, last);
  }

  void assign(std::initializer_list<T> il) { assign(il.begin(), il.end()); }

  void swap(vector& other) throw() {
    std::swap(start_, other.start_);
    std::swap(finish_, other.finish_);
    std::swap(end_of_storage_, other.end_of_storage_);
    std::swap(alloc_, other.alloc_);
  }

 private:
  template <typename Iterator>
  void range_initialize(Iterator first, Iterator last) {
    const size_type n = static_cast<size_type>(last - first);
    if (n == 0) {
      return;
    }
    start_ = alloc_.allocate(n);
    finish_ = uninitialized_copy(first, last, start_);
    end_of_storage_ = finish_;
  }

  void fill_initialize(size_type n, const T& value) {
    start_ = alloc_.allocate(n);
    end_of_storage_ = start_ + n;
    finish_ = uninitialized_fill_n(start_, n, value);
  }

  void destroy_and_deallocate() {
    if (start_) {
      destroy(start_, finish_);
      alloc_.deallocate(start_, capacity());
      start_ = 0;
      finish_ = 0;
      end_of_storage_ = 0;
    }
  }

  void reallocate(size_type new_cap) {
    T* new_start = alloc_.allocate(new_cap);
    const size_type old_size = size();
    if (old_size > 0) {
      finish_ = uninitialized_copy(start_, finish_, new_start);
    } else {
      finish_ = new_start;
    }
    if (start_) {
      destroy(start_, start_ + old_size);
      alloc_.deallocate(start_, capacity());
    }
    start_ = new_start;
    end_of_storage_ = new_start + new_cap;
  }

  size_type new_capacity(size_type min_cap) const {
    size_type new_cap = capacity() > 0 ? capacity() : 1;
    while (new_cap < min_cap) {
      const size_type doubled = new_cap * 2;
      new_cap = doubled > new_cap ? doubled : min_cap;
    }
    return new_cap;
  }

  void insert_aux(iterator position, const T& value) {
    if (size() + 1 > capacity()) {
      const size_type idx = static_cast<size_type>(position - begin());
      reallocate(new_capacity(size() + 1));
      position = begin() + static_cast<difference_type>(idx);
    }
    if (position != finish_) {
      const T copy(value);
      construct(finish_, *(finish_ - 1));
      ++finish_;
      for (iterator it = finish_ - 1; it > position; --it) {
        *it = *(it - 1);
      }
      *position = copy;
    } else {
      construct(finish_, value);
      ++finish_;
    }
  }

  void insert_aux(iterator position, T&& value) {
    if (size() + 1 > capacity()) {
      const size_type idx = static_cast<size_type>(position - begin());
      reallocate(new_capacity(size() + 1));
      position = begin() + static_cast<difference_type>(idx);
    }
    if (position != finish_) {
      construct(finish_, static_cast<T&&>(*(finish_ - 1)));
      ++finish_;
      for (iterator it = finish_ - 1; it > position; --it) {
        *it = static_cast<T&&>(*(it - 1));
      }
      *position = static_cast<T&&>(value);
    } else {
      construct(finish_, static_cast<T&&>(value));
      ++finish_;
    }
  }

  template <typename... Args>
  void emplace_back_aux(Args&&... args) {
    if (size() + 1 > capacity()) {
      reallocate(new_capacity(size() + 1));
    }
    construct(finish_, static_cast<Args&&>(args)...);
    ++finish_;
  }

  T* start_;
  T* finish_;
  T* end_of_storage_;
  Alloc alloc_;
};

template <typename T, typename Alloc>
inline bool operator==(const vector<T, Alloc>& a, const vector<T, Alloc>& b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

template <typename T, typename Alloc>
inline bool operator!=(const vector<T, Alloc>& a, const vector<T, Alloc>& b) {
  return !(a == b);
}

template <typename T, typename Alloc>
inline void swap(vector<T, Alloc>& a, vector<T, Alloc>& b) {
  a.swap(b);
}

}  // namespace lstl

#endif  // LSTL_VECTOR_H
