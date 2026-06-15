#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <list>
#include <array>

namespace zero {

// ============ TimerWheel (分层时间轮) ============
//
// 5 层结构, 覆盖 1ms ~ 2^32 ms (~49 天)
//
// Level  Slots  Granularity  Range
//   0     256       1 ms        0 ~ 255 ms
//   1      64     256 ms      256 ms ~ 16.6 s
//   2      64    16.4 s      16.4 s ~ 17.5 min
//   3      64    17.5 min    17.5 min ~ 18.6 h
//   4      64    18.6 h      18.6 h ~ 49.7 d
//
// 所有操作 O(1) (均摊)
// 单线程使用 (per-thread), 无需锁
class TimerWheel {
public:
    using TimerCallback = std::function<void()>;

    TimerWheel();
    ~TimerWheel();

    // 添加定时器
    // @param delay_ms  延迟毫秒数
    // @param cb        回调函数
    // @param recurring 是否循环
    // @return          定时器 ID (非零)
    uint64_t addTimer(uint64_t delay_ms, TimerCallback cb, bool recurring = false);

    // 取消定时器
    bool cancelTimer(uint64_t timer_id);

    // 推进时钟, 收集到期回调
    // @param now_ms  当前时间戳 (毫秒)
    // @param cbs     输出: 到期的回调列表
    void tick(uint64_t now_ms, std::vector<TimerCallback>& cbs);

    // 获取最近定时器的到期时间, 用于计算 epoll_wait 超时
    // @return 到期时间戳 (ms), ~0ull 表示无定时器
    uint64_t nextExpireMs() const;

    // 是否有定时器
    bool empty() const { return timer_count_ == 0; }
    size_t count() const { return timer_count_; }

private:
    struct TimerNode {
        uint64_t id;
        uint64_t expire_ms;       // 绝对到期时间戳
        uint64_t interval_ms;     // 循环间隔 (0 = 一次性)
        TimerCallback cb;
        TimerNode* next = nullptr;

        TimerNode(uint64_t i, uint64_t exp, uint64_t interval, TimerCallback c)
            : id(i), expire_ms(exp), interval_ms(interval), cb(std::move(c)) {}
    };

    // 级联: 高层到期定时器降级到更低层
    void cascade(uint64_t now_ms);

    // 从指定 slot 链表中收集所有到期定时器
    void collectExpired(TimerNode*& head, uint64_t now_ms,
                        std::vector<TimerCallback>& cbs);

    // 插入定时器到合适的 slot
    void insert(TimerNode* node);

    // 各层参数
    static constexpr int    kLevels = 5;
    static constexpr size_t kL0Slots = 256;
    static constexpr size_t kLNSlots = 64;

    static constexpr uint64_t kL0Mask  = 0xFF;            // 256 - 1
    static constexpr uint64_t kL0Shift = 8;               // log2(256)
    static constexpr uint64_t kLNMask  = 0x3F;            // 64 - 1
    static constexpr uint64_t kLNShift = 6;               // log2(64)

    // 每层当前索引和 slot 数组
    struct Level {
        uint64_t current_index = 0;   // 当前正在处理的 slot
        std::vector<TimerNode*> slots; // slot 链表头
    };

    Level levels_[kLevels];
    uint64_t last_tick_ms_ = 0;   // 上次 tick 的时间

    size_t timer_count_ = 0;      // 当前定时器总数
    uint64_t next_id_ = 1;        // 下一个 timer ID

    // 待删除的 timer ID 集合 (惰性删除)
    std::vector<uint64_t> pending_cancel_;
};

} // namespace zero
