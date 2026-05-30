// SGI STL 单线程二级空间配置器参考实现（__default_alloc_template<false,0>）
// 用于与 lstl::pool_alloc 公平对比，非生产代码。
#ifndef BENCH_SGI_POOL_ALLOC_H
#define BENCH_SGI_POOL_ALLOC_H

#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace bench {
namespace sgi {

struct malloc_alloc {
  static void* allocate(size_t n) {
    void* p = std::malloc(n);
    if (!p) {
      std::abort();
    }
    return p;
  }

  static void deallocate(void* p, size_t) {
    std::free(p);
  }
};

class pool_alloc {
 public:
  enum { ALIGN = 8 };
  enum { MAX_BYTES = 128 };
  enum { NFREELISTS = 16 };

  static void* allocate(size_t n) {
    if (n > static_cast<size_t>(MAX_BYTES)) {
      return malloc_alloc::allocate(n);
    }

    Obj** my_free_list = free_list() + freelist_index(n);
    Obj* result = *my_free_list;
    if (!result) {
      return refill(round_up(n));
    }
    *my_free_list = result->free_list_link;
    return result;
  }

  static void deallocate(void* p, size_t n) {
    if (n > static_cast<size_t>(MAX_BYTES)) {
      malloc_alloc::deallocate(p, n);
      return;
    }
    Obj* q = static_cast<Obj*>(p);
    Obj** my_free_list = free_list() + freelist_index(n);
    q->free_list_link = *my_free_list;
    *my_free_list = q;
  }

 private:
  union Obj {
    Obj* free_list_link;
    char client_data[1];
  };

  static size_t round_up(size_t bytes) {
    return (bytes + static_cast<size_t>(ALIGN) - 1) &
           ~(static_cast<size_t>(ALIGN) - 1);
  }

  static size_t freelist_index(size_t bytes) {
    return ((bytes + static_cast<size_t>(ALIGN) - 1) /
            static_cast<size_t>(ALIGN)) -
           1;
  }

  static Obj** free_list() {
    static Obj* lists[NFREELISTS];
    static bool inited = false;
    if (!inited) {
      std::memset(lists, 0, sizeof(lists));
      inited = true;
    }
    return lists;
  }

  static char*& start_free() {
    static char* p = 0;
    return p;
  }

  static char*& end_free() {
    static char* p = 0;
    return p;
  }

  static size_t& heap_size() {
    static size_t s = 0;
    return s;
  }

  static void* refill(size_t n) {
    int nobjs = 20;
    char* chunk = chunk_alloc(n, nobjs);
    if (nobjs == 1) {
      return chunk;
    }

    Obj** my_free_list = free_list() + freelist_index(n);
    Obj* result = reinterpret_cast<Obj*>(chunk);
    *my_free_list = reinterpret_cast<Obj*>(chunk + n);

    char* cursor = chunk + n;
    for (int i = 1; i < nobjs - 1; ++i) {
      Obj* current = reinterpret_cast<Obj*>(cursor);
      cursor += n;
      Obj* next = reinterpret_cast<Obj*>(cursor);
      current->free_list_link = next;
    }
    reinterpret_cast<Obj*>(cursor)->free_list_link = 0;
    return result;
  }

  static char* chunk_alloc(size_t size, int& nobjs) {
    char*& sfree = start_free();
    char*& efree = end_free();
    size_t& hsize = heap_size();

    const size_t total_bytes = size * static_cast<size_t>(nobjs);
    const size_t bytes_left =
        efree > sfree ? static_cast<size_t>(efree - sfree) : 0;

    if (bytes_left >= total_bytes) {
      char* result = sfree;
      sfree += total_bytes;
      return result;
    }
    if (bytes_left >= size) {
      nobjs = static_cast<int>(bytes_left / size);
      char* result = sfree;
      sfree += size * static_cast<size_t>(nobjs);
      return result;
    }

    size_t bytes_to_get = 2 * total_bytes + round_up(hsize >> 4);
    if (bytes_left > 0) {
      Obj** my_free_list = free_list() + freelist_index(bytes_left);
      reinterpret_cast<Obj*>(sfree)->free_list_link = *my_free_list;
      *my_free_list = reinterpret_cast<Obj*>(sfree);
    }

    sfree = static_cast<char*>(malloc_alloc::allocate(bytes_to_get));
    hsize += bytes_to_get;
    efree = sfree + bytes_to_get;
    return chunk_alloc(size, nobjs);
  }
};

}  // namespace sgi
}  // namespace bench

#endif  // BENCH_SGI_POOL_ALLOC_H
