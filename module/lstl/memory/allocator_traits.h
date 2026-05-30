#ifndef LSTL_ALLOCATOR_TRAITS_H
#define LSTL_ALLOCATOR_TRAITS_H

#include <cstddef>
#include <new>

#include "construct.h"

namespace lstl {

template <typename Alloc>
struct allocator_traits {
  typedef Alloc allocator_type;
  typedef typename Alloc::value_type value_type;
  typedef typename Alloc::pointer pointer;
  typedef typename Alloc::const_pointer const_pointer;
  typedef typename Alloc::void_pointer void_pointer;
  typedef typename Alloc::const_void_pointer const_void_pointer;
  typedef typename Alloc::difference_type difference_type;
  typedef typename Alloc::size_type size_type;

  template <typename T>
  struct rebind_alloc {
    typedef typename Alloc::template rebind<T>::other other;
  };

  static pointer allocate(Alloc& a, size_type n) { return a.allocate(n); }

  static pointer allocate(Alloc& a, size_type n, const_void_pointer) {
    return a.allocate(n);
  }

  static void deallocate(Alloc& a, pointer p, size_type n) { a.deallocate(p, n); }

  template <typename T, typename V>
  static void construct(Alloc&, T* p, const V& val) {
    lstl::construct(p, val);
  }

  static void destroy(Alloc&, pointer p) { lstl::destroy(p); }

  static size_type max_size(const Alloc& a) throw() { return a.max_size(); }

  static Alloc select_on_container_copy_construction(const Alloc& a) { return a; }
};

// allocator<void> 无 void_pointer 成员时的特化
template <typename T>
struct allocator_traits<allocator<T> > {
  typedef allocator<T> allocator_type;
  typedef T value_type;
  typedef T* pointer;
  typedef const T* const_pointer;
  typedef void* void_pointer;
  typedef const void* const_void_pointer;
  typedef ptrdiff_t difference_type;
  typedef size_t size_type;

  template <typename U>
  struct rebind_alloc {
    typedef allocator<U> other;
  };

  static pointer allocate(allocator_type& a, size_type n) { return a.allocate(n); }

  static pointer allocate(allocator_type& a, size_type n, const_void_pointer) {
    return a.allocate(n);
  }

  static void deallocate(allocator_type& a, pointer p, size_type n) {
    a.deallocate(p, n);
  }

  template <typename U, typename V>
  static void construct(allocator_type&, U* p, const V& val) {
    lstl::construct(p, val);
  }

  static void destroy(allocator_type&, pointer p) { lstl::destroy(p); }

  static size_type max_size(const allocator_type& a) throw() { return a.max_size(); }

  static allocator_type select_on_container_copy_construction(const allocator_type& a) {
    return a;
  }
};

}  // namespace lstl

#endif  // LSTL_ALLOCATOR_TRAITS_H
