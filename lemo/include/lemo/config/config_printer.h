#pragma once

#include "lemo/config/load_stats.h"

#include <cstdio>
#include <map>
#include <string>

namespace lemo {
namespace config {

// 人类可读的配置说明与加载结果输出
class ConfigPrinter {
 public:
  static void PrintHelp(FILE* out = stdout);

  static void PrintParsedFile(const std::string& path,
                              const std::map<std::string, std::string>& kv,
                              FILE* out = stdout);

  static void PrintLoadStats(const LoadStats& stats, FILE* out = stdout);

  static void PrintRegisteredVars(FILE* out = stdout);

  static void PrintLogAssembly(FILE* out = stdout);

  // 加载完成后输出摘要（stdout + 写入已装配的 root logger）
  static void ReportLoadComplete(const std::string& source,
                                 const LoadStats& stats,
                                 FILE* out = stdout);
};

}  // namespace config
}  // namespace lemo
