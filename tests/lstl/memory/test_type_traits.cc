#include "lstl_test_common.h"

#include "type_traits.h"

int main() {
  typedef lstl::__type_traits<int>::this_type_is_POD_type int_pod;
  typedef lstl::__type_traits<lstl::__false_type>::this_type_is_POD_type false_pod;

  (void)sizeof(int_pod);
  (void)sizeof(false_pod);

  LSTL_CHECK(sizeof(lstl::__true_type) == 1);
  LSTL_CHECK(sizeof(lstl::__false_type) == 1);

  std::printf("PASS test_type_traits\n");
  return 0;
}
