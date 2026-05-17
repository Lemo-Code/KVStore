#include "test_common.h"

#include "allocator.h"
#include "allocator_traits.h"
#include "construct.h"

namespace {

struct Widget {
  int id;
  explicit Widget(int x = 0) : id(x) {}
};

}  // namespace

int main() {
  lstl::allocator<int> int_alloc;
  int* p = int_alloc.allocate(4);
  LSTL_CHECK(p != 0);
  int_alloc.construct(p, 100);
  LSTL_CHECK(*p == 100);
  int_alloc.destroy(p);
  int_alloc.deallocate(p, 4);

  lstl::allocator<Widget> w_alloc;
  Widget* w = w_alloc.allocate(2);
  w_alloc.construct(w, Widget(42));
  w_alloc.construct(w + 1, Widget(43));
  LSTL_CHECK(w[0].id == 42 && w[1].id == 43);
  w_alloc.destroy(w);
  w_alloc.destroy(w + 1);
  w_alloc.deallocate(w, 2);

  typedef lstl::allocator<int>::rebind<double>::other double_alloc_type;
  lstl::allocator<double> d_alloc;
  (void)double_alloc_type();
  double* dp = d_alloc.allocate(1);
  d_alloc.construct(dp, 3.14);
  LSTL_CHECK(*dp > 3.0);
  d_alloc.destroy(dp);
  d_alloc.deallocate(dp, 1);

  lstl::allocator<int> a1;
  lstl::allocator<int> a2;
  LSTL_CHECK(a1 == a2);
  LSTL_CHECK(!(a1 != a2));

  lstl::allocator<char> c_alloc;
  char* buf = lstl::allocator_traits<lstl::allocator<char> >::allocate(c_alloc, 16);
  LSTL_CHECK(buf != 0);
  lstl::allocator_traits<lstl::allocator<char> >::deallocate(c_alloc, buf, 16);

  std::printf("PASS test_allocator\n");
  return 0;
}
