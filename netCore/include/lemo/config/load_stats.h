#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace lemo {
namespace config {

// 一次加载的统计信息
struct LoadStats {
  size_t parsed_keys = 0;
  size_t applied_keys = 0;
  size_t ignored_keys = 0;
  std::vector<std::string> ignored;
  std::vector<std::string> failed;
};

}  // namespace config
}  // namespace lemo
