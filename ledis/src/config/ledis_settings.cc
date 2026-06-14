#include "ledis/config/ledis_settings.h"

#include "ledis/config/ledis_yaml_config.h"
#include "ledis/store/eviction.h"

#include <cstdlib>
#include <cstring>

namespace ledis {

namespace {

bool shortOptionTakesValue(char opt) { return opt == 'c'; }

bool longOptionTakesValue(const char* arg) {
  return std::strcmp(arg, "--io-threads") == 0 ||
         std::strcmp(arg, "--maxclients") == 0 ||
         std::strcmp(arg, "--maxmemory") == 0 ||
         std::strcmp(arg, "--maxmemory-policy") == 0 ||
         std::strcmp(arg, "--requirepass") == 0 || std::strcmp(arg, "--dir") == 0 ||
         std::strcmp(arg, "--dbfilename") == 0 ||
         std::strcmp(arg, "--appendfilename") == 0 ||
         std::strcmp(arg, "--appendfsync") == 0 ||
         std::strcmp(arg, "--config") == 0 || std::strcmp(arg, "--pidfile") == 0;
}

}  // namespace

void ApplyLedisCliOverrides(int argc, char** argv, LedisSettings* settings) {
  if (settings == nullptr) {
    return;
  }

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--config") == 0) {
      ++i;
      continue;
    }
    if (std::strcmp(argv[i], "--async") == 0) {
      settings->single_thread_mode = false;
      continue;
    }
    if (std::strcmp(argv[i], "--io-threads") == 0 && i + 1 < argc) {
      settings->io_threads =
          static_cast<uint32_t>(std::atoi(argv[++i]));
      continue;
    }
    if (std::strcmp(argv[i], "--maxclients") == 0 && i + 1 < argc) {
      settings->maxclients = static_cast<size_t>(std::strtoul(argv[++i], nullptr, 10));
      continue;
    }
    if (std::strcmp(argv[i], "--maxmemory") == 0 && i + 1 < argc) {
      settings->maxmemory = static_cast<size_t>(std::strtoull(argv[++i], nullptr, 10));
      continue;
    }
    if (std::strcmp(argv[i], "--maxmemory-policy") == 0 && i + 1 < argc) {
      bool ok = false;
      const MaxmemoryPolicy policy = parseMaxmemoryPolicy(argv[++i], &ok);
      if (ok) {
        settings->maxmemory_policy = maxmemoryPolicyName(policy);
      }
      continue;
    }
    if (std::strcmp(argv[i], "--requirepass") == 0 && i + 1 < argc) {
      settings->requirepass = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
      settings->dir = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--dbfilename") == 0 && i + 1 < argc) {
      settings->dbfilename = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--appendonly") == 0) {
      settings->appendonly = true;
      continue;
    }
    if (std::strcmp(argv[i], "--appendfilename") == 0 && i + 1 < argc) {
      settings->appendfilename = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--appendfsync") == 0 && i + 1 < argc) {
      settings->appendfsync = argv[++i];
      continue;
    }
  }

  bool positional_done = false;
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      if (argv[i][1] == '-' && argv[i][2] != '\0') {
        if (longOptionTakesValue(argv[i]) && i + 1 < argc &&
            argv[i + 1][0] != '-') {
          ++i;
        }
        continue;
      }
      if (argv[i][1] != '\0' && argv[i][2] == '\0') {
        if (shortOptionTakesValue(argv[i][1]) && i + 1 < argc &&
            argv[i + 1][0] != '-') {
          ++i;
        }
        continue;
      }
      continue;
    }
    if (!positional_done) {
      settings->host = argv[i];
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        settings->port = static_cast<uint16_t>(std::atoi(argv[i + 1]));
      }
      positional_done = true;
      break;
    }
  }

  if (settings->io_threads == 0) {
    settings->io_threads = 1;
  }
}

LedisSettings ParseLedisSettingsFromArgs(int argc, char** argv) {
  LedisSettings settings;
  String config_path;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
      config_path = argv[++i];
      continue;
    }
  }
  if (!config_path.empty()) {
    settings.config_file = config_path;
    (void)LoadLedisSettingsFromYamlFile(config_path, &settings);
  }
  ApplyLedisCliOverrides(argc, argv, &settings);
  return settings;
}

}  // namespace ledis