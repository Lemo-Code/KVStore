#ifndef __SYLAR_FIBER_LOCAL_H__
#define __SYLAR_FIBER_LOCAL_H__

#include "sylar/fiber.h"
#include <memory>
#include <atomic>

namespace sylar {

// 全局存储由 fiber_local.cc 实现，模板仅声明
size_t FiberLocalAllocKey();
void FiberLocalSet(uint64_t fiber_id, size_t key, std::shared_ptr<void> value);
std::shared_ptr<void> FiberLocalGet(uint64_t fiber_id, size_t key);
void FiberLocalEraseFiber(uint64_t fiber_id);

/**
 * @brief 协程局部变量（FiberLocal）
 * 每个协程拥有独立副本，多任务调度时互不污染。
 * 协程销毁时需调用 FiberLocalEraseFiber(fiber_id) 清理。
 */
template<typename T>
class FiberLocal {
public:
    FiberLocal() {
        m_key = FiberLocalAllocKey();
    }

    T* get() {
        uint64_t fid = Fiber::GetFiberId();
        std::shared_ptr<void> p = FiberLocalGet(fid, m_key);
        return static_cast<T*>(p.get());
    }

    void set(std::shared_ptr<T> v) {
        uint64_t fid = Fiber::GetFiberId();
        FiberLocalSet(fid, m_key, std::static_pointer_cast<void>(v));
    }

    void set(const T& v) {
        set(std::make_shared<T>(v));
    }

    void reset() {
        uint64_t fid = Fiber::GetFiberId();
        FiberLocalSet(fid, m_key, nullptr);
    }

private:
    size_t m_key;
};

} // namespace sylar

#endif
