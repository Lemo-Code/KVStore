#include "lemo/config/netty_config.h"

#include "lemo/config/config_center.h"
#include "lemo/config/config_printer.h"
#include "lemo/config/log_config.h"
#include "lemo/config/property_loader.h"
#include "lemo/config/yaml_loader.h"
#include "lemo/io/runtime.h"
#include "lemo/memory/stack_pool.h"
#include "lemo/utils/lexical_cast.h"
#include "lemo/utils/string_util.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <set>

namespace lemo {
namespace config {
namespace {

const char* kLogLoggerPrefix = "log.logger.";

bool PathEndsWith(const std::string& path, const char* suffix) {
  const size_t n = std::strlen(suffix);
  return path.size() >= n && path.compare(path.size() - n, n, suffix) == 0;
}

bool LoadConfigMapFromPath(const std::string& path,
                           std::map<std::string, std::string>* kv) {
  const std::string lower = utils::ToLower(path);
  if (PathEndsWith(lower, ".conf")) {
    return PropertyLoader::LoadFile(path, kv);
  }
  return YamlLoader::LoadFile(path, kv);
}

template <typename T>
T LookupValue(const std::string& key, const T& default_val) {
  typename ConfigVar<std::string>::ptr var =
      ConfigCenter::Lookup<std::string>(key);
  if (!var) {
    return default_val;
  }
  typedef utils::LexicalCast<std::string, T> Cast;
  return Cast()(var->GetValue());
}

void RegisterDynamicLogKeys(const std::map<std::string, std::string>& kv) {
  std::set<std::string> names;
  for (std::map<std::string, std::string>::const_iterator it = kv.begin();
       it != kv.end(); ++it) {
    if (!utils::StartsWith(it->first, kLogLoggerPrefix)) {
      continue;
    }
    ConfigCenter::Lookup<std::string>(it->first, "");
    const std::string suffix = it->first.substr(std::string(kLogLoggerPrefix).size());
    const size_t dot = suffix.rfind('.');
    if (dot == std::string::npos) {
      continue;
    }
    names.insert(suffix.substr(0, dot));
  }
  (void)names;
}

bool ApplyConfigMap(const std::map<std::string, std::string>& kv,
                    LoadStats* stats) {
  InitNettyConfigVars();
  RegisterDynamicLogKeys(kv);
  if (stats) {
    stats->parsed_keys = kv.size();
  }
  return ConfigCenter::LoadFlatMap(kv, stats);
}

void FinishNettyConfigLoad(const std::string& source, const LoadStats& stats) {
  ApplyNettyConfig();
  ApplyLogConfig();
  ConfigPrinter::ReportLoadComplete(source, stats, stdout);
}

}  // namespace

void InitNettyConfigVars() {
  InitLogConfigVars();

  ConfigCenter::Lookup<std::string>("server.name", "app", "server / app name");
  ConfigCenter::Lookup<std::string>("server.host", "0.0.0.0", "bind address");
  ConfigCenter::Lookup<std::string>("server.port", "9000", "listen port");

  ConfigCenter::Lookup<std::string>("io.threads", "4", "IOManager worker count");
  ConfigCenter::Lookup<std::string>("io.use_caller", "false",
                                     "scheduler use_caller");
  ConfigCenter::Lookup<std::string>("io.name", "main", "runtime / IOManager name");

  ConfigCenter::Lookup<std::string>("fiber.stackpool.max_tls_cached", "32",
                                     "StackPool per-thread cache");
  ConfigCenter::Lookup<std::string>("fiber.stackpool.max_global_cached", "64",
                                     "StackPool global cache");
}

void ApplyNettyConfig() {
  memory::StackPool::Config cfg;
  cfg.max_tls_cached =
      LookupValue<uint32_t>("fiber.stackpool.max_tls_cached", 32);
  cfg.max_global_cached =
      LookupValue<uint32_t>("fiber.stackpool.max_global_cached", 64);
  memory::StackPool::set_config(cfg);
}

NettySettings GetNettySettings() {
  NettySettings s;
  s.server_name = LookupValue<std::string>("server.name", "app");
  s.server_host = LookupValue<std::string>("server.host", "0.0.0.0");
  s.server_port =
      static_cast<uint16_t>(LookupValue<uint32_t>("server.port", 9000));
  s.io_threads = static_cast<size_t>(LookupValue<uint32_t>("io.threads", 4));
  s.io_use_caller = LookupValue<bool>("io.use_caller", false);
  s.io_name = LookupValue<std::string>("io.name", "main");
  s.stackpool_max_tls_cached =
      LookupValue<uint32_t>("fiber.stackpool.max_tls_cached", 32);
  s.stackpool_max_global_cached =
      LookupValue<uint32_t>("fiber.stackpool.max_global_cached", 64);
  return s;
}

std::shared_ptr<io::Runtime> CreateRuntimeFromConfig(
    const std::string& name_override) {
  const NettySettings s = GetNettySettings();
  const std::string name =
      name_override.empty() ? s.io_name : name_override;
  return std::shared_ptr<io::Runtime>(
      new io::Runtime(s.io_threads, s.io_use_caller, name));
}

bool LoadNettyConfigYamlFile(const std::string& path) {
  std::map<std::string, std::string> kv;
  if (!YamlLoader::LoadFile(path, &kv)) {
    return false;
  }
  LoadStats stats;
  const bool ok = ApplyConfigMap(kv, &stats);
  FinishNettyConfigLoad(path, stats);
  return ok;
}

bool LoadNettyConfigYamlString(const std::string& content) {
  std::map<std::string, std::string> kv;
  if (!YamlLoader::LoadString(content, &kv)) {
    return false;
  }
  LoadStats stats;
  const bool ok = ApplyConfigMap(kv, &stats);
  FinishNettyConfigLoad("<yaml-string>", stats);
  return ok;
}

bool LoadNettyConfigFile(const std::string& path) {
  std::map<std::string, std::string> kv;
  if (!LoadConfigMapFromPath(path, &kv)) {
    return false;
  }
  LoadStats stats;
  const bool ok = ApplyConfigMap(kv, &stats);
  FinishNettyConfigLoad(path, stats);
  return ok;
}

bool LoadNettyConfigFileVerbose(const std::string& path, FILE* out) {
  if (!out) {
    out = stdout;
  }

  ConfigPrinter::PrintHelp(out);

  std::map<std::string, std::string> kv;
  if (!LoadConfigMapFromPath(path, &kv)) {
    std::fprintf(out, "\n错误: 无法读取配置文件 %s\n", path.c_str());
    return false;
  }
  ConfigPrinter::PrintParsedFile(path, kv, out);

  LoadStats stats;
  const bool ok = ApplyConfigMap(kv, &stats);
  ApplyNettyConfig();
  ApplyLogConfig();
  ConfigPrinter::ReportLoadComplete(path, stats, out);
  ConfigPrinter::PrintRegisteredVars(out);
  ConfigPrinter::PrintLogAssembly(out);
  PrintNettySettings(out);
  std::fflush(out);
  return ok;
}

void PrintNettySettings(FILE* out) {
  if (!out) {
    out = stdout;
  }
  const NettySettings s = GetNettySettings();
  std::fprintf(out, "\n── NettySettings ──\n");
  std::fprintf(out, "  server.name = %s\n", s.server_name.c_str());
  std::fprintf(out, "  server.host = %s\n", s.server_host.c_str());
  std::fprintf(out, "  server.port = %u\n", static_cast<unsigned>(s.server_port));
  std::fprintf(out, "  io.threads = %zu\n", s.io_threads);
  std::fprintf(out, "  io.use_caller = %s\n", s.io_use_caller ? "true" : "false");
  std::fprintf(out, "  io.name = %s\n", s.io_name.c_str());
  std::fprintf(out, "  fiber.stackpool.max_tls_cached = %u\n",
               s.stackpool_max_tls_cached);
  std::fprintf(out, "  fiber.stackpool.max_global_cached = %u\n",
               s.stackpool_max_global_cached);
}

}  // namespace config
}  // namespace lemo
