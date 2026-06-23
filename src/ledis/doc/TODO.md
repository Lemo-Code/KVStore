# Ledis Cluster — 待办 & 已知问题

> 更新时间: 2026-06-22  
> 分支: main (87e691f6 base)

---

## 已完成 ✅

### RPC 框架 (lrpc/)
- [x] 二进制协议: Magic + FrameLen + CallId + Headers + Body
- [x] RpcNode: epoll I/O 线程, connect/sendOneWay/sendRequest/sendResponse
- [x] 请求-响应 CallId 匹配 + 超时
- [x] 直接发送 + EPOLLOUT fallback
- [x] EINPROGRESS 异步连接 + SO_ERROR 确认

### 集群层 (ledis/cluster/)
- [x] ClusterTopology: 16384 槽位, CRC16, NodeInfo bitmap
- [x] ClusterRouter: 200+ 命令路由表, MOVED 重定向
- [x] ClusterGossip: PING/PONG/MEET/FAIL, 故障检测投票
- [x] ClusterManager: CLUSTER 16 个子命令, nodes.conf 持久化
- [x] 自动故障转移: master FAIL → replica 提升 + 接管槽位
- [x] ClusterConfig + CLI 参数解析 (--cluster-enabled 等)

### 集成
- [x] 集群消息在 RPC I/O 线程直接处理 (避免 fiber 模型阻塞)
- [x] main.cc 主循环 100ms clusterTick
- [x] 三节点集群互相发现
- [x] 性能: 单节点集群 ~90万 SET QPS, ~110万 GET QPS (管道化)

---

## 已知 Bug 🐛

### P0 - CLUSTERDOWN 间歇出现
- **现象**: MEET + REPLICATE 后 SET 偶尔返回 `CLUSTERDOWN Hash slot not served`
- **频率**: 约 1-2/5 次
- **原因**: gossip `addNode` 回环时 slot_owner_ 被覆盖/清空, 自愈代码缓解但未根治
- **定位方向**: `ClusterTopology::addNode` self-protection 逻辑, gossip 数据竞态
- **临时绕过**: 操作前等 3 秒让 gossip 稳定

### P1 - 管道化压测长跑 BrokenPipe
- **现象**: >30s 连续管道化压测 (batch=500) 时连接断开
- **原因**: 疑似 handleClient 发送缓冲区溢出或 recv 被 zero hook 拦截
- **影响**: 生产环境高并发可能触发

### P2 - nodes.conf 端口信息丢失
- **现象**: 重启后 cluster_port/client_port 可能为 0
- **原因**: serialization/deserialization 格式不完整

---

## 未实现功能 📋

### 复制 & 数据一致性
- [ ] **REPL_ACK 复制确认**: 副本收到复制数据后回复 ACK
- [ ] **复制偏移跟踪**: 主节点记录每个副本的复制进度
- [ ] **部分重同步**: 副本断连后增量同步 (当前全量)
- [ ] **WAIT 命令**: 同步复制确认 (等待 N 个副本确认)
- [ ] **复制数据一致性测试**: 端到端验证主→副本→故障转移数据不丢

### 槽位迁移 (slot migration)
- [ ] MIGRATE/IMPORTING 状态机
- [ ] 逐个 key 迁移 (RESTORE 序列化)
- [ ] 迁移期间 ASK 重定向
- [ ] 迁移提交/回滚

### 故障转移 & 高可用
- [ ] **cluster-require-full-coverage**: 全槽位覆盖检查
- [ ] **手动故障转移 TAKEOVER/FORCE**: CLUSTER FAILOVER 子命令
- [ ] **脑裂保护**: 网络分区检测 + 多数派判断
- [ ] **epoch 递增**: 配置变更时 bumpEpoch
- [ ] **FAILOVER 端到端测试**: 杀主→验证副本提升→验证数据

### 命令完整性
- [ ] **ASKING 重定向**: 迁移期间的临时重定向
- [ ] **READONLY**: 副本读 (当前全部 MOVED)
- [ ] **MULTI/EXEC 跨槽位**: 分布式事务
- [ ] **Pub/Sub 跨节点广播**: CLUSTER PUBLISH
- [ ] **SCAN 跨节点**: 遍历所有节点的 key
- [ ] **DBSIZE 集群总计**: 汇总所有节点

### 客户端 & 生态
- [ ] **集群模式 redis-cli**: 自动处理 MOVED/ASK
- [ ] **--cluster-seeds 自动加入**: 修复时序问题
- [ ] **redis-benchmark 集群兼容**
- [ ] **CLUSTER SLOTS 格式兼容**: 对齐 Redis 输出

### 运维 & 稳定性
- [ ] **日志系统**: 集群事件日志 (节点加入/离开/故障)
- [ ] **监控指标**: QPS/延迟/连接数/复制延迟
- [ ] **节点健康检查**: 定期自检, 异常告警
- [ ] **优雅关闭**: SHUTDOWN 时通知其他节点
- [ ] **配置热重载**: CONFIG SET cluster-*

### 性能 & 优化
- [ ] **slot_owner_ 与 bitmap 同步优化**: 消除自愈开销
- [ ] **命令路由缓存**: 热点 key 的路由缓存
- [ ] **批量命令扇出**: MGET/DEL 跨节点并行执行
- [ ] **连接池**: 复用 RPC 连接
- [ ] **零拷贝 I/O**: io_uring 批量读/写

### 测试 & 文档
- [ ] **单元测试**: RPC codec, topology, 路由表
- [ ] **集成测试**: 故障转移, 槽位迁移
- [ ] **混沌测试**: 随机杀节点/网络分区
- [ ] **性能基准**: redis-benchmark 对比
- [ ] **内存泄漏**: valgrind 24h
- [ ] **API 文档**: CLUSTER 命令说明

---

## 架构备注

### 线程模型
```
main thread (main.cc)
  └─ clusterTick() 每 100ms
       └─ processPendingMessages() + gossip tick

fiber scheduler (1 thread)
  └─ handleClient() → 客户端请求处理
       ├─ cluster check (每轮循环)
       ├─ route() → MOVED 或本地 dispatchCommand
       └─ onWriteCommand() → 发送 REPL_ACK 到副本

RPC I/O thread (std::thread)
  ├─ epoll_wait → handleRead/Write/Accept
  ├─ dispatchMessage → msg_handler
  └─ 直接处理 gossip (PING/PONG/MEET/FAIL)
       └─ 加 topo_.mtx 保护 ClusterTopology
```

### 锁顺序
```
topo_.mtx → send_mutex_ (I/O 线程 gossip)
server fiber → topo_.mtx (route/tick)
不会出现反向获取, 无死锁风险
```

### 关键文件
```
lrpc/protocol.h              - RPC 协议定义 + 编解码
lrpc/rpc_connection.h         - RpcNode: 连接管理 + I/O 线程
ledis/cluster/cluster_types.h - NodeInfo/NodeState/槽位常量
ledis/cluster/cluster_config.h- 集群配置
ledis/cluster/cluster_topology.h - 槽位映射 + 节点管理
ledis/cluster/cluster_router.h   - 命令路由表
ledis/cluster/cluster_gossip.h   - Gossip 协议
ledis/cluster/cluster_manager.h  - 总协调器 (接口)
ledis/cluster/cluster_manager.cc - 总协调器 (实现, ~1100行)
ledis/server/server.h        - 集群集成点
ledis/main.cc                - CLI + clusterTick
```
