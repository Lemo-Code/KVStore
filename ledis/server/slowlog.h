#pragma once

#include <cstdint>
#include <ctime>
#include <lstl/container/deque.h>
#include <string>
#include <string_view>
#include <lstl/container/vector.h>

namespace ledis {

// ============================================================
// Slowlog — 慢查询日志
// ============================================================
struct SlowlogEntry {
    uint64_t id;
    int64_t  timestamp;       // Unix 秒
    int64_t  duration_us;     // 执行微秒
    lstl::vector<std::string> args;
    std::string client_addr;
};

class Slowlog {
public:
    Slowlog() = default;

    // 记录一条慢查询
    void record(int64_t duration_us, const lstl::vector<std::string_view>& args,
                const std::string& client_addr) {
        if (duration_us < slower_than_us_) return;

        SlowlogEntry entry;
        entry.id = next_id_++;
        entry.timestamp = time(nullptr);
        entry.duration_us = duration_us;
        entry.client_addr = client_addr;
        for (auto& a : args) entry.args.push_back(std::string(a));

        entries_.push_front(std::move(entry));
        while (static_cast<int>(entries_.size()) > max_len_)
            entries_.pop_back();
    }

    // SLOWLOG GET [count]
    lstl::vector<SlowlogEntry> get(int count) const {
        lstl::vector<SlowlogEntry> result;
        int n = std::min(count, static_cast<int>(entries_.size()));
        for (int i = 0; i < n; ++i) result.push_back(entries_[i]);
        return result;
    }

    // SLOWLOG RESET
    void reset() {
        entries_.clear();
        next_id_ = 0;
    }

    // SLOWLOG LEN
    int len() const { return static_cast<int>(entries_.size()); }

    // 运行时调整
    void setSlowerThan(int64_t us) { slower_than_us_ = us; }
    void setMaxLen(int n) { max_len_ = n; }
    int64_t slowerThan() const { return slower_than_us_; }

private:
    lstl::deque<SlowlogEntry> entries_;
    uint64_t next_id_ = 0;
    int64_t slower_than_us_ = 10000;   // 默认 10ms
    int max_len_ = 128;
};

} // namespace ledis
