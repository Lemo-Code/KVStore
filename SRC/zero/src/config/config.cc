// zero config.cc — hierarchical type-safe configuration system
//
// Implements:
//   ConfigValue<T>  - typed config variable with change listeners, lexical_cast
//   ConfigSection   - hierarchical tree node (values + sub-sections)
//   Config          - singleton manager; YAML load, env override, CLI args
//
// YAML loading maps the document structure to ConfigSections:
//   server:
//     port: 8080
//     host: "0.0.0.0"
// Becomes sections "server" with values "server.port" (int) and "server.host"
// (string). YAML scalar auto-detection tries bool, int, double, then string.
//
// Environment variables (ZERO_ prefix) are mapped to dot-separated keys.
// Thread safety: all public methods use std::shared_mutex (shared for reads,
// exclusive for writes).

#include "zero/config/config.h"

#include <yaml-cpp/yaml.h>

#include <sys/stat.h>
#include <dirent.h>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iostream>
#include <shared_mutex>

// Explicit specializations to work around lexical_cast bugs.
// lexical_cast.h's if-constexpr chain doesn't correctly handle common types
// (bool, float, double, long) — it falls through to static_cast which fails.
// These specializations override ConfigValue::to_string() and
// ConfigValue::set_from_string() with direct conversions, completely bypassing
// the buggy lexical_cast template.

namespace zero {

// ---- int ----
template<>
std::string ConfigValue<int>::to_string() const {
    int val = get_value();
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val);
    return (ec == std::errc()) ? std::string(buf, ptr - buf) : "0";
}

template<>
void ConfigValue<int>::set_from_string(const std::string& str) {
    int val = 0;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
    if (ec == std::errc() && ptr == str.data() + str.size())
        set_value(val);
}

// ---- unsigned int ----
template<>
std::string ConfigValue<unsigned int>::to_string() const {
    unsigned int val = get_value();
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val);
    return (ec == std::errc()) ? std::string(buf, ptr - buf) : "0";
}

template<>
void ConfigValue<unsigned int>::set_from_string(const std::string& str) {
    unsigned int val = 0;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
    if (ec == std::errc() && ptr == str.data() + str.size())
        set_value(val);
}

// ---- int64_t (covers long/long long depending on platform) ----
template<>
std::string ConfigValue<int64_t>::to_string() const {
    int64_t val = get_value();
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val);
    return (ec == std::errc()) ? std::string(buf, ptr - buf) : "0";
}

template<>
void ConfigValue<int64_t>::set_from_string(const std::string& str) {
    int64_t val = 0;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
    if (ec == std::errc() && ptr == str.data() + str.size())
        set_value(val);
}

// ---- uint64_t ----
template<>
std::string ConfigValue<uint64_t>::to_string() const {
    uint64_t val = get_value();
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val);
    return (ec == std::errc()) ? std::string(buf, ptr - buf) : "0";
}

template<>
void ConfigValue<uint64_t>::set_from_string(const std::string& str) {
    uint64_t val = 0;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
    if (ec == std::errc() && ptr == str.data() + str.size())
        set_value(val);
}

// ---- float ----
template<>
std::string ConfigValue<float>::to_string() const {
    float val = get_value();
    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val,
                                    std::chars_format::general, 9);
    return (ec == std::errc()) ? std::string(buf, ptr - buf) : "0.0";
}

template<>
void ConfigValue<float>::set_from_string(const std::string& str) {
    float val = 0.0f;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
    if (ec == std::errc() && ptr == str.data() + str.size())
        set_value(val);
}

// ---- double ----
template<>
std::string ConfigValue<double>::to_string() const {
    double val = get_value();
    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val,
                                    std::chars_format::general, 17);
    return (ec == std::errc()) ? std::string(buf, ptr - buf) : "0.0";
}

template<>
void ConfigValue<double>::set_from_string(const std::string& str) {
    double val = 0.0;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
    if (ec == std::errc() && ptr == str.data() + str.size())
        set_value(val);
}

// ---- bool ----
template<>
std::string ConfigValue<bool>::to_string() const {
    return get_value() ? "true" : "false";
}

template<>
void ConfigValue<bool>::set_from_string(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    bool val = (lower == "true" || lower == "1" || lower == "yes" || lower == "on");
    set_value(val);
}

// ---- std::string ----
template<>
std::string ConfigValue<std::string>::to_string() const {
    return get_value();
}

