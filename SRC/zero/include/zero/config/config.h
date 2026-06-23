// zero Config — type-safe, YAML-based configuration system
//
// Features:
//   - Hierarchical key-value config with dot-separated paths
//     (e.g., "server.http.port")
//   - Type-safe accessors: get_int, get_string, get_bool, get_double
//   - Generic get<T>() with lexical_cast for conversion
//   - Change listeners: receive callbacks when a config value changes
//   - Hot-reload from file and directory
//   - YAML file and directory loading
//   - Environment variable override
//   - Default values for all accessors
//   - Thread-safe reads and writes
//
// Usage:
//   Config::instance().load_from_file("/etc/myapp/config.yaml");
//   int port = Config::instance().get<int>("server.port", 8080);
//   std::string host = Config::instance().get<std::string>("server.host",
//   "0.0.0.0");
//
// With change listeners:
//   Config::instance().watch<int>("server.port", [](int old_v, int new_v) {
//       // Restart server on new port
//   });
#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include <any>
#include <mutex>
#include <shared_mutex>
#include <typeindex>

#include "zero/base/lexical_cast.h"
#include "zero/thread/rwlock.h"

namespace zero {

// ============================================================
// ConfigValue — type-erased config variable
// ============================================================

class ConfigValueBase {
public:
    virtual ~ConfigValueBase() = default;
    virtual const std::string& name() const = 0;
    virtual std::string to_string() const = 0;
    virtual const std::type_info& type_info() const = 0;
    virtual void set_from_string(const std::string& str) = 0;
};

template <typename T>
class ConfigValue : public ConfigValueBase {
public:
    using Listener = std::function<void(const T& old_val, const T& new_val)>;

    ConfigValue(const std::string& name, const T& default_value)
        : name_(name), value_(default_value), default_(default_value) {}

    const std::string& name() const override { return name_; }

    T get_value() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return value_;
    }

    void set_value(const T& val) {
        T old;
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            old = value_;
            value_ = val;
        }
        // Notify listeners outside the lock
        notify_listeners(old, val);
    }

    T default_value() const { return default_; }

    std::string to_string() const override {
        return lexical_cast<std::string>(get_value());
    }

    const std::type_info& type_info() const override { return typeid(T); }

    void set_from_string(const std::string& str) override {
        set_value(lexical_cast<T>(str));
    }

    void add_listener(Listener cb) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        listeners_.push_back(std::move(cb));
    }

private:
    void notify_listeners(const T& old_val, const T& new_val) {
        // Read listeners under shared lock
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (auto& listener : listeners_) {
            listener(old_val, new_val);
        }
    }

    std::string name_;
    T value_;
    T default_;
    mutable std::shared_mutex mutex_;
    std::vector<Listener> listeners_;
};

// ============================================================
// ConfigSection — hierarchical config node
// ============================================================

class ConfigSection {
public:
    explicit ConfigSection(const std::string& name = "");
    ~ConfigSection();

    // Get or create a sub-section
    std::shared_ptr<ConfigSection> get_section(const std::string& name,
                                                 bool create = true);

    // Set a typed value
    template <typename T>
    void set(const std::string& key, const T& value) {
        auto val = std::make_shared<ConfigValue<T>>(key, value);
        std::unique_lock<std::shared_mutex> lock(mutex_);
        values_[key] = val;
    }

    // Get a typed value with default
    template <typename T>
    T get(const std::string& key, const T& default_val = T{}) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = values_.find(key);
        if (it != values_.end() && it->second) {
            auto* cv = dynamic_cast<ConfigValue<T>*>(it->second.get());
            if (cv) {
                return cv->get_value();
            }
            // Type mismatch: try lexical_cast
            try {
                return lexical_cast<T>(it->second->to_string());
            } catch (...) {
                // Fall through to default
            }
        }
        return default_val;
    }

    // Convenience type-specific accessors
    int get_int(const std::string& key, int default_val = 0) const;
    int64_t get_int64(const std::string& key, int64_t default_val = 0) const;
    double get_double(const std::string& key,
                       double default_val = 0.0) const;
    bool get_bool(const std::string& key,
                   bool default_val = false) const;
    std::string get_string(const std::string& key,
                            const std::string& default_val = "") const;

    // Set convenience
    void set_int(const std::string& key, int value);
    void set_int64(const std::string& key, int64_t value);
    void set_double(const std::string& key, double value);
    void set_bool(const std::string& key, bool value);
    void set_string(const std::string& key, const std::string& value);

    // Watch for changes on a key
    template <typename T>
    void watch(const std::string& key,
                typename ConfigValue<T>::Listener cb) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = values_.find(key);
        if (it != values_.end() && it->second) {
            auto* cv = dynamic_cast<ConfigValue<T>*>(it->second.get());
            if (cv) {
                cv->add_listener(std::move(cb));
            }
        }
    }

    // Check if a key exists
    bool has(const std::string& key) const;

    // Remove a key or section
    bool remove(const std::string& key);

    // List all keys in this section
    std::vector<std::string> keys() const;

    // Convert to string (for debugging)
    std::string to_string(int indent = 0) const;

    const std::string& name() const noexcept { return name_; }

