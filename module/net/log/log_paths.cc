#include "log/log_paths.h"

#include <cerrno>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>

namespace net {

namespace {

bool IsAbsolutePath(const std::string& path) {
  return !path.empty() && path[0] == '/';
}

}  // namespace

std::string GetLogDir() {
  const char* env = std::getenv("NET_LOG_DIR");
  if (env != nullptr && env[0] != '\0') {
    std::string dir(env);
    if (dir.back() == '/') {
      dir.pop_back();
    }
    return dir;
  }
  return "log";
}

bool EnsureLogDir() {
  const std::string dir = GetLogDir();
  if (dir.empty()) {
    return false;
  }
  if (mkdir(dir.c_str(), 0755) == 0) {
    return true;
  }
  return errno == EEXIST;
}

std::string ResolveLogPath(const std::string& path) {
  if (path.empty()) {
    return path;
  }
  if (IsAbsolutePath(path)) {
    return path;
  }
  EnsureLogDir();
  const std::string dir = GetLogDir();
  if (path.compare(0, dir.size() + 1, dir + "/") == 0) {
    return path;
  }
  if (path.compare(0, 4, "log/") == 0 && dir == "log") {
    return path;
  }
  return dir + "/" + path;
}

}  // namespace net
