#include "ledis/app/env.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <unistd.h>

namespace ledis {

bool Env::init(int argc, char** argv) {
  char link[1024] = {0};
  char path[1024] = {0};
  std::snprintf(link, sizeof(link), "/proc/%d/exe", getpid());
  if (readlink(link, path, sizeof(path) - 1) == -1) {
    exe_.clear();
    cwd_.clear();
  } else {
    exe_ = path;
    const Size pos = exe_.find_last_of('/');
    cwd_ = (pos == String::npos) ? "./" : exe_.substr(0, pos + 1);
  }

  if (argc <= 0 || argv == nullptr) {
    return false;
  }
  program_ = argv[0];

  const char* pending_key = nullptr;
  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (arg[0] != '-') {
      if (pending_key) {
        add(pending_key, arg);
        pending_key = nullptr;
      }
      continue;
    }

    if (arg[1] == '-' && arg[2] != '\0') {
      pending_key = nullptr;
      continue;
    }

    if (std::strlen(arg) <= 1) {
      std::cerr << "invalid arg idx=" << i << " val=" << arg << std::endl;
      return false;
    }

    if (pending_key) {
      add(pending_key, "");
    }
    pending_key = arg + 1;
  }
  if (pending_key) {
    add(pending_key, "");
  }
  return true;
}

void Env::add(const String& key, const String& val) {
  args_[key] = val;
}

void Env::addHelp(const String& key, const String& desc) {
  for (auto it = helps_.begin(); it != helps_.end();) {
    if (it->first == key) {
      it = helps_.erase(it);
    } else {
      ++it;
    }
  }
  helps_.push_back(std::make_pair(key, desc));
}

void Env::printManual() const {
  std::cout
      << "================================================================\n"
      << " Ledis Server — Redis 兼容内存 KV 服务 (KVStore)\n"
      << " 配置详解: docs/ledis/configuration.md\n"
      << " 功能说明: docs/ledis/features.md\n"
      << "================================================================\n\n"
      << "Usage: " << program_ << " [host] [port] [options]\n\n"
      << "Config load order (later overrides earlier):\n"
      << "  1. defaults  2. YAML (--config / -c / conf/ledis.yaml)\n"
      << "  3. CLI flags and positional [host] [port]\n\n"
      << "Run mode:\n";
  for (const auto& item : helps_) {
    std::cout << "  -" << std::setw(4) << std::left << item.first << " "
              << item.second << "\n";
  }
  std::cout << "\nServer options (also available as --long-option):\n"
            << "  --config PATH          YAML config file\n"
            << "  --async                IO/DB async mode\n"
            << "  --io-threads N         IO worker count\n"
            << "  --maxclients N         max TCP connections\n"
            << "  --maxmemory N          max memory bytes (0=unlimited)\n"
            << "  --maxmemory-policy P   allkeys-lru | volatile-lru | ...\n"
            << "  --requirepass PASS     AUTH password\n"
            << "  --dir PATH             RDB/AOF working directory\n"
            << "  --dbfilename NAME      RDB snapshot filename\n"
            << "  --appendonly           enable AOF\n"
            << "  --appendfilename NAME  AOF filename\n"
            << "  --appendfsync POLICY   always | everysec | no\n"
            << "  --pidfile PATH         pid file (default: <dir>/ledis.pid)\n"
            << "\nTips:\n"
            << "  -p / -h               print this manual and exit\n"
            << "  redis-cli -p PORT     connect after server is ready\n"
            << "================================================================\n"
            << std::flush;
}

bool Env::has(const String& key) const {
  return args_.find(key) != args_.end();
}

String Env::get(const String& key, const String& default_value) const {
  const auto it = args_.find(key);
  return it != args_.end() ? it->second : default_value;
}

String Env::absolutePath(const String& path) const {
  if (path.empty()) {
    return "/";
  }
  if (path[0] == '/') {
    return path;
  }
  return cwd_ + path;
}

String Env::configPath() const {
  return absolutePath(get("c", "conf"));
}

}  // namespace ledis