private:
    std::string name_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<ConfigValueBase>> values_;
    std::unordered_map<std::string, std::shared_ptr<ConfigSection>> sections_;
};

// ============================================================
// Config — global configuration manager
// ============================================================

class Config {
public:
    // Singleton access
    static Config& instance();

    // ============================================================
    // File loading
    // ============================================================

    // Load configuration from a YAML file.
    // Merges with existing config (does not clear).
    // Returns true on success.
    bool load_from_file(const std::string& filepath);

    // Load from all .yaml/.yml files in a directory.
    // Files are loaded in alphabetical order.
    bool load_from_dir(const std::string& dirpath);

    // Load from command-line arguments (--key=value format).
    // Keys are converted to dot-separated paths.
    void load_from_args(int argc, char** argv);

    // Load from environment variables.
    // E.g., ZERO_SERVER_PORT=8080 sets "server.port" = 8080
    void load_from_env(const std::string& prefix = "ZERO_");

    // ============================================================
    // Access configuration values
    // ============================================================

    // Get a typed value with a dot-separated path.
    // Path: "section.subsection.key"
    template <typename T>
    T get(const std::string& path, const T& default_val = T{}) const {
        auto p = parse_path(path);
        auto* section = find_section(p.section_path);
        if (section) {
            return section->get<T>(p.key, default_val);
        }
        return default_val;
    }

    // Set a typed value
    template <typename T>
    void set(const std::string& path, const T& value) {
        auto p = parse_path(path);
        auto section = get_or_create_section(p.section_path);
        section->set<T>(p.key, value);
    }

    // Convenience accessors
    int get_int(const std::string& path, int default_val = 0) const;
    int64_t get_int64(const std::string& path,
                        int64_t default_val = 0) const;
    double get_double(const std::string& path,
                        double default_val = 0.0) const;
    bool get_bool(const std::string& path,
                    bool default_val = false) const;
    std::string get_string(const std::string& path,
                             const std::string& default_val = "") const;

    // Set convenience
    void set_int(const std::string& path, int value);
    void set_int64(const std::string& path, int64_t value);
    void set_double(const std::string& path, double value);
    void set_bool(const std::string& path, bool value);
    void set_string(const std::string& path, const std::string& value);

    // ============================================================
    // Change listeners
    // ============================================================

    // Watch a config path for changes
    template <typename T>
    void watch(const std::string& path,
                typename ConfigValue<T>::Listener cb) {
        auto p = parse_path(path);
        auto section = get_or_create_section(p.section_path);
        section->set<T>(p.key, T{});  // Ensure the key exists
        section->watch<T>(p.key, std::move(cb));
    }

    // ============================================================
    // Utilities
    // ============================================================

    // Check if a config path exists
    bool has(const std::string& path) const;

    // Remove a config path
    bool remove(const std::string& path);

    // Get all config as a YAML-like string (for debugging/dumping)
    std::string to_string() const;

    // Clear all configuration
    void clear();

private:
    Config() = default;
    ~Config() = default;

    struct PathParts {
        std::string section_path;  // Everything before the last dot
        std::string key;           // Everything after the last dot
    };

    static PathParts parse_path(const std::string& path);

    const ConfigSection* find_section(const std::string& path) const;
    std::shared_ptr<ConfigSection> get_or_create_section(
        const std::string& path);

    mutable std::shared_mutex mutex_;
    std::shared_ptr<ConfigSection> root_;
};

} // namespace zero
