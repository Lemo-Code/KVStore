#include "lstl_test_common.h"

#include "temporary_buffer.h"

int main() {
  lstl::pair<int*, ptrdiff_t> buf = lstl::get_temporary_buffer<int>(16);
  LSTL_CHECK(buf.first != 0);
  LSTL_CHECK(buf.second == 16);

  buf.first[0] = 123;
  buf.first[15] = 456;
  LSTL_CHECK(buf.first[0] == 123);
  LSTL_CHECK(buf.first[15] == 456);

  lstl::return_temporary_buffer(buf.first);

  lstl::pair<int*, ptrdiff_t> empty = lstl::get_temporary_buffer<int>(0);
  LSTL_CHECK(empty.first == 0);
  LSTL_CHECK(empty.second == 0);

  std::printf("PASS test_temporary_buffer\n");
  return 0;
}
