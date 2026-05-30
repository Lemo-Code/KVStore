#ifndef NET_CONFIG_VAR_H
#define NET_CONFIG_VAR_H

#include "config/lexical_cast.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <algorithm>

namespace net {

/**
 * @brief 配置项基类
 * 
 * 所有配置项的抽象基类，提供统一的字符串序列化接口
 */
class ConfigVarBase {
 public:
  typedef std::shared_ptr<ConfigVarBase> ptr;

  /**
   * @param name 配置项名称（会被转换为小写）
   * @param description 配置项描述
   */
  ConfigVarBase(const std::string& name, const std::string& description = "")
      : name_(name), description_(description) {
    // 名称统一转小写
    std::transform(name_.begin(), name_.end(), name_.begin(), ::tolower);
  }

  virtual ~ConfigVarBase() = default;

  /** 获取配置项名称 */
  std::string getName() const { return name_; }

  /** 获取配置项描述 */
  std::string getDescription() const { return description_; }

  /** 将值转换为字符串 */
  virtual std::string toString() = 0;

  /** 从字符串解析值 */
  virtual bool fromString(const std::string& val) = 0;

  /** 获取类型名称 */
  virtual std::string getTypeName() const = 0;

 protected:
  std::string name_;
  std::string description_;
};

/**
 * @brief 类型安全的配置项模板类
 * 
 * @tparam T 值类型
 * @tparam FromStr 字符串到类型的转换器
 * @tparam ToStr 类型到字符串的转换器
 */
template<typename T,
         typename FromStr = LexicalCast<std::string, T>,
         typename ToStr = LexicalCast<T, std::string>>
class ConfigVar : public ConfigVarBase {
 public:
  typedef std::shared_ptr<ConfigVar> ptr;
  typedef std::function<void(const T& old_val, const T& new_val)> OnChangeCallback;
  typedef std::mutex MutexType;

  /**
   * @param name 配置项名称
   * @param default_val 默认值
   * @param description 描述
   */
  ConfigVar(const std::string& name, const T& default_val,
            const std::string& description = "")
      : ConfigVarBase(name, description) {
    setValue(default_val);
  }

  /**
   * @brief 设置配置值
   * 
   * 如果值发生变化，会触发所有注册的回调函数
   */
  void setValue(const T& val) {
    T old_val;
    std::map<uint64_t, OnChangeCallback> callbacks_copy;
    {
      std::lock_guard<MutexType> lock(mutex_);
      if (val == value_) {
        return;
      }
      old_val = value_;
      callbacks_copy = callbacks_;
      value_ = val;
    }
    for (auto& cb : callbacks_copy) {
      cb.second(old_val, val);
    }
  }

  /**
   * @brief 获取配置值
   */
  T getValue() {
    std::lock_guard<MutexType> lock(mutex_);
    return value_;
  }

  /**
   * @brief 将值转换为字符串
   */
  std::string toString() override {
    try {
      std::lock_guard<MutexType> lock(mutex_);
      return ToStr()(value_);
    } catch (std::exception& e) {
      return "";
    }
  }

  /**
   * @brief 从字符串解析值
   */
  bool fromString(const std::string& val) override {
    try {
      setValue(FromStr()(val));
      return true;
    } catch (std::exception& e) {
      return false;
    }
  }

  /**
   * @brief 获取类型名称
   */
  std::string getTypeName() const override {
    return typeid(T).name();
  }

  /**
   * @brief 注册值变更回调
   * @param cb 回调函数
   * @return 回调ID，用于删除回调
   */
  uint64_t addListener(OnChangeCallback cb) {
    static uint64_t s_id = 0;
    std::lock_guard<MutexType> lock(mutex_);
    ++s_id;
    callbacks_[s_id] = cb;
    return s_id;
  }

  /**
   * @brief 删除回调
   * @param id 回调ID
   */
  void delListener(uint64_t id) {
    std::lock_guard<MutexType> lock(mutex_);
    callbacks_.erase(id);
  }

  /**
   * @brief 清空所有回调
   */
  void clearListeners() {
    std::lock_guard<MutexType> lock(mutex_);
    callbacks_.clear();
  }

 private:
  T value_;
  std::map<uint64_t, OnChangeCallback> callbacks_;
  mutable MutexType mutex_;
};

}  // namespace net

#endif  // NET_CONFIG_VAR_H
