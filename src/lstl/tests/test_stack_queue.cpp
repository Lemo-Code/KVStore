// Stack and Queue adapter test
#include <lstl/container/stack.h>
#include <lstl/container/queue.h>
#include <lstl/container/priority_queue.h>
#include <cassert>

int main() {
    using namespace lstl;

    // stack
    stack<int> s;
    assert(s.empty());
    s.push(1); s.push(2); s.push(3);
    assert(s.size() == 3);
    assert(s.top() == 3);
    s.pop();
    assert(s.top() == 2);
    s.pop();
    assert(s.top() == 1);

    // queue
    queue<int> q;
    q.push(1); q.push(2); q.push(3);
    assert(q.front() == 1);
    assert(q.back() == 3);
    q.pop();
    assert(q.front() == 2);

    // priority_queue
    priority_queue<int> pq;
    pq.push(3); pq.push(1); pq.push(2);
    assert(pq.top() == 3);
    pq.pop();
    assert(pq.top() == 2);
    pq.pop();
    assert(pq.top() == 1);

    return 0;
}
