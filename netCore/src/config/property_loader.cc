#include "lemo/config/property_loader.h"

#include "lemo/utils/string_util.h"

#include <fstream>
#include <sstream>

namespace lemo {
namespace config {

bool PropertyLoader::LoadString(const std::string& content,
                                std::map<std::string, std::string>* out) {
  if (!out) return false;
  out->clear();

  std::istringstream in(content);
  std::string line;
  while (std::getline(in, line)) {
    line = utils::Trim(line);
    if (line.empty() || line[0] == '#') continue;

    const size_t pos = line.find('=');
    if (pos == std::string::npos) continue;

    std::string key = utils::Trim(line.substr(0, pos));
    std::string value = utils::Trim(line.substr(pos + 1));
    if (key.empty()) continue;
    (*out)[utils::ToLower(key)] = value;
  }
  return true;
}

bool PropertyLoader::LoadFile(const std::string& path,
                            std::map<std::string, std::string>* out) {
  std::ifstream in(path.c_str());
  if (!in) return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  return LoadString(ss.str(), out);
}

}  // namespace config
}  // namespace lemo
