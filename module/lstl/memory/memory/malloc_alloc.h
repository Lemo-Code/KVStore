#ifndef LSTL_MEMORY_MALLOC_ALLOC_H
#define LSTL_MEMORY_MALLOC_ALLOC_H

#include <cstddef>
#include <cstdlib>

#include "../config.h"
#include "oom.h"

namespace lstl {

// 一级空间配置器：封装 malloc / free，支持 OOM handler 重试
struct malloc_alloc_t {
  static void* allocate(size_t n) {
    if (n == 0) {
      return 0;
    }
    void* p = ::malloc(n);
    while (!p) {
      void (*handler)() = oom_handler();
      if (!handler) {
        detail::lstl_oom_fail();
      }
      handler();
      p = ::malloc(n);
    }
    return p;
  }

  static void deallocate(void* p, size_t /*n*/) {
    if (p) {
      ::free(p);
    }
  }

  static void (*set_malloc_handler(void (*f)()))() {
    void (*old)() = oom_handler();
    oom_handler() = f;
    return old;
  }

 private:
  static void (*&oom_handler())() {
    static void (*handler)() = 0;
    return handler;
  }
};

}  // namespace lstl

#endif  // LSTL_MEMORY_MALLOC_ALLOC_H
