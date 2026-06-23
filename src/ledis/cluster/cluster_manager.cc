// ============================================================
// cluster_manager.cc — ClusterManager 实现
// ============================================================

#include "ledis/cluster/cluster_manager.h"
#include "zero/log/log.h"

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>

namespace ledis {
namespace cluster {

// ============================================================
// ClusterRouter 实现
// ============================================================

ClusterRouter::ClusterRouter(ClusterTopology* topo, ClusterManager* mgr)
    : topo_(topo), mgr_(mgr)
{
    buildTable();
}

void ClusterRouter::buildTable() {
    // 无 key 命令 (first_key = 0)
    for (auto* name : {"ping","echo","command","auth","multi","exec","discard",
         "watch","unwatch","dbsize","flushdb","randomkey","scan","config","info",
         "client","shutdown","monitor","slowlog","memory","acl","bgsave","save",
         "time","select","hello","pubsub","cluster","subscribe","unsubscribe",
         "psubscribe","punsubscribe","publish","script"}) {
        addRule(name, 0, 0, 1, false, false);
    }

    // 单 key 命令
    for (auto* name : {"append","decr","decrby","expire","expireat","get",
         "getdel","getrange","getset","incr","incrby","incrbyfloat","persist",
         "pexpire","pexpireat","psetex","pttl","set","setex","setnx","setrange",
         "strlen","ttl","type","expiretime","pexpiretime","object",
         "hdel","hexists","hget","hgetall","hincrby","hincrbyfloat","hkeys",
         "hlen","hset","hsetnx","hvals","hrandfield","hstrlen","hscan",
         "hmget","hmset",
         "lindex","llen","lpop","lpush","lrange","lrem","lset","ltrim",
         "rpop","rpush","lpos",
         "sadd","scard","sismember","smembers","spop","srandmember","srem",
         "sscan","smismember",
         "zadd","zcard","zcount","zincrby","zrange","zrangebyscore","zrank",
         "zrem","zremrangebyrank","zremrangebyscore","zrevrange","zrevrank",
         "zscore","zpopmin","zpopmax","zrandmember","zlexcount","zrangebylex",
         "zrevrangebyscore","zremrangebylex","zscan",
         "pfadd","pfcount","geoadd","geodist","geohash","geopos","georadius",
         "xadd","xdel","xlen","xack","xgroup","xpending","xrange",
         "xread","xreadgroup","sort"}) {
        addRule(name, 1, 1, 1, false, false);
    }

    // 扇出命令 (多个 key 可分布在不同节点)
    for (auto* name : {"del","exists","mget","unlink","touch"}) {
        addRule(name, 1, -1, 1, true, false);
    }

    // MSET/MSETNX: key-value pairs
    addRule("mset",   1, -1, 2, true, false);
    addRule("msetnx", 1, -1, 2, true, false);

    // 禁止跨槽位
    for (auto* name : {"rename","renamenx","sinter","sinterstore","sunion",
         "sunionstore","sdiff","sdiffstore","smove","bitop","lmove","copy",
         "zinter","zunion","zinterstore","zunionstore","zdiff","zdiffstore",
         "pfmerge","bzpopmin","bzpopmax","blpop","brpop"}) {
        addRule(name, 1, -1, 1, false, true);
    }
}

void ClusterRouter::addRule(const char* name, int first, int last, int step,
                             bool fanout, bool cross_slot) {
    rules_[name] = {first, last, step, fanout, cross_slot};
}

bool ClusterRouter::route(CmdContext& ctx) {
    if (ctx.args.empty()) return false;

    // 集群拓扑访问需要加锁 (I/O 线程可能并发修改)
    std::lock_guard<std::mutex> lk(topo_->mtx);

    // 小写命令名
    std::string cmd_lower;
    cmd_lower.reserve(ctx.args[0].size());
    for (char c : ctx.args[0]) cmd_lower += static_cast<char>(c | 0x20);

    // CLUSTER 命令特殊处理
    if (cmd_lower == "cluster") {
        mgr_->handleClusterCommand(ctx);
        return true;
    }

    auto it = rules_.find(cmd_lower);
    if (it == rules_.end()) return false;  // 未知命令, 本地执行

    const RouteRule& rule = it->second;
    if (rule.first_key == 0) return false;  // 无 key, 本地执行

    // 提取 key
    lstl::vector<std::string_view> keys;
    int last = rule.last_key;
    if (last < 0) last = static_cast<int>(ctx.args.size()) + last;

    for (int i = rule.first_key;
         i <= last && i < static_cast<int>(ctx.args.size());
         i += rule.key_step) {
        if (i >= 1)
            keys.push_back(ctx.args[static_cast<size_t>(i)]);
    }

    if (keys.empty()) return false;  // 没有有效 key

    // 检查每个 key 的归属
    const std::string* target = nullptr;
    bool same_node = true;

    for (auto& k : keys) {
        uint16_t slot = ClusterTopology::keySlot(k);
        const std::string* owner = topo_->slotOwner(slot);
        if (!owner || owner->empty()) {
            ctx.replyError("CLUSTERDOWN Hash slot not served");
            return true;
        }
        if (!target) target = owner;
        else if (*owner != *target) same_node = false;
    }

    if (!same_node) {
        if (rule.cross_slot_error) {
            ctx.replyError("CROSSSLOT Keys in request don't hash to the same slot");
            return true;
        }
        if (!rule.fanout) {
            ctx.replyError("CROSSSLOT Keys in request don't hash to the same slot");
            return true;
        }
        // 扇出: 暂不支持，回退到本地
        // TODO: 实现真正的扇出
        return false;
    }

    // 同节点: 检查是否是本节点
    if (topo_->isLocalKey(keys[0]))
        return false;  // 本地执行

    // 非本节点: 返回 MOVED 重定向 (Redis Cluster 标准行为)
    // 格式: MOVED <slot> <ip>:<port>
    const NodeInfo* tgt_node = topo_->getNode(*target);
    std::string moved_addr;
    if (tgt_node) {
        int port = tgt_node->client_port > 0 ? tgt_node->client_port : 6379;
        moved_addr = tgt_node->ip + ":" + std::to_string(port);
    } else {
        moved_addr = *target;
    }
    ctx.replyError("MOVED " + std::to_string(ClusterTopology::keySlot(keys[0]))
                   + " " + moved_addr);
    return true;
}

// ============================================================
// ClusterGossip 实现
// ============================================================

ClusterGossip::ClusterGossip(ClusterTopology* topo, lrpc::RpcNode* rpc,
                               ClusterManager* mgr, int interval_ms, int timeout_ms,
                               zero::Logger::ptr logger)
    : topo_(topo), rpc_(rpc), mgr_(mgr),
      interval_ms_(interval_ms), timeout_ms_(timeout_ms), log_(logger)
{}

void ClusterGossip::tick() {
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    if (now - last_tick_ms_ < static_cast<uint64_t>(interval_ms_))
        return;
    last_tick_ms_ = now;

    checkTimeouts();

    lstl::vector<std::string> targets;
    selectTargets(targets);
    for (auto& t : targets)
        sendPing(t);
}

void ClusterGossip::meet(const std::string& ip, int cluster_port) {
    // 连接到目标并发送 MEET
    std::string temp_id = ip + ":" + std::to_string(cluster_port);
    rpc_->connectTo(temp_id, ip, cluster_port);

    // 构建 MEET body: "self_id self_ip:self_client_port:self_cluster_port serialized_self"
    const NodeInfo* self = topo_->selfNode();
    if (!self) return;

    std::string addr = (self->ip.empty() ? ip : self->ip) + ":"
                     + std::to_string(self->client_port) + ":"
                     + std::to_string(self->cluster_port);

    std::string body = self->id + " " + addr + " " + topo_->serializeNode(*self);

    lstl::vector<std::pair<std::string, std::string>> headers;
    headers.push_back({"sender_id", self->id});

    rpc_->sendOneWay(temp_id, static_cast<uint16_t>(lrpc::MsgType::MEET), body, headers);
}

void ClusterGossip::markFailed(const std::string& node_id) {
    topo_->updateState(node_id, NodeState::FAIL);

    lstl::vector<std::pair<std::string, std::string>> headers;
    headers.push_back({"sender_id", topo_->selfId()});

    rpc_->broadcast(static_cast<uint16_t>(lrpc::MsgType::FAIL),
                    node_id, headers);

    // 清除该节点的槽位
    for (uint16_t s = 0; s < HASH_SLOTS; ++s) {
        const auto* owner = topo_->slotOwner(s);
        if (owner && *owner == node_id)
            topo_->setSlot(s, "", false);
    }
}

void ClusterGossip::onPing(const std::string& sender_id) {
    // 更新心跳时间
    NodeInfo* sender = topo_->getNodeMutable(sender_id);
    if (sender) {
        sender->pong_received_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        if (sender->state == NodeState::PFAIL)
            sender->state = NodeState::ACTIVE;
    }
    sendPong(sender_id);
}

void ClusterGossip::onPong(const std::string& sender_id, const std::string& body) {
    NodeInfo* sender = topo_->getNodeMutable(sender_id);
    if (sender) {
        sender->pong_received_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        if (sender->state == NodeState::PFAIL || sender->state == NodeState::UNKNOWN)
            sender->state = NodeState::ACTIVE;
    }
    processGossipData(body);
}

void ClusterGossip::onMeet(const std::string& sender_id, const std::string& body) {
    // body: "self_id ip:client_port:cluster_port serialized_node"
    size_t sp1 = body.find(' ');
    if (sp1 == std::string::npos) return;
    size_t sp2 = body.find(' ', sp1 + 1);
    if (sp2 == std::string::npos) return;

    // 提取地址 (第二个字段) 和序列化节点数据 (第三个字段以后)
    std::string addr = body.substr(sp1 + 1, sp2 - sp1 - 1);
    std::string node_data = body.substr(sp2 + 1);

    // 解析地址: ip:client_port:cluster_port
    size_t c1 = addr.find(':');
    size_t c2 = addr.find(':', c1 + 1);
    if (c1 == std::string::npos || c2 == std::string::npos) return;

    std::string ip = addr.substr(0, c1);
    int client_port  = std::stoi(addr.substr(c1 + 1, c2 - c1 - 1));
    int cluster_port = std::stoi(addr.substr(c2 + 1));

    // 检查是否已有此节点
    if (topo_->getNode(sender_id)) return;

    // 创建新节点
    NodeInfo new_node;
    if (!topo_->deserializeNode(node_data, new_node)) {
        new_node.id = sender_id;
    }
    // 始终使用 MEET 消息中携带的地址 (最准确)
    new_node.id           = sender_id;
    new_node.ip           = ip;
    new_node.cluster_port = cluster_port;
    new_node.client_port  = client_port;
    if (new_node.state == NodeState::UNKNOWN)
        new_node.state = NodeState::ACTIVE;

    topo_->addNode(new_node);
    rpc_->connectTo(sender_id, ip, cluster_port);

    ZERO_LOG_INFO(log_) << "Cluster: new node joined via MEET: " << sender_id.substr(0,16)
                         << " @ " << ip << ":" << client_port;

    // 回复 PONG
    sendPong(sender_id);
}

void ClusterGossip::onFail(const std::string& sender_id, const std::string& body) {
    // body = failed_node_id
    const std::string& failed_id = body;

    auto& votes = fail_votes_[failed_id];
    bool found = false;
    for (auto& v : votes)
        if (v == sender_id) { found = true; break; }
    if (!found) votes.push_back(sender_id);

    // 多数确认?
    size_t active = 0;
    for (auto& kv : topo_->allNodes()) {
        if (kv.first != failed_id && kv.second.state != NodeState::FAIL)
            active++;
    }

    if (votes.size() >= active / 2 + 1) {
        topo_->updateState(failed_id, NodeState::FAIL);
        fail_votes_.erase(failed_id);

        // 收集故障节点的槽位
        lstl::vector<uint16_t> failed_slots;
        for (uint16_t s = 0; s < HASH_SLOTS; ++s) {
            const auto* o = topo_->slotOwner(s);
            if (o && *o == failed_id) {
                failed_slots.push_back(s);
                topo_->setSlot(s, "", false);
            }
        }

        // 自动故障转移: 如果本节点是该故障节点的副本，提升为主
        NodeInfo* self = topo_->selfNodeMut();
        if (self && !self->is_master && self->master_id == failed_id) {
            ZERO_LOG_WARN(log_) << "Cluster: master " << failed_id.substr(0,16)
                                << " failed, auto-promoting self to master";
            self->is_master = true;
            self->master_id.clear();

            // 接管槽位
            for (auto s : failed_slots)
                topo_->setSlot(s, topo_->selfId(), true);

            // 广播槽位变更
            std::string update_body;
            for (auto s : failed_slots)
                update_body += std::to_string(s) + ":" + topo_->selfId() + ",";

            lstl::vector<std::pair<std::string, std::string>> headers;
            headers.push_back({"sender_id", topo_->selfId()});
            rpc_->broadcast(static_cast<uint16_t>(lrpc::MsgType::UPDATE_CONFIG),
                            update_body, headers);
        }
    }
}

void ClusterGossip::sendPing(const std::string& target_id) {
    const NodeInfo* self = topo_->selfNode();
    if (!self) return;

    std::string body = serializeGossipData();

    lstl::vector<std::pair<std::string, std::string>> headers;
    headers.push_back({"sender_id", self->id});

    rpc_->sendOneWay(target_id, static_cast<uint16_t>(lrpc::MsgType::PING),
                     body, headers);

    // 更新时间戳
    NodeInfo* self_mut = topo_->selfNodeMut();
    if (self_mut) {
        self_mut->ping_sent_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
}

void ClusterGossip::sendPong(const std::string& target_id) {
    const NodeInfo* self = topo_->selfNode();
    if (!self) return;

    std::string body = serializeGossipData();

    lstl::vector<std::pair<std::string, std::string>> headers;
    headers.push_back({"sender_id", self->id});

    rpc_->sendOneWay(target_id, static_cast<uint16_t>(lrpc::MsgType::PONG),
                     body, headers);
}

void ClusterGossip::checkTimeouts() {
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    for (auto& kv : topo_->allNodes()) {
        const NodeInfo& node = kv.second;
        if (node.id == topo_->selfId()) continue;
        if (node.state == NodeState::FAIL) continue;

        int64_t elapsed = static_cast<int64_t>(now - node.pong_received_ms);
        if (elapsed > timeout_ms_) {
            topo_->updateState(node.id, NodeState::PFAIL);
            // 广播投票
            lstl::vector<std::pair<std::string, std::string>> headers;
            headers.push_back({"sender_id", topo_->selfId()});
            rpc_->broadcast(static_cast<uint16_t>(lrpc::MsgType::FAIL),
                            node.id, headers);
        }
    }
}

void ClusterGossip::selectTargets(lstl::vector<std::string>& targets) {
    // 选择最多 3 个随机活跃节点
    lstl::vector<const NodeInfo*> active;
    for (auto& kv : topo_->allNodes()) {
        if (kv.second.id != topo_->selfId() &&
            kv.second.state != NodeState::FAIL)
            active.push_back(&kv.second);
    }

    int count = std::min(3, static_cast<int>(active.size()));
    for (int i = 0; i < count && !active.empty(); ++i) {
        int idx = rand() % static_cast<int>(active.size());
        targets.push_back(active[static_cast<size_t>(idx)]->id);
        active.erase(active.begin() + idx);
    }
}

std::string ClusterGossip::serializeGossipData() {
    std::string data;
    for (auto& kv : topo_->allNodes()) {
        if (!data.empty()) data += "|";
        data += topo_->serializeNode(kv.second);
    }
    return data;
}

void ClusterGossip::processGossipData(const std::string& data) {
    if (data.empty()) return;

    size_t pos = 0;
    while (pos < data.size()) {
        size_t pipe = data.find('|', pos);
        std::string entry = (pipe == std::string::npos)
            ? data.substr(pos) : data.substr(pos, pipe - pos);
        pos = (pipe == std::string::npos) ? data.size() : pipe + 1;

        if (entry.empty()) continue;

        NodeInfo remote;
        if (topo_->deserializeNode(entry, remote)) {
            const NodeInfo* local = topo_->getNode(remote.id);
            if (!local) {
                topo_->addNode(remote);
                if (remote.state != NodeState::FAIL) {
                    rpc_->connectTo(remote.id, remote.ip, remote.cluster_port);
                }
            } else if (remote.epoch > local->epoch) {
                topo_->addNode(remote);
            }
        }
    }
}

// ============================================================
// ClusterTopology 序列化
// ============================================================

std::string ClusterTopology::serializeNode(const NodeInfo& node) const {
    std::stringstream ss;
    ss << node.id << ","
       << node.ip << ","
       << node.client_port << ","
       << node.cluster_port << ","
       << static_cast<int>(node.state) << ","
       << node.epoch << ","
       << (node.is_master ? 'M' : 'S') << ","
       << (node.master_id.empty() ? "-" : node.master_id);

    // 槽位: 用范围格式 (如 "0-100,200-300") 替代位图 hex
    int slot_count = node.slotCount();
    if (slot_count > 0) {
        ss << ",slots:";
        bool in_range = false;
        uint16_t range_start = 0;
        for (uint16_t s = 0; s < HASH_SLOTS; ++s) {
            if (node.ownsSlot(s)) {
                if (!in_range) { range_start = s; in_range = true; }
            } else {
                if (in_range) {
                    if (range_start == s - 1)
                        ss << range_start << ",";
                    else
                        ss << range_start << "-" << (s - 1) << ",";
                    in_range = false;
                }
            }
        }
        if (in_range) {
            if (range_start == HASH_SLOTS - 1)
                ss << range_start;
            else
                ss << range_start << "-" << (HASH_SLOTS - 1);
        } else {
            // 去掉末尾逗号
            std::string s = ss.str();
            if (s.back() == ',') { s.pop_back(); ss.str(""); ss << s; }
        }
    }

    return ss.str();
}

bool ClusterTopology::deserializeNode(const std::string& data, NodeInfo& node) const {
    node.clearSlots();

    size_t pos = 0;
    std::string token;
    auto next = [&]() -> bool {
        size_t end = data.find(',', pos);
        if (end == std::string::npos) {
            if (pos < data.size()) { token = data.substr(pos); pos = data.size(); return true; }
            return false;
        }
        token = data.substr(pos, end - pos);
        pos = end + 1;
        return true;
    };

    if (!next()) return false; node.id = token;
    if (!next()) return false; node.ip = token;
    if (!next()) return false;
    try { node.client_port = std::stoi(token); } catch (...) { return false; }
    if (!next()) return false;
    try { node.cluster_port = std::stoi(token); } catch (...) { return false; }
    if (!next()) return false;
    try { node.state = static_cast<NodeState>(std::stoi(token)); } catch (...) { return false; }
    if (!next()) return false;
    try { node.epoch = std::stoull(token); } catch (...) { return false; }
    if (!next()) return false; node.is_master = (token == "M");
    if (!next()) return false; node.master_id = (token == "-" ? "" : token);

    // 可选 slots: 范围格式 "0-100,200-300" 或 "5,10,15"
    if (pos < data.size() && data.substr(pos, 6) == "slots:") {
        pos += 6;
        std::string slots_str = data.substr(pos);
        size_t comma_pos = 0;
        while (comma_pos < slots_str.size()) {
            size_t next_comma = slots_str.find(',', comma_pos);
            std::string range = (next_comma == std::string::npos)
                ? slots_str.substr(comma_pos) : slots_str.substr(comma_pos, next_comma - comma_pos);
            comma_pos = (next_comma == std::string::npos) ? slots_str.size() : next_comma + 1;

            if (range.empty()) continue;
            size_t dash = range.find('-');
            if (dash != std::string::npos) {
                try {
                    uint16_t s = static_cast<uint16_t>(std::stoi(range.substr(0, dash)));
                    uint16_t e = static_cast<uint16_t>(std::stoi(range.substr(dash + 1)));
                    for (uint16_t i = s; i <= e && i < HASH_SLOTS; ++i)
                        node.setSlot(i, true);
                } catch (...) {}
            } else {
                try {
                    uint16_t s = static_cast<uint16_t>(std::stoi(range));
                    if (s < HASH_SLOTS) node.setSlot(s, true);
                } catch (...) {}
            }
        }
    }
    return true;
}

// ============================================================
// ClusterManager 实现
// ============================================================

ClusterManager::ClusterManager(const ClusterConfig& cfg, StorageEngine* engine,
                                 zero::Logger::ptr logger)
    : config_(cfg), log_(logger ? logger : ZERO_LOG_ROOT()), engine_(engine)
{
    rpc_ = std::make_unique<lrpc::RpcNode>();
    router_ = new ClusterRouter(&topo_, this);
    gossip_ = new ClusterGossip(&topo_, rpc_.get(), this,
                                 cfg.gossip_interval_ms, cfg.node_timeout_ms, log_);
    event_fd_ = eventfd(0, EFD_NONBLOCK);
}

ClusterManager::~ClusterManager() {
    stop();
    delete router_;
    delete gossip_;
    if (event_fd_ >= 0) { ::close(event_fd_); event_fd_ = -1; }
}

bool ClusterManager::start() {
    initNodeId();

    // 将本节点加入拓扑
    NodeInfo self;
    self.id = config_.node_id;
    self.ip = (config_.bind_addr == "0.0.0.0") ? "127.0.0.1" : config_.bind_addr;
    self.cluster_port = config_.cluster_port;
    self.state    = NodeState::ACTIVE;
    self.is_master = true;
    topo_.addNode(self);
    topo_.setSelfId(config_.node_id);

    // 启动 RPC 监听
    if (!rpc_->start(config_.cluster_port, config_.bind_addr))
        return false;

    // 设置 RPC 消息处理器 (在 I/O 线程中调用，只做入队)
    setupRpcHandler();

    running_ = true;

    // 加载持久化或加入集群
    if (!loadNodesConf()) {
        if (!config_.seeds.empty()) {
            joinCluster();
        } else {
            bootstrapAsFirstNode();
        }
    }

    return true;
}

void ClusterManager::stop() {
    if (!running_.exchange(false)) return;
    rpc_->stop();
    saveNodesConf();
}

bool ClusterManager::routeCommand(CmdContext& ctx) {
    if (!running_) return false;
    return router_->route(ctx);
}

void ClusterManager::tick() {
    if (!running_) return;

    // 处理待处理消息 (带锁)
    {
        std::lock_guard<std::mutex> lk(topo_.mtx);
        processPendingMessages();
        gossip_->tick();
    }
}

void ClusterManager::processPendingMessages() {
    lstl::vector<PendingMsg> msgs;
    {
        std::lock_guard<std::mutex> lk(msg_mutex_);
        msgs.swap(pending_msgs_);
    }

    for (auto& m : msgs) {
        switch (static_cast<lrpc::MsgType>(m.msg_type)) {
        case lrpc::MsgType::PING:
            gossip_->onPing(m.sender_id);
            break;
        case lrpc::MsgType::PONG:
            gossip_->onPong(m.sender_id, m.body);
            break;
        case lrpc::MsgType::MEET:
            gossip_->onMeet(m.sender_id, m.body);
            break;
        case lrpc::MsgType::FAIL:
            gossip_->onFail(m.sender_id, m.body);
            break;
        case lrpc::MsgType::FORWARD: {
            // 执行转发命令
            std::string result = executeForwarded(m.body);
            // 使用 sendResponse 回复 (携带 call_id 以匹配请求)
            if (m.call_id != 0) {
                rpc_->sendResponse(m.sender_id, m.call_id,
                    static_cast<uint16_t>(lrpc::MsgType::FORWARD_REPLY),
                    result);
            } else {
                // fallback: 无 call_id 时用 one-way
                lstl::vector<std::pair<std::string, std::string>> headers;
                headers.push_back({"sender_id", config_.node_id});
                rpc_->sendOneWay(m.sender_id,
                    static_cast<uint16_t>(lrpc::MsgType::FORWARD_REPLY),
                    result, headers);
            }
            break;
        }
        case lrpc::MsgType::REPL_ACK: {
            // 副本接收复制数据: body = "offset serialized_cmd"
            size_t sp = m.body.find(' ');
            if (sp != std::string::npos) {
                std::string cmd = m.body.substr(sp + 1);
                executeForwarded(cmd);  // 在本地执行复制命令
            }
            break;
        }
        case lrpc::MsgType::UPDATE_CONFIG:
            break;
        default:
            break;
        }
    }
}

void ClusterManager::onWriteCommand(const lstl::vector<std::string_view>& args) {
    if (args.size() < 2) return;

    // 查找所有副本 (不限 replicas 配置, 有副本就复制)
    lstl::vector<std::string> replicas;
    for (auto& kv : topo_.allNodes()) {
        const NodeInfo& n = kv.second;
        if (!n.is_master && n.master_id == config_.node_id
            && n.state != NodeState::FAIL) {
            replicas.push_back(n.id);
        }
    }
    if (replicas.empty()) return;

    // 序列化命令
    std::string cmd;
    RespWriter::writeArrayHeader(cmd, static_cast<int64_t>(args.size()));
    for (auto& a : args)
        RespWriter::writeBulkString(cmd, a);

    // 广播给所有副本节点
    lstl::vector<std::pair<std::string, std::string>> headers;
    headers.push_back({"sender_id", config_.node_id});

    std::string body = std::to_string(repl_offset_++) + " " + cmd;

    // 找到所有非主节点
    for (auto& kv : topo_.allNodes()) {
        const NodeInfo& n = kv.second;
        if (!n.is_master && n.master_id == config_.node_id
            && n.state != NodeState::FAIL) {
            rpc_->sendOneWay(n.id,
                static_cast<uint16_t>(lrpc::MsgType::REPL_ACK),
                body, headers);
        }
    }
}

// ---- 初始化 ----

void ClusterManager::initNodeId() {
    if (!config_.node_id.empty()) return;

    unsigned char random[20];
    int fd = ::open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t total = 0;
        while (total < 20) {
            ssize_t n = ::read(fd, random + total, 20 - total);
            if (n <= 0) break;
            total += n;
        }
        ::close(fd);

        if (total == 20) {
            char hex[41];
            for (int i = 0; i < 20; ++i)
                snprintf(hex + i * 2, 3, "%02x", random[i]);
            hex[40] = '\0';
            config_.node_id = hex;
            return;
        }
    }

    // 回退
    srand(static_cast<unsigned>(time(nullptr)));
    char hex[41];
    for (int i = 0; i < 40; ++i)
        hex[i] = "0123456789abcdef"[rand() % 16];
    hex[40] = '\0';
    config_.node_id = hex;
}

void ClusterManager::bootstrapAsFirstNode() {
    topo_.assignAllSlotsToSelf();
}

void ClusterManager::joinCluster() {
    // 加入已有集群: 清除本地槽位
    NodeInfo* self = topo_.selfNodeMut();
    if (self) {
        self->clearSlots();
        for (uint16_t s = 0; s < HASH_SLOTS; ++s)
            if (topo_.slotOwner(s) && *topo_.slotOwner(s) == topo_.selfId())
                topo_.setSlot(s, "", false);
    }
    for (auto& seed : config_.seeds) {
        size_t colon = seed.find(':');
        if (colon == std::string::npos) continue;
        std::string ip  = seed.substr(0, colon);
        int port = std::stoi(seed.substr(colon + 1));
        gossip_->meet(ip, port);
    }
}

// ---- RPC 消息处理器 ----

void ClusterManager::setupRpcHandler() {
    rpc_->setMessageHandler(
        [this](uint16_t msg_type, const std::string& sender_id,
               const lstl::vector<std::pair<std::string, std::string>>& headers,
               const std::string& body,
               uint32_t call_id, uint8_t flags) {
            // FORWARD 消息: 入队等待 server 线程处理
            if (msg_type == static_cast<uint16_t>(lrpc::MsgType::FORWARD)) {
                PendingMsg msg;
                msg.msg_type  = msg_type;
                msg.call_id   = (flags & lrpc::FLAG_ONEWAY) ? 0 : call_id;
                msg.sender_id = sender_id;
                msg.body      = body;
                {
                    std::lock_guard<std::mutex> lk(msg_mutex_);
                    pending_msgs_.push_back(std::move(msg));
                }
                uint64_t val = 1;
                ::write(event_fd_, &val, sizeof(val));
                return;
            }

            // 直接处理 gossip 消息 (I/O 线程, 加锁保护拓扑)
            {
                std::lock_guard<std::mutex> lk(topo_.mtx);
                switch (static_cast<lrpc::MsgType>(msg_type)) {
                case lrpc::MsgType::PING:
                    gossip_->onPing(sender_id);
                    break;
                case lrpc::MsgType::PONG:
                    gossip_->onPong(sender_id, body);
                    break;
                case lrpc::MsgType::MEET:
                    gossip_->onMeet(sender_id, body);
                    break;
                case lrpc::MsgType::FAIL:
                    gossip_->onFail(sender_id, body);
                    break;
                default:
                    break;
                }
            }
        });
}

// ---- Forward 执行 ----

std::string ClusterManager::executeForwarded(const std::string& serialized_cmd) {
    RespParser parser;
    const char* data = serialized_cmd.data();
    size_t remaining = serialized_cmd.size();
    size_t consumed = 0;

    auto r = parser.feed(data, remaining, consumed);
    if (r != RespParser::Result::OK || parser.args().empty()) {
        return "-ERR Failed to parse forwarded command\r\n";
    }

    std::string response;
    CmdContext ctx;
    ctx.engine   = engine_;
    ctx.args     = parser.args();
    ctx.response = &response;

    dispatchCommand(ctx);
    return response;
}

// ---- CLUSTER 命令 ----

void ClusterManager::handleClusterCommand(CmdContext& ctx) {
    if (ctx.args.size() < 2) {
        cmdClusterInfo(ctx);
        return;
    }

    std::string sub;
    for (char c : ctx.args[1]) sub += static_cast<char>(c | 0x20);

    if (sub == "info")            cmdClusterInfo(ctx);
    else if (sub == "nodes")      cmdClusterNodes(ctx);
    else if (sub == "slots")      cmdClusterSlots(ctx);
    else if (sub == "meet")       cmdClusterMeet(ctx);
    else if (sub == "keyslot")    cmdClusterKeyslot(ctx);
    else if (sub == "myid")       cmdClusterMyid(ctx);
    else if (sub == "addslots")   cmdClusterAddslots(ctx);
    else if (sub == "delslots")   cmdClusterDelslots(ctx);
    else if (sub == "forget")     cmdClusterForget(ctx);
    else if (sub == "setslot")    cmdClusterSetslot(ctx);
    else if (sub == "replicate")  cmdClusterReplicate(ctx);
    else if (sub == "failover")   cmdClusterFailover(ctx);
    else if (sub == "flushslots") cmdClusterFlushslots(ctx);
    else if (sub == "saveconfig") cmdClusterSaveconfig(ctx);
    else ctx.replyError("ERR Unknown CLUSTER subcommand");
}

void ClusterManager::cmdClusterInfo(CmdContext& ctx) {
    std::string body;
    body += "cluster_state:";
    body += topo_.isFullCoverage() ? "ok" : "fail";
    body += "\r\ncluster_slots_assigned:";
    int assigned = 0;
    for (uint16_t s = 0; s < HASH_SLOTS; ++s)
        if (topo_.slotOwner(s)) assigned++;
    body += std::to_string(assigned);
    body += "\r\ncluster_slots_ok:";
    body += std::to_string(assigned);
    body += "\r\ncluster_known_nodes:";
    body += std::to_string(topo_.allNodes().size());
    body += "\r\ncluster_size:";
    int masters = 0;
    for (auto& kv : topo_.allNodes())
        if (kv.second.is_master) masters++;
    body += std::to_string(masters);
    body += "\r\ncluster_current_epoch:";
    body += std::to_string(topo_.epoch());
    body += "\r\ncluster_my_epoch:1\r\n";

    RespWriter::writeBulkString(*ctx.response, body);
}

void ClusterManager::cmdClusterNodes(CmdContext& ctx) {
    std::string body;
    for (auto& kv : topo_.allNodes()) {
        const NodeInfo& n = kv.second;
        body += n.id + " ";
        body += n.ip + ":" + std::to_string(n.client_port)
             + "@" + std::to_string(n.cluster_port) + " ";
        if (n.id == config_.node_id) body += "myself,";
        body += n.is_master ? "master" : "slave";
        if (n.state == NodeState::FAIL) body += ",fail";
        else if (n.state == NodeState::PFAIL) body += ",fail?";
        body += " ";
        body += (n.master_id.empty() ? "-" : n.master_id) + " ";
        body += "0 0 " + std::to_string(n.epoch) + " ";
        body += (n.id == config_.node_id ||
                 rpc_->isConnected(n.id)) ? "connected" : "disconnected";

        if (n.slotCount() > 0) {
            body += " ";
            bool in_range = false;
            uint16_t start = 0;
            for (uint16_t s = 0; s < HASH_SLOTS; ++s) {
                if (n.ownsSlot(s)) {
                    if (!in_range) { start = s; in_range = true; }
                } else {
                    if (in_range) {
                        if (start == s - 1) body += std::to_string(start) + " ";
                        else body += std::to_string(start) + "-" + std::to_string(s - 1) + " ";
                        in_range = false;
                    }
                }
            }
            if (in_range) {
                if (start == HASH_SLOTS - 1) body += std::to_string(start);
                else body += std::to_string(start) + "-" + std::to_string(HASH_SLOTS - 1);
            }
        }
        body += "\n";
    }
    RespWriter::writeBulkString(*ctx.response, body);
}

void ClusterManager::cmdClusterSlots(CmdContext& ctx) {
    // 按节点聚合槽位范围
    lstl::unordered_map<std::string, lstl::vector<std::pair<uint16_t, uint16_t>>> ranges;
    for (auto& kv : topo_.allNodes()) {
        if (!kv.second.is_master) continue;
        const NodeInfo& n = kv.second;
        bool in_range = false;
        uint16_t start = 0;
        for (uint16_t s = 0; s < HASH_SLOTS; ++s) {
            if (n.ownsSlot(s)) {
                if (!in_range) { start = s; in_range = true; }
            } else {
                if (in_range) {
                    ranges[n.id].push_back({start, static_cast<uint16_t>(s - 1)});
                    in_range = false;
                }
            }
        }
        if (in_range) ranges[n.id].push_back({start, HASH_SLOTS - 1});
    }

    int total = 0;
    for (auto& kv : ranges) total += static_cast<int>(kv.second.size());
    RespWriter::writeArrayHeader(*ctx.response, total);

    for (auto& kv : ranges) {
        const NodeInfo* n = topo_.getNode(kv.first);
        if (!n) continue;
        for (auto& rng : kv.second) {
            RespWriter::writeArrayHeader(*ctx.response, 3);
            RespWriter::writeInteger(*ctx.response, rng.first);
            RespWriter::writeInteger(*ctx.response, rng.second);
            RespWriter::writeArrayHeader(*ctx.response, 3);
            RespWriter::writeBulkString(*ctx.response, n->ip);
            RespWriter::writeInteger(*ctx.response, n->client_port);
            RespWriter::writeBulkString(*ctx.response, n->id);
        }
    }
}

void ClusterManager::cmdClusterMeet(CmdContext& ctx) {
    if (ctx.args.size() < 4) {
        ctx.replyError("ERR Wrong number of arguments for CLUSTER MEET");
        return;
    }
    std::string ip(ctx.args[2]);
    int port = std::stoi(std::string(ctx.args[3]));

    // 加入已有集群: 清除本节点自分配的槽位
    NodeInfo* self = topo_.selfNodeMut();
    if (self) {
        self->clearSlots();
        for (uint16_t s = 0; s < HASH_SLOTS; ++s)
            if (topo_.isLocalSlot(s))
                topo_.setSlot(s, "", false);
    }

    gossip_->meet(ip, port);
    ctx.replyOK();
}

void ClusterManager::cmdClusterKeyslot(CmdContext& ctx) {
    if (ctx.args.size() < 3) { ctx.replyError("ERR Wrong number of arguments"); return; }
    ctx.replyInteger(static_cast<int64_t>(ClusterTopology::keySlot(ctx.args[2])));
}

void ClusterManager::cmdClusterMyid(CmdContext& ctx) {
    RespWriter::writeBulkString(*ctx.response, config_.node_id);
}

void ClusterManager::cmdClusterAddslots(CmdContext& ctx) {
    if (ctx.args.size() < 3) { ctx.replyError("ERR No slots specified"); return; }
    for (size_t i = 2; i < ctx.args.size(); ++i) {
        int slot = std::stoi(std::string(ctx.args[i]));
        if (slot < 0 || slot >= HASH_SLOTS) { ctx.replyError("ERR Slot out of range"); return; }
        const auto* o = topo_.slotOwner(static_cast<uint16_t>(slot));
        if (o && !o->empty()) { ctx.replyError("ERR Slot already assigned"); return; }
    }
    for (size_t i = 2; i < ctx.args.size(); ++i) {
        int slot = std::stoi(std::string(ctx.args[i]));
        topo_.setSlot(static_cast<uint16_t>(slot), config_.node_id, true);
    }
    saveNodesConf();
    ctx.replyOK();
}

void ClusterManager::cmdClusterDelslots(CmdContext& ctx) {
    if (ctx.args.size() < 3) { ctx.replyError("ERR No slots specified"); return; }
    for (size_t i = 2; i < ctx.args.size(); ++i) {
        int slot = std::stoi(std::string(ctx.args[i]));
        if (slot < 0 || slot >= HASH_SLOTS) { ctx.replyError("ERR Slot out of range"); return; }
        if (!topo_.isLocalSlot(static_cast<uint16_t>(slot))) {
            ctx.replyError("ERR Slot not owned by this node"); return;
        }
    }
    for (size_t i = 2; i < ctx.args.size(); ++i) {
        int slot = std::stoi(std::string(ctx.args[i]));
        topo_.setSlot(static_cast<uint16_t>(slot), "", false);
    }
    saveNodesConf();
    ctx.replyOK();
}

void ClusterManager::cmdClusterForget(CmdContext& ctx) {
    if (ctx.args.size() < 3) { ctx.replyError("ERR No node ID specified"); return; }
    topo_.removeNode(std::string(ctx.args[2]));
    ctx.replyOK();
}

void ClusterManager::cmdClusterSetslot(CmdContext& ctx) {
    if (ctx.args.size() < 4) { ctx.replyError("ERR Wrong number of arguments"); return; }
    int slot = std::stoi(std::string(ctx.args[2]));
    std::string sub;
    for (char c : ctx.args[3]) sub += static_cast<char>(c | 0x20);

    if (sub == "node" && ctx.args.size() >= 5) {
        topo_.setSlot(static_cast<uint16_t>(slot), std::string(ctx.args[4]), true);
        saveNodesConf();
        ctx.replyOK();
    } else if (sub == "stable") {
        ctx.replyOK();
    } else {
        ctx.replyError("ERR Unsupported CLUSTER SETSLOT action");
    }
}

void ClusterManager::cmdClusterReplicate(CmdContext& ctx) {
    if (ctx.args.size() < 3) { ctx.replyError("ERR Wrong number of arguments"); return; }
    std::string master_id(ctx.args[2]);
    if (!topo_.getNode(master_id)) { ctx.replyError("ERR Unknown node"); return; }
    NodeInfo* self = topo_.selfNodeMut();
    if (self) {
        self->is_master = false;
        self->master_id = master_id;
        is_replicating_ = true;
        master_id_ = master_id;
    }
    saveNodesConf();
    ctx.replyOK();
}

void ClusterManager::cmdClusterFailover(CmdContext& ctx) {
    NodeInfo* self = topo_.selfNodeMut();
    if (!self || self->is_master) { ctx.replyError("ERR Already master"); return; }

    lstl::vector<uint16_t> slots;
    NodeInfo* master = topo_.getNodeMutable(self->master_id);
    if (master) master->getSlotList(slots);

    self->is_master = true;
    self->master_id.clear();
    is_replicating_ = false;

    for (auto s : slots)
        topo_.setSlot(s, topo_.selfId(), true);

    saveNodesConf();
    ctx.replyOK();
}

void ClusterManager::cmdClusterFlushslots(CmdContext& ctx) {
    NodeInfo* self = topo_.selfNodeMut();
    if (self) {
        for (uint16_t s = 0; s < HASH_SLOTS; ++s)
            if (self->ownsSlot(s)) topo_.setSlot(s, "", false);
    }
    ctx.replyOK();
}

void ClusterManager::cmdClusterSaveconfig(CmdContext& ctx) {
    saveNodesConf();
    ctx.replyOK();
}

// ---- 持久化 ----

void ClusterManager::saveNodesConf() {
    if (config_.nodes_conf_path.empty()) return;

    std::string data;
    for (auto& kv : topo_.allNodes()) {
        data += topo_.serializeNode(kv.second) + "\n";
    }
    data += "vars currentEpoch " + std::to_string(topo_.epoch()) + "\n";

    int fd = ::open(config_.nodes_conf_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        ::write(fd, data.data(), data.size());
        ::close(fd);
    }
}

bool ClusterManager::loadNodesConf() {
    if (config_.nodes_conf_path.empty()) return false;

    int fd = ::open(config_.nodes_conf_path.c_str(), O_RDONLY);
    if (fd < 0) return false;

    std::string content;
    content.reserve(65536);
    char buf[65536]; ssize_t n;
    while ((n = ::read(fd, buf, sizeof(buf))) > 0)
        content.append(buf, static_cast<size_t>(n));
    ::close(fd);

    if (content.empty()) return false;

    // 清除默认 self
    topo_.removeNode(topo_.selfId());

    size_t pos = 0;
    while (pos < content.size()) {
        size_t nl = content.find('\n', pos);
        if (nl == std::string::npos) nl = content.size();
        std::string line = content.substr(pos, nl - pos);
        pos = nl + 1;

        if (line.empty()) continue;
        if (line.find("vars ") == 0) {
            // vars currentEpoch N
            size_t sp = line.rfind(' ');
            if (sp != std::string::npos)
                topo_.setEpoch(std::stoull(line.substr(sp + 1)));
            continue;
        }

        NodeInfo node;
        if (topo_.deserializeNode(line, node)) {
            if (node.id == config_.node_id) {
                config_.node_id = node.id;
                topo_.setSelfId(node.id);
            }
            topo_.addNode(node);
        }
    }

    return true;
}

} // namespace cluster
} // namespace ledis
