#include "sylar/fiber_semphore.h"

namespace sylar
{
FiberSemaphore::FiberSemaphore(size_t initial_concurrency)
    :m_concurrency(initial_concurrency) {
}

FiberSemaphore::~FiberSemaphore() {
    SYLAR_ASSERT(m_waiters.empty());
}

bool FiberSemaphore::tryWait() {
    SYLAR_ASSERT(Scheduler::GetThis());
    {
        MutexType::Lock lock(m_mutex);
        if(m_concurrency > 0u) {
            --m_concurrency;
            return true;
        }
        return false;
    }
}

void FiberSemaphore::wait() {
    SYLAR_ASSERT(Scheduler::GetThis());
    {
        MutexType::Lock lock(m_mutex);
        if(m_concurrency > 0u) {
            --m_concurrency;
            return;
        }
        m_waiters.push_back(std::make_pair(Scheduler::GetThis(), Fiber::GetThis()));
    }
    Fiber::YeildToHold();
}

void FiberSemaphore::notify() {
    MutexType::Lock lock(m_mutex);
    if(!m_waiters.empty()) {
        auto next = m_waiters.front();
        m_waiters.pop_front();
        next.first->schedule(next.second);
    } else {
        ++m_concurrency;
    }
}


}