#pragma once

#include <memory>

namespace zero {

// 线程安全的 Singleton 模板 (C++11 Magic Statics)
// 使用: using MyMgr = Singleton<MyManager>;
template<typename T>
class Singleton {
public:
    static T* GetInstance() {
        static T instance;
        return &instance;
    }

protected:
    Singleton() = default;
    ~Singleton() = default;
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
};

} // namespace zero
