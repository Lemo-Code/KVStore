#pragma once

#include "ledis/types.h"

#include <cstdint>
#include <functional>

namespace ledis {

struct ProcessInfo {
  pid_t parent_id = 0;
  pid_t main_id = 0;
  uint64_t parent_start_time = 0;
  uint64_t main_start_time = 0;
  uint32_t restart_count = 0;
};

using MainCallback = Function<int(int argc, char** argv)>;

/** 前台直接执行 main_cb；守护进程模式下 fork + 异常退出自动重启。 */
int startDaemon(int argc, char** argv, MainCallback main_cb, bool is_daemon,
                uint32_t restart_interval_sec = 5);

ProcessInfo& processInfo();

}  // namespace ledis
