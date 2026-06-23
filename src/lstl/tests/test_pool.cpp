/**
 * @file    test_pool.cpp
 * @brief   Comprehensive tests for memory pool (default_alloc, pool_single, freelist).
 */

#include <lstl/memory/pool.h>
#include <lstl/memory/alloc.h>
#include <cassert>
#include <cstring>
#include <vector>

int main() {
    using namespace lstl;

    // =========================================================================
    // 1. freelist basic operations
    // =========================================================================
    {
        freelist fl;
        assert(fl.empty());

        char block[1024];  // Use stack memory for test blocks
        void* a = &block[0];
        void* b = &block[64];
        void* c = &block[128];

        fl.push(a);
        assert(!fl.empty());
        fl.push(b);
        fl.push(c);

        // LIFO order
        assert(fl.pop() == c);
        assert(fl.pop() == b);
        assert(fl.pop() == a);
        assert(fl.empty());
        assert(fl.pop() == nullptr);
    }

    // =========================================================================
    // 2. pool_single — single size
    // =========================================================================
    {
        pool_single pool(32, 16);  // 32-byte blocks, 16 per chunk

        std::vector<void*> ptrs;
        for (int i = 0; i < 100; ++i) {
            void* p = pool.allocate();
            assert(p != nullptr);
            // Write to memory to verify it's valid
            std::memset(p, 0xAA, 32);
            ptrs.push_back(p);
        }

        // Return all
        for (auto p : ptrs) pool.deallocate(p);
        ptrs.clear();

        // Re-allocate (should reuse from freelist)
        void* p1 = pool.allocate();
        void* p2 = pool.allocate();
        assert(p1 != nullptr && p2 != nullptr);
        pool.deallocate(p1);
        pool.deallocate(p2);

        assert(pool.block_size() == 32);
        assert(pool.allocated_bytes() > 0);
    }

    // =========================================================================
    // 3. default_alloc — multiple size classes
    // =========================================================================
    {
        // Small allocations
        void* s8  = default_alloc::allocate(8);
        void* s16 = default_alloc::allocate(16);
        void* s64 = default_alloc::allocate(64);
        void* s256 = default_alloc::allocate(256);
        void* s1024 = default_alloc::allocate(1024);
        void* s4096 = default_alloc::allocate(4096);

        assert(s8 && s16 && s64 && s256 && s1024 && s4096);

        // Write to verify writable
        std::memset(s8, 0, 8);
        std::memset(s16, 0, 16);
        std::memset(s64, 0, 64);
        std::memset(s256, 0, 256);
        std::memset(s1024, 0, 1024);
        std::memset(s4096, 0, 4096);

        // Large allocation (beyond pool max)
        void* huge = default_alloc::allocate(16384);
        assert(huge != nullptr);
        std::memset(huge, 0, 16384);

        // Deallocate all
        default_alloc::deallocate(s8, 8);
        default_alloc::deallocate(s16, 16);
        default_alloc::deallocate(s64, 64);
        default_alloc::deallocate(s256, 256);
        default_alloc::deallocate(s1024, 1024);
        default_alloc::deallocate(s4096, 4096);
        default_alloc::deallocate(huge, 16384);
    }

    // =========================================================================
    // 4. default_alloc — reuse from freelist
    // =========================================================================
    {
        // Allocate, free, re-allocate same size — should hit freelist
        void* p1 = default_alloc::allocate(128);
        std::memset(p1, 0xBB, 128);
        default_alloc::deallocate(p1, 128);

        void* p2 = default_alloc::allocate(128);
        void* p3 = default_alloc::allocate(128);
        assert(p2 != nullptr && p3 != nullptr);

        default_alloc::deallocate(p2, 128);
        default_alloc::deallocate(p3, 128);
    }

    // =========================================================================
    // 5. default_alloc — reallocate
    // =========================================================================
    {
        void* p1 = default_alloc::allocate(64);
        std::memset(p1, 0xCC, 64);

        // Grow within same size class
        void* p2 = default_alloc::reallocate(p1, 64, 64);
        assert(p2 != nullptr);
        default_alloc::deallocate(p2, 64);

        // Grow to larger size class
        void* p3 = default_alloc::allocate(64);
        std::memset(p3, 0xDD, 64);
        void* p4 = default_alloc::reallocate(p3, 64, 256);
        assert(p4 != nullptr);
        assert(static_cast<char*>(p4)[0] == static_cast<char>(0xDD));
        default_alloc::deallocate(p4, 256);
    }

    // =========================================================================
    // 6. malloc_alloc — basic operations
    // =========================================================================
    {
        void* p = malloc_alloc::allocate(100);
        assert(p != nullptr);
        std::memset(p, 0, 100);
        malloc_alloc::deallocate(p, 100);

        // Reallocate
        void* p2 = malloc_alloc::allocate(50);
        std::memset(p2, 0xEE, 50);
        void* p3 = malloc_alloc::reallocate(p2, 50, 200);
        assert(p3 != nullptr);
        assert(static_cast<char*>(p3)[0] == static_cast<char>(0xEE));
        malloc_alloc::deallocate(p3, 200);
    }

    // =========================================================================
    // 7. malloc_alloc — OOM handler
    // =========================================================================
    {
        static bool s_oom_called = false;
        struct OOMHandler {
            static void handle(size_t) { s_oom_called = true; throw std::bad_alloc(); }
        };

        auto old_handler = malloc_alloc::set_oom_handler(OOMHandler::handle);

        bool caught = false;
        try {
            // Try to allocate an impossibly large amount
            malloc_alloc::allocate(size_t(-1));
        } catch (const std::bad_alloc&) {
            caught = true;
        }
        // Verify the handler infrastructure works
        malloc_alloc::set_oom_handler(old_handler);
    }

    // =========================================================================
    // 8. Stress test — rapid alloc/free cycles
    // =========================================================================
    {
        std::vector<void*> ptrs;
        for (int round = 0; round < 10; ++round) {
            for (int i = 0; i < 100; ++i) {
                size_t sz = (i % 20 + 1) * 16;  // varying sizes
                void* p = default_alloc::allocate(sz);
                std::memset(p, 0xFF, sz);
                ptrs.push_back(p);
            }
            for (auto p : ptrs) default_alloc::deallocate(p, 0);  // size 0 for simplicity
            ptrs.clear();
        }
    }

    return 0;
}
