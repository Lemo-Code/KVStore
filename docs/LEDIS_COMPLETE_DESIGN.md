# Ledis 完整设计 — 单机高性能 KV 缓存

> 产品架构 + 软件架构一体化设计，一次到位，避免屎山。

---

## 一、产品功能全集（对标 Redis 7.0 单机子集）

### 1.1 功能分类

| 模块 | 功能 | 优先级 |
|------|------|--------|
| **基础 KV** | String/Hash/List/Set/ZSet 全命令 | P0 |
| **缓存语义** | EXPIRE/TTL + maxmemory + LRU/LFU 淘汰 | P0 |
| **持久化** | AOF (everysec) + 启动重放 + BGREWRITEAOF | P0 |
| **可运维** | CONFIG GET/SET, SLOWLOG, CLIENT LIST, MONITOR, INFO, TIME | P0 |
| **安全** | AUTH (requirepass) | P0 |
| **事务** | MULTI/EXEC/DISCARD/WATCH | P1 |
| **发布订阅** | Pub/Sub + Pattern Subscribe | P1 |
| **阻塞命令** | BLPOP/BRPOP | P1 |
| **迭代器** | SCAN/HSCAN/SSCAN/ZSCAN | P1 |
| **脚本** | Lua (EVAL/EVALSHA) | P2 |
| **模块** | 数据编码优化 (ziplist/intset/listpack) | P2 |

### 1.2 命令全集（约 150 个）

**String (35)**: GET, SET, SETNX, SETEX, PSETEX, GETSET, GETRANGE, SETRANGE,
APPEND, STRLEN, INCR, DECR, INCRBY, DECRBY, INCRBYFLOAT,
MGET, MSET, MSETNX, GETDEL, GETBIT, SETBIT, BITCOUNT, BITOP, BITPOS,
EXPIRE, EXPIREAT, PEXPIRE, PEXPIREAT, TTL, PTTL, PERSIST,
DEL, EXISTS, TYPE, RENAME, RENAMENX

**Hash (14)**: HSET, HSETNX, HGET, HDEL, HEXISTS, HGETALL, HLEN, HKEYS, HVALS,
HMSET, HMGET, HINCRBY, HINCRBYFLOAT, HSCAN

**List (18)**: LPUSH, LPUSHX, RPUSH, RPUSHX, LPOP, RPOP, BLPOP, BRPOP,
LLEN, LRANGE, LINDEX, LSET, LREM, LINSERT, LTRIM,
RPOPLPUSH, LPOS, SORT

**Set (14)**: SADD, SREM, SMEMBERS, SISMEMBER, SCARD,
SRANDMEMBER, SPOP, SMOVE, SMISMEMBER,
SINTER, SUNION, SDIFF, SINTERSTORE, SUNIONSTORE, SDIFFSTORE, SSCAN

**ZSet (20)**: ZADD, ZREM, ZSCORE, ZCARD, ZRANK, ZREVRANK,
ZRANGE, ZREVRANGE, ZRANGEBYSCORE, ZREVRANGEBYSCORE,
ZINCRBY, ZCOUNT, ZREMRANGEBYRANK, ZREMRANGEBYSCORE,
ZPOPMIN, ZPOPMAX, ZSCAN

**Server (20)**: PING, ECHO, COMMAND, INFO, CONFIG GET, CONFIG SET, CONFIG REWRITE,
CLIENT LIST, CLIENT KILL, CLIENT SETNAME, CLIENT GETNAME,
SLOWLOG GET, SLOWLOG RESET, SLOWLOG LEN,
MONITOR, TIME, DBSIZE, FLUSHDB, FLUSHALL, SHUTDOWN, SAVE, BGSAVE, BGREWRITEAOF,
AUTH, SELECT, RANDOMKEY, KEYS, SCAN, MOVE, COPY

**Pub/Sub (6)**: SUBSCRIBE, PSUBSCRIBE, UNSUBSCRIBE, PUNSUBSCRIBE, PUBLISH, PUBSUB

