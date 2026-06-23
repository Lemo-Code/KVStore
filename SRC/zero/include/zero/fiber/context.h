// zero Context — register snapshot for fiber switching (x86_64 + ARM64)
//
// x86_64 System V ABI callee-saved registers:
//   rsp, rbp, rbx, r12, r13, r14, r15, rip
//   (8 registers * 8 bytes = 64 bytes)
//
// ARM64 AAPCS64 callee-saved registers:
//   x19-x30 (12 integer), d8-d15 (8 FP/SIMD), sp
//   (21 registers * 8 bytes = 168 bytes + potential alignment = 176 bytes)
//
// The context_swap(ctx_from, ctx_to) assembly routine:
//   1. Saves callee-saved registers to *ctx_from
//   2. Restores registers from *ctx_to
//   3. Returns into the restored context (via ret on x86, ret via lr on ARM)
#pragma once

#include <cstdint>
#include <cstddef>

namespace zero {

#if defined(__x86_64__) || defined(_M_X64)

struct Context {
    // Callee-saved registers in layout order matching context.S save/restore
    // Offsets: rsp=0, rbp=8, rbx=16, r12=24, r13=32, r14=40, r15=48, rip=56
    uint64_t rsp;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;

    // Initialize context to start executing fn() when the context is
    // swapped in for the first time.
    //
    // The context_swap restore sequence is:
    //   movq 0(%rsi), %rsp     ; restore stack pointer
    //   movq 56(%rsi), %rax    ; load rip into rax
    //   ... restore other regs ...
    //   pushq %rax             ; push target address
    //   ret                    ; jump to fn
    //
    // So we set rsp = stack_top (high end, stack grows down)
    // and rip = address of fn.
    //
    // stack_top must point to the HIGH end of the allocated stack region.
    void init(void (*fn)(), void* stack_top) noexcept {
        uint64_t* sp = reinterpret_cast<uint64_t*>(stack_top);
        rsp = reinterpret_cast<uint64_t>(sp);
        rbp = 0;   // Frame pointer = 0 for stack trace termination
        rbx = 0;
        r12 = 0;
        r13 = 0;
        r14 = 0;
        r15 = 0;
        rip = reinterpret_cast<uint64_t>(fn);
    }

    // Mark this context as uninitialized (null rip). Used to detect
    // contexts that should never be restored (e.g., a terminated fiber).
    void invalidate() noexcept {
        rip = 0;
        rsp = 0;
    }

    bool valid() const noexcept {
        return rip != 0;
    }
};

#elif defined(__aarch64__) || defined(__arm64__)

struct Context {
    // Callee-saved integer registers (x19 through x30 = 12 registers)
    // x29 = frame pointer (fp), x30 = link register (lr)
    uint64_t x19, x20, x21, x22, x23, x24, x25, x26;
    uint64_t x27, x28, x29, x30;

    // Callee-saved floating-point / SIMD registers (d8 through d15 = 8 registers)
    uint64_t d8, d9, d10, d11;
    uint64_t d12, d13, d14, d15;

    // Stack pointer
    uint64_t sp;

    // Initialize context to start executing fn() when swapped in.
    // lr (x30) is set to fn address; when context_swap does "ret",
    // it will branch to lr.
    void init(void (*fn)(), void* stack_top) noexcept {
        sp = reinterpret_cast<uint64_t>(stack_top);
        x30 = reinterpret_cast<uint64_t>(fn);  // lr = target function
        x29 = 0;  // fp = 0 for stack trace termination
        // Other registers are zero-initialized (caller's responsibility
        // via the Context default constructor or explicit zeroing)
    }

    void invalidate() noexcept {
        sp = 0;
        x30 = 0;
    }

    bool valid() const noexcept {
        return x30 != 0;
    }

    // Default-initialize all registers to 0 for clean initial state
    Context() noexcept
        : x19(0), x20(0), x21(0), x22(0),
          x23(0), x24(0), x25(0), x26(0),
          x27(0), x28(0), x29(0), x30(0),
          d8(0), d9(0), d10(0), d11(0),
          d12(0), d13(0), d14(0), d15(0), sp(0) {}
};

#else
#error "Unsupported architecture: only x86_64 and aarch64 are supported"
#endif

// Low-level context save/restore — implemented in context.S
//
// Signature: void context_swap(Context* from, Context* to);
//
// Saves the current execution context (callee-saved registers) into *from,
// then restores the execution context from *to. After the restore, execution
// continues at the point where *to was saved (which may be in a completely
// different fiber).
//
// For a newly-initialized context, execution begins at the function pointer
// stored during Context::init().
extern "C" void context_swap(Context* from, Context* to) noexcept;

} // namespace zero
