#ifndef NET_LEXICAL_CAST_H
#define NET_LEXICAL_CAST_H

#include <yaml-cpp/yaml.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
#include <list>
#include <set>
#include <map>

namespace net {

/**
 * @brief 通用类型转换模板（类似 boost::lexical_cast）
 * 
 * 默认使用 stringstream 进行转换，基本类型可特化优化
 */
template<typename From, typename To>
struct LexicalCast {
  To operator()(const From& val) {
    std::stringstream ss;
    ss << val;
    To result;
    ss >> result;
    return result;
  }
};

// string -> string 特化
template<>
struct LexicalCast<std::string, std::string> {
  std::string operator()(const std::string& val) { return val; }
};

// string -> int 特化
template<>
struct LexicalCast<std::string, int> {
  int operator()(const std::string& val) {
    return std::stoi(val);
  }
};

// string -> uint32_t 特化
template<>
struct LexicalCast<std::string, uint32_t> {
  uint32_t operator()(const std::string& val) {
    return static_cast<uint32_t>(std::stoul(val));
  }
};

// string -> uint64_t 特化
template<>
struct LexicalCast<std::string, uint64_t> {
  uint64_t operator()(const std::string& val) {
    return static_cast<uint64_t>(std::stoull(val));
  }
};

// string -> bool 特化
template<>
struct LexicalCast<std::string, bool> {
  bool operator()(const std::string& val) {
    if (val.empty()) return false;
    char c = val[0];
    return c == '1' || c == 't' || c == 'T' || c == 'y' || c == 'Y';
  }
};

// string -> double 特化
template<>
struct LexicalCast<std::string, double> {
  double operator()(const std::string& val) {
    return std::stod(val);
  }
};

// int -> string 特化
template<>
struct LexicalCast<int, std::string> {
  std::string operator()(int val) {
    return std::to_string(val);
  }
};

// uint32_t -> string 特化
template<>
struct LexicalCast<uint32_t, std::string> {
  std::string operator()(uint32_t val) {
    return std::to_string(val);
  }
};

// uint64_t -> string 特化
template<>
struct LexicalCast<uint64_t, std::string> {
  std::string operator()(uint64_t val) {
    return std::to_string(val);
  }
};

// bool -> string 特化
template<>
struct LexicalCast<bool, std::string> {
  std::string operator()(bool val) {
    return val ? "true" : "false";
  }
};

// double -> string 特化
template<>
struct LexicalCast<double, std::string> {
  std::string operator()(double val) {
    return std::to_string(val);
  }
};

// ==================== YAML 支持 ====================

// string -> YAML::Node 特化
template<>
struct LexicalCast<std::string, YAML::Node> {
  YAML::Node operator()(const std::string& val) {
    try {
      return YAML::Load(val);
    } catch (...) {
      YAML::Node node;
      node = val;
      return node;
    }
  }
};

// YAML::Node -> string 特化
template<>
struct LexicalCast<YAML::Node, std::string> {
  std::string operator()(const YAML::Node& val) {
    if (val.IsScalar()) {
      return val.Scalar();
    }
    YAML::Emitter emitter;
    emitter << val;
    return emitter.c_str();
  }
};

// YAML::Node -> T 特化（标量类型）
template<typename T>
struct LexicalCast<YAML::Node, T> {
  T operator()(const YAML::Node& node) {
    if (node.IsScalar()) {
      return LexicalCast<std::string, T>()(node.Scalar());
    }
    // 非标量转为字符串再转换
    YAML::Emitter emitter;
    emitter << node;
    return LexicalCast<std::string, T>()(emitter.c_str());
  }
};

// T -> YAML::Node 特化
template<typename T>
struct LexicalCast<T, YAML::Node> {
  YAML::Node operator()(const T& val) {
    std::string str = LexicalCast<T, std::string>()(val);
    try {
      return YAML::Load(str);
    } catch (...) {
      YAML::Node node;
      node = str;
      return node;
    }
  }
};

// ==================== 容器类型 ====================

// string -> vector<T> 特化（逗号分隔）
template<typename T>
struct LexicalCast<std::string, std::vector<T>> {
  std::vector<T> operator()(const std::string& val) {
    std::vector<T> result;
    if (val.empty()) return result;
    
    // 先尝试作为 YAML 解析
    try {
      YAML::Node node = YAML::Load(val);
      if (node.IsSequence()) {
        for (size_t i = 0; i < node.size(); ++i) {
          result.push_back(LexicalCast<YAML::Node, T>()(node[i]));
        }
        return result;
      }
    } catch (...) {
      // 回退到逗号分隔解析
    }
    
    std::stringstream ss(val);
    std::string item;
    while (std::getline(ss, item, ',')) {
      // 去除前后空格
      size_t start = item.find_first_not_of(" \t");
      size_t end = item.find_last_not_of(" \t");
      if (start != std::string::npos && end != std::string::npos) {
        item = item.substr(start, end - start + 1);
      }
      if (!item.empty()) {
        result.push_back(LexicalCast<std::string, T>()(item));
      }
    }
    return result;
  }
};

// vector<T> -> string 特化
template<typename T>
struct LexicalCast<std::vector<T>, std::string> {
  std::string operator()(const std::vector<T>& val) {
    YAML::Node node;
    for (const auto& item : val) {
      node.push_back(LexicalCast<T, YAML::Node>()(item));
    }
    YAML::Emitter emitter;
    emitter << node;
    return emitter.c_str();
  }
};

// string -> list<T> 特化
template<typename T>
struct LexicalCast<std::string, std::list<T>> {
  std::list<T> operator()(const std::string& val) {
    std::list<T> result;
    if (val.empty()) return result;
    
    // 先尝试作为 YAML 解析
    try {
      YAML::Node node = YAML::Load(val);
      if (node.IsSequence()) {
        for (size_t i = 0; i < node.size(); ++i) {
          result.push_back(LexicalCast<YAML::Node, T>()(node[i]));
        }
        return result;
      }
    } catch (...) {
      // 回退到逗号分隔解析
    }
    
    std::stringstream ss(val);
    std::string item;
    while (std::getline(ss, item, ',')) {
      size_t start = item.find_first_not_of(" \t");
      size_t end = item.find_last_not_of(" \t");
      if (start != std::string::npos && end != std::string::npos) {
        item = item.substr(start, end - start + 1);
      }
      if (!item.empty()) {
        result.push_back(LexicalCast<std::string, T>()(item));
      }
    }
    return result;
  }
};

// list<T> -> string 特化
template<typename T>
struct LexicalCast<std::list<T>, std::string> {
  std::string operator()(const std::list<T>& val) {
    YAML::Node node;
    for (const auto& item : val) {
      node.push_back(LexicalCast<T, YAML::Node>()(item));
    }
    YAML::Emitter emitter;
    emitter << node;
    return emitter.c_str();
  }
};

// string -> set<T> 特化
template<typename T>
struct LexicalCast<std::string, std::set<T>> {
  std::set<T> operator()(const std::string& val) {
    std::set<T> result;
    if (val.empty()) return result;
    
    // 先尝试作为 YAML 解析
    try {
      YAML::Node node = YAML::Load(val);
      if (node.IsSequence()) {
        for (size_t i = 0; i < node.size(); ++i) {
          result.insert(LexicalCast<YAML::Node, T>()(node[i]));
        }
        return result;
      }
    } catch (...) {
      // 回退到逗号分隔解析
    }
    
    std::stringstream ss(val);
    std::string item;
    while (std::getline(ss, item, ',')) {
      size_t start = item.find_first_not_of(" \t");
      size_t end = item.find_last_not_of(" \t");
      if (start != std::string::npos && end != std::string::npos) {
        item = item.substr(start, end - start + 1);
      }
      if (!item.empty()) {
        result.insert(LexicalCast<std::string, T>()(item));
      }
    }
    return result;
  }
};

// set<T> -> string 特化
template<typename T>
struct LexicalCast<std::set<T>, std::string> {
  std::string operator()(const std::set<T>& val) {
    YAML::Node node;
    for (const auto& item : val) {
      node.push_back(LexicalCast<T, YAML::Node>()(item));
    }
    YAML::Emitter emitter;
    emitter << node;
    return emitter.c_str();
  }
};

// string -> map<string, T> 特化
template<typename T>
struct LexicalCast<std::string, std::map<std::string, T>> {
  std::map<std::string, T> operator()(const std::string& val) {
    std::map<std::string, T> result;
    if (val.empty()) return result;
    
    // 先尝试作为 YAML 解析
    try {
      YAML::Node node = YAML::Load(val);
      if (node.IsMap()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
          std::string key = it->first.as<std::string>();
          result[key] = LexicalCast<YAML::Node, T>()(it->second);
        }
        return result;
      }
    } catch (...) {
      // 回退到逗号分隔解析
    }
    