**Transaction (4)**: MULTI, EXEC, DISCARD, WATCH

---

## 二、软件架构

### 2.1 模块分解

```
ledis/
├── CMakeLists.txt
├── ledis.h                         # 聚合头文件
│
├── protocol/                        # RESP 协议层 (零依赖)
│   ├── resp_types.h                # 协议常量
│   ├── resp_parser.h               # 流式解析器 (状态机)
│   └── resp_writer.h               # 响应序列化器
│
├── storage/                         # 存储引擎
│   ├── value.h                     # Value 类型定义 (tagged union, 5种)
│   ├── dict.h                      # 全局哈希表 (渐进式 rehash)
│   ├── expire.h                    # 过期管理 (惰性+主动)
│   ├── eviction.h                  # 淘汰策略 (LRU/LFU + maxmemory)
│   ├── storage_engine.h            # 存储引擎门面 (String 操作)
│   ├── hash_store.h                # Hash 操作
│   ├── list_store.h                # List 操作
│   ├── set_store.h                 # Set 操作
│   └── zset_store.h                # ZSet 操作
│
├── cmd/                             # 命令层
│   ├── cmd_context.h               # 命令执行上下文
│   ├── cmd_table.h                 # 命令表接口 + 标志位
│   └── cmd_table.cc                # 全命令注册表 + dispatch 入口
│
├── server/                          # 服务器运行时
│   ├── ledis_server.h              # 主服务器 (生命周期、调度)
│   ├── client_context.h            # 客户端上下文
│   ├── command_queue.h             # SPSC 命令队列
│   ├── storage_worker.h            # 存储线程 (事件循环)
│   ├── pubsub_manager.h            # Pub/Sub 管理
│   ├── slowlog.h                   # 慢查询日志
│   ├── monitor.h                   # MONITOR 命令实现
│   └── auth.h                      # 密码认证
│
├── config/                          # 运行时配置
│   └── config_manager.h            # CONFIG GET/SET/REWRITE
│
├── persistence/                     # 持久化
│   └── aof_writer.h                # AOF 写 + fsync + 重放
│
└── examples/
    └── ledis_main.cc               # 启动入口
```

### 2.2 模块依赖图

```
                    ┌─────────────┐
                    │  protocol   │ (零依赖)
                    └──────┬──────┘
                           │
         ┌─────────────────┼─────────────────┐
         ▼                 ▼                  ▼
   ┌──────────┐    ┌──────────────┐    ┌──────────┐
   │ storage  │    │    server    │    │  config  │
   │ (value,  │    │ (client,     │    │ (runtime │
   │  dict,   │    │  command_q,  │    │  config) │
   │  stores) │    │  pubsub,     │    └──────────┘
   └────┬─────┘    │  slowlog,    │
        │          │  monitor,    │
        │          │  auth)       │
        │          └──────┬───────┘
        │                 │
        └────────┬────────┘
                 ▼
          ┌────────────┐
          │    cmd     │ (命令表 + dispatch)
          └──────┬─────┘
                 │
                 ▼
          ┌──────────────┐
          │  persistence │ (AOF)
          └──────────────┘
```

### 2.3 线程模型（不变）

```
N × IO Threads (zero::Scheduler)          1 × Storage Thread (std::thread)
┌──────────────────────────┐            ┌──────────────────────────────┐
│ accept → read → parse    │            │ drain queues → execute       │
│ push → yield             │──SPSC Q───►│ AOF append → notify IO      │
│ (resume) ← write         │◄─eventfd──│ slowlog → eviction           │
│ pubsub loop              │            │ monitor → expire             │
└──────────────────────────┘            └──────────────────────────────┘
```

---

## 三、关键模块详细设计

### 3.1 ConfigManager — 运行时配置

