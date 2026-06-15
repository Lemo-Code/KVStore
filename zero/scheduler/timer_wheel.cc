#include "zero/scheduler/timer_wheel.h"
#include "zero/base/macro.h"
#include <algorithm>
#include <cstdio>

namespace zero {

TimerWheel::TimerWheel() {
    levels_[0].slots.resize(kL0Slots, nullptr);
    for (int i = 1; i < kLevels; ++i) {
        levels_[i].slots.resize(kLNSlots, nullptr);
    }
}

TimerWheel::~TimerWheel() {
    // 清理所有定时器节点
    for (int lvl = 0; lvl < kLevels; ++lvl) {
        auto& slots = levels_[lvl].slots;
        for (size_t i = 0; i < slots.size(); ++i) {
            TimerNode* node = slots[i];
            while (node) {
                TimerNode* next = node->next;
                delete node;
                node = next;
            }
        }
    }
}

// ====================================================================
// 插入定时器
// ====================================================================
void TimerWheel::insert(TimerNode* node) {
    uint64_t expire = node->expire_ms;
    uint64_t delta = expire - last_tick_ms_;

    if (delta < kL0Slots) {
        // Level 0: 0-255ms
        size_t idx = (levels_[0].current_index + delta) & kL0Mask;
        node->next = levels_[0].slots[idx];
        levels_[0].slots[idx] = node;
    } else if (delta < (kL0Slots << kLNShift)) {
        // Level 1: 256ms ~ 16.4s
        // delta >> 8 = 1..63, mapping to slots 0..62
        size_t idx = ((levels_[1].current_index + (delta >> kL0Shift)) - 1) & kLNMask;
        node->next = levels_[1].slots[idx];
        levels_[1].slots[idx] = node;
    } else if (delta < (kL0Slots << (kLNShift * 2))) {
        // Level 2: 16.4s ~ 17.5min
        size_t idx = ((levels_[2].current_index + (delta >> (kL0Shift + kLNShift))) - 1) & kLNMask;
        node->next = levels_[2].slots[idx];
        levels_[2].slots[idx] = node;
    } else if (delta < (kL0Slots << (kLNShift * 3))) {
        // Level 3: 17.5min ~ 18.6h
        size_t idx = ((levels_[3].current_index + (delta >> (kL0Shift + kLNShift * 2))) - 1) & kLNMask;
        node->next = levels_[3].slots[idx];
        levels_[3].slots[idx] = node;
    } else {
        // Level 4: > 18.6h
        size_t idx = ((levels_[4].current_index + (delta >> (kL0Shift + kLNShift * 3))) - 1) & kLNMask;
        node->next = levels_[4].slots[idx];
        levels_[4].slots[idx] = node;
    }
}

uint64_t TimerWheel::addTimer(uint64_t delay_ms, TimerCallback cb, bool recurring) {
    if (delay_ms == 0) delay_ms = 1;  // 最小 1ms

    // 首次添加定时器时初始化时钟
    if (last_tick_ms_ == 0) {
        last_tick_ms_ = GetCurrentMS();
    }

    uint64_t id = next_id_++;
    uint64_t expire = last_tick_ms_ + delay_ms;
    uint64_t interval = recurring ? delay_ms : 0;

    auto* node = new TimerNode(id, expire, interval, std::move(cb));
    insert(node);
    ++timer_count_;
    return id;
}

bool TimerWheel::cancelTimer(uint64_t timer_id) {
    // 惰性删除: 记录 ID, 在 tick 时清理
    pending_cancel_.push_back(timer_id);
    return true;
}

// ====================================================================
// Cascade: 高层定时器降级
// ====================================================================
void TimerWheel::cascade(uint64_t now_ms) {
    // Level 1 降级触发条件
    if ((now_ms & kL0Mask) == 0) {
        // L0 转了一圈 → 从 L1 降级一批到 L0
        size_t l1_idx = levels_[1].current_index;
        TimerNode* node = levels_[1].slots[l1_idx];
        levels_[1].slots[l1_idx] = nullptr;

        while (node) {
            TimerNode* next = node->next;
            node->next = nullptr;
            // 重新插入 (会进 L0 或其他层)
            insert(node);
            node = next;
        }

        levels_[1].current_index = (l1_idx + 1) & kLNMask;

        // L1 绕回 (处理完 slot 63) → 从 L2 降级
        if (l1_idx == kLNMask) {
            size_t l2_idx = levels_[2].current_index;
            TimerNode* n2 = levels_[2].slots[l2_idx];
            levels_[2].slots[l2_idx] = nullptr;

            while (n2) {
                TimerNode* next = n2->next;
                n2->next = nullptr;
                insert(n2);
                n2 = next;
            }

            levels_[2].current_index = (l2_idx + 1) & kLNMask;

            // L2 绕回 → 从 L3 降级
            if (l2_idx == kLNMask) {
                size_t l3_idx = levels_[3].current_index;
                TimerNode* n3 = levels_[3].slots[l3_idx];
                levels_[3].slots[l3_idx] = nullptr;

                while (n3) {
                    TimerNode* next = n3->next;
                    n3->next = nullptr;
                    insert(n3);
                    n3 = next;
                }

                levels_[3].current_index = (l3_idx + 1) & kLNMask;

                // L3 绕回 → 从 L4 降级
                if (l3_idx == kLNMask) {
                    size_t l4_idx = levels_[4].current_index;
                    TimerNode* n4 = levels_[4].slots[l4_idx];
                    levels_[4].slots[l4_idx] = nullptr;

                    while (n4) {
                        TimerNode* next = n4->next;
                        n4->next = nullptr;
                        insert(n4);
                        n4 = next;
                    }

                    levels_[4].current_index = (l4_idx + 1) & kLNMask;
                }
            }
        }
    }
}

// ====================================================================
// Tick: 推进时钟
// ====================================================================
void TimerWheel::tick(uint64_t now_ms, std::vector<TimerCallback>& cbs) {
    // 首次 tick: 初始化时钟
    if (last_tick_ms_ == 0) {
        last_tick_ms_ = now_ms;
        return;
    }

    // 处理惰性删除
    if (!pending_cancel_.empty()) {
        for (uint64_t cancel_id : pending_cancel_) {
            // 遍历所有 slot 找到并删除
            for (int lvl = 0; lvl < kLevels; ++lvl) {
                auto& slots = levels_[lvl].slots;
                for (size_t i = 0; i < slots.size(); ++i) {
                    TimerNode** pp = &slots[i];
                    while (*pp) {
                        if ((*pp)->id == cancel_id) {
                            TimerNode* del = *pp;
                            *pp = del->next;
                            delete del;
                            --timer_count_;
                            goto next_cancel;
                        }
                        pp = &(*pp)->next;
                    }
                }
            }
            next_cancel:;
        }
        pending_cancel_.clear();
    }

    if (now_ms <= last_tick_ms_) return;

    uint64_t step = now_ms - last_tick_ms_;
    // 安全限制: 如果差值过大 (超过 60 秒), 分批处理
    // 这种情况在正常运行时不会出现
    if (step > 60000) {
        step = 60000;
    }

    for (uint64_t i = 0; i < step; ++i) {
        ++last_tick_ms_;

        // Level 0 当前 slot 到期
        size_t l0_idx = levels_[0].current_index;
        TimerNode* node = levels_[0].slots[l0_idx];
        levels_[0].slots[l0_idx] = nullptr;

        // 收集到期的定时器
        while (node) {
            TimerNode* next = node->next;
            if (node->expire_ms <= last_tick_ms_) {
                if (node->interval_ms > 0) {
                    // 循环定时器 — copy cb, 节点继续使用
                    cbs.push_back(node->cb);
                    node->expire_ms = last_tick_ms_ + node->interval_ms;
                    node->next = nullptr;
                    insert(node);
                } else {
                    // 一次性 — move cb, 删除节点
                    cbs.push_back(std::move(node->cb));
                    delete node;
                    --timer_count_;
                }
            } else {
                // 未到期 (不应该出现, 但安全处理)
                node->next = nullptr;
                insert(node);
            }
            node = next;
        }

        // 推进 L0 指针并处理级联
        levels_[0].current_index = (l0_idx + 1) & kL0Mask;
        cascade(last_tick_ms_);
    }
}

// ====================================================================
// Next Expire
// ====================================================================
uint64_t TimerWheel::nextExpireMs() const {
    // 快速检查: 遍历 L0 slots 找到最小的 expire_ms
    uint64_t min_expire = ~0ull;

    for (int lvl = 0; lvl < kLevels; ++lvl) {
        const auto& slots = levels_[lvl].slots;
        for (size_t i = 0; i < slots.size(); ++i) {
            TimerNode* node = slots[i];
            while (node) {
                if (node->expire_ms < min_expire) {
                    min_expire = node->expire_ms;
                }
                node = node->next;
            }
        }
    }

    return min_expire;
}

} // namespace zero
