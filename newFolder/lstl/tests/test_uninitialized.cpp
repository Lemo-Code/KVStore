// Uninitialized memory test
#include <lstl/memory/uninitialized.h>
#include <lstl/memory/allocator.h>
#include <cassert>
#include <cstring>

int main() {
    using namespace lstl;
    allocator<int> alloc;

    // uninitialized_copy
    int src[] = {1, 2, 3, 4, 5};
    int* dst = alloc.allocate(5);
    uninitialized_copy(src, src + 5, dst);
    for (int i = 0; i < 5; ++i) assert(dst[i] == src[i]);
    destroy(dst, dst + 5);
    alloc.deallocate(dst, 5);

    // uninitialized_fill
    int* buf = alloc.allocate(10);
    uninitialized_fill_n(buf, 10, 42);
    for (int i = 0; i < 10; ++i) assert(buf[i] == 42);
    destroy(buf, buf + 10);
    alloc.deallocate(buf, 10);

    return 0;
}
