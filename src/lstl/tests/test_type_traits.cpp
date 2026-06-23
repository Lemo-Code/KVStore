// Basic type traits verification
#include <lstl/memory/type_traits.h>
#include <cassert>
#include <string>

struct PodType { int x; float y; };
struct NonPodType { int x; std::string s; };

int main() {
    using namespace lstl;

    // is_pod
    static_assert(is_pod<int>::value, "int should be POD");
    static_assert(is_pod<PodType>::value, "PodType should be POD");
    static_assert(!is_pod<NonPodType>::value, "NonPodType should not be POD");

    // conditional
    static_assert(is_same<conditional_t<true, int, float>, int>::value,
                  "conditional true");
    static_assert(is_same<conditional_t<false, int, float>, float>::value,
                  "conditional false");

    // enable_if
    static_assert(is_same<enable_if_t<true, int>, int>::value, "enable_if");

    // is_same
    static_assert(is_same<int, int>::value, "is_same should be true");
    static_assert(!is_same<int, float>::value, "is_same should be false");

    return 0;
}
