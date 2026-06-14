#ifndef NET_DESIGN_RUNTIME_CONTEXT_H
#define NET_DESIGN_RUNTIME_CONTEXT_H

#include <cstddef>

namespace net {

/**
 * @brief 协程栈上下文切换的唯一点。
 *
 * Phase 1: ucontext 实现
 * Phase 2: fcontext / 平台汇编
 */
class Context {
 public:
  using Entry = void (*)();

  Context(Entry entry, void* stack, size_t stack_size);
  ~Context();

  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  void swapIn(Context& from);
  void swapOut(Context& to);

  void* stackTop() const;

 private:
  struct Impl;
  Impl* impl_ = nullptr;
};

}  // namespace net

#endif  // NET_DESIGN_RUNTIME_CONTEXT_H
