#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "zero/base/noncopyable.h"
#include "zero/fiber/fiber.h"

namespace zero {

// ============ FiberPool (Fiber 对象池) ============
//
// 复用 Fiber 对象，避免频繁构造/析构带来的 shared_ptr 分配开销。
//
// 用法:
//   Fiber::ptr fiber = FiberPool::GetInstance().acquire(cb);
//   // ... fiber 执行完毕后自动归还到池中
class FiberPool : public Noncopyable {
public:
    // @param prealloc 预分配 Fiber 数量 (默认 128)
    explicit FiberPool(size_t prealloc = 128);
    ~FiberPool();

    // 获取一个 Fiber (优先从池中取, 池空则新建)
    Fiber::ptr acquire(std::function<void()> cb,
                      size_t stack_size = 0,
                      const std::string& name = "");

    // 归还 Fiber 到池中 (由 Fiber::~Fiber 或 Scheduler 调用)
    void release(Fiber* fiber);

    // 统计信息
    size_t available() const;
    size_t totalCreated() const { return total_created_; }

    // 单例
    static FiberPool& GetInstance();

private:
    Fiber* do_create(std::function<void()> cb,
                     size_t stack_size,
                     const std::string& name);

    std::vector<Fiber*> pool_;      // 空闲 Fiber 列表
    mutable std::mutex mutex_;
    std::atomic<size_t> total_created_{0};
};

} // namespace zero
