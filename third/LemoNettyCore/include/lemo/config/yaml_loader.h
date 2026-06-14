#pragma once

#include "lemo/nettycore_export.h"

#include <map>
#include <string>

namespace lemo {
namespace config {

/** 将 YAML 打平为 key=value（如 server.port=9000），供 ConfigCenter 加载。 */
class LEMO_NETTYCORE_API YamlLoader {
 public:
  static bool LoadFile(const std::string& path,
                       std::map<std::string, std::string>* out);
  static bool LoadString(const std::string& content,
                         std::map<std::string, std::string>* out);
};

}  // namespace config
}  // namespace lemo
