#pragma once
// ============================================================
// cluster_router.h — 命令路由 (决定本地执行还是转发)
// ============================================================

#include <cstdint>
#include <string>
#include <string_view>
#include <functional>
#include <lstl/container/vector.h>
#include <lstl/container/unordered_map.h>

#include "ledis/core/command.h"
#include "ledis/protocol/resp_writer.h"
#include "ledis/cluster/cluster_types.h"
#include "ledis/cluster/cluster_topology.h"

namespace ledis {
namespace cluster {

// 前向声明
class ClusterManager;

// ---- 路由规则 ----
struct RouteRule {
    int  first_key = 0;   // 第一个 key 的 1-based 索引 (0 = 无 key)
    int  last_key  = 0;   // 最后一个 key 的 1-based 索引 (-1 = 最后)
    int  key_step  = 1;
    bool fanout    = false;  // 允许扇出到多节点 (MGET/DEL/EXISTS)
    bool cross_slot_error = false;  // 跨槽位报错 (RENAME/SINTER 等)
};

class ClusterRouter {
public:
    ClusterRouter(ClusterTopology* topo, ClusterManager* mgr);

    // 路由决策: 返回 true 表示已处理 (本地执行或转发完成)
    // 返回 false 表示需要本地执行 (response 未写入)
    bool route(CmdContext& ctx);

private:
    void buildTable();
    void addRule(const char* name, int first, int last, int step,
                 bool fanout, bool cross_slot);

    ClusterTopology* topo_;
    ClusterManager*  mgr_;
    lstl::unordered_map<std::string, RouteRule> rules_;
};

} // namespace cluster
} // namespace ledis
