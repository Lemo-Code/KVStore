#pragma once

#include "ledis/app/env.h"
#include "ledis/config/ledis_settings.h"
#include "ledis/server/ledis_server.h"
#include "ledis/types.h"

#include <csignal>

namespace ledis {

/** L5 应用入口：Env 解析 → 配置加载 → 守护进程/前台 → 优雅退出。 */
class Application {
 public:
  Application();
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  bool init(int argc, char** argv);
  int run();

  bool helpRequested() const { return help_requested_; }
  int exitCode() const { return exit_code_; }

  static Application* instance() { return s_instance; }

 private:
  int main(int argc, char** argv);
  void installStopSignals();
  void waitForStop();
  String pidfilePath() const;

  Env env_;
  LedisSettings settings_;
  UniquePtr<LedisServer> server_;

  int argc_ = 0;
  char** argv_ = nullptr;
  bool help_requested_ = false;
  bool daemon_mode_ = false;
  int exit_code_ = 0;
  String pidfile_;

  static Application* s_instance;
};

}  // namespace ledis
