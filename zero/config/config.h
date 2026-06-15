#pragma once

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <vector>
#include <yaml-cpp/yaml.h>

#include "zero/thread/mutex.h"
#include "zero/base/noncopyable.h"
#include "zero/log/log.h"

namespace zero {

// 类型特征: 是否是容器 (fromString 不可用, 必须走 fromYaml)
template<typename T, typename = void>
struct is_container : std::false_type {};

template<typename T>
struct is_container<std::vector<T>> : std::true_type {};

template<typename T>
struct is_container<std::list<T>> : std::true_type {};

template<typename T>
struct is_container<std::set<T>> : std::true_type {};

template<typename T>
struct is_container<std::map<std::string, T>> : std::true_type {};

template<typename T>
inline constexpr bool is_container_v = is_container<T>::value;

// ============ ConfigVarBase ============
class ConfigVarBase {
public:
    using ptr = std::shared_ptr<ConfigVarBase>;

    ConfigVarBase(const std::string& name, const std::string& description);
    virtual ~ConfigVarBase() = default;

    const std::string& getName()        const { return name_; }
    const std::string& getDescription() const { return description_; }

    virtual std::string toString() const = 0;
    virtual bool fromString(const std::string& val) = 0;
    virtual std::string getTypeName() const = 0;

    // 从 YAML 节点直接设置 (用于容器类型)
    virtual bool fromYaml(const YAML::Node& node) {
        if (!node.IsScalar()) return false;
        return fromString(node.as<std::string>());
    }

    // 导出为 YAML 节点
    virtual YAML::Node toYaml() const {
        return YAML::Node(toString());
    }

private:
    std::string name_;
    std::string description_;
};

// ============ YAML 序列化辅助 ============
// 标量: 直接 as<T>()
template<typename T>
struct YamlSerializer {
    static T from(const YAML::Node& node) {
        if (node.IsScalar()) return node.as<T>();
        return T{};
    }
    static YAML::Node to(const T& val) { return YAML::Node(val); }
};

// vector<T>
template<typename T>
struct YamlSerializer<std::vector<T>> {
    static std::vector<T> from(const YAML::Node& node) {
        std::vector<T> result;
        if (node.IsSequence()) {
            for (const auto& item : node) {
                result.push_back(YamlSerializer<T>::from(item));
            }
        }
        return result;
    }
    static YAML::Node to(const std::vector<T>& val) {
        YAML::Node node(YAML::NodeType::Sequence);
        for (const auto& item : val) {
            node.push_back(YamlSerializer<T>::to(item));
        }
        return node;
    }
};

// list<T>
template<typename T>
struct YamlSerializer<std::list<T>> {
    static std::list<T> from(const YAML::Node& node) {
        std::list<T> result;
        if (node.IsSequence()) {
            for (const auto& item : node) {
                result.push_back(YamlSerializer<T>::from(item));
            }
        }
        return result;
    }
    static YAML::Node to(const std::list<T>& val) {
        YAML::Node node(YAML::NodeType::Sequence);
        for (const auto& item : val) node.push_back(YamlSerializer<T>::to(item));
        return node;
    }
};

// set<T>
template<typename T>
struct YamlSerializer<std::set<T>> {
    static std::set<T> from(const YAML::Node& node) {
        std::set<T> result;
        if (node.IsSequence()) {
            for (const auto& item : node) {
                result.insert(YamlSerializer<T>::from(item));
            }
        }
        return result;
    }
    static YAML::Node to(const std::set<T>& val) {
        YAML::Node node(YAML::NodeType::Sequence);
        for (const auto& item : val) node.push_back(YamlSerializer<T>::to(item));
        return node;
    }
};

// map<string, T>
template<typename T>
struct YamlSerializer<std::map<std::string, T>> {
    static std::map<std::string, T> from(const YAML::Node& node) {
        std::map<std::string, T> result;
        if (node.IsMap()) {
            for (auto it = node.begin(); it != node.end(); ++it) {
                result[it->first.as<std::string>()] =
                    YamlSerializer<T>::from(it->second);
            }
        }
        return result;
    }
    static YAML::Node to(const std::map<std::string, T>& val) {
        YAML::Node node(YAML::NodeType::Map);
        for (const auto& [k, v] : val) {
            node[k] = YamlSerializer<T>::to(v);
        }
        return node;
    }
};

// ============ ConfigVar<T> ============
template<typename T>
class ConfigVar : public ConfigVarBase {
public:
    using ptr = std::shared_ptr<ConfigVar<T>>;
    using onChangeCb = std::function<void(const T& old_val, const T& new_val)>;

    ConfigVar(const std::string& name, const T& default_val,
              const std::string& description = "");

    void setValue(const T& val);
    const T& getValue() const { return val_; }

    std::string toString() const override {
        if constexpr (is_container_v<T>) {
            return "container (use YAML)";
        } else {
            RWMutex::ReadLock lock(mutex_);
            std::stringstream ss;
            ss << val_;
            return ss.str();
        }
    }

    bool fromString(const std::string& val) override {
        // 容器不支持字符串转换
        if constexpr (is_container_v<T>) {
            (void)val;
            return false;
        } else {
            std::stringstream ss(val);
            T new_val;
            ss >> new_val;
            if (ss.fail()) return false;
            setValue(new_val);
            return true;
        }
    }

    std::string getTypeName() const override {
        return typeid(T).name();
    }

    // YAML 支持 (所有类型通用)
    bool fromYaml(const YAML::Node& node) override {
        try {
            T new_val = YamlSerializer<T>::from(node);
            setValue(new_val);
            return true;
        } catch (const std::exception& e) {
            ZERO_LOG_ERROR(ZERO_LOG_ROOT()) << "ConfigVar::fromYaml error for " << name << ": " << e.what(); // was:
                    getName().c_str(), e.what());
            return false;
        }
    }

    YAML::Node toYaml() const override {
        RWMutex::ReadLock lock(mutex_);
        return YamlSerializer<T>::to(val_);
    }

    // 变更回调
    uint64_t addListener(onChangeCb cb) {
        uint64_t id = s_cb_id.fetch_add(1);
        RWMutex::WriteLock lock(mutex_);
        listeners_[id] = std::move(cb);
        return id;
    }
    void delListener(uint64_t id) {
        RWMutex::WriteLock lock(mutex_);
        listeners_.erase(id);
    }
    void clearListeners() {
        RWMutex::WriteLock lock(mutex_);
        listeners_.clear();
    }

private:
    T val_;
    mutable RWMutex mutex_;
    std::map<uint64_t, onChangeCb> listeners_;
    static std::atomic<uint64_t> s_cb_id;
};

// ============ Config ============
class Config {
public:
    template<typename T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name,
                                              const T& default_val,
                                              const std::string& desc = "");

    template<typename T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name);

    static void LoadFromYaml(const std::string& filepath);
    static void LoadFromYaml(const YAML::Node& root);

    static void Visit(std::function<void(ConfigVarBase::ptr)> cb);

private:
    using ConfigVarMap = std::map<std::string, ConfigVarBase::ptr>;
    static ConfigVarMap& GetDatas();
    static RWMutex& GetMutex();
};

} // namespace zero
