#include "test_common.h"

#include <cstring>

#include "construct.h"
#include "uninitialized.h"

namespace {

struct NonPod {
  static int copies;
  int v;

  NonPod() : v(0) {}
  NonPod(int x) : v(x) {}
  NonPod(const NonPod& o) : v(o.v) { ++copies; }
  ~NonPod() {}
};

int NonPod::copies = 0;

}  // namespace

int main() {
  const int src[] = {1, 2, 3, 4, 5};
  int dst[5];

  lstl::uninitialized_copy(src, src + 5, dst);
  LSTL_CHECK(dst[0] == 1 && dst[4] == 5);

  char buf[sizeof(NonPod) * 3];
  NonPod* out = reinterpret_cast<NonPod*>(buf);
  NonPod in[] = {NonPod(10), NonPod(20), NonPod(30)};

  NonPod::copies = 0;
  lstl::uninitialized_copy(in, in + 3, out);
  LSTL_CHECK(NonPod::copies == 3);
  LSTL_CHECK(out[1].v == 20);
  lstl::destroy(out, out + 3);

  int filled[6];
  lstl::uninitialized_fill(filled, filled + 6, 99);
  for (int i = 0; i < 6; ++i) {
    LSTL_CHECK(filled[i] == 99);
  }

  int filled_n[4];
  lstl::uninitialized_fill_n(filled_n, 4, 7);
  LSTL_CHECK(filled_n[3] == 7);

  std::printf("PASS test_uninitialized\n");
  return 0;
}
