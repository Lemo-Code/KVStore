#pragma once

#include <cstdio>
#include <string>

namespace lemo {
namespace config {

void InitLogConfigVars();
void ApplyLogConfig();

bool LoadLogConfigFile(const std::string& path);
bool LoadLogConfigString(const std::string& content);

// 加载并打印完整过程（演示 / 调试入口）
bool LoadLogConfigFileVerbose(const std::string& path, FILE* out = stdout);

}  // namespace config
}  // namespace lemo
