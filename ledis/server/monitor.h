#pragma once

#include <string>
#include <string_view>
#include <lstl/container/unordered_set.h>
#include <lstl/container/vector.h>

namespace ledis {

struct ClientContext;

// ============================================================
// MonitorManager — 实时命令监视 (MONITOR)
// ============================================================
class MonitorManager {
public:
    void add(ClientContext* c)   { monitors_.insert(c); }
    void remove(ClientContext* c) { monitors_.erase(c); }
    bool isMonitoring(ClientContext* c) const { return monitors_.count(c) > 0; }
    int  count() const { return static_cast<int>(monitors_.size()); }

    // 向所有 MONITOR 客户端广播命令
    void broadcast(const lstl::vector<std::string_view>& args,
                   const std::string& client_addr);

private:
    lstl::unordered_set<ClientContext*> monitors_;
};

} // namespace ledis
