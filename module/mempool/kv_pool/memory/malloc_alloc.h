#ifndef KV_POOL_MALLOC_ALLOC_H
#define KV_POOL_MALLOC_ALLOC_H

#include <cstddef>
#include <cstdlib>
#include <new>

namespace kv {
namespace detail {

struct malloc_alloc {
  static void* allocate(size_t n) {
    if (n == 0) {
      return 0;
    }
    void* p = std::malloc(n);
    if (!p) {
      throw std::bad_alloc();
    }
    return p;
  }

  static void deallocate(void* p, size_t) {
    std::free(p);
  }
};

}  // namespace detail
}  // namespace kv

#endif  // KV_POOL_MALLOC_ALLOC_H
