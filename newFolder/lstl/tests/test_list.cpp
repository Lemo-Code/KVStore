// List test
#include <lstl/container/list.h>
#include <cassert>

int main() {
    using namespace lstl;

    list<int> l;
    assert(l.empty());

    // push_back / push_front
    l.push_back(1);
    l.push_back(2);
    l.push_front(0);
    assert(l.size() == 3);

    // Iteration
    auto it = l.begin();
    assert(*it == 0); ++it;
    assert(*it == 1); ++it;
    assert(*it == 2); ++it;
    assert(it == l.end());

    // Front/back
    assert(l.front() == 0);
    assert(l.back() == 2);

    // pop
    l.pop_front();
    assert(l.front() == 1);
    l.pop_back();
    assert(l.back() == 1);
    assert(l.size() == 1);

    // insert
    l.insert(l.begin(), -1);
    assert(l.front() == -1);
    assert(l.size() == 2);

    // erase
    l.erase(l.begin());
    assert(l.front() == 1);

    // copy
    list<int> l2(l);
    assert(l2.size() == 1);
    assert(l2.front() == 1);

    // clear
    l.clear();
    assert(l.empty());

    return 0;
}
