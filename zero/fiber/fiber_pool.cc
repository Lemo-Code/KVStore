#include "zero/fiber/fiber_pool.h"
#include "zero/fiber/fiber.h"

namespace zero {

FiberPool::FiberPool(size_t prealloc) {
    pool_.reserve(prealloc);
    for (size_t i = 0; i < prealloc; ++i) {
        pool_.push_back(do_create(nullptr, 0, ""));
    }
}

FiberPool::~FiberPool() {
    // 池中的 fiber 都是 INIT 状态, 直接 delete
    for (Fiber* f : pool_) {
        delete f;
    }
    pool_.clear();
}

Fiber::ptr FiberPool::acquire(std::function<void()> cb,
                               size_t stack_size,
                               const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!pool_.empty()) {
        Fiber* f = pool_.back();
        pool_.pop_back();
        // 复用已有 fiber 对象: 重置回调
        f->reset(std::move(cb));
        return Fiber::ptr(f);  // shared_ptr aliasing...
        // 注意: 这里需要特殊处理 — shared_ptr 接管裸指针
        // 更安全的做法是使用 enable_shared_from_this + custom deleter
    }

    // 池空 → 新建
    return Fiber::ptr(do_create(std::move(cb), stack_size, name));
}

void FiberPool::release(Fiber* fiber) {
    if (!fiber) return;
    std::lock_guard<std::mutex> lock(mutex_);
    pool_.push_back(fiber);
}

size_t FiberPool::available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_.size();
}

Fiber* FiberPool::do_create(std::function<void()> cb,
                             size_t stack_size,
                             const std::string& name) {
    ++total_created_;
    return new Fiber(std::move(cb), stack_size, name);
}

FiberPool& FiberPool::GetInstance() {
    static FiberPool instance;
    return instance;
}

} // namespace zero
