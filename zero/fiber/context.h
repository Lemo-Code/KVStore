#pragma once

#include <cstddef>
#include <cstdint>

namespace zero {

// Fiber 上下文 — ARM64 (AArch64) 版本
//
// 保存/恢复 AAPCS64 callee-saved 寄存器:
//   GP:  x19 - x28  (10 个, 80 bytes)
//        x29 = FP   (frame pointer)
//        x30 = LR   (link register / return address)
//   SP:             (stack pointer)
//   SIMD: d8 - d15  (8 个 64-bit FP/SIMD, 64 bytes)
//
// 总计: 21 × 8 = 168 bytes
// 结构体布局与 context.S 强耦合, 修改需同步

struct Context {
    // GP callee-saved registers (offset 0-79)
    void* x19;   //  0
    void* x20;   //  8
    void* x21;   // 16
    void* x22;   // 24
    void* x23;   // 32
    void* x24;   // 40
    void* x25;   // 48
    void* x26;   // 56
    void* x27;   // 64
    void* x28;   // 72

    // Frame pointer and link register (offset 80-95)
    void* fp;    // x29 = 80
    void* lr;    // x30 = 88

    // Stack pointer (offset 96-103)
    void* sp;    // 96

    // SIMD/FP callee-saved (offset 104-167)
    uint64_t d8;   // 104
    uint64_t d9;   // 112
    uint64_t d10;  // 120
    uint64_t d11;  // 128
    uint64_t d12;  // 136
    uint64_t d13;  // 144
    uint64_t d14;  // 152
    uint64_t d15;  // 160
};

static_assert(sizeof(Context) == 168, "Context size must be 168 bytes (21 × 8) for ARM64");

// context_swap:  保存当前上下文到 from, 恢复 to 的上下文
// context_init:  初始化上下文以在首次恢复时执行 entry(arg)
extern "C" {

void context_swap(Context* from, Context* to) noexcept;

void context_init(Context* ctx, void* stack_base, size_t stack_size,
                  void (*entry)(void*), void* arg) noexcept;

}

} // namespace zero
