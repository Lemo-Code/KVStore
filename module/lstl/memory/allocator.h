#ifndef LSTL_ALLOCATOR_H
#define LSTL_ALLOCATOR_H

#include <cstddef>
#include <limits>
#include <new>

#include "alloc.h"
#include "construct.h"

namespace lstl {

template <typename T>
class allocator {
 public:
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  typedef T* pointer;
  typedef const T* const_pointer;
  typedef T& reference;
  typedef const T& const_reference;
  typedef T value_type;

  template <typename U>
  struct rebind {
    typedef allocator<U> other;
  };

  allocator() throw() {}
  allocator(const allocator&) throw() {}

  template <typename U>
  allocator(const allocator<U>&) throw() {}

  pointer allocate(size_type n, const void* = 0) {
    if (n == 0) {
      return 0;
    }
    if (n > max_size()) {
      throw std::bad_alloc();
    }
    return static_cast<pointer>(alloc::allocate(n * sizeof(T)));
  }

  void deallocate(pointer p, size_type n) {
    if (p == 0 || n == 0) {
      return;
    }
    alloc::deallocate(p, n * sizeof(T));
  }

  void construct(pointer p, const T& val) { lstl::construct(p, val); }

  void destroy(pointer p) { lstl::destroy(p); }

  size_type max_size() const throw() {
    return std::numeric_limits<size_type>::max() / sizeof(T);
  }
};

template <>
class allocator<void> {
 public:
  typedef void value_type;
  typedef void* pointer;
  typedef const void* const_pointer;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  template <typename U>
  struct rebind {
    typedef allocator<U> other;
  };
};

template <typename T1, typename T2>
inline bool operator==(const allocator<T1>&, const allocator<T2>&) throw() {
  return true;
}

template <typename T1, typename T2>
inline bool operator!=(const allocator<T1>&, const allocator<T2>&) throw() {
  return false;
}

}  // namespace lstl

#endif  // LSTL_ALLOCATOR_H
