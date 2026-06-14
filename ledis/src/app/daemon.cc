#include "ledis/app/daemon.h"

#include <cerrno>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <sys/wait.h>

namespace ledis {
namespace {

ProcessInfo g_process_info;

int realStart(int argc, char** argv, MainCallback main_cb) {
  return main_cb(argc, argv);
}

int realDaemon(int argc, char** argv, MainCallback main_cb,
               uint32_t restart_interval_sec) {
  if (daemon(1, 0) != 0) {
    std::fprintf(stderr, "ledis-server: daemon(3) failed: %s\n",
                 std::strerror(errno));
    return 1;
  }

  g_process_info.parent_id = getpid();
  g_process_info.parent_start_time = static_cast<uint64_t>(time(nullptr));

  while (true) {
    const pid_t pid = fork();
    if (pid == 0) {
      g_process_info.main_id = getpid();
      g_process_info.main_start_time = static_cast<uint64_t>(time(nullptr));
      return realStart(argc, argv, main_cb);
    }
    if (pid < 0) {
      std::fprintf(stderr, "ledis-server: fork failed: %s\n",
                   std::strerror(errno));
      return 1;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
      std::fprintf(stderr, "ledis-server: waitpid failed: %s\n",
                   std::strerror(errno));
      return 1;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
      break;
    }

    g_process_info.restart_count += 1;
    std::fprintf(stderr,
                 "ledis-server: worker exited abnormally (status=0x%x), "
                 "restart in %u sec (count=%u)\n",
                 status, restart_interval_sec, g_process_info.restart_count);
    sleep(restart_interval_sec > 0 ? restart_interval_sec : 1);
  }
  return 0;
}

}  // namespace

int startDaemon(int argc, char** argv, MainCallback main_cb, bool is_daemon,
                uint32_t restart_interval_sec) {
  if (!is_daemon) {
    return realStart(argc, argv, main_cb);
  }
  return realDaemon(argc, argv, main_cb, restart_interval_sec);
}

ProcessInfo& processInfo() { return g_process_info; }

}  // namespace ledis
