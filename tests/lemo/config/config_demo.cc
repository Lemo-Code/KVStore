#include "lemo/config/config.h"
#include "lemo/config/log_config.h"

#include <cstdio>
#include <cstdlib>

#ifndef LEMO_DEFAULT_CONF
#define LEMO_DEFAULT_CONF "tests/lemo/config/fixtures/lemo_test.conf"
#endif

int main(int argc, char* argv[]) {
  const char* path = (argc > 1) ? argv[1] : LEMO_DEFAULT_CONF;

  if (argc > 1 && (std::string(argv[1]) == "-h" ||
                   std::string(argv[1]) == "--help")) {
    lemo::config::ConfigPrinter::PrintHelp();
    return 0;
  }

  lemo::config::ConfigCenter::Clear();
  const bool ok = lemo::config::LoadLogConfigFileVerbose(path, stdout);
  return ok ? 0 : 1;
}
