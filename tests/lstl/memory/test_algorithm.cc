#include "lstl_test_common.h"

#include <cstdio>

#include "algorithm.h"
#include "functional.h"

int main() {
  int arr[] = {5, 1, 4, 2, 8, 3};
  lstl::sort(arr, arr + 6);
  LSTL_CHECK(arr[0] == 1);
  LSTL_CHECK(arr[5] == 8);

  int arr2[] = {9, 7, 7, 3, 1};
  lstl::sort(arr2, arr2 + 5, lstl::greater<int>());
  LSTL_CHECK(arr2[0] == 9);
  LSTL_CHECK(arr2[4] == 1);

  int a[] = {1, 2, 3};
  int b[] = {1, 2, 3};
  int c[] = {1, 2, 4};
  LSTL_CHECK(lstl::equal(a, a + 3, b));
  LSTL_CHECK(!lstl::equal(a, a + 3, c));

  int rev[] = {1, 2, 3, 4};
  lstl::reverse(rev, rev + 4);
  LSTL_CHECK(rev[0] == 4 && rev[3] == 1);

  LSTL_CHECK(lstl::less<int>()(1, 2));
  LSTL_CHECK(lstl::equal_to<int>()(3, 3));
  LSTL_CHECK(lstl::not_equal_to<int>()(1, 2));

  std::printf("PASS test_algorithm\n");
  return 0;
}
