#pragma once

#include <cstdint>
#include <sstream>
#include <string>

namespace lemo {
namespace utils {

template <typename From, typename To>
struct LexicalCast {
  To operator()(const From& val) const {
    std::stringstream ss;
    ss << val;
    To result = To();
    ss >> result;
    return result;
  }
};

template <>
struct LexicalCast<std::string, std::string> {
  std::string operator()(const std::string& val) const { return val; }
};

template <>
struct LexicalCast<std::string, int> {
  int operator()(const std::string& val) const { return std::stoi(val); }
};

template <>
struct LexicalCast<std::string, uint32_t> {
  uint32_t operator()(const std::string& val) const {
    return static_cast<uint32_t>(std::stoul(val));
  }
};

template <>
struct LexicalCast<std::string, uint64_t> {
  uint64_t operator()(const std::string& val) const {
    return static_cast<uint64_t>(std::stoull(val));
  }
};

template <>
struct LexicalCast<std::string, bool> {
  bool operator()(const std::string& val) const {
    if (val.empty()) return false;
    const char c = val[0];
    return c == '1' || c == 't' || c == 'T' || c == 'y' || c == 'Y';
  }
};

template <>
struct LexicalCast<std::string, double> {
  double operator()(const std::string& val) const { return std::stod(val); }
};

template <>
struct LexicalCast<int, std::string> {
  std::string operator()(const int& val) const { return std::to_string(val); }
};

template <>
struct LexicalCast<uint32_t, std::string> {
  std::string operator()(const uint32_t& val) const {
    return std::to_string(val);
  }
};

template <>
struct LexicalCast<uint64_t, std::string> {
  std::string operator()(const uint64_t& val) const {
    return std::to_string(val);
  }
};

template <>
struct LexicalCast<bool, std::string> {
  std::string operator()(const bool& val) const { return val ? "true" : "false"; }
};

template <>
struct LexicalCast<double, std::string> {
  std::string operator()(const double& val) const { return std::to_string(val); }
};

}  // namespace utils
}  // namespace lemo
