#include "ledis/app/fs_util.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

namespace ledis {
namespace {

bool pathExists(const String& path) {
  struct stat st {};
  return lstat(path.c_str(), &st) == 0;
}

String parentPath(const String& path) {
  const Size pos = path.find_last_of('/');
  if (pos == String::npos) {
    return String();
  }
  if (pos == 0) {
    return "/";
  }
  return path.substr(0, pos);
}

}  // namespace

bool FsUtil::mkdirRecursive(const String& dirname) {
  if (dirname.empty()) {
    return false;
  }
  if (pathExists(dirname)) {
    struct stat st {};
    if (lstat(dirname.c_str(), &st) != 0) {
      return false;
    }
    return S_ISDIR(st.st_mode);
  }

  const String parent = parentPath(dirname);
  if (!parent.empty() && parent != dirname &&
      !mkdirRecursive(parent)) {
    return false;
  }

  if (mkdir(dirname.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) !=
          0 &&
      errno != EEXIST) {
    return false;
  }
  return true;
}

bool FsUtil::isRunningPidfile(const String& pidfile) {
  struct stat st {};
  if (lstat(pidfile.c_str(), &st) != 0) {
    return false;
  }

  std::ifstream ifs(pidfile.c_str());
  String line;
  if (!ifs || !std::getline(ifs, line) || line.empty()) {
    return false;
  }

  const pid_t pid = static_cast<pid_t>(std::atoi(line.c_str()));
  if (pid <= 1) {
    return false;
  }
  return kill(pid, 0) == 0;
}

bool FsUtil::writePidfile(const String& pidfile) {
  const String parent = parentPath(pidfile);
  if (!parent.empty() && !mkdirRecursive(parent)) {
    return false;
  }

  std::ofstream ofs(pidfile.c_str(), std::ios::trunc);
  if (!ofs) {
    return false;
  }
  ofs << getpid();
  return static_cast<bool>(ofs);
}

void FsUtil::removePidfile(const String& pidfile) {
  if (pidfile.empty()) {
    return;
  }
  (void)unlink(pidfile.c_str());
}

}  // namespace ledis