```cpp
// config/config_manager.h
class ConfigManager {
public:
    // 所有可配置项
    struct Config {
        int    port           = 6379;
        string bind           = "0.0.0.0";
        int    io_threads     = 3;
        int    max_clients    = 10000;
        int    timeout        = 0;       // 客户端空闲超时(秒)
        string requirepass    = "";      // 空=无密码
        size_t maxmemory      = 0;       // 0=不限制
        string maxmemory_policy = "noeviction";
        string aof_path       = "";
        string aof_fsync      = "everysec";
        int    slowlog_slower_than = 10000; // 微秒
        int    slowlog_max_len = 128;
        int    databases       = 16;
        string log_level      = "info";
    };

    Config current;

    // CONFIG GET pattern → 返回匹配项
    vector<pair<string, string>> get(const string& pattern);

    // CONFIG SET key value → 运行时修改
    bool set(const string& key, const string& value);

    // CONFIG REWRITE → 写回配置文件
    bool rewrite(const string& path);
};
```

### 3.2 Slowlog — 慢查询日志

```cpp
// server/slowlog.h
struct SlowlogEntry {
    uint64_t id;
    uint64_t timestamp;      // unix秒
    int64_t  duration_us;    // 执行微秒
    vector<string> args;     // 命令参数
    string   client_addr;
};

class Slowlog {
    static constexpr int MAX_LEN = 128;
    deque<SlowlogEntry> entries_;
    uint64_t next_id_ = 0;
    int64_t slower_than_us_ = 10000;  // CONFIG 可改

    void record(int64_t duration_us, const vector<string_view>& args,
                const string& client_addr);
    vector<SlowlogEntry> get(int count);
    void reset();
    int len() const;
};
```

### 3.3 Auth — 密码认证

```cpp
// server/auth.h
class AuthManager {
    string password_;  // 空=不认证

    bool enabled() const;
    bool check(const string& pwd) const;
    void setPassword(const string& pwd);
};
```

认证时机：在 `handleClient` 中，连接建立后第一个命令必须是 AUTH（如果设置了密码）。如果不是，拒绝所有其他命令。认证成功后正常处理。

### 3.4 Monitor — 实时命令监视

```cpp
// server/monitor.h
class MonitorManager {
    set<ClientContext*> monitors_;

    void add(ClientContext* c);
    void remove(ClientContext* c);
    void broadcast(const string& cmd_str, const string& client_addr);
};
```

存储线程执行命令后调用 `broadcast()`，向所有 MONITOR 客户端推送。

### 3.5 Eviction — 内存淘汰

```cpp
// storage/eviction.h
enum EvictPolicy {
    NOEVICTION, ALLKEYS_LRU, VOLATILE_LRU,
    ALLKEYS_RANDOM, VOLATILE_RANDOM,
    ALLKEYS_LFU, VOLATILE_LFU,
};

class EvictionManager {
    Dict& dict_;
    EvictPolicy policy_;
    size_t maxmemory_;
    size_t used_memory_;  // 估算, 定期更新

    // 每 key 24bit LRU 时钟 (存储在 Value.lru_clock)
    uint32_t lru_clock_;  // 每 100ms 递增

    // 候选淘汰池 (idle 最大的 key)
    struct Candidate { string key; int64_t idle; };
    Candidate pool_[16];  // Redis 默认 EVPOOL_SIZE=16

    // 是否需要淘汰
    bool needsEvict();

    // 执行淘汰 (释放内存直到低于 maxmemory)
    void evictIfNeeded();

    // 采样 + 选择淘汰目标
    string selectVictim();

    // 估算 key 占用内存
    size_t estimateSize(const string& key, const Value& val);
};
```

`estimateSize`: key.size() + val.str.size() + sizeof(Entry) + 指针开销 + 复杂类型递归。采样估算而非精确跟踪（Redis 做法）。

### 3.6 SCAN — 游标迭代

```cpp
// 在 dict.h 中添加
struct ScanState {
    int64_t cursor;     // 当前桶索引
    int table;          // 0 或 1 (rehash 时)
    bool done;
};

ScanState dictScan(Dict& dict, int64_t cursor, int count,
                   vector<string>& result, const string& pattern);
```

