#pragma once

#include <string>
#include <lstl/container/unordered_map.h>
#include <lstl/container/list.h>
#include <lstl/container/vector.h>
#include <ctime>

namespace ledis {

struct Session;

class BlockingManager {
public:
    struct Waiter {
        Session* session;
        lstl::vector<std::string> keys;
        uint64_t timeout_ms;  // 0 = 无限
    };

    // 注册阻塞 (BLPOP/BRPOP 无数据时)
    void addWaiter(Session* s, const lstl::vector<std::string>& keys,
                   int64_t timeout_sec) {
        auto* w = new Waiter();
        w->session = s;
        for (auto& k : keys) w->keys.push_back(k);
        w->timeout_ms = (timeout_sec > 0)
            ? (static_cast<uint64_t>(time(nullptr)) * 1000
               + static_cast<uint64_t>(timeout_sec) * 1000) : 0;
        waiters_.push_back(w);

        for (auto& k : w->keys)
            key_map_[k].push_back(w);
    }

    // key 有数据时, 获取该 key 上的第一个等待者
    Waiter* popWaiter(const std::string& key) {
        auto it = key_map_.find(key);
        if (it == key_map_.end() || it->second.empty()) return nullptr;

        Waiter* w = it->second.front();
        it->second.erase(it->second.begin());
        if (it->second.empty()) key_map_.erase(it->first);

        // 从 waiters_ 移除
        waiters_.remove(w);
        return w;
    }

    // 检查超时
    lstl::vector<Waiter*> checkTimeout(uint64_t now_ms) {
        lstl::vector<Waiter*> timed_out;
        auto it = waiters_.begin();
        while (it != waiters_.end()) {
            auto* w = *it;
            if (w->timeout_ms > 0 && now_ms >= w->timeout_ms) {
                timed_out.push_back(w);
                // 从 key_map_ 清理
                for (auto& k : w->keys) {
                    auto ki = key_map_.find(k);
                    if (ki != key_map_.end()) ki->second.remove(w);
                }
                it = waiters_.erase(it);
                delete w;
            } else { ++it; }
        }
        return timed_out;
    }

    // 清理 session
    void cleanup(Session* s) {
        auto it = waiters_.begin();
        while (it != waiters_.end()) {
            if ((*it)->session == s) {
                for (auto& k : (*it)->keys) {
                    auto ki = key_map_.find(k);
                    if (ki != key_map_.end()) ki->second.remove(*it);
                }
                delete *it;
                it = waiters_.erase(it);
            } else { ++it; }
        }
    }

private:
    lstl::list<Waiter*> waiters_;
    lstl::unordered_map<std::string, lstl::list<Waiter*>> key_map_;
};

} // namespace ledis
