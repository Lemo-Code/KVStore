#include "lemo/config/log_config.h"

#include "lemo/config/config_center.h"
#include "lemo/config/config_printer.h"
#include "lemo/config/property_loader.h"
#include "lemo/log/appender_registry.h"
#include "lemo/log/level.h"
#include "lemo/log/log_runtime.h"
#include "lemo/log/logger_repository.h"
#include "lemo/log/pattern_layout.h"
#include "lemo/thread/thread.h"
#include "lemo/utils/string_util.h"

#include <cstdio>
#include <map>
#include <set>
#include <string>

namespace lemo {
namespace config {
namespace {

const char* kPrefix = "log.logger.";

log::AppenderConfig BuildAppenderConfig(const std::string& logger_name) {
  log::AppenderConfig cfg;
  cfg.properties["logger_name"] = logger_name;
  cfg.properties["path"] =
      ConfigCenter::Lookup<std::string>("log.file.path", "lemo.log")->GetValue();
  cfg.properties["max_bytes"] = ConfigCenter::Lookup<std::string>(
                                    "log.file.max_bytes", "104857600")
                                    ->GetValue();
  cfg.properties["max_files"] =
      ConfigCenter::Lookup<std::string>("log.file.max_files", "10")->GetValue();
  cfg.properties["roll_interval"] =
      ConfigCenter::Lookup<std::string>("log.file.roll_interval", "day")
          ->GetValue();
  cfg.properties["delegate"] =
      ConfigCenter::Lookup<std::string>("log.async.delegate", "file")->GetValue();
  cfg.properties["target"] =
      ConfigCenter::Lookup<std::string>("log.console.target", "stdout")->GetValue();
  return cfg;
}

void ApplyRootLogger() {
  log::LoggerRepository& repo = log::LoggerRepository::Instance();
  log::Logger::ptr root = repo.GetRoot();
  if (!root) return;

  const std::string level_str =
      ConfigCenter::Lookup<std::string>("log.level", "DEBUG")->GetValue();
  root->SetLevel(log::LogLevel::FromString(level_str));

  const std::string pattern =
      ConfigCenter::Lookup<std::string>("log.pattern", "%m")->GetValue();
  root->SetLayout(log::Layout::ptr(new log::PatternLayout(pattern)));

  root->ClearAppenders();
  const std::string appender_type =
      ConfigCenter::Lookup<std::string>("log.appender", "console")->GetValue();
  log::AppenderConfig cfg = BuildAppenderConfig("root");
  log::Appender::ptr appender =
      log::AppenderRegistry::Instance().Create(appender_type, cfg);
  if (appender) root->AddAppender(appender);
}

void ApplyNamedLoggers() {
  std::set<std::string> names;
  ConfigCenter::Visit([&names](ConfigVarBase::ptr var) {
    if (!var) return;
    const std::string key = var->GetName();
    if (!utils::StartsWith(key, kPrefix)) return;
    const std::string suffix = key.substr(std::string(kPrefix).size());
    const size_t dot = suffix.rfind('.');
    if (dot == std::string::npos) return;
    names.insert(suffix.substr(0, dot));
  });

  log::LoggerRepository& repo = log::LoggerRepository::Instance();
  for (std::set<std::string>::const_iterator it = names.begin();
       it != names.end(); ++it) {
    log::Logger::ptr logger = repo.GetLogger(*it);
    if (!logger) continue;

    const std::string level_key = std::string(kPrefix) + *it + ".level";
    ConfigVar<std::string>::ptr level_var =
        ConfigCenter::Lookup<std::string>(level_key);
    if (level_var) {
      logger->SetLevel(log::LogLevel::FromString(level_var->GetValue()));
    }

    const std::string additive_key = std::string(kPrefix) + *it + ".additive";
    ConfigVar<std::string>::ptr additive_var =
        ConfigCenter::Lookup<std::string>(additive_key);
    if (additive_var) {
      logger->SetAdditive(
          utils::LexicalCast<std::string, bool>()(additive_var->GetValue()));
    }
  }
}

void RegisterDynamicLogKeys(const std::map<std::string, std::string>& kv) {
  for (std::map<std::string, std::string>::const_iterator it = kv.begin();
       it != kv.end(); ++it) {
    if (utils::StartsWith(it->first, kPrefix)) {
      ConfigCenter::Lookup<std::string>(it->first, "");
    }
  }
}

bool ApplyConfigMap(const std::map<std::string, std::string>& kv,
                    LoadStats* stats) {
  InitLogConfigVars();
  RegisterDynamicLogKeys(kv);
  if (stats) stats->parsed_keys = kv.size();
  return ConfigCenter::LoadFlatMap(kv, stats);
}

void ApplyLogRuntimeSettings() {
  ConfigVar<std::string>::ptr worker_name =
      ConfigCenter::Lookup<std::string>("log.async.worker_name");
  if (worker_name) {
    log::LogRuntime::SetAsyncWorkerName(worker_name->GetValue());
  }

  ConfigVar<std::string>::ptr main_name =
      ConfigCenter::Lookup<std::string>("log.thread.main_name");
  if (main_name && !main_name->GetValue().empty()) {
    thread::Thread::SetName(main_name->GetValue());
  }
}

void FinishLogConfigLoad(const std::string& source, const LoadStats& stats) {
  ApplyLogRuntimeSettings();
  ApplyRootLogger();
  ApplyNamedLoggers();
  ConfigPrinter::ReportLoadComplete(source, stats, stdout);
}

}  // namespace

void InitLogConfigVars() {
  ConfigCenter::Lookup<std::string>("log.level", "DEBUG", "root logger level");
  ConfigCenter::Lookup<std::string>("log.pattern", "%m", "root log pattern");
  ConfigCenter::Lookup<std::string>("log.appender", "console", "root appender type");
  ConfigCenter::Lookup<std::string>("log.file.path", "lemo.log", "file appender path");
  ConfigCenter::Lookup<std::string>("log.file.max_bytes", "104857600",
                                     "rolling max bytes");
  ConfigCenter::Lookup<std::string>("log.file.max_files", "10",
                                     "rolling max files");
  ConfigCenter::Lookup<std::string>("log.file.roll_interval", "day",
                                     "rolling interval");
  ConfigCenter::Lookup<std::string>("log.async.delegate", "file",
                                     "async inner appender");
  ConfigCenter::Lookup<std::string>("log.async.worker_name", "lemo-log-async",
                                     "async dispatcher worker thread name");
  ConfigCenter::Lookup<std::string>("log.thread.main_name", "",
                                     "main thread name (empty=unchanged)");
  ConfigCenter::Lookup<std::string>("log.console.target", "stdout",
                                     "console target stdout/stderr");
}

void ApplyLogConfig() {
  InitLogConfigVars();
  ApplyLogRuntimeSettings();
  ApplyRootLogger();
  ApplyNamedLoggers();
}

bool LoadLogConfigString(const std::string& content) {
  std::map<std::string, std::string> kv;
  if (!PropertyLoader::LoadString(content, &kv)) return false;
  LoadStats stats;
  const bool ok = ApplyConfigMap(kv, &stats);
  FinishLogConfigLoad("<string>", stats);
  return ok;
}

bool LoadLogConfigFile(const std::string& path) {
  std::map<std::string, std::string> kv;
  if (!PropertyLoader::LoadFile(path, &kv)) return false;
  LoadStats stats;
  const bool ok = ApplyConfigMap(kv, &stats);
  FinishLogConfigLoad(path, stats);
  return ok;
}

bool LoadLogConfigFileVerbose(const std::string& path, FILE* out) {
  if (!out) out = stdout;

  ConfigPrinter::PrintHelp(out);

  std::map<std::string, std::string> kv;
  if (!PropertyLoader::LoadFile(path, &kv)) {
    std::fprintf(out, "\n错误: 无法读取配置文件 %s\n", path.c_str());
    return false;
  }
  ConfigPrinter::PrintParsedFile(path, kv, out);

  LoadStats stats;
  const bool ok = ApplyConfigMap(kv, &stats);
  ConfigPrinter::PrintLoadStats(stats, out);
  FinishLogConfigLoad(path, stats);
  ConfigPrinter::PrintRegisteredVars(out);
  ConfigPrinter::PrintLogAssembly(out);
  std::fflush(out);
  return ok;
}

}  // namespace config
}  // namespace lemo
