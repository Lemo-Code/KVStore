/**
 * @file    alloc.h
 * @brief   Concrete allocator implementations: malloc_alloc and default_alloc.
 * @author  lstl team
 * @date    2025
 *
 * Provides two concrete allocator backends:
 * - malloc_alloc:  Thin wrapper around std::malloc/std::free with
 *                  OOM (out-of-memory) handler support.
 * - default_alloc: The default pool-based allocator (full definition
 *                  in pool.h). Forward-declared here for dependency
 *                  ordering.
 *
 * @ingroup memory
 */

#pragma once

#include <cstdlib>
#include <cstddef>
#include <new>

namespace lstl {

// =========================================================================
// malloc_alloc
// =========================================================================

/**
 * @brief  Allocator backed by std::malloc and std::free.
 *
 * This is the simplest allocator in lstl. It is used as:
 * - The fallback for default_alloc when allocation size exceeds the
 *   pool's maximum block size (kMaxPoolSize = 8192 bytes).
 * - A baseline allocator for debugging and comparison.
 *
 * Supports custom out-of-memory (OOM) handlers for graceful degradation.
 *
 * @note  All methods are static; no instance is needed.
 */
class malloc_alloc {
public:
    /**
     * @brief  Allocates n bytes of memory.
     *
     * On failure, calls the OOM handler (if set). If the handler
     * returns, retries; if it throws or the handler is null, throws
     * std::bad_alloc.
     *
     * @param  n  Number of bytes to allocate.
     * @return    Pointer to at least n bytes of memory.
     *
     * @throws std::bad_alloc  If allocation fails.
     */
    static void* allocate(size_t n) {
        void* result = std::malloc(n);
        if (result == nullptr) {
            handle_oom(n);
        }
        return result;
    }

    /**
     * @brief  Deallocates memory previously returned by allocate().
     * @param  p  Pointer to deallocate (must be from malloc_alloc::allocate()).
     * @param  n  Size of the original allocation (unused, for interface compatibility).
     */
    static void deallocate(void* p, size_t /*n*/) {
        std::free(p);
    }

    /**
     * @brief  Reallocates memory, preserving existing contents.
     *
     * @param  p         Existing allocation (may be nullptr).
     * @param  old_size  Original size (unused).
     * @param  new_size  Desired new size in bytes.
     * @return           Pointer to the reallocated memory.
     *
     * @throws std::bad_alloc  If reallocation fails.
     */
    static void* reallocate(void* p, size_t /*old_size*/, size_t new_size) {
        void* result = std::realloc(p, new_size);
        if (result == nullptr) {
            handle_oom(new_size);
        }
        return result;
    }

    // ---- OOM Handler ----

    /**
     * @brief  Type of an out-of-memory handler function.
     *
     * The handler receives the failed allocation size. It may:
     * - Free some memory and return (the allocator will retry).
     * - Throw an exception (propagated to the caller).
     * - Call std::abort() or exit().
     *
     * @param  size  The number of bytes that could not be allocated.
     */
    typedef void (*oom_handler)(size_t);

    /**
     * @brief  Sets a custom out-of-memory handler.
     *
     * @param  h  The new handler (nullptr to reset to default).
     * @return    The previous handler.
     *
     * @note  This is a global setting — it affects all code using malloc_alloc.
     */
    static oom_handler set_oom_handler(oom_handler h) {
        oom_handler& holder = s_oom_handler();
        oom_handler old = holder;
        holder = h;
        return old;
    }

    /**
     * @brief  Called when allocation fails.
     *
     * Invokes the user-registered OOM handler (if any), then throws
     * std::bad_alloc if the handler returns without resolving the issue.
     *
     * @param  n  The failed allocation size.
     *
     * @throws std::bad_alloc  Always throws if the handler doesn't.
     */
    static void handle_oom(size_t n) {
        if (s_oom_handler()) {
            s_oom_handler()(n);
        }
        throw std::bad_alloc();
    }

private:
    /**
     * @brief  C++14-compatible static variable (function-local static).
     *
     * Avoids the C++17 inline variable feature for portability.
     *
     * @return  Reference to the static OOM handler.
     */
    static oom_handler& s_oom_handler() {
        static oom_handler handler = nullptr;
        return handler;
    }
};

// =========================================================================
// default_alloc forward declaration
// =========================================================================

/**
 * @brief  The default pool-based allocator.
 *
 * Full definition is in pool.h. This forward declaration allows
 * code that only needs the allocator's name (e.g., template
 * parameters) to compile without including the full pool
 * implementation.
 *
 * @see pool.h for the complete definition.
 */
class default_alloc;

} // namespace lstl