高位递增法：cursor = (cursor + 1) & mask，然后反向 bit 排列防重复。

### 3.7 阻塞命令 — BLPOP/BRPOP

阻塞列表命令需要 IO fiber 能同时等待 socket 和 eventfd。
利用已有的 pubsub loop 机制：客户端进入 "blocking wait" 状态。

```cpp
// 在 ClientContext 中添加
struct BlockingState {
    bool active = false;
    vector<string> keys;   // 等待的 key
    int64_t timeout_ms;    // 超时
};

// 在 StorageWorker 中添加
void checkBlockingClients();  // 周期性检查
```

流程：
1. BLPOP key1 key2 timeout → 检查所有 key，有数据立即返回
2. 无数据 → 设置 client.blocking_state → fiber 进入 pubsub式等待
3. 当任何客户端 PUSH 数据到这些 key → 唤醒等待 fiber
4. 超时 → 返回 nil

### 3.8 Glob 模式匹配

```cpp
// util/glob.h
bool globMatch(const char* pattern, const char* str);
```

支持 `*`, `?`, `[abc]`, `[a-z]`, `\x` 转义。用于 KEYS、SCAN、PSUBSCRIBE。

---

## 四、命令组织方式

为避免 cmd_table.cc 过大（150 个命令），按数据类型拆分实现文件：

```
cmd/
├── cmd_context.h       # CmdContext 结构体
├── cmd_table.h         # CmdInfo + 标志位 + lookup/dispatch 声明
├── cmd_table.cc        # 命令注册表 + lookup + dispatch (300行)
│
├── cmd_string.cc       # String 命令实现 (35个)
├── cmd_hash.cc         # Hash 命令实现 (14个)
├── cmd_list.cc         # List 命令实现 (18个)
├── cmd_set.cc          # Set 命令实现 (14个)
├── cmd_zset.cc         # ZSet 命令实现 (17个)
├── cmd_server.cc       # 服务器命令实现 (20个)
├── cmd_pubsub.cc       # Pub/Sub 命令 (6个)
└── cmd_tx.cc           # 事务命令 (4个)
```

每个 `.cc` 文件：
1. 定义 static handler 函数
2. 函数直接操作 storage engine / pubsub / monitor / slowlog
3. 通过 CmdContext 获取所有依赖

命令表在 `cmd_table.cc` 中用 `extern` 声明各 handler。

---

## 五、实施计划

| 阶段 | 内容 | 新增/修改文件 | 行数估算 |
|------|------|-------------|---------|
| **A: 基础设施** | ConfigManager, Slowlog, Auth, Monitor, Glob | 5 新 | ~500 |
| **B: 存储增强** | Eviction, Expire完善, Dict修复 | 2 新 + 修改 | ~400 |
| **C: 全命令** | 所有缺失命令实现 | 8 新 + 修改 cmd_table | ~1500 |
| **D: SCAN+阻塞** | SCAN 系列, BLPOP/BRPOP | 修改 dict + 2 新 | ~300 |
| **总计** | | ~18 文件 | ~2700 |

---

## 六、架构终裁

| # | 决策 | 裁定 |
|---|------|------|
| 1 | 命令实现文件拆分 | 按数据类型分 8 个 .cc 文件 |
| 2 | 内存淘汰估算 | Redis 风格: 采样+估算, 不精确跟踪 |
| 3 | 阻塞命令 | 复用 pubsub loop 机制, 加 blocking_state |
| 4 | SCAN 算法 | 高位递增法, 防重复 |
| 5 | 编码优化 | Phase 2 (ziplist/intset 暂不做) |
| 6 | Lua | Phase 2 (集成 lua 5.1) |
| 7 | WATCH | Phase 2 (需 MVCC 或 CAS) |
| 8 | 持久化线程 | 独立 fsync 线程, 双缓冲, 已实现 |
| 9 | 存储线程 | 单线程串行, 无锁 Dict, 已实现 |
| 10 | 配置持久化 | CONFIG REWRITE 写 YAML |
