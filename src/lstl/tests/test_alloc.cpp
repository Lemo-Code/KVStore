// Alloc test
#include <lstl/memory/alloc.h>
#include <lstl/memory/pool.h>
#include <lstl/memory/allocator.h>
#include <cassert>

int main() {
    using namespace lstl;

    // malloc_alloc
    void* p = malloc_alloc::allocate(100);
    assert(p != nullptr);
    malloc_alloc::deallocate(p, 100);

    // default_alloc
    void* p2 = default_alloc::allocate(64);
    assert(p2 != nullptr);
    default_alloc::deallocate(p2, 64);

    // simple_alloc
    typedef simple_alloc<int, default_alloc> int_alloc;
    int* p3 = int_alloc::allocate(10);
    assert(p3 != nullptr);
    int_alloc::deallocate(p3, 10);

    return 0;
}