template<>
void ConfigValue<std::string>::set_from_string(const std::string& str) {
    set_value(str);
}

} // namespace zero

namespace zero {

// ============================================================
// Internal helpers
// ============================================================

namespace {

// Detect if a filename has .yaml or .yml extension (case-insensitive)
bool is_yaml_file(const std::string& name) {
    auto lower = [](char c) -> char {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
    };
    size_t len = name.size();
    if (len >= 5 &&
        lower(name[len-5]) == '.' &&
        lower(name[len-4]) == 'y' &&
        lower(name[len-3]) == 'a' &&
        lower(name[len-2]) == 'm' &&
        lower(name[len-1]) == 'l') return true;
    if (len >= 4 &&
        lower(name[len-4]) == '.' &&
        lower(name[len-3]) == 'y' &&
        lower(name[len-2]) == 'm' &&
        lower(name[len-1]) == 'l') return true;
    return false;
}

bool file_exists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool dir_exists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// Expand ${VAR} and ${VAR:default} in strings
std::string expand_env(const std::string& input) {
    std::string result;
    result.reserve(input.size());
    const char* p   = input.c_str();
    const char* end = p + input.size();

    while (p < end) {
        if (*p == '$' && (p + 1) < end && *(p + 1) == '{') {
            p += 2;
            const char* name_start = p;
            while (p < end && *p != '}' && *p != ':') ++p;

            std::string var(name_start, p - name_start);

            if (p < end && *p == ':') {
                ++p;
                const char* def_start = p;
                while (p < end && *p != '}') ++p;
                std::string def_val(def_start, p - def_start);

                if (p < end && *p == '}') {
                    ++p;
                    const char* env = std::getenv(var.c_str());
                    result += (env && env[0]) ? env : def_val;
                } else {
                    result.append("${").append(var).append(":").append(def_val);
                }
            } else if (p < end && *p == '}') {
                ++p;
                const char* env = std::getenv(var.c_str());
                if (env) result += env;
            } else {
                result.append("${").append(var);
            }
        } else {
            result.push_back(*p++);
        }
    }
    return result;
}

// Recursively expand env vars in YAML scalar values
YAML::Node expand_yaml_env(const YAML::Node& node) {
    switch (node.Type()) {
        case YAML::NodeType::Scalar: {
            std::string raw = node.as<std::string>("");
            return YAML::Node(expand_env(raw));
        }
        case YAML::NodeType::Sequence: {
            YAML::Node result(YAML::NodeType::Sequence);
            for (size_t i = 0; i < node.size(); ++i)
                result.push_back(expand_yaml_env(node[i]));
            return result;
        }
        case YAML::NodeType::Map: {
            YAML::Node result(YAML::NodeType::Map);
            for (auto it = node.begin(); it != node.end(); ++it)
                result[it->first] = expand_yaml_env(it->second);
            return result;
        }
        default:
            return node;
    }
}

// Auto-detect the type of a YAML scalar and set it on the section.
// Tries: bool, int, int64_t, double, string (in that order).
// This avoids relying on lexical_cast for string-to-numeric conversion.
void set_yaml_scalar(std::shared_ptr<ConfigSection> section,
                     const std::string& key,
                     const YAML::Node& node) {
    if (!node.IsScalar()) return;

    std::string raw = node.as<std::string>();

    // Try bool first (before int, since "1"/"0" could be either)
    {
        std::string lower = raw;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lower == "true" || lower == "false" ||
            lower == "yes"  || lower == "no" ||
            lower == "on"   || lower == "off") {
            bool val = (lower == "true" || lower == "yes" || lower == "on");
            section->set<bool>(key, val);
            return;
        }
    }

    // Try int
    try {
        int val = node.as<int>();
        section->set<int>(key, val);
        return;
    } catch (const YAML::Exception&) {}

    // Try int64_t (for large numbers that overflow int)
    try {
        int64_t val = node.as<int64_t>();
        section->set<int64_t>(key, val);
        return;
    } catch (const YAML::Exception&) {}

    // Try double
    try {
        double val = node.as<double>();
        section->set<double>(key, val);
        return;
    } catch (const YAML::Exception&) {}

