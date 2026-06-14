#include "sylar/time_wheel.h"
#include "sylar/log.h"

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

TimeWheel::TimeWheel(uint64_t tick_ms, size_t slot_count)
    : m_tickMs(tick_ms)
    , m_curSlot(0)
    , m_curMs(0) {
    if (slot_count == 0) slot_count = 256;
    m_slots.resize(slot_count);
}

uint64_t TimeWheel::nextId() {
    return m_nextId++;
}

size_t TimeWheel::slotIndex(uint64_t expire_ms) const {
    uint64_t ticks = expire_ms / m_tickMs;
    return static_cast<size_t>((m_curSlot + ticks) % m_slots.size());
}

uint64_t TimeWheel::add(std::function<void()> cb, uint64_t delay_ms, bool repeat) {
    if (!cb || delay_ms == 0) return 0;
    sylar::Mutex::Lock lock(m_mutex);
    uint64_t id = nextId();
    uint64_t expire = m_curMs + delay_ms;
    Task t;
    t.id = id;
    t.expire_ms = expire;
    t.cb = std::move(cb);
    t.repeat = repeat;
    t.interval_ms = delay_ms;
    size_t idx = slotIndex(expire);
    m_slots[idx].push_back(std::move(t));
    auto it = std::prev(m_slots[idx].end());
    m_tasks[id] = std::make_pair(idx, it);
    return id;
}

bool TimeWheel::cancel(uint64_t id) {
    sylar::Mutex::Lock lock(m_mutex);
    auto it = m_tasks.find(id);
    if (it == m_tasks.end()) return false;
    size_t idx = it->second.first;
    m_slots[idx].erase(it->second.second);
    m_tasks.erase(it);
    return true;
}

void TimeWheel::runSlot(Slot& s) {
    for (auto it = s.begin(); it != s.end(); ) {
        Task t = std::move(*it);
        it = s.erase(it);
        if (t.cb) {
            try {
                t.cb();
            } catch (const std::exception& e) {
                SYLAR_LOG_ERROR(g_logger) << "TimeWheel task exception: " << e.what();
            }
            if (t.repeat && t.cb) {
                add(std::move(t.cb), t.interval_ms, true);
            }
        }
    }
}

void TimeWheel::tick() {
    Slot s;
    {
        sylar::Mutex::Lock lock(m_mutex);
        m_curMs += m_tickMs;
        s.swap(m_slots[m_curSlot]);
        for (auto& t : s) m_tasks.erase(t.id);
        m_curSlot = (m_curSlot + 1) % m_slots.size();
    }
    runSlot(s);
}

void TimeWheel::tickTo(uint64_t now_ms) {
    while (m_curMs + m_tickMs <= now_ms) {
        tick();
    }
}

} // namespace sylar
