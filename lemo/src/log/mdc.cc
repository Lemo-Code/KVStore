#include "lemo/log/mdc.h"

#include <unordered_map>

namespace lemo {
namespace log {

typedef std::unordered_map<std::string, std::string> ContextMap;

ContextMap& LocalContext() {
  thread_local ContextMap ctx;
  return ctx;
}

void MDC::Put(const std::string& key, const std::string& value) {
  LocalContext()[key] = value;
}

void MDC::Remove(const std::string& key) { LocalContext().erase(key); }

std::string MDC::Get(const std::string& key) {
  ContextMap::const_iterator it = LocalContext().find(key);
  if (it == LocalContext().end()) return std::string();
  return it->second;
}

std::map<std::string, std::string> MDC::GetCopy() {
  const ContextMap& ctx = LocalContext();
  return std::map<std::string, std::string>(ctx.begin(), ctx.end());
}

void MDC::Clear() { LocalContext().clear(); }

}  // namespace log
}  // namespace lemo