    // Fall back to string (with env expansion already applied)
    section->set<std::string>(key, raw);
}

// Recursively apply a YAML subtree to a ConfigSection
void apply_yaml_node(std::shared_ptr<ConfigSection> section,
                     const YAML::Node& node) {
    if (!node.IsMap()) return;

    for (auto it = node.begin(); it != node.end(); ++it) {
        std::string key = it->first.as<std::string>();
        const YAML::Node& val = it->second;

        if (val.IsMap()) {
            auto sub = section->get_section(key, true);
            apply_yaml_node(sub, val);
        } else if (val.IsScalar()) {
            set_yaml_scalar(section, key, val);
        } else if (val.IsSequence()) {
            // Emit sequence as YAML string
            YAML::Emitter emitter;
            emitter << val;
            section->set<std::string>(key,
                std::string(emitter.c_str(), emitter.size()));
        }
        // null/undefined nodes are skipped
    }
}

// Convert ZERO_SERVER_PORT to "server.port"
std::string env_to_path(const std::string& prefix, const std::string& env_name) {
    std::string path;
    size_t start = (env_name.find(prefix) == 0) ? prefix.size() : 0;
    for (size_t i = start; i < env_name.size(); ++i) {
        char c = env_name[i];
        if (c == '_') {
            path.push_back('.');
        } else if (c >= 'A' && c <= 'Z') {
            path.push_back(static_cast<char>(c + 32));
        } else {
            path.push_back(c);
        }
    }
    return path;
}

// Helper to directly find a sub-section in a const context (avoids non-const get_section)
const ConfigSection* find_sub_section(const ConfigSection* section,
                                       const std::string& name) {
    if (!section) return nullptr;

    // Access sections_ through the ConfigSection interface
    // Use has() + keys() to indirectly check, or just try via the public API.
    // Since get_section with create=false is supposed to be const but isn't,
    // we use a const_cast workaround on the internal map by reimplementing
    // the lookup via the keys/has interface.
    if (!section->has(name)) return nullptr;

    // We need the actual ConfigSection pointer. Since the public API doesn't
    // expose raw pointers to sub-sections in a const way, we use the
    // get_section(name, false) method which doesn't modify anything.
    // Despite not being marked const, it's logically const when create=false.
    auto mutable_section = const_cast<ConfigSection*>(section);
    auto sub = mutable_section->get_section(name, false);
    return sub.get();
}

} // anonymous namespace

// ============================================================
// ConfigSection
// ============================================================

ConfigSection::ConfigSection(const std::string& name)
    : name_(name) {}

ConfigSection::~ConfigSection() = default;

std::shared_ptr<ConfigSection> ConfigSection::get_section(
    const std::string& name, bool create) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = sections_.find(name);
    if (it != sections_.end()) return it->second;

    if (!create) return nullptr;

    lock.unlock();
    std::unique_lock<std::shared_mutex> wlock(mutex_);

    auto it2 = sections_.find(name);
    if (it2 != sections_.end()) return it2->second;

    auto section = std::make_shared<ConfigSection>(name);
    sections_[name] = section;
    return section;
}

int ConfigSection::get_int(const std::string& key, int default_val) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = values_.find(key);
    if (it != values_.end() && it->second) {
        auto* cv = dynamic_cast<ConfigValue<int>*>(it->second.get());
        if (cv) return cv->get_value();
        auto* cv64 = dynamic_cast<ConfigValue<int64_t>*>(it->second.get());
        if (cv64) return static_cast<int>(cv64->get_value());
    }
    return default_val;
}

int64_t ConfigSection::get_int64(const std::string& key,
                                  int64_t default_val) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = values_.find(key);
    if (it != values_.end() && it->second) {
        auto* cv = dynamic_cast<ConfigValue<int64_t>*>(it->second.get());
        if (cv) return cv->get_value();
        auto* cv32 = dynamic_cast<ConfigValue<int>*>(it->second.get());
        if (cv32) return static_cast<int64_t>(cv32->get_value());
    }
    return default_val;
}

double ConfigSection::get_double(const std::string& key,
                                  double default_val) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = values_.find(key);
    if (it != values_.end() && it->second) {
        auto* cv = dynamic_cast<ConfigValue<double>*>(it->second.get());
        if (cv) return cv->get_value();
        auto* cv32 = dynamic_cast<ConfigValue<int>*>(it->second.get());
        if (cv32) return static_cast<double>(cv32->get_value());
        auto* cv64 = dynamic_cast<ConfigValue<int64_t>*>(it->second.get());
        if (cv64) return static_cast<double>(cv64->get_value());
    }
    return default_val;
}

