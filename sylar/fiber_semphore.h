#ifndef __SYLAR_FIBER_SEPHORE_H__
#define __SYLAR_FIBER_SEPHORE_H__
#include "sylar/mutex.h"
#include "sylar/scheduler.h"
#include "sylar/noncopyable.h"

namespace sylar
{
class Scheduler;
class FiberSemaphore : Noncopyable {
public:
    typedef Spinlock MutexType;

    FiberSemaphore(size_t initial_concurrency = 0);
    ~FiberSemaphore();

    bool tryWait();
    void wait();
    void notify();

    size_t getConcurrency() const { return m_concurrency;}
    void reset() { m_concurrency = 0;}
private:
    MutexType m_mutex;
    std::list<std::pair<Scheduler*, Fiber::ptr> > m_waiters;
    size_t m_concurrency;
};


}
#endif