// Construct/destroy verification
#include <lstl/memory/construct.h>
#include <lstl/memory/allocator.h>
#include <cassert>

static int construct_count = 0;
static int destruct_count = 0;

struct Tracked {
    int val;
    Tracked() : val(0) { ++construct_count; }
    Tracked(int v) : val(v) { ++construct_count; }
    Tracked(const Tracked& o) : val(o.val) { ++construct_count; }
    ~Tracked() { ++destruct_count; }
};

int main() {
    using namespace lstl;

    {
        construct_count = 0;
        destruct_count = 0;

        // Single construct/destroy
        allocator<Tracked> alloc;
        Tracked* p = alloc.allocate(1);
        lstl::construct(p, 42);
        assert(p->val == 42);
        assert(construct_count == 1);

        lstl::destroy(p);
        assert(destruct_count == 1);
        alloc.deallocate(p, 1);

        // Range construct/destroy
        construct_count = 0;
        destruct_count = 0;
        Tracked* arr = alloc.allocate(5);
        for (int i = 0; i < 5; ++i) {
            lstl::construct(arr + i, i);
        }
        assert(construct_count == 5);

        lstl::destroy(arr, arr + 5);
        assert(destruct_count == 5);
        alloc.deallocate(arr, 5);
    }

    return 0;
}
