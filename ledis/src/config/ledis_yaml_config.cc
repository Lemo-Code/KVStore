#include "ledis/config/ledis_yaml_config.h"

#include "ledis/store/eviction.h"

#include <yaml-cpp/yaml.h>

#include <cctype>
#include <cstdlib>
#include <map>

namespace ledis {
namespace {

bool isValidKey(const String& key) {
  if (key.empty()) {
    return false;
  }
  for (size_t i = 0; i < key.size(); ++i) {
    const char c = key[i];
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
        c == '_') {
      continue;
    }
    return false;
  }
  return true;
}

String toLower(String s) {
  for (size_t i = 0; i < s.size(); ++i) {
    s[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
  }
  return s;
}

void flattenNode(const String& prefix, const YAML::Node& node,
                 std::map<std::string, std::string>* out) {
  if (!out) {
    return;
  }
  if (node.IsScalar()) {
    const String key = toLower(prefix);
    if (isValidKey(key)) {
      (*out)[key] = node.Scalar();
    }
    return;
  }
  if (node.IsSequence()) {
    YAML::Emitter emitter;
    emitter << node;
    const String key = toLower(prefix);
    if (isValidKey(key)) {
      (*out)[key] = emitter.c_str();
    }
    return;
  }
  if (node.IsMap()) {
    for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
      const String part = it->first.as<std::string>();
      const String child = prefix.empty() ? part : prefix + "." + part;
      flattenNode(child, it->second, out);
    }
  }
}

const char* lookupFlat(const std::map<std::string, std::string>& flat,
                       const char* key) {
  const auto it = flat.find(key);
  return it == flat.end() ? nullptr : it->second.c_str();
}

bool parseBool(const char* value, bool* out) {
  if (!value || !out) {
    return false;
  }
  String v(value);
  for (size_t i = 0; i < v.size(); ++i) {
    v[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(v[i])));
  }
  if (v == "1" || v == "true" || v == "yes" || v == "on") {
    *out = true;
    return true;
  }
  if (v == "0" || v == "false" || v == "no" || v == "off") {
    *out = false;
    return true;
  }
  return false;
}

bool parseSizeT(const char* value, size_t* out) {
  if (!value || !out || value[0] == '\0') {
    return false;
  }
  char* end = nullptr;
  const unsigned long long n = std::strtoull(value, &end, 10);
  if (end == value || *end != '\0') {
    return false;
  }
  *out = static_cast<size_t>(n);
  return true;
}

bool parseU32(const char* value, uint32_t* out) {
  if (!value || !out || value[0] == '\0') {
    return false;
  }
  char* end = nullptr;
  const unsigned long n = std::strtoul(value, &end, 10);
  if (end == value || *end != '\0') {
    return false;
  }
  *out = static_cast<uint32_t>(n);
  return true;
}

bool parseU16(const char* value, uint16_t* out) {
  uint32_t tmp = 0;
  if (!parseU32(value, &tmp) || tmp > 65535) {
    return false;
  }
  *out = static_cast<uint16_t>(tmp);
  return true;
}

void applyFlat(const std::map<std::string, std::string>& flat, LedisSettings* s) {
  if (!s) {
    return;
  }
  if (const char* v = lookupFlat(flat, "server.host")) {
    s->host = v;
  }
  if (const char* v = lookupFlat(flat, "server.port")) {
    (void)parseU16(v, &s->port);
  }
  if (const char* v = lookupFlat(flat, "io.threads")) {
    (void)parseU32(v, &s->io_threads);
  }
  if (const char* v = lookupFlat(flat, "ledis.single_thread_mode")) {
    (void)parseBool(v, &s->single_thread_mode);
  }
  if (const char* v = lookupFlat(flat, "ledis.io_threads")) {
    (void)parseU32(v, &s->io_threads);
  }
  if (const char* v = lookupFlat(flat, "ledis.maxclients")) {
    (void)parseSizeT(v, &s->maxclients);
  }
  if (const char* v = lookupFlat(flat, "ledis.max_pending_commands")) {
    (void)parseSizeT(v, &s->max_pending_commands);
  }
  if (const char* v = lookupFlat(flat, "ledis.query_buffer_limit")) {
    (void)parseSizeT(v, &s->query_buffer_limit);
  }
  if (const char* v = lookupFlat(flat, "ledis.maxmemory")) {
    (void)parseSizeT(v, &s->maxmemory);
  }
  if (const char* v = lookupFlat(flat, "ledis.maxmemory_policy")) {
    bool ok = false;
    s->maxmemory_policy = maxmemoryPolicyName(parseMaxmemoryPolicy(v, &ok));
    if (!ok) {
      s->maxmemory_policy = "allkeys-lru";
    }
  }
  if (const char* v = lookupFlat(flat, "ledis.requirepass")) {
    s->requirepass = v;
  }
  if (const char* v = lookupFlat(flat, "ledis.active_expire_enabled")) {
    (void)parseBool(v, &s->active_expire_enabled);
  }
  if (const char* v = lookupFlat(flat, "ledis.active_expire_cycle_keys")) {
    (void)parseSizeT(v, &s->active_expire_cycle_keys);
  }
  if (const char* v = lookupFlat(flat, "ledis.dir")) {
    s->dir = v;
  }
  if (const char* v = lookupFlat(flat, "ledis.dbfilename")) {
    s->dbfilename = v;
  }
  if (const char* v = lookupFlat(flat, "ledis.appendonly")) {
    (void)parseBool(v, &s->appendonly);
  }
  if (const char* v = lookupFlat(flat, "ledis.appendfilename")) {
    s->appendfilename = v;
  }
  if (const char* v = lookupFlat(flat, "ledis.appendfsync")) {
    s->appendfsync = v;
  }
}

}  // namespace

bool LoadLedisSettingsFromYamlFile(const String& path, LedisSettings* settings) {
  if (!settings) {
    return false;
  }
  try {
    const YAML::Node root = YAML::LoadFile(path);
    std::map<std::string, std::string> flat;
    flattenNode("", root, &flat);
    if (flat.empty()) {
      return false;
    }
    applyFlat(flat, settings);
    if (settings->io_threads == 0) {
      settings->io_threads = 1;
    }
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace ledis
