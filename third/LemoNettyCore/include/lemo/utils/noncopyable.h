#pragma once

namespace lemo {
namespace utils {

class NonCopyable {
 public:
  NonCopyable() {}
  ~NonCopyable() {}

  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;
};

}  // namespace utils
}  // namespace lemo
