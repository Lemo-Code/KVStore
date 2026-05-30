#include "lstl_test_common.h"

#include "construct.h"

namespace {

struct Tracked {
  static int constructed;
  static int destroyed;

  int v;

  Tracked() : v(0) { ++constructed; }
  explicit Tracked(int x) : v(x) { ++constructed; }
  ~Tracked() { ++destroyed; }
};

int Tracked::constructed = 0;
int Tracked::destroyed = 0;

}  // namespace

int main() {
  char buf[sizeof(Tracked) * 4];

  Tracked* p = reinterpret_cast<Tracked*>(buf);
  lstl::construct(p, 7);
  LSTL_CHECK(p->v == 7);
  LSTL_CHECK(Tracked::constructed == 1);

  lstl::construct(p + 1);
  LSTL_CHECK((p + 1)->v == 0);
  LSTL_CHECK(Tracked::constructed == 2);

  lstl::destroy(p);
  LSTL_CHECK(Tracked::destroyed == 1);

  lstl::destroy(p + 1, p + 2);
  LSTL_CHECK(Tracked::destroyed == 2);

  int arr[4] = {1, 2, 3, 4};
  lstl::destroy(arr, arr + 4);
  LSTL_CHECK(arr[0] == 1);

  std::printf("PASS test_construct\n");
  return 0;
}
