#include "lemo/utils/string_util.h"

namespace lemo {
namespace utils {

std::string Trim(const std::string& s) {
  const size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return std::string();
  const size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

std::string ToLower(std::string s) {
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] >= 'A' && s[i] <= 'Z') s[i] = static_cast<char>(s[i] - 'A' + 'a');
  }
  return s;
}

bool StartsWith(const std::string& s, const char* prefix) {
  if (!prefix) return false;
  const size_t n = std::string(prefix).size();
  return s.size() >= n && s.compare(0, n, prefix) == 0;
}

std::vector<std::string> Split(const std::string& s, char delim) {
  std::vector<std::string> out;
  std::string item;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == delim) {
      out.push_back(item);
      item.clear();
    } else {
      item.push_back(s[i]);
    }
  }
  out.push_back(item);
  return out;
}

}  // namespace utils
}  // namespace lemo
