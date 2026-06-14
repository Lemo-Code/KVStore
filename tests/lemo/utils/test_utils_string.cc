#include "test_common.h"

#include "lemo/utils/string_util.h"

#include <vector>

int main() {
  LEMO_CHECK(lemo::utils::Trim("  abc  ") == "abc");
  LEMO_CHECK(lemo::utils::ToLower("AbC") == "abc");
  LEMO_CHECK(lemo::utils::StartsWith("log.level", "log."));
  std::vector<std::string> parts = lemo::utils::Split("a,b,c", ',');
  LEMO_CHECK(parts.size() == 3);
  LEMO_CHECK(parts[1] == "b");
  std::printf("PASS test_utils_string\n");
  return 0;
}
