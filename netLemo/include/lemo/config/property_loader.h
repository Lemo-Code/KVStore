#pragma once

#include <map>
#include <string>

namespace lemo {
namespace config {

// 解析 key = value 属性文件（# 开头为注释）
class PropertyLoader {
 public:
  static bool LoadFile(const std::string& path,
                       std::map<std::string, std::string>* out);
  static bool LoadString(const std::string& content,
                         std::map<std::string, std::string>* out);
};

}  // namespace config
}  // namespace lemo
