#include "lstl_test_common.h"

#include "memory/malloc_alloc.h"

namespace {

int handler_invoked = 0;

void test_handler() { ++handler_invoked; }

}  // namespace

int main() {
  void* p = lstl::malloc_alloc_t::allocate(32);
  LSTL_CHECK(p != 0);
  lstl::malloc_alloc_t::deallocate(p, 32);

  void (*prev)() = lstl::malloc_alloc_t::set_malloc_handler(test_handler);
  LSTL_CHECK(prev == 0);

  void* q = lstl::malloc_alloc_t::allocate(16);
  LSTL_CHECK(q != 0);
  lstl::malloc_alloc_t::deallocate(q, 16);

  lstl::malloc_alloc_t::set_malloc_handler(0);
  LSTL_CHECK(handler_invoked == 0);

#ifdef LSTL_OOM_MODE_CERR
  std::printf("OOM mode: cerr+abort\n");
#else
  std::printf("OOM mode: throw bad_alloc\n");
#endif

  std::printf("PASS test_oom_policy\n");
  return 0;
}
