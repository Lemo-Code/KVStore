#pragma once

#include <cstdint>
#include <string>

namespace lemo {
namespace utils {

uint32_t GetThreadId();
const std::string& GetThreadName();
void SetThreadName(const std::string& name);

}  // namespace utils
}  // namespace lemo
