# Ledis 总体架构

> **状态**：设计冻结（Phase 0）  
> **版本**：v0.1  
> **网络栈**：LemoNettyCore v0.4（`third/LemoNettyCore`，动态库 `lib/liblemo_nettycore.so`）

---

## 1. 背景与目标

### 1.1 背景

KVStore 已完成高性能协程网络库 **LemoNettyCore**，具备 `TcpServer`、`Runtime`、syscall hook、定时器与 YAML 配置能力。下一步在其上实现 **Ledis**——语义对齐 Redis 的内存 KV 服务，作为仓库内「网络 + 存储 + 内存池」的端到端验证载体。

仓库内 **无 Ledis 实现代码**；`sylar/db/redis_*` 仅为 Redis **客户端**，不可复用为服务端。

### 1.2 设计目标

| # | 目标 |
|---|------|
| G1 | **Redis 子集兼容**：MVP 支持 `redis-cli` 对 String 命令的基本交互（RESP2） |
| G2 | **分层可测**：协议、存储、命令、网络接入各自可单测，存储层不依赖 socket |
| G3 | **单线程命令语义**：与 Redis 一致，热路径 dict 无锁 |
| G4 | **协程 IO 优势**：多连接轻量、阻塞 read/write 自动 yield |
| G5 | **可演进**：预留持久化、多类型、RESP3，不推翻 Phase 1 接口 |

### 1.3 非目标（首版）

- 集群 / 主从复制 / Sentinel
- Redis Module API
- 与 Redis 7 全命令 100% 行为一致
- Phase 1 持久化（RDB/AOF）

---

## 2. 在 KVStore 中的位置

```
┌─────────────────────────────────────────────────────────────┐
│  ledis-server          进程入口、信号、配置                    │
├─────────────────────────────────────────────────────────────┤
│  ledis-session         连接会话、Pipeline、命令投递            │
├─────────────────────────────────────────────────────────────┤
│  ledis-command         命令注册表、参数校验、错误码            │
├─────────────────────────────────────────────────────────────┤
│  ledis-protocol        RESP2 流式解析与编码                  │
├─────────────────────────────────────────────────────────────┤
│  ledis-store           DB/keyspace/对象/过期                 │
├─────────────────────────────────────────────────────────────┤
│  LemoNettyCore         TcpServer · Runtime · Fiber · Timer   │
├─────────────────────────────────────────────────────────────┤
│  lstl · kv_pool        容器 · 小对象内存池（store 内部选用）   │
│  storage/lsmtree       持久化后端（Phase 4+ 可选）             │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 依赖规则

```
ledis-server → ledis-session → ledis-command → ledis-store
                    ↓              ↓
              ledis-protocol    lstl / kv_pool
                    ↓
              LemoNettyCore（链接 liblemo_nettycore.so）
```

**禁止**：

- `ledis-store` / `ledis-protocol` 反向依赖 `lemo::server` 或 socket
- 存储层 include 网络头文件

---

## 3. 分层架构

```
┌──────────────────────────────────────────────────────────────────┐
│ L5  Application     ledis-server · 配置 · 优雅退出               │
├──────────────────────────────────────────────────────────────────┤
│ L4  Session         Session · SessionContext · CommandQueue     │
│                     （LemoNettyCore TcpServer 回调内）            │
├──────────────────────────────────────────────────────────────────┤
│ L3  Command         CommandRegistry · Handlers · ClientContext  │
├──────────────────────────────────────────────────────────────────┤
│ L2  Protocol        RespReader · RespWriter · RespValue         │
├──────────────────────────────────────────────────────────────────┤
│ L1  Store           DBManager · Keyspace · LedisObject · Expire │
├──────────────────────────────────────────────────────────────────┤
│ L0  Foundation      sds/key · lstl · kv_pool · lsmtree(后期)    │
├──────────────────────────────────────────────────────────────────┤
│ ⊥   Network         LemoNettyCore（正交注入，store 不可见）       │
└──────────────────────────────────────────────────────────────────┘
```

### 3.1 各层职责

| 层 | 模块 | 职责 |
|----|------|------|
| L5 | `ledis-server` | `main`、加载 YAML、创建 Runtime/TcpServer、注册信号 |
| L4 | `ledis-session` | 每连接协程：读缓冲、解析、投递命令、写回响应 |
| L3 | `ledis-command` | 命令分发表、`SELECT`/`GET`/`SET` 等 handler |
| L2 | `ledis-protocol` | RESP2 增量解析、编码、半包处理 |
| L1 | `ledis-store` | 16 个 DB、key→对象、过期删除 |
| L0 | foundation | 二进制安全 key、内存分配策略 |
| ⊥ | LemoNettyCore | TCP 接入、调度、定时器 |

---

## 4. 并发模型

Ledis 最关键的设计决策：**命令执行单线程，IO 可多协程**。

### 4.1 推荐模型（Phase 1 起采用）

```
                    ┌─────────────────┐
  Client × N ──────►│  IO Runtime     │
                    │  (io.threads)   │
                    │  Session 协程   │
                    └────────┬────────┘
                             │ 已解析 Command + ReplySlot
                             ▼
                    ┌─────────────────┐
                    │  CommandQueue   │  MPSC / Fiber 队列
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │  DB Runtime     │  threads = 1
                    │  执行命令        │  dict 无锁
                    │  过期采样        │
                    └─────────────────┘