    std::stringstream ss(val);
    std::string item;
    while (std::getline(ss, item, ',')) {
      size_t start = item.find_first_not_of(" \t");
      size_t end = item.find_last_not_of(" \t");
      if (start != std::string::npos && end != std::string::npos) {
        item = item.substr(start, end - start + 1);
      }
      
      size_t colon = item.find(':');
      if (colon != std::string::npos) {
        std::string key = item.substr(0, colon);
        std::string value = item.substr(colon + 1);
        
        // 去除 key/value 的前后空格
        size_t key_start = key.find_first_not_of(" \t");
        size_t key_end = key.find_last_not_of(" \t");
        if (key_start != std::string::npos && key_end != std::string::npos) {
          key = key.substr(key_start, key_end - key_start + 1);
        }
        
        size_t val_start = value.find_first_not_of(" \t");
        size_t val_end = value.find_last_not_of(" \t");
        if (val_start != std::string::npos && val_end != std::string::npos) {
          value = value.substr(val_start, val_end - val_start + 1);
        }
        
        if (!key.empty()) {
          result[key] = LexicalCast<std::string, T>()(value);
        }
      }
    }
    return result;
  }
};

// map<string, T> -> string 特化
template<typename T>
struct LexicalCast<std::map<std::string, T>, std::string> {
  std::string operator()(const std::map<std::string, T>& val) {
    YAML::Node node;
    for (const auto& kv : val) {
      node[kv.first] = LexicalCast<T, YAML::Node>()(kv.second);
    }
    YAML::Emitter emitter;
    emitter << node;
    return emitter.c_str();
  }
};

}  // namespace net

#endif  // NET_LEXICAL_CAST_H
