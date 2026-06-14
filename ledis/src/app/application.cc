#include "ledis/app/application.h"

#include "ledis/app/daemon.h"
#include "ledis/app/fs_util.h"
#include "ledis/config/ledis_yaml_config.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <unistd.h>

namespace ledis {

Application* Application::s_instance = nullptr;

namespace {

volatile sig_atomic_t g_stop_requested = 0;

void onStopSignal(int) { g_stop_requested = 1; }

void loadYamlBaseConfig(const Env& env, int argc, char** argv,
                        LedisSettings* settings) {
  if (settings == nullptr) {
    return;
  }

  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], "--config") == 0) {
      settings->config_file = argv[i + 1];
      (void)LoadLedisSettingsFromYamlFile(settings->config_file, settings);
      return;
    }
  }

  if (env.has("c")) {
    const String conf = env.get("c");
    if (conf.find(".yaml") != String::npos ||
        conf.find(".yml") != String::npos) {
      settings->config_file = env.absolutePath(conf);
    } else {
      settings->config_file = env.absolutePath(conf + "/ledis.yaml");
    }
    (void)LoadLedisSettingsFromYamlFile(settings->config_file, settings);
    return;
  }

  const String default_yaml = env.absolutePath("conf/ledis.yaml");
  if (LoadLedisSettingsFromYamlFile(default_yaml, settings)) {
    settings->config_file = default_yaml;
  }
}

}  // namespace

Application::Application() { s_instance = this; }

Application::~Application() {
  if (s_instance == this) {
    s_instance = nullptr;
  }
}

void Application::installStopSignals() {
  struct sigaction sa {};
  sa.sa_handler = onStopSignal;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
  signal(SIGPIPE, SIG_IGN);
}

void Application::waitForStop() {
  while (!g_stop_requested) {
    sleep(1);
  }
}

String Application::pidfilePath() const {
  if (!pidfile_.empty()) {
    return pidfile_;
  }
  String path = settings_.dir;
  if (!path.empty() && path.back() != '/') {
    path += '/';
  }
  return path + "ledis.pid";
}

bool Application::init(int argc, char** argv) {
  argc_ = argc;
  argv_ = argv;
  g_stop_requested = 0;

  env_.addHelp("s", "run in foreground (default when -d is not set)");
  env_.addHelp("d", "run as daemon (auto-restart on crash)");
  env_.addHelp("c", "config directory or YAML file (default: ./conf)");
  env_.addHelp("p", "print help and exit");
  env_.addHelp("h", "print help and exit");

  if (!env_.init(argc, argv)) {
    help_requested_ = true;
    env_.printHelp();
    return false;
  }

  if (env_.has("p") || env_.has("h")) {
    help_requested_ = true;
    env_.printHelp();
    return false;
  }

  settings_ = LedisSettings();
  loadYamlBaseConfig(env_, argc, argv, &settings_);
  ApplyLedisCliOverrides(argc, argv, &settings_);

  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], "--pidfile") == 0) {
      pidfile_ = argv[i + 1];
      break;
    }
  }

  daemon_mode_ = env_.has("d");
  if (env_.has("s") && env_.has("d")) {
    std::fprintf(stderr, "ledis-server: -s and -d are mutually exclusive\n");
    exit_code_ = 1;
    return false;
  }

  if (!FsUtil::mkdirRecursive(settings_.dir)) {
    std::fprintf(stderr, "ledis-server: failed to create dir %s: %s\n",
                 settings_.dir.c_str(), std::strerror(errno));
    exit_code_ = 1;
    return false;
  }

  const String pidfile = pidfilePath();
  if (FsUtil::isRunningPidfile(pidfile)) {
    std::fprintf(stderr, "ledis-server: already running (pidfile %s)\n",
                 pidfile.c_str());
    exit_code_ = 1;
    return false;
  }

  return true;
}

int Application::run() {
  return startDaemon(
      argc_, argv_,
      [this](int argc, char** argv) { return this->main(argc, argv); },
      daemon_mode_);
}

int Application::main(int, char**) {
  env_.printManual();
  std::cout << std::flush;
  std::printf("\n");

  installStopSignals();

  if (chdir(settings_.dir.c_str()) != 0) {
    std::fprintf(stderr, "ledis-server: warning: chdir(%s) failed: %s\n",
                 settings_.dir.c_str(), std::strerror(errno));
  }

  const String pidfile = pidfilePath();
  if (!FsUtil::writePidfile(pidfile)) {
    std::fprintf(stderr, "ledis-server: failed to write pidfile %s\n",
                 pidfile.c_str());
    return 1;
  }

  server_.reset(new LedisServer(settings_));
  if (!server_->start()) {
    std::fprintf(stderr, "ledis-server: failed to bind %s:%u\n",
                 settings_.host.c_str(), settings_.port);
    FsUtil::removePidfile(pidfile);
    return 1;
  }

  const String memory_desc =
      settings_.maxmemory == 0
          ? String("unlimited")
          : (std::to_string(settings_.maxmemory) + " bytes");

  std::printf(
      "ledis-server ready\n"
      "  listen   %s:%u\n"
      "  mode     %s\n"
      "  io       %u thread(s)\n"
      "  clients  max %zu\n"
      "  memory   %s\n"
      "  persist  rdb=%s/%s aof=%s\n"
      "  config   %s\n"
      "  pid      %d (%s)\n",
      settings_.host.c_str(), server_->boundPort(),
      settings_.single_thread_mode ? "single-thread" : "io/db-async",
      settings_.io_threads, settings_.maxclients, memory_desc.c_str(),
      settings_.dir.c_str(), settings_.dbfilename.c_str(),
      settings_.appendonly ? "on" : "off",
      settings_.config_file.empty() ? "(defaults)" : settings_.config_file.c_str(),
      static_cast<int>(getpid()), pidfile.c_str());

  waitForStop();

  std::printf("ledis-server shutting down...\n");
  server_->stop();
  server_.reset();
  FsUtil::removePidfile(pidfile);
  return 0;
}

}  // namespace ledis