```

| 角色 | 线程/协程 | 可访问 store |
|------|-----------|--------------|
| Session | IO Runtime 内协程 | **否**（仅投递） |
| DB Worker | 独立 Runtime，`threads=1` | **是**（唯一写者） |
| 过期任务 | DB Runtime 内 Timer 回调 | **是** |

### 4.2 Phase 0 简化路径（可选）

MVP 最初可 **单 Runtime、`io.threads=1`**，Session 与 DB 同线程，省略队列。通过配置开关 `ledis.single_thread_mode: true` 启用，便于调试。**Phase 2 前必须切换到 IO/DB 分离**，避免 IO 阻塞命令执行。

### 4.3 与 Redis 6+ 的对应关系

| Redis | Ledis |
|-------|-------|
| 主线程执行命令 | DB Runtime 单线程 |
| IO 线程读写字节 | IO Runtime Session 协程 |
| 单进程 16 DB | `DBManager` 固定 16 个 `Keyspace` |

---

## 5. 核心数据流

### 5.1 连接建立

```
TcpServer::setConnectionHandler(SessionMain)
  → SessionMain(sock):
       创建 SessionContext { db=0, authed=false }
       ChainBuffer chain（长连接读缓冲，连接内复用）
       循环直到连接关闭 / EOF
       chain.clear()
```

### 5.2 命令处理（Pipeline 友好，长连接）

```
chain.readFd(sock)              // 追加到连接读缓冲
while TryParseOne(chain, cmd):
  CommandQueue.submit(ctx, cmd)
  Session.send(RespWriter.encode(result))
// 半包留在 chain，下次 read 继续
```

### 5.3 键过期

双策略，与 Redis 一致：

| 策略 | 触发点 | 实现 |
|------|--------|------|
| 惰性 | GET 等读命令 | 访问 key 时比较 `expire_at` |
| 主动 | 定时 | DB Runtime 上 `Timer` 周期调用 `ActiveExpireCycle(limit)` |

---

## 6. 存储模型

### 6.1 对象（对标 redisObject）

```
LedisObject
  type:     kString | kHash | kList | kSet | kZSet   （Phase 1 仅 kString）
  encoding: kRaw | kInt                              （Phase 1 仅 kRaw）
  data:     指向 sds 或子结构
```

### 6.2 Keyspace

```
DBManager
  databases_[16]  →  Keyspace
                          dict_:     key → LedisObject
                          expires_:  key → expire_at_ms（辅助索引，可选）
```

- **Key**：二进制安全，SDS 风格（`len + buf`），允许 `\0`
- **Dict**：Phase 1 使用 `lstl::unordered_map`；数据量增大后可换链式哈希 + 渐进 rehash
- **内存**：`LedisObject`、dict 节点优先 `kv_pool`（见 `docs/lstl/memory_pool_design.md` §13）

### 6.3 持久化路线（Phase 4+）

| 方案 | 优点 | 选用时机 |
|------|------|----------|
| RDB-lite | 与 `redis-cli --rdb` 生态接近，实现快 | **首选 v1 持久化** |
| lsmtree | 与 KVStore 存储栈统一，WAL 可恢复 | 需要与 LSM 引擎深度集成时 |

Store 层预留接口：`SnapshotWriter` / `LoadSnapshot`，Phase 1 空实现。

---

## 7. 协议层（RESP2）

### 7.1 支持类型

| 前缀 | 类型 | Ledis 用途 |
|------|------|------------|
| `+` | Simple String | OK、PONG |
| `-` | Error | ERR 系列 |
| `:` | Integer | DEL 返回值、TTL |
| `$` | Bulk String | key/value |
| `*` | Array | 命令帧 |

### 7.2 约束

- **增量解析**：不得假设单次 `recv` 读满一帧
- **query_buffer_limit**：超限断开并返回 `-ERR max query buffer reached`
- **Inline 命令**：Phase 3 可选（`GET foo` 单行），MVP 仅数组格式

---

## 8. 命令层

### 8.1 命令上下文

```cpp
struct ClientContext {
  int db_index;           // 0..15
  bool authenticated;
  // Phase 3+: name, flags
};
```

### 8.2 分阶段命令集

详见 [implementation_order.md](./implementation_order.md)。概要：

| 阶段 | 命令 |
|------|------|
| P1 | PING, GET, SET, DEL, EXISTS |
| P2 | SELECT, DBSIZE, EXPIRE, TTL, PERSIST, FLUSHDB |
| P3 | MGET, MSET, INCR, INCRBY, DECR, DECRBY |
| P4 | HGET, HSET, HDEL, HGETALL, LPUSH, RPOP, … |
| P5 | INFO, CONFIG GET, AUTH(requirepass) |

### 8.3 错误语义

对齐 Redis 常用错误：`ERR unknown command`、`WRONGTYPE`、`NOAUTH` 等，由 command 层生成，protocol 层只编码。

---

## 9. 配置

**详细说明见 [configuration.md](./configuration.md)**（CLI 全量参数、CONFIG GET、默认值、持久化路径）。

当前 **`ledis-server` 仅支持命令行**；`CONFIG GET` 提供只读子集。YAML 加载为规划项（见 configuration.md §6）。

在 LemoNettyCore YAML 基础上增加 `ledis` 段（**目标形态**，尚未接入）：

```yaml
server:
  name: ledis
  host: 0.0.0.0
  port: 6379

