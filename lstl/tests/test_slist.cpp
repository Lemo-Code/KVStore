// Singly-linked list test
#include <lstl/container/slist.h>
#include <cassert>

int main() {
    using namespace lstl;

    slist<int> sl;
    assert(sl.empty());

    sl.push_front(3);
    sl.push_front(2);
    sl.push_front(1);
    assert(sl.size() == 3);

    // Check order: 1 -> 2 -> 3
    auto it = sl.begin();
    assert(*it == 1); ++it;
    assert(*it == 2); ++it;
    assert(*it == 3); ++it;
    assert(it == sl.end());

    assert(sl.front() == 1);

    sl.pop_front();
    assert(sl.front() == 2);
    assert(sl.size() == 2);

    // insert_after
    it = sl.begin();
    sl.insert_after(it, 10);
    it = sl.begin();
    assert(*it == 2); ++it;
    assert(*it == 10);

    // reverse
    sl.reverse();
    it = sl.begin();
    assert(*it == 3);

    sl.clear();
    assert(sl.empty());

    return 0;
}
