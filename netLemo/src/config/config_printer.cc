#include "lemo/config/config_printer.h"

#include "lemo/config/config_center.h"
#include "lemo/log/level.h"
#include "lemo/log/log.h"
#include "lemo/log/logger_repository.h"
#include "lemo/utils/string_util.h"

#include <algorithm>
#include <cstdio>
#include <list>
#include <map>
#include <string>
#include <vector>

namespace lemo {
namespace config {

namespace {

void PrintLine(FILE* out, const char* title) {
  std::fprintf(out, "\n%s\n", title);
  std::fprintf(out, "----------------------------------------\n");
}

struct VarRow {
  std::string key;
  std::string value;
  std::string desc;
};

bool VarRowLess(const VarRow& a, const VarRow& b) { return a.key < b.key; }

}  // namespace

void ConfigPrinter::PrintHelp(FILE* out) {
  std::fprintf(out,
               "Lemo 配置加载流程\n"
               "================\n"
               "1. 代码中 Lookup 声明配置项（约定优于配置）\n"
               "2. 从 .conf 文件解析 key = value\n"
               "3. 仅覆盖已声明的 key；未声明的 key 会被忽略\n"
               "4. log.* 键通过 LogConfig 装配 Logger / Appender\n\n"
               "常用 log 键:\n"
               "  log.level / log.pattern / log.appender\n"
               "  log.file.path / log.file.max_bytes / log.file.max_files\n"
               "  log.logger.<name>.level / log.logger.<name>.additive\n\n"
               "示例: config_demo path/to/lemo.conf\n");
}

void ConfigPrinter::PrintParsedFile(
    const std::string& path, const std::map<std::string, std::string>& kv,
    FILE* out) {
  PrintLine(out, "[1] 文件解析结果");
  std::fprintf(out, "文件: %s\n", path.c_str());
  std::fprintf(out, "共解析 %zu 个 key:\n", kv.size());
  for (std::map<std::string, std::string>::const_iterator it = kv.begin();
       it != kv.end(); ++it) {
    std::fprintf(out, "  %s = %s\n", it->first.c_str(), it->second.c_str());
  }
}

void ConfigPrinter::PrintLoadStats(const LoadStats& stats, FILE* out) {
  PrintLine(out, "[2] 加载统计");
  std::fprintf(out, "解析: %zu  生效: %zu  忽略(未声明): %zu  失败: %zu\n",
               stats.parsed_keys, stats.applied_keys, stats.ignored_keys,
               stats.failed.size());

  if (!stats.ignored.empty()) {
    std::fprintf(out, "\n被忽略的 key（代码中未 Lookup 声明）:\n");
    for (size_t i = 0; i < stats.ignored.size(); ++i) {
      std::fprintf(out, "  - %s\n", stats.ignored[i].c_str());
    }
  }

  if (!stats.failed.empty()) {
    std::fprintf(out, "\n解析失败的 key:\n");
    for (size_t i = 0; i < stats.failed.size(); ++i) {
      std::fprintf(out, "  - %s\n", stats.failed[i].c_str());
    }
  }
}

void ConfigPrinter::PrintRegisteredVars(FILE* out) {
  PrintLine(out, "[3] 当前已注册配置项（生效值）");

  std::vector<VarRow> rows;
  ConfigCenter::Visit([&rows](ConfigVarBase::ptr var) {
    if (!var) return;
    VarRow row;
    row.key = var->GetName();
    row.value = var->ToString();
    row.desc = var->GetDescription();
    rows.push_back(row);
  });
  std::sort(rows.begin(), rows.end(), VarRowLess);

  if (rows.empty()) {
    std::fprintf(out, "(无)\n");
    return;
  }

  for (size_t i = 0; i < rows.size(); ++i) {
    if (rows[i].desc.empty()) {
      std::fprintf(out, "  %-36s = %s\n", rows[i].key.c_str(),
                   rows[i].value.c_str());
    } else {
      std::fprintf(out, "  %-36s = %s  # %s\n", rows[i].key.c_str(),
                   rows[i].value.c_str(), rows[i].desc.c_str());
    }
  }
}

void ConfigPrinter::PrintLogAssembly(FILE* out) {
  PrintLine(out, "[4] 日志装配结果");

  log::LoggerRepository& repo = log::LoggerRepository::Instance();
  log::Logger::ptr root = repo.GetRoot();
  if (!root) {
    std::fprintf(out, "root logger 不存在\n");
    return;
  }

  std::fprintf(out, "root:\n");
  std::fprintf(out, "  level     = %s\n",
               log::LogLevel::ToString(root->GetLevel()));
  std::fprintf(out, "  additive  = true\n");
  std::fprintf(out, "  appenders = ");
  const std::list<log::Appender::ptr>& appenders = root->GetAppenders();
  if (appenders.empty()) {
    std::fprintf(out, "(无)\n");
  } else {
    bool first = true;
    for (std::list<log::Appender::ptr>::const_iterator it = appenders.begin();
         it != appenders.end(); ++it) {
      if (!*it) continue;
      std::fprintf(out, "%s%s", first ? "" : ", ", (*it)->Type());
      first = false;
    }
    std::fprintf(out, "\n");
  }

  std::vector<std::string> names;
  ConfigCenter::Visit([&names](ConfigVarBase::ptr var) {
    if (!var) return;
    const std::string key = var->GetName();
    const char* prefix = "log.logger.";
    if (!utils::StartsWith(key, prefix)) return;
    const std::string suffix = key.substr(std::string(prefix).size());
    const size_t dot = suffix.rfind('.');
    if (dot == std::string::npos) return;
    const std::string name = suffix.substr(0, dot);
    if (std::find(names.begin(), names.end(), name) == names.end()) {
      names.push_back(name);
    }
  });
  std::sort(names.begin(), names.end());

  if (names.empty()) {
    std::fprintf(out, "\n命名 logger: (无)\n");
    return;
  }

  std::fprintf(out, "\n命名 logger:\n");
  for (size_t i = 0; i < names.size(); ++i) {
    log::Logger::ptr logger = repo.GetLogger(names[i]);
    if (!logger) continue;
    std::fprintf(out, "  [%s]\n", names[i].c_str());
    std::fprintf(out, "    level    = %s\n",
                 log::LogLevel::ToString(logger->GetLevel()));
    std::fprintf(out, "    additive = %s\n",
                 logger->IsAdditive() ? "true" : "false");
  }
}

void ConfigPrinter::ReportLoadComplete(const std::string& source,
                                       const LoadStats& stats, FILE* out) {
  if (!out) out = stdout;

  std::fprintf(out, "\n=== lemo 配置已加载: %s ===\n", source.c_str());
  std::fprintf(out, "解析 %zu 项 | 生效 %zu 项 | 忽略 %zu 项\n",
               stats.parsed_keys, stats.applied_keys, stats.ignored_keys);

  std::vector<VarRow> rows;
  ConfigCenter::Visit([&rows](ConfigVarBase::ptr var) {
    if (!var) return;
    VarRow row;
    row.key = var->GetName();
    row.value = var->ToString();
    rows.push_back(row);
  });
  std::sort(rows.begin(), rows.end(), VarRowLess);

  if (!rows.empty()) {
    std::fprintf(out, "生效配置:\n");
    for (size_t i = 0; i < rows.size(); ++i) {
      std::fprintf(out, "  %s = %s\n", rows[i].key.c_str(),
                   rows[i].value.c_str());
    }
  }

  log::Logger::ptr root = log::LoggerRepository::Instance().GetRoot();
  if (root) {
    const std::string appender =
        ConfigCenter::Lookup<std::string>("log.appender", "console")
            ->GetValue();
    const std::string file_path =
        ConfigCenter::Lookup<std::string>("log.file.path", "lemo.log")
            ->GetValue();

    LEMO_LOG_WARN(root) << "[config] loaded from " << source << " (parsed="
                        << stats.parsed_keys << " applied="
                        << stats.applied_keys << " ignored="
                        << stats.ignored_keys << ")";
    LEMO_LOG_WARN(root) << "[config] root level="
                        << log::LogLevel::ToString(root->GetLevel())
                        << " appender=" << appender << " path=" << file_path;

    for (size_t i = 0; i < rows.size(); ++i) {
      if (utils::StartsWith(rows[i].key, "log.logger.")) {
        LEMO_LOG_WARN(root) << "[config] " << rows[i].key << "="
                            << rows[i].value;
      }
    }
    root->Flush();
    std::fprintf(out, "（以上摘要已写入 root logger，详见日志输出位置）\n");
  }
  std::fflush(out);
}

}  // namespace config
}  // namespace lemo
