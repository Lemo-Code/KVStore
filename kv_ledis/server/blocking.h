#pragma once

// ============================================================
// BlockingManager — 阻塞命令管理 (BLPOP/BRPOP/BZPOPMIN/BZPOPMAX)
// ============================================================

#include <string>
#include <lstl/container/unordered_map.h>
#include <lstl/container/list.h>
#include <lstl/container/vector.h>
#include <ctime>

#include "kv_ledis/protocol/resp_writer.h"

namespace ledis {

struct Session;
struct Value;

struct BlockedClient {
    Session* session;
    lstl::vector<std::string> keys;
    uint64_t timeout_ms;  // 绝对超时时间, 0 = 无限
    // 区分 BLPOP/BRPOP/BZPOPMIN/BZPOPMAX
    enum OpType { BLPOP, BRPOP, BZPOPMIN, BZPOPMAX } op;
};

class BlockingManager {
public:
    // 注册阻塞等待
    void block(Session* s, const lstl::vector<std::string_view>& keys,
               int64_t timeout_sec, BlockedClient::OpType op) {
        auto* bc = new BlockedClient();
        bc->session = s;
        for (auto& k : keys) bc->keys.push_back(std::string(k));
        bc->timeout_ms = (timeout_sec > 0)
            ? (static_cast<uint64_t>(time(nullptr)) * 1000 + static_cast<uint64_t>(timeout_sec) * 1000)
            : 0;
        bc->op = op;
        blocked_.push_back(bc);

        // 注册到 key 等待队列
        for (auto& k : bc->keys) {
            waiters_[k].push_back(bc);
        }
    }

    // key 有数据时唤醒等待者
    // 返回被唤醒的 client 列表 (需要发响应)
    lstl::vector<BlockedClient*> unblockOnKey(const std::string& key) {
        lstl::vector<BlockedClient*> result;
        auto it = waiters_.find(key);
        if (it == waiters_.end()) return result;

        for (auto* bc : it->second) {
            result.push_back(bc);
            // 从其他 key 的等待队列中移除
            for (auto& k : bc->keys) {
                if (k != key) {
                    auto wit = waiters_.find(k);
                    if (wit != waiters_.end())
                        wit->second.remove(bc);
                }
            }
            // 从 blocked_ 列表移除
            blocked_.remove(bc);
        }
        waiters_.erase(it);
        return result;
    }

    // 检查超时 (每秒调用一次)
    lstl::vector<BlockedClient*> checkTimeout(uint64_t now_ms) {
        lstl::vector<BlockedClient*> result;
        auto it = blocked_.begin();
        while (it != blocked_.end()) {
            auto* bc = *it;
            if (bc->timeout_ms > 0 && now_ms >= bc->timeout_ms) {
                result.push_back(bc);
                // 清理等待队列
                for (auto& k : bc->keys) {
                    auto wit = waiters_.find(k);
                    if (wit != waiters_.end())
                        wit->second.remove(bc);
                }
                it = blocked_.erase(it);
                continue;
            }
            ++it;
        }
        return result;
    }

    // session 断开时清理
    void cleanup(Session* s) {
        auto it = blocked_.begin();
        while (it != blocked_.end()) {
            if ((*it)->session == s) {
                for (auto& k : (*it)->keys) {
                    auto wit = waiters_.find(k);
                    if (wit != waiters_.end())
                        wit->second.remove(*it);
                }
                delete *it;
                it = blocked_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    lstl::list<BlockedClient*> blocked_;
    lstl::unordered_map<std::string, lstl::list<BlockedClient*>> waiters_;
};

} // namespace ledis
