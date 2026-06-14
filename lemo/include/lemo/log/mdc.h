#pragma once

#include <map>
#include <string>

namespace lemo {
namespace log {

// log4j Mapped Diagnostic Context
class MDC {
 public:
  static void Put(const std::string& key, const std::string& value);
  static void Remove(const std::string& key);
  static std::string Get(const std::string& key);
  static std::map<std::string, std::string> GetCopy();
  static void Clear();
};

}  // namespace log
}  // namespace lemo
