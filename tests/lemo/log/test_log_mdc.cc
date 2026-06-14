#include "test_common.h"

#include "lemo/log/mdc.h"

int main() {
  lemo::log::MDC::Clear();
  lemo::log::MDC::Put("uid", "42");
  LEMO_CHECK(lemo::log::MDC::Get("uid") == "42");
  lemo::log::MDC::Remove("uid");
  LEMO_CHECK(lemo::log::MDC::Get("uid").empty());
  std::printf("PASS test_log_mdc\n");
  return 0;
}
