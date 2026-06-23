// zero context_swap — context save/restore for fiber switching
//
// Implements the low-level context switching between two fiber contexts.
// Saves callee-saved registers into *from, restores from *to.
//
// C function signature:
//   extern "C" void context_swap(Context* from, Context* to) noexcept;
//
// Calling convention: rdi = from (x0 on ARM), rsi = to (x1 on ARM)

#if defined(__x86_64__) || defined(_M_X64)

.text
.global context_swap
.type context_swap, @function
context_swap:
    // ============================================================
    // Save current context to 'from' (rdi)
    // ============================================================
    // Offsets: rsp=0, rbp=8, rbx=16, r12=24, r13=32, r14=40, r15=48, rip=56
    movq %rsp, 0(%rdi)
    movq %rbp, 8(%rdi)
    movq %rbx, 16(%rdi)
    movq %r12, 24(%rdi)
    movq %r13, 32(%rdi)
    movq %r14, 40(%rdi)
    movq %r15, 48(%rdi)
    // Save return address (rip) from the top of the stack
    movq (%rsp), %rax
    movq %rax, 56(%rdi)

    // ============================================================
    // Restore context from 'to' (rsi)
    // ============================================================
    // Restore stack pointer first — subsequent accesses use the new stack
    movq 0(%rsi), %rsp
    movq 8(%rsi), %rbp
    movq 16(%rsi), %rbx
    movq 24(%rsi), %r12
    movq 32(%rsi), %r13
    movq 40(%rsi), %r14
    movq 48(%rsi), %r15
    // Load rip into rax, push it, then ret to jump to target
    movq 56(%rsi), %rax
    pushq %rax
    ret

.size context_swap, .-context_swap

#elif defined(__aarch64__) || defined(__arm64__)

.text
.global context_swap
.type context_swap, @function
context_swap:
    // ============================================================
    // Save callee-saved integer registers (x19-x30) to from->x19..
    // ============================================================
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]    // fp (x29), lr (x30)

    // ============================================================
    // Save callee-saved floating-point registers (d8-d15)
    // ============================================================
    stp d8,  d9,  [x0, #96]
    stp d10, d11, [x0, #112]
    stp d12, d13, [x0, #128]
    stp d14, d15, [x0, #144]

    // Save stack pointer
    mov x2, sp
    str x2, [x0, #160]

    // ============================================================
    // Restore context from 'to'
    // ============================================================

    // Restore stack pointer first
    ldr x2, [x1, #160]
    mov sp, x2

    // Restore floating-point registers
    ldp d14, d15, [x1, #144]
    ldp d12, d13, [x1, #128]
    ldp d10, d11, [x1, #112]
    ldp d8,  d9,  [x1, #96]

    // Restore integer registers
    ldp x29, x30, [x1, #80]
    ldp x27, x28, [x1, #64]
    ldp x25, x26, [x1, #48]
    ldp x23, x24, [x1, #32]
    ldp x21, x22, [x1, #16]
    ldp x19, x20, [x1, #0]

    ret

.size context_swap, .-context_swap

#else
#error "Unsupported architecture: only x86_64 and aarch64 are supported"
#endif
