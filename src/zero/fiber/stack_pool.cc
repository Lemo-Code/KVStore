#include "zero/fiber/stack_pool.h"

#include <sys/mman.h>
#include <cstring>
#include <stdexcept>

namespace zero {

static constexpr size_t kGuardPageSize = 4096;  // 4KB guard page

StackPool::StackPool(size_t stack_size, size_t prealloc)
    : stack_size_(stack_size)
    , total_size_(stack_size_ + kGuardPageSize) {

    // 预分配栈块
    free_list_.reserve(prealloc);
    for (size_t i = 0; i < prealloc; ++i) {
        free_list_.push_back(do_allocate());
    }
}

StackPool::~StackPool() {
    // 归还所有栈到 OS
    for (void* ptr : free_list_) {
        // ptr 是栈的可用区域起始地址
        // 需要找回 mmap 的原始地址 (= ptr - kGuardPageSize)
        char* base = static_cast<char*>(ptr) - kGuardPageSize;
        munmap(base, total_size_);
    }
    free_list_.clear();
}

void* StackPool::allocate() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!free_list_.empty()) {
        void* stack = free_list_.back();
        free_list_.pop_back();
        return stack;
    }

    // 池空 → 动态分配
    return do_allocate();
}

void StackPool::deallocate(void* stack) {
    std::lock_guard<std::mutex> lock(mutex_);
    free_list_.push_back(stack);
}

size_t StackPool::available() const {
    return free_list_.size();
}

void* StackPool::do_allocate() {
    // mmap 分配: guard page + stack
    // 布局 (低地址 → 高地址):
    //   [guard page  4KB]  PROT_NONE  (不可读写, 溢出时 SIGSEGV)
    //   [usable stack N KB] PROT_READ | PROT_WRITE
    //
    // 返回 usable stack 的起始地址 (高地址减方向使用)

    void* addr = mmap(nullptr, total_size_,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1, 0);

    if (addr == MAP_FAILED) {
        throw std::runtime_error("StackPool: mmap failed for " +
                                 std::to_string(total_size_) + " bytes");
    }

    // 设置 guard page 为不可访问
    if (mprotect(addr, kGuardPageSize, PROT_NONE) != 0) {
        munmap(addr, total_size_);
        throw std::runtime_error("StackPool: mprotect guard page failed");
    }

    // usable stack 从 guard page 之后开始, 一直延伸到末尾
    void* usable_start = static_cast<char*>(addr) + kGuardPageSize;

    ++total_allocated_;
    return usable_start;
}

StackPool& StackPool::GetInstance() {
    static StackPool instance;
    return instance;
}

} // namespace zero
