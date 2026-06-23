#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include "zero/base/noncopyable.h"

namespace zero {

// ============ StackPool (栈内存池) ============
//
// 管理预分配的协程栈内存块。
// 避免每次创建/销毁 fiber 时频繁 mmap/munmap。
//
// 设计:
//   - 每个栈块: stack_size + guard_page (4KB)
//   - guard page 用 mprotect(PROT_NONE) 保护, 栈溢出触发 SIGSEGV
//   - 释放的栈不归还 OS, 放入 free list 供后续复用
//   - 线程安全 (生产-消费场景锁竞争低)
class StackPool : public Noncopyable {
public:
    // @param stack_size  每个栈的可用大小 (默认 128KB)
    // @param prealloc    预分配栈块数量 (默认 8)
    explicit StackPool(size_t stack_size = 128 * 1024, size_t prealloc = 8);
    ~StackPool();

    // 申请一个栈 (返回栈基址)
    void* allocate();

    // 归还栈 (放回池中, 不释放)
    void deallocate(void* stack);

    // 总容量 (预分配 + 动态分配)
    size_t capacity() const { return total_allocated_; }

    // 空闲栈数量
    size_t available() const;

    // 栈可用大小 (不含 guard page)
    size_t stackSize() const { return stack_size_; }

    // 总分配大小 (含 guard page)
    size_t totalSize() const { return total_size_; }

    // 单例 (全局共享)
    static StackPool& GetInstance();

private:
    struct StackBlock {
        void*  ptr;       // mmap 返回的地址
        size_t total_size;// 总映射大小 (stack_size + guard_page)
    };

    void* do_allocate();  // 真正分配新栈 (mmap)

    size_t stack_size_;   // 可用栈大小
    size_t total_size_;   // stack_size_ + 4KB guard page
    size_t total_allocated_{0};

    std::vector<void*> free_list_;  // 空闲栈列表
    std::mutex mutex_;              // 保护 free_list_
};

} // namespace zero
