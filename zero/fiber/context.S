// ARM64 (AArch64) 协程上下文切换
//
// AAPCS64 callee-saved registers:
//   GP:   x19-x28, x29(FP), x30(LR)
//   SIMD: d8-d15
//   SP
//
// Context layout (21 × 8 = 168 bytes):
//   [0-79]:   x19-x28   (stp/ldp pairs)
//   [80-95]:  x29, x30  (FP, LR)
//   [96-103]: SP
//   [104-167]: d8-d15   (stp/ldp pairs)

.text

// ============================================================
// context_swap(Context* from=x0, Context* to=x1)
//
// 保存当前 callee-saved 状态到 from, 从 to 恢复
// ============================================================
.globl context_swap
.type  context_swap, %function
context_swap:
    // ---- 保存到 from (x0) ----
    // GP: x19-x28
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]

    // FP + LR
    stp x29, x30, [x0, #80]

    // SP
    mov x2, sp
    str x2, [x0, #96]

    // SIMD/FP: d8-d15
    stp d8,  d9,  [x0, #104]
    stp d10, d11, [x0, #120]
    stp d12, d13, [x0, #136]
    stp d14, d15, [x0, #152]

    // ---- 从 to (x1) 恢复 ----
    // GP: x19-x28
    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ldp x23, x24, [x1, #32]
    ldp x25, x26, [x1, #48]
    ldp x27, x28, [x1, #64]

    // FP + LR
    ldp x29, x30, [x1, #80]

    // SP
    ldr x2, [x1, #96]
    mov sp, x2

    // SIMD/FP: d8-d15
    ldp d8,  d9,  [x1, #104]
    ldp d10, d11, [x1, #120]
    ldp d12, d13, [x1, #136]
    ldp d14, d15, [x1, #152]

    // x30(LR) 已恢复 — ret 跳转到目标上下文
    ret

.size context_swap, .-context_swap


// ============================================================
// fiber_entry_trampoline — 新建 fiber 的入口蹦床
//
// context_init 在栈上布置了:
//   [padding][padding][arg][entry_fn]
//   ↑ SP 初始指向这里 (最低地址)
//
// context_swap 执行 ret 后到达此函数, SP 指向栈上数据
// ============================================================
.globl fiber_entry_trampoline
.type  fiber_entry_trampoline, %function
fiber_entry_trampoline:
    // 从栈上加载 arg 和 entry_fn
    // SP 指向 [arg][entry_fn][padding][padding]
    ldp x0, x1, [sp]            // x0 = arg (sp+0), x1 = entry_fn (sp+8)
    add sp, sp, #32             // 恢复 SP 到对齐的栈顶

    // 调用 entry(arg)
    // 调用前 SP 16 字节对齐 ✓
    blr x1

    // entry 返回 — 不应该到达这里
    // fiber 应在业务代码中通过 YieldToHold/YeildToReady 退出
    // 或通过 MainFunc 的 context_swap 回到调度器
    udf #0                      // 触发未定义指令异常 → 调试可见

.size fiber_entry_trampoline, .-fiber_entry_trampoline


// ============================================================
// context_init(Context* ctx=x0, void* stack_base=x1,
//              size_t stack_size=x2,
//              void (*entry)(void*)=x3, void* arg=x4)
//
// 在栈上布置初始帧:
//   栈顶 (高地址)
//   [padding]    ← x1 = top - 8
//   [padding]    ← x1 = top - 16
//   [arg]        ← x1 = top - 24
//   [entry_fn]   ← x1 = top - 32  ← SP 初始值
//
//   LR(x30) = fiber_entry_trampoline
// ============================================================
.globl context_init
.type  context_init, %function
context_init:
    // 计算栈顶: top = stack_base + stack_size, 16字节对齐
    add x5, x1, x2              // x5 = base + size
    and x5, x5, #-16            // x5 = 16-byte aligned top

    // 布置初始栈帧 (从高地址向下 push)
    // 栈布局 (低→高): [arg][entry_fn][padding][padding]  共 32 bytes
    sub x5, x5, #8
    str xzr, [x5]               // sp+24: padding

    sub x5, x5, #8
    str xzr, [x5]               // sp+16: padding

    sub x5, x5, #8
    str x3, [x5]                // sp+8:  entry_fn

    sub x5, x5, #8
    str x4, [x5]                // sp+0:  arg

    // x5 现在指向 arg (最低地址), 即初始 SP 的值

    // ---- 写入 Context ----
    // SP
    str x5, [x0, #96]           // ctx->sp = x5

    // LR → trampoline (ret 时跳转到此处)
    adr x6, fiber_entry_trampoline
    str x6, [x0, #88]           // ctx->lr = trampoline_addr

    // GP 寄存器清零 (新 fiber 无历史状态)
    stp xzr, xzr, [x0, #0]      // x19, x20
    stp xzr, xzr, [x0, #16]     // x21, x22
    stp xzr, xzr, [x0, #32]     // x23, x24
    stp xzr, xzr, [x0, #48]     // x25, x26
    stp xzr, xzr, [x0, #64]     // x27, x28
    str xzr, [x0, #80]          // FP(x29)

    // SIMD 寄存器清零
    stp xzr, xzr, [x0, #104]    // d8,  d9
    stp xzr, xzr, [x0, #120]    // d10, d11
    stp xzr, xzr, [x0, #136]    // d12, d13
    stp xzr, xzr, [x0, #152]    // d14, d15

    ret

.size context_init, .-context_init