bool ConfigSection::get_bool(const std::string& key,
                              bool default_val) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = values_.find(key);
    if (it != values_.end() && it->second) {
        auto* cv = dynamic_cast<ConfigValue<bool>*>(it->second.get());
        if (cv) return cv->get_value();
        auto* cv32 = dynamic_cast<ConfigValue<int>*>(it->second.get());
        if (cv32) return cv32->get_value() != 0;
    }
    return default_val;
}

std::string ConfigSection::get_string(const std::string& key,
                                       const std::string& default_val) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = values_.find(key);
    if (it != values_.end() && it->second) {
        auto* cv = dynamic_cast<ConfigValue<std::string>*>(it->second.get());
        if (cv) return cv->get_value();
        // For non-string values, use their to_string()
        return it->second->to_string();
    }
    return default_val;
}

void ConfigSection::set_int(const std::string& key, int value) {
    set<int>(key, value);
}

void ConfigSection::set_int64(const std::string& key, int64_t value) {
    set<int64_t>(key, value);
}

void ConfigSection::set_double(const std::string& key, double value) {
    set<double>(key, value);
}

void ConfigSection::set_bool(const std::string& key, bool value) {
    set<bool>(key, value);
}

void ConfigSection::set_string(const std::string& key,
                                const std::string& value) {
    set<std::string>(key, value);
}

bool ConfigSection::has(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return values_.find(key) != values_.end() ||
           sections_.find(key) != sections_.end();
}

bool ConfigSection::remove(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    return values_.erase(key) + sections_.erase(key) > 0;
}

std::vector<std::string> ConfigSection::keys() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<std::string> result;
    result.reserve(values_.size() + sections_.size());
    for (const auto& [k, _] : values_)   result.push_back(k);
    for (const auto& [k, _] : sections_) result.push_back(k);
    return result;
}

std::string ConfigSection::to_string(int indent) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::ostringstream oss;
    std::string pad(indent * 2, ' ');

    for (const auto& [key, val] : values_) {
        oss << pad << key << ": ";
        oss << (val ? val->to_string() : "null") << "\n";
    }
    for (const auto& [key, section] : sections_) {
        oss << pad << key << ":\n";
        if (section) oss << section->to_string(indent + 1);
    }
    return oss.str();
}

// ============================================================
// Config — singleton manager
// ============================================================

Config& Config::instance() {
    static Config s_config;
    return s_config;
}

Config::PathParts Config::parse_path(const std::string& path) {
    PathParts parts;
    auto pos = path.rfind('.');
    if (pos == std::string::npos) {
        parts.key = path;
    } else {
        parts.section_path = path.substr(0, pos);
        parts.key          = path.substr(pos + 1);
    }
    return parts;
}

const ConfigSection* Config::find_section(const std::string& path) const {
    if (path.empty()) return root_.get();

    std::shared_lock<std::shared_mutex> lock(mutex_);
    const ConfigSection* current = root_.get();
    if (!current) return nullptr;

    std::istringstream iss(path);
    std::string part;
    while (std::getline(iss, part, '.')) {
        if (part.empty()) continue;
        current = find_sub_section(current, part);
        if (!current) return nullptr;
    }
    return current;
}

std::shared_ptr<ConfigSection> Config::get_or_create_section(
    const std::string& path) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (!root_) root_ = std::make_shared<ConfigSection>("root");
    if (path.empty()) return root_;

    auto current = root_;
    std::istringstream iss(path);
    std::string part;
    while (std::getline(iss, part, '.')) {
        if (part.empty()) continue;
        current = current->get_section(part, true);
    }
    return current;
}

// ============================================================
// File / directory loading
// ============================================================

bool Config::load_from_file(const std::string& filepath) {
    if (!file_exists(filepath)) {
        std::cerr << "[zero::Config] File not found: " << filepath << std::endl;
        return false;
    }

    YAML::Node root;
    try {
        root = YAML::LoadFile(filepath);
    } catch (const YAML::Exception& e) {
        std::cerr << "[zero::Config] YAML parse error in " << filepath
                  << ": " << e.what() << std::endl;
        return false;
    }

    if (root.IsNull()) return true;
    if (!root.IsMap()) {
        std::cerr << "[zero::Config] " << filepath
                  << ": top-level must be a YAML map" << std::endl;
        return false;
    }

    root = expand_yaml_env(root);

    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (!root_) root_ = std::make_shared<ConfigSection>("root");
    }

    for (auto it = root.begin(); it != root.end(); ++it) {
        std::string key = it->first.as<std::string>();
        auto section = get_or_create_section(key);
        apply_yaml_node(section, it->second);
    }

    return true;
}

