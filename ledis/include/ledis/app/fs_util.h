#pragma once

#include "ledis/types.h"

namespace ledis {

/** 进程启动相关的轻量文件系统工具。 */
class FsUtil {
 public:
  static bool mkdirRecursive(const String& path);
  static bool isRunningPidfile(const String& pidfile);
  static bool writePidfile(const String& pidfile);
  static void removePidfile(const String& pidfile);
};

}  // namespace ledis
