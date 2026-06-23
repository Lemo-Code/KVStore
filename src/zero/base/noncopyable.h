#pragma once

namespace zero {

// 禁止拷贝和赋值，用于资源管理类 (Thread, Socket, Fiber 等)
class Noncopyable {
public:
    Noncopyable() = default;
    ~Noncopyable() = default;

    Noncopyable(const Noncopyable&) = delete;
    Noncopyable& operator=(const Noncopyable&) = delete;

    Noncopyable(Noncopyable&&) = default;
    Noncopyable& operator=(Noncopyable&&) = default;
};

} // namespace zero
