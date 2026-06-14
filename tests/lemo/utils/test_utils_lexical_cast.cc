#include "test_common.h"

#include "lemo/utils/lexical_cast.h"

int main() {
  using lemo::utils::LexicalCast;
  LEMO_CHECK((LexicalCast<std::string, int>()("42") == 42));
  LEMO_CHECK((LexicalCast<int, std::string>()(7) == "7"));
  LEMO_CHECK((LexicalCast<std::string, bool>()("true") == true));
  LEMO_CHECK((LexicalCast<std::string, bool>()("false") == false));
  std::printf("PASS test_utils_lexical_cast\n");
  return 0;
}
