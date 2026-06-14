#pragma once

#include "lemo/utils/lexical_cast.h"
#include "lemo/utils/string_util.h"

#include <memory>
#include <mutex>
#include <string>

namespace lemo {
namespace config {

class ConfigVarBase {
 public:
  typedef std::shared_ptr<ConfigVarBase> ptr;

  ConfigVarBase(const std::string& name, const std::string& description);
  virtual ~ConfigVarBase() {}

  std::string GetName() const { return name_; }
  std::string GetDescription() const { return description_; }

  virtual std::string ToString() = 0;
  virtual bool FromString(const std::string& val) = 0;

 protected:
  std::string name_;
  std::string description_;
};

template <typename T,
          typename FromStr = utils::LexicalCast<std::string, T>,
          typename ToStr = utils::LexicalCast<T, std::string>>
class ConfigVar : public ConfigVarBase {
 public:
  typedef std::shared_ptr<ConfigVar> ptr;

  ConfigVar(const std::string& name, const T& default_val,
            const std::string& description = "")
      : ConfigVarBase(name, description), value_(default_val) {}

  void SetValue(const T& val) {
    std::lock_guard<std::mutex> lock(mutex_);
    value_ = val;
  }

  T GetValue() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return value_;
  }

  std::string ToString() override {
    std::lock_guard<std::mutex> lock(mutex_);
    return ToStr()(value_);
  }

  bool FromString(const std::string& val) override {
    try {
      SetValue(FromStr()(val));
      return true;
    } catch (...) {
      return false;
    }
  }

 private:
  T value_;
  mutable std::mutex mutex_;
};

}  // namespace config
}  // namespace lemo
