#ifndef __SYLAR_TIME_WHEEL_H__
#define __SYLAR_TIME_WHEEL_H__

#include "sylar/mutex.h"
#include <functional>
#include <list>
#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>

namespace sylar {

/**
 * @brief 时间轮定时器
 * 适用于大量短周期定时任务，add/cancel/tick 均摊 O(1)。
 * 槽数为 N，每 tick_ms 推进一槽，延迟 delay_ms 的任务落在 (cur + delay_ms/tick_ms) % N。
 */
class TimeWheel {
public:
    typedef std::shared_ptr<TimeWheel> ptr;

    /**
     * @param tick_ms 每槽代表的时间（毫秒）
     * @param slot_count 槽数量，建议 2 的幂
     */
    TimeWheel(uint64_t tick_ms = 10, size_t slot_count = 256);

    /** 添加定时任务，返回任务 id，可用于 cancel */
    uint64_t add(std::function<void()> cb, uint64_t delay_ms, bool repeat = false);
    /** 取消任务 */
    bool cancel(uint64_t id);
    /** 推进一 tick，执行当前槽中到期任务 */
    void tick();
    /** 推进到指定时间（毫秒），执行所有到期任务 */
    void tickTo(uint64_t now_ms);

    uint64_t getTickMs() const { return m_tickMs; }
    size_t getSlotCount() const { return m_slots.size(); }

private:
    struct Task {
        uint64_t id;
        uint64_t expire_ms;
        std::function<void()> cb;
        bool repeat;
        uint64_t interval_ms;
    };
    typedef std::list<Task> Slot;

    uint64_t nextId();
    size_t slotIndex(uint64_t expire_ms) const;
    void runSlot(Slot& s);

    uint64_t m_tickMs;
    std::vector<Slot> m_slots;
    size_t m_curSlot;
    uint64_t m_curMs;
    std::unordered_map<uint64_t, std::pair<size_t, typename Slot::iterator>> m_tasks;
    std::atomic<uint64_t> m_nextId{1};
    sylar::Mutex m_mutex;
};

} // namespace sylar

#endif
