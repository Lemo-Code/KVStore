#pragma once

#include <string>
#include <vector>

namespace lemo {
namespace utils {

std::string Trim(const std::string& s);
std::string ToLower(std::string s);
bool StartsWith(const std::string& s, const char* prefix);
std::vector<std::string> Split(const std::string& s, char delim);

}  // namespace utils
}  // namespace lemo
