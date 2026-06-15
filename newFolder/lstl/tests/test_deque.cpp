// Deque test
#include <lstl/container/deque.h>
#include <cassert>

int main() {
    using namespace lstl;

    deque<int> d;
    assert(d.empty());

    // push_back
    for (int i = 0; i < 10; ++i) d.push_back(i);
    assert(d.size() == 10);
    for (int i = 0; i < 10; ++i) assert(d[i] == i);

    // push_front
    d.push_front(-1);
    assert(d.size() == 11);
    assert(d.front() == -1);

    // pop
    d.pop_front();
    assert(d.front() == 0);
    d.pop_back();
    assert(d.back() == 8);
    assert(d.size() == 9);

    // Random access
    assert(d[0] == 0);
    assert(d[4] == 4);

    // Iterators
    int sum = 0;
    for (auto& x : d) sum += x;
    assert(sum == 0+1+2+3+4+5+6+7+8);

    d.clear();
    assert(d.empty());

    return 0;
}
