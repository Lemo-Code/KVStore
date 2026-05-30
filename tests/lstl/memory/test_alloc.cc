#include "lstl_test_common.h"

#include <cstring>

#include "alloc.h"
#include "memory/malloc_alloc.h"

namespace {

struct counting_alloc {
  static size_t alive;

  static void* allocate(size_t n) {
    void* p = lstl::malloc_alloc_t::allocate(n);
    if (p) {
      ++alive;
    }
    return p;
  }

  static void deallocate(void* p, size_t n) {
    if (p) {
      --alive;
    }
    lstl::malloc_alloc_t::deallocate(p, n);
  }
};

size_t counting_alloc::alive = 0;

struct Obj {
  int v;
  explicit Obj(int x = 0) : v(x) {}
};

}  // namespace

int main() {
  void* p = lstl::malloc_alloc_t::allocate(64);
  LSTL_CHECK(p != 0);
  lstl::malloc_alloc_t::deallocate(p, 64);

  LSTL_CHECK(lstl::malloc_alloc_t::allocate(0) == 0);
  lstl::malloc_alloc_t::deallocate(0, 0);

  int* arr = lstl::simple_alloc<int>::allocate(8);
  LSTL_CHECK(arr != 0);
  arr[0] = 42;
  LSTL_CHECK(arr[0] == 42);
  lstl::simple_alloc<int>::deallocate(arr, 8);

  LSTL_CHECK(lstl::simple_alloc<int>::allocate(0) == 0);
  lstl::simple_alloc<int>::deallocate(0, 0);

  Obj* objs = lstl::simple_alloc<Obj, counting_alloc>::allocate(3);
  LSTL_CHECK(objs != 0);
  LSTL_CHECK(counting_alloc::alive == 1);
  lstl::simple_alloc<Obj, counting_alloc>::deallocate(objs, 3);
  LSTL_CHECK(counting_alloc::alive == 0);

  static int oom_calls = 0;
  auto handler = []() { ++oom_calls; };
  void (*old)() = lstl::malloc_alloc_t::set_malloc_handler(handler);
  LSTL_CHECK(old == 0);
  lstl::malloc_alloc_t::set_malloc_handler(0);

  std::printf("PASS test_alloc\n");
  return 0;
}
