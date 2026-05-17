#ifndef LSTL_ALLOC_H
#define LSTL_ALLOC_H

#include <cstddef>
#include <new>

#include "config.h"
#include "memory/malloc_alloc.h"
#include "memory/pool.h"

namespace lstl {

#ifdef LSTL_USER_ALLOC
typedef LSTL_USER_ALLOC alloc;
#elif LSTL_USE_MALLOC_ALLOC
typedef malloc_alloc_t alloc;
#else
typedef pool_alloc_t alloc;
#endif

// 二级封装：按元素个数分配/释放，字节数委托给策略 Alloc
template <typename T, typename AllocPolicy = alloc>
struct simple_alloc {
  typedef T value_type;
  typedef size_t size_type;

  static T* allocate(size_type n) {
    if (n == 0) {
      return 0;
    }
    return static_cast<T*>(AllocPolicy::allocate(n * sizeof(T)));
  }

  static void deallocate(T* p, size_type n) {
    if (p == 0 || n == 0) {
      return;
    }
    AllocPolicy::deallocate(p, n * sizeof(T));
  }
};

}  // namespace lstl

#endif  // LSTL_ALLOC_H
