// Utility: pair, swap, move, forward
#include <lstl/memory/utility.h>
#include <cassert>
#include <string>

int main() {
    using namespace lstl;

    // pair construction
    pair<int, int> p1(1, 2);
    assert(p1.first == 1);
    assert(p1.second == 2);

    // make_pair
    auto p2 = make_pair(3, 4);
    assert(p2.first == 3);
    assert(p2.second == 4);

    // comparison
    assert(p1 < p2);
    assert(p2 > p1);
    assert(p1 != p2);
    assert((p1 == pair<int, int>(1, 2)));

    // swap
    swap(p1, p2);
    assert(p1.first == 3 && p1.second == 4);
    assert(p2.first == 1 && p2.second == 2);

    // min/max
    assert(max(5, 10) == 10);
    assert(min(5, 10) == 5);

    // index_sequence
    using seq = make_index_sequence<3>;
    static_assert(seq::size() == 3, "index_sequence size");

    return 0;
}
