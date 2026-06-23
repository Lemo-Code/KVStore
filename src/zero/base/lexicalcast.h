#pragma once

#include <sstream>
#include <string>
#include <typeinfo>
#include <cstdlib>

namespace zero {

// 类型转换工具: Src → Dst
// 特化实现支持自定义类型; 默认使用 stringstream

template<typename Dst, typename Src>
class LexicalCast {
public:
    Dst operator()(const Src& src) {
        Dst dst;
        std::stringstream ss;
        ss << src;
        ss >> dst;
        return dst;
    }
};

// 特化: string → int
template<>
class LexicalCast<int, std::string> {
public:
    int operator()(const std::string& src) {
        return std::atoi(src.c_str());
    }
};

// 特化: string → double
template<>
class LexicalCast<double, std::string> {
public:
    double operator()(const std::string& src) {
        return std::atof(src.c_str());
    }
};

// 特化: string → int64_t
template<>
class LexicalCast<int64_t, std::string> {
public:
    int64_t operator()(const std::string& src) {
        return std::atoll(src.c_str());
    }
};

// 特化: int → string
template<>
class LexicalCast<std::string, int> {
public:
    std::string operator()(int src) {
        return std::to_string(src);
    }
};

// 特化: double → string
template<>
class LexicalCast<std::string, double> {
public:
    std::string operator()(double src) {
        return std::to_string(src);
    }
};

// 特化: int64_t → string
template<>
class LexicalCast<std::string, int64_t> {
public:
    std::string operator()(int64_t src) {
        return std::to_string(src);
    }
};

// 特化: string → string (直通)
template<>
class LexicalCast<std::string, std::string> {
public:
    const std::string& operator()(const std::string& src) {
        return src;
    }
};

} // namespace zero