io:
  threads: 4
  use_caller: false
  name: ledis-io

ledis:
  single_thread_mode: false    # true = MVP 调试模式
  db_count: 16
  maxmemory: 0                 # 0 = 不限制
  maxclients: 10000
  query_buffer_limit: 1048576
  requirepass: ""
  active_expire_enabled: true
  active_expire_cycle_us: 1000

log:
  level: INFO
  # … 复用 nettycore log 配置
```

配置加载顺序：`InitNettyConfigVars` → 加载 YAML → `InitLedisConfigVars` → `Apply*`。

---

## 10. 目录与构建

### 10.1 源码目录（规划）

```
ledis/
├── CMakeLists.txt
├── include/ledis/
│   ├── protocol/
│   │   ├── resp_value.h
│   │   ├── resp_reader.h
│   │   └── resp_writer.h
│   ├── store/
│   │   ├── sds.h
│   │   ├── object.h
│   │   ├── keyspace.h
│   │   └── db_manager.h
│   ├── command/
│   │   ├── command.h
│   │   ├── registry.h
│   │   └── handlers/
│   ├── session/
│   │   ├── session.h
│   │   └── command_queue.h
│   ├── config/
│   │   └── ledis_config.h
│   └── server/
│       └── ledis_server.h
└── src/
    └── （与 include 对称）

tests/ledis/
├── protocol/
├── store/
├── command/
└── integration/

bin/Ledis/
└── ledis-server
```

### 10.2 CMake 目标（规划）

| 目标 | 类型 | 说明 |
|------|------|------|
| `ledis_protocol` | STATIC | RESP 解析/编码 |
| `ledis_store` | STATIC | 存储引擎 |
| `ledis_command` | STATIC | 命令层 |
| `ledis_session` | STATIC | 会话 + 队列 |
| `ledis_server` | EXECUTABLE | 链接 `lemo_nettycore` |

---

## 11. 测试策略

| 层级 | 目录 | 方式 |
|------|------|------|
| protocol | `tests/ledis/protocol/` | 字节流单测，含半包/粘包 |
| store | `tests/ledis/store/` | 纯 C++，无网络 |
| command | `tests/ledis/command/` | mock store |
| session | `tests/ledis/session/` | loopback + 内存队列 |
| integration | `tests/ledis/integration/` | 启动 `ledis-server`，`redis-cli` 验证 |

CTest 前缀建议：`Ledis.`（与 `LemoNettyCore.` 一致）。

---

## 12. 风险与对策

| 风险 | 对策 |
|------|------|
| IO/DB 队列延迟 | 可配置队列上限；背压断开慢客户端 |
| 协程栈占用 | `maxclients` + StackPool 配置（`fiber.stackpool`） |
| 兼容范围膨胀 | 文档声明子集；每命令需集成测试才标记 supported |
| 持久化与 lsmtree 键类型 | Phase 4 单独设计 record 编码，不阻塞前期 |
| 多线程误用 store | Code review + store 头文件不暴露给 session |

---

## 13. 架构决策记录（ADR）

| ID | 决策 | 理由 |
|----|------|------|
| ADR-001 | 命令单线程执行 | 对齐 Redis 语义，dict 热路径无锁 |
| ADR-002 | Phase 1 仅 RESP2 | 生态成熟，`redis-cli` 默认行为 |
| ADR-003 | Phase 1 仅 String 类型 | 最小可用 KV |
| ADR-004 | 持久化首选 RDB-lite | 运维工具兼容；lsmtree 作为 Phase 5 选项 |
| ADR-005 | 模块目录顶层 `ledis/` | 与 `lemo/`、`netCore/` 平级，非 `module/` 下 |

---

## 14. 参考

- Redis 设计与实现（单线程、RESP、对象编码）
- [LemoNettyCore README](../../third/LemoNettyCore/README.md)
- [layer_contracts.md](./layer_contracts.md) — 模块 API 契约
- [implementation_order.md](./implementation_order.md) — 实施顺序