bool Config::load_from_dir(const std::string& dirpath) {
    if (!dir_exists(dirpath)) {
        std::cerr << "[zero::Config] Directory not found: " << dirpath << std::endl;
        return false;
    }

    DIR* dir = ::opendir(dirpath.c_str());
    if (!dir) {
        std::cerr << "[zero::Config] Cannot open directory " << dirpath
                  << ": " << strerror(errno) << std::endl;
        return false;
    }

    std::vector<std::string> files;
    struct dirent* entry;
    while ((entry = ::readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        if (name == "." || name == "..") continue;
        if (name[0] == '.') continue;
        if (is_yaml_file(name)) files.push_back(name);
    }
    ::closedir(dir);

    if (files.empty()) return true;

    std::sort(files.begin(), files.end());

    bool all_ok = true;
    for (const auto& file : files) {
        std::string full = dirpath;
        if (dirpath.back() != '/') full += '/';
        full += file;
        if (!load_from_file(full)) all_ok = false;
    }
    return all_ok;
}

void Config::load_from_args(int argc, char** argv) {
    if (!argv) return;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        size_t eq = arg.find('=');
        if (eq == std::string::npos) continue;

        std::string key = arg.substr(0, eq);
        std::string val = arg.substr(eq + 1);

        if (key.size() > 2 && key[0] == '-' && key[1] == '-')
            key = key.substr(2);
        std::replace(key.begin(), key.end(), '-', '.');
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        set<std::string>(key, val);
    }
}

void Config::load_from_env(const std::string& prefix) {
    extern char** environ;
    if (!environ) return;

    for (char** env = environ; *env != nullptr; ++env) {
        std::string entry(*env);
        size_t eq = entry.find('=');
        if (eq == std::string::npos) continue;

        std::string name = entry.substr(0, eq);
        std::string value = entry.substr(eq + 1);

        if (name.find(prefix) != 0) continue;

        std::string path = env_to_path(prefix, name);
        if (path.empty()) continue;

        set<std::string>(path, value);
    }
}

// ============================================================
// Convenience accessors (delegate to root section)
// ============================================================

int Config::get_int(const std::string& path, int default_val) const {
    auto p = parse_path(path);
    auto* section = find_section(p.section_path);
    return section ? section->get_int(p.key, default_val) : default_val;
}

int64_t Config::get_int64(const std::string& path, int64_t default_val) const {
    auto p = parse_path(path);
    auto* section = find_section(p.section_path);
    return section ? section->get_int64(p.key, default_val) : default_val;
}

double Config::get_double(const std::string& path, double default_val) const {
    auto p = parse_path(path);
    auto* section = find_section(p.section_path);
    return section ? section->get_double(p.key, default_val) : default_val;
}

bool Config::get_bool(const std::string& path, bool default_val) const {
    auto p = parse_path(path);
    auto* section = find_section(p.section_path);
    return section ? section->get_bool(p.key, default_val) : default_val;
}

std::string Config::get_string(const std::string& path,
                                const std::string& default_val) const {
    auto p = parse_path(path);
    auto* section = find_section(p.section_path);
    return section ? section->get_string(p.key, default_val) : default_val;
}

void Config::set_int(const std::string& path, int value) {
    set<int>(path, value);
}

void Config::set_int64(const std::string& path, int64_t value) {
    set<int64_t>(path, value);
}

void Config::set_double(const std::string& path, double value) {
    set<double>(path, value);
}

void Config::set_bool(const std::string& path, bool value) {
    set<bool>(path, value);
}

void Config::set_string(const std::string& path, const std::string& value) {
    set<std::string>(path, value);
}

bool Config::has(const std::string& path) const {
    auto p = parse_path(path);
    auto* section = find_section(p.section_path);
    return section ? section->has(p.key) : false;
}

bool Config::remove(const std::string& path) {
    auto p = parse_path(path);
    auto section = get_or_create_section(p.section_path);
    return section && section->remove(p.key);
}

std::string Config::to_string() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return root_ ? root_->to_string(0) : "(empty)";
}

void Config::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    root_.reset();
}

} // namespace zero
