# Ledis — 高性能 KV 缓存服务器 架构设计

> 仿 Redis 协议，基于 Zero 网络库，目标 100K+ QPS per core

---

## 一、核心理念

### 1.1 设计法则

| 原则 | 说明 |
|------|------|
| **主线程存储，IO 线程解析** | 存储引擎单线程无锁；协议解析多线程并行 |
| **Lock-free fast path** | IO→Storage 用 MPSC 无锁队列；响应写回用原子操作 |
| **Zero-copy everywhere** | parse 阶段零拷贝引用；Buffer 链式块 + iovec 批量 flush |
| **对象池化** | Command 对象池、RESP 响应池、Client 上下文池 |
| **RESP 全兼容** | redis-cli 直连，支持 pipeline、事务(MULTI/EXEC)、Pub/Sub |
| **存储按需演化** | 小 value → 嵌入；大 value → 外部分配；冷数据 → 压缩 |

### 1.2 线程模型

```
                    ┌───────────────┐
                    │  Main Thread  │  (accept + dispatch)
                    └───────┬───────┘
                            │ SO_REUSEPORT round-robin
            ┌───────────────┼───────────────┐
            ▼               ▼               ▼
     ┌────────────┐  ┌────────────┐  ┌────────────┐
     │ IO Thread 0│  │ IO Thread 1│  │ IO Thread 2│  ... N
     │  (epoll)   │  │  (epoll)   │  │  (epoll)   │
     │            │  │            │  │            │
     │ RESP Parse │  │ RESP Parse │  │ RESP Parse │
     │ Resp Write │  │ Resp Write │  │ Resp Write │
     └─────┬──────┘  └─────┬──────┘  └─────┬──────┘
           │               │               │
           │  Command*     │  Command*     │  Command*
           │  (MPSC queue) │  (MPSC queue) │  (MPSC queue)
           │               │               │
           └───────────────┼───────────────┘
                           ▼
     ┌─────────────────────────────────────────────┐
     │              Storage Thread                  │
     │                                              │
     │  ┌──────────┐ ┌────────┐ ┌──────────────┐   │
     │  │   Dict   │ │Expire  │ │  AOF Writer  │   │
     │  │ (全局KV) │ │Manager │ │  (子线程)    │   │
     │  └──────────┘ └────────┘ └──────────────┘   │
     │                                              │
     │  Event Loop:                                 │
     │    batch = drain_all_queues()                │
     │    for cmd in batch:                         │
     │      result = execute(cmd)                   │
     │      cmd->set_response(result)               │
     │      cmd->notify_io_thread()                 │
     │    check_expire() / periodic_tasks()         │
     └──────────────────────────────────────────────┘
```

**为什么不是完全单线程（像 Redis 6.0 之前）？**

解析 RESP 协议是 CPU-bound 操作（字符串扫描、数字解析）。
10K QPS 下，解析消耗约 15-20% CPU。当 QPS 到达 100K 时，
单线程解析成为瓶颈。多线程解析 + 单线程存储是最优解：

| 阶段 | 线程 | 原因 |
|------|------|------|
| `read()` 网络 IO | IO 线程 | zero hook 自动 fiber yield，不占 CPU |
| RESP 解析 | IO 线程 | **CPU-bound，可并行** |
| 存储执行 | 存储线程 | **必须串行**，保证一致性 |
| RESP 写回 | IO 线程 | 仅仅是 memcpy + writev |

---

## 二、整体分层架构

```
┌──────────────────────────────────────────────────────────────────┐
│                      Application Layer                            │
│                 redis-cli / redis-benchmark / SDK                  │
├──────────────────────────────────────────────────────────────────┤
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                  RESP Protocol Layer                        │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │  │
│  │  │ RespParser   │  │ RespWriter   │  │ WireFormat   │     │  │
│  │  │ (状态机,零拷贝)│  │ (预分配buf) │  │ (RESP2/RESP3)│     │  │
│  │  └──────────────┘  └──────────────┘  └──────────────┘     │  │
│  └────────────────────────────────────────────────────────────┘  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                  Command Dispatch Layer                     │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │  │
│  │  │ CmdTable     │  │ CmdContext   │  │ CmdExecutor  │     │  │
│  │  │ (完美哈希)   │  │ (per-request)│  │ (类型分发)   │     │  │
│  │  └──────────────┘  └──────────────┘  └──────────────┘     │  │
│  └────────────────────────────────────────────────────────────┘  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                    Storage Engine                           │  │
│  │  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐          │  │
│  │  │ Global │  │ String │  │ Hash   │  │ List   │          │  │
│  │  │ Dict   │  │ Store  │  │ Store  │  │ Store  │          │  │
│  │  ├────────┤  ├────────┤  ├────────┤  ├────────┤          │  │
│  │  │ Set    │  │ ZSet   │  │ Expire │  │ PubSub │          │  │
│  │  │ Store  │  │ Store  │  │ Mgr    │  │ Mgr    │          │  │
│  │  └────────┘  └────────┘  └────────┘  └────────┘          │  │
│  └────────────────────────────────────────────────────────────┘  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                    Persistence Layer                        │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │  │
│  │  │ AOF Writer   │  │ RDB Snapshot │  │ Checkpoint   │     │  │
│  │  │ (append-only)│  │ (fork+COW)   │  │ Manager      │     │  │
│  │  └──────────────┘  └──────────────┘  └──────────────┘     │  │
│  └────────────────────────────────────────────────────────────┘  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                    Eviction Layer                           │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │  │
│  │  │ LRU Clock    │  │ LFU Counter  │  │ Sampler      │     │  │
│  │  │ (近似LRU)    │  │ (对数计数)   │  │ (随机采样)   │     │  │
│  │  └──────────────┘  └──────────────┘  └──────────────┘     │  │
│  └────────────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────────────┤
│               Zero Network Library (Fiber + epoll)                 │
└──────────────────────────────────────────────────────────────────┘
```

---

## 三、目录结构

```
ledis/
├── CMakeLists.txt
├── ledis.h                         # 聚合头文件
│
├── protocol/                        # RESP 协议层
│   ├── resp_parser.h               # 请求解析器 (状态机)
│   ├── resp_writer.h               # 回复序列化器
│   └── resp_types.h                # RESP 类型枚举 + 常量
│
├── cmd/                             # 命令层
│   ├── cmd_table.h                  # 命令注册表 (编译期完美哈希)
│   ├── cmd_context.h                # 命令执行上下文
│   ├── cmd_dispatcher.h             # 命令分发器
│   ├── cmd_string.h/.cc             # GET/SET/DEL/INCR/APPEND...
│   ├── cmd_hash.h/.cc               # HGET/HSET/HDEL/HGETALL/HLEN...
│   ├── cmd_list.h/.cc               # LPUSH/RPOP/LRANGE/LLEN...
│   ├── cmd_set.h/.cc                # SADD/SMEMBERS/SINTER/SUNION...
│   ├── cmd_zset.h/.cc               # ZADD/ZRANGE/ZRANK/ZSCORE...
│   ├── cmd_keys.h/.cc               # KEYS/EXISTS/TYPE/EXPIRE/TTL...
│   ├── cmd_server.h/.cc             # INFO/PING/DBSIZE/FLUSHDB/CLIENT...
│   └── cmd_pubsub.h/.cc             # PUBLISH/SUBSCRIBE/PSUBSCRIBE...
│
├── storage/                         # 存储引擎
│   ├── storage_engine.h             # 统一存储入口
│   ├── dict.h/.cc                   # 全局字典 (渐进式 rehash)
│   ├── value.h                      # 值类型定义 (tagged union)
│   ├── string_store.h               # String 操作
│   ├── hash_store.h                 # Hash 操作 (ziplist→hashtable 阈值切换)
│   ├── list_store.h                 # List 操作 (quicklist)
│   ├── set_store.h                  # Set 操作 (intset→hashtable)
│   ├── zset_store.h                 # ZSet 操作 (skiplist+dict)
│   ├── expire_manager.h/.cc         # 过期管理 (惰性+主动)
│   └── eviction.h/.cc               # 淘汰策略 (LRU/LFU)
│
├── persistence/                     # 持久化
│   ├── aof_writer.h/.cc             # AOF 追加写
│   └── rdb_snapshot.h/.cc           # RDB 快照
│
├── server/                          # 服务器层
│   ├── ledis_server.h/.cc           # 主服务器类 (生命周期管理)
│   ├── io_worker.h/.cc              # IO 工作线程
│   ├── storage_worker.h/.cc         # 存储工作线程
│   ├── client_context.h             # 客户端上下文 (per-connection)
│   └── command_queue.h              # MPSC 命令队列
│
└── examples/
    ├── ledis_main.cc                # 启动入口
    └── ledis.conf                   # 默认配置
```

---

## 四、各模块详细设计

### 4.1 RESP 协议层 — 零拷贝解析

RESP 协议有 5 种类型：

```
Simple String:  +OK\r\n
Error:          -ERR unknown command\r\n
Integer:        :1000\r\n
Bulk String:    $5\r\nhello\r\n
Array:          *2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n
Null:           $-1\r\n
```

**RespParser 设计 — 流式状态机**：

```cpp
// protocol/resp_parser.h
class RespParser {
public:
    enum State : uint8_t {
        TOP_LEVEL,          // 期待下一个类型的首字节
        READING_BULK_LEN,   // 读 $<len>\r\n
        READING_BULK_DATA,  // 读 <len> bytes + \r\n
        READING_ARRAY_LEN,  // 读 *<len>\r\n
    };

    enum class Result : uint8_t {
        OK,           // 解析完成一个完整命令
        NEED_MORE,    // 数据不足，需要更多字节
        PROTO_ERR,    // 协议错误
    };

    // 喂入数据，返回解析结果
    // 零拷贝: args_ 中的 Bulk String 指向原始 buffer 的 string_view
    Result feed(const char* data, size_t len, size_t& consumed);

    // 获取解析完成的命令参数 (仅在 OK 后有效)
    const std::vector<std::string_view>& args() const { return args_; }

    // 重置解析器，复用对象
    void reset();

private:
    State state_ = TOP_LEVEL;
    int   array_depth_ = 0;       // 嵌套数组深度
    std::vector<int> array_counts_; // 每层数组剩余元素数

    size_t bulk_len_ = 0;          // 当前 bulk string 剩余长度
    std::vector<std::string_view> args_; // 零拷贝参数视图
};
```

**设计要点**：
- `args_` 中的值使用 `std::string_view` 指向原始网络 buffer，**不拷贝字符串**
- `feed()` 返回 `consumed` 告知调用方可释放/移动的字节数
- 支持 pipeline：一次 `feed()` 可解析出多个命令（用循环）

**RespWriter 设计 — 预分配缓冲区**：

```cpp
// protocol/resp_writer.h
class RespWriter {
public:
    // 写入 RESP 回复到 ByteBuffer
    static void writeOK(ByteBuffer& buf);
    static void writeError(ByteBuffer& buf, const std::string& msg);
    static void writeInteger(ByteBuffer& buf, int64_t v);
    static void writeBulkString(ByteBuffer& buf, std::string_view s);
    static void writeNull(ByteBuffer& buf);
    static void writeArray(ByteBuffer& buf, int64_t len);  // 头，后面逐个写元素

    // 批量写: 一次调用写完整数组回复
    static void writeStringArray(ByteBuffer& buf,
                                  const std::vector<std::string>& arr);
};
```

### 4.2 命令系统 — 完美哈希表驱动

**命令注册表** — 使用编译期生成的完美哈希（gperf 或手工最小完美哈希）：

```cpp
// cmd/cmd_table.h

// 命令标志位
enum CmdFlag : uint32_t {
    CMD_READONLY    = 1 << 0,   // 只读命令 → 未来可由 IO 线程直接读
    CMD_WRITE       = 1 << 1,   // 写命令
    CMD_FAST        = 1 << 2,   // O(1) 命令，不需要慢速路径
    CMD_DENYOOM     = 1 << 3,   // 内存满时拒绝
    CMD_NOSCRIPT    = 1 << 4,   // 不可用于脚本
    CMD_PUBSUB      = 1 << 5,   // Pub/Sub 相关
};

// 命令处理函数签名
using CmdHandler = void (*)(CmdContext& ctx);

struct CmdInfo {
    const char* name;           // "GET", "SET" ...
    CmdHandler  handler;        // 处理函数指针
    int         arity;          // -2 = 至少2个参数, 2 = 恰好2个参数
    CmdFlag     flags;
    const char* sflags;         // "rF" ... (兼容 Redis 的字符串标志)
    uint64_t    calls = 0;      // 调用计数
    uint64_t    usecs = 0;      // 累计微秒
};

// cmd/cmd_context.h
struct CmdContext {
    // 输入
    ClientContext* client;          // 发起请求的客户端
    std::vector<std::string_view>& args; // 命令参数 (零拷贝引用)

    // 输出
    ByteBuffer* response_buf;       // 写入 RESP 回复的目标 buffer

    // 快捷方法
    void replyOK()           { RespWriter::writeOK(*response_buf); }
    void replyNull()         { RespWriter::writeNull(*response_buf); }
    void replyInteger(int64_t v) { RespWriter::writeInteger(*response_buf, v); }
    void replyBulk(std::string_view s) { RespWriter::writeBulkString(*response_buf, s); }
    void replyError(const std::string& msg) { RespWriter::writeError(*response_buf, msg); }
    void replyStringArray(const std::vector<std::string>& arr) {
        RespWriter::writeStringArray(*response_buf, arr);
    }
};
```

**命令注册宏**：

```cpp
// 在 cmd_table.cc 中注册所有命令
#define REGISTER_CMD(name, handler, arity, flags) \
    { name, handler, arity, flags, #flags }

static const CmdInfo cmd_table[] = {
    // String
    REGISTER_CMD("GET",     cmd_string_get,      2, CMD_READONLY | CMD_FAST),
    REGISTER_CMD("SET",     cmd_string_set,     -3, CMD_WRITE),
    REGISTER_CMD("DEL",     cmd_string_del,     -2, CMD_WRITE),
    REGISTER_CMD("INCR",    cmd_string_incr,     2, CMD_WRITE | CMD_FAST),
    REGISTER_CMD("APPEND",  cmd_string_append,   3, CMD_WRITE),
    REGISTER_CMD("MGET",    cmd_string_mget,    -2, CMD_READONLY),
    REGISTER_CMD("MSET",    cmd_string_mset,    -3, CMD_WRITE),
    // ... 更多命令
};
```

### 4.3 存储引擎 — 全局 Dict + 类型化 Store

**Value 类型 — Tagged Union**：

```cpp
// storage/value.h
enum ValueType : uint8_t {
    VAL_STRING = 0,
    VAL_LIST   = 1,
    VAL_HASH   = 2,
    VAL_SET    = 3,
    VAL_ZSET   = 4,
};

struct Value {
    ValueType type;

    // 基础数据
    // - STRING: str 即值
    // - LIST:   list_ptr 指向 quicklist
    // - HASH:   hash_ptr 指向 dict / ziplist
    // - SET:    set_ptr 指向 intset / dict
    // - ZSET:   zset_ptr 指向 skiplist+dict 对
    union {
        std::string str;                     // STRING 类型
        void*       opaque_ptr;             // 复杂类型指针
        int64_t     int_val;               // 小整数嵌入
    };

    // 元数据
    uint64_t expire_at_ms = 0;   // 过期时间 (0 = 永不过期)
    uint32_t lru_clock : 24;     // LRU 时钟 (秒级精度，8M秒≈97天)
    uint8_t  lfu_count : 8;      // LFU 计数器 (对数)
    uint8_t  encoding : 4;       // 内部编码 (raw/embstr/int/ziplist/...)

    // 构造函数
    static Value createString(std::string s);
    static Value createList();
    static Value createHash();
    static Value createSet();
    static Value createZSet();
    static Value createInt(int64_t v);  // 小整数嵌入

    // 释放复杂类型
    void destroy();

    ~Value() { destroy(); }
    Value(Value&& other) noexcept;
    Value& operator=(Value&& other) noexcept;

    // 禁止拷贝
    Value(const Value&) = delete;
    Value& operator=(const Value&) = delete;
};
```

**全局字典 — 渐进式 Rehash**：

借鉴 Redis dict 的设计：

```cpp
// storage/dict.h
class Dict {
public:
    struct Entry {
        std::string key;
        Value       value;
        Entry*      next = nullptr;  // 链表解决哈希冲突
    };

    Dict();
    ~Dict();

    // 基础操作
    Value*      find(const std::string& key);
    bool        insert(std::string key, Value value);
    bool        remove(const std::string& key);
    size_t      size() const { return used_; }

    // 渐进式 rehash
    // 每次操作触发 N 步 rehash (摊销 O(1))
    void rehashStep(int n = 1);

    // 随机采样 (用于过期/淘汰)
    Entry* randomEntry();

    // 迭代器 (用于 KEYS / SCAN)
    class Iterator { /* ... */ };

private:
    Entry** table_[2] = {nullptr, nullptr};  // 双表
    size_t  size_mask_[2] = {0, 0};          // 大小掩码 (size-1, size为2的幂)
    int64_t rehash_idx_ = -1;                // -1 = 未在 rehash
    size_t  used_ = 0;

    uint64_t hash(const std::string& key) const;
    void     expandIfNeeded();
    void     shrinkIfNeeded();
};
```

**为什么用渐进式 Rehash 而不是直接 resize？**

一次性 rehash 在百万级 key 时会卡住存储线程数十毫秒，
导致所有连接超时。渐进式 rehash 把操作分散到每次访问中，
每次只搬几个桶，对延迟影响在微秒级。

**各类型 Store 的设计**：

```cpp
// String Store
class StringStore {
    // GET/SET 直接走 Value.str
    // INCR/DECR: 如果 Value 是 int 嵌入则直接加减
    // APPEND: 字符串拼接
    // 小字符串用 embstr 编码 (≤44字节嵌入 Value 旁边)
};

// Hash Store — ziplist → hashtable 自适应
class HashStore {
    // 元素数 < 512 且 value < 64B → ziplist (紧凑连续内存)
    // 超过阈值 → lstl::unordered_map
    // HGET/HSET/HGETALL/HDEL/HLEN/HEXISTS/HINCRBY
};

// List Store — quicklist
class ListStore {
    // quicklist = ziplist 组成的双向链表
    // 每个节点是一个 ziplist (减少指针开销 + 内存碎片)
    // LPUSH/RPUSH/LPOP/RPOP/LLEN/LRANGE/LTRIM
};

// Set Store — intset → hashtable
class SetStore {
    // 全整数 → intset (有序数组，二分查找)
    // 有字符串 → lstl::unordered_set
    // SADD/SREM/SMEMBERS/SISMEMBER/SCARD/SINTER/SUNION
};

// ZSet Store — skiplist + dict
class ZSetStore {
    // skiplist: 按 score 排序，支持 ZRANGE
    // dict: member → score，支持 ZSCORE O(1)
    // ZADD/ZREM/ZSCORE/ZRANK/ZRANGE/ZRANGEBYSCORE/ZCOUNT
    // 使用 lstl::skip_list
};
```

### 4.4 过期管理 — 惰性 + 主动

```cpp
// storage/expire_manager.h
class ExpireManager {
public:
    explicit ExpireManager(Dict& dict);

    // 惰性删除: 每次访问 key 时检查
    // 返回 true 表示 key 已过期
    bool checkExpired(const std::string& key);

    // 主动过期: 周期性调用 (每 100ms)
    // 随机采样 → 删除过期 → 若 >25% 过期则继续
    void activeExpireCycle();

    // 设置过期时间
    void setExpire(const std::string& key, uint64_t abs_ms);

    // 获取 TTL
    int64_t getTTL(const std::string& key);  // -1=永久, -2=不存在, >0=剩余秒

private:
    Dict& dict_;

    // 主动过期参数
    static constexpr int  MAX_SAMPLES  = 20;       // 每次采样数
    static constexpr int  MAX_LOOPS    = 16;       // 最大循环次数
    static constexpr long MAX_TIME_US  = 25000;     // 最多占 25ms CPU
    static constexpr int  EXPIRE_RATIO = 25;        // 过期>25% 就继续循环
};
```

### 4.5 淘汰策略

```cpp
// storage/eviction.h
enum EvictionPolicy {
    NOEVICTION,         // 不淘汰，写满报错
    ALLKEYS_LRU,        // 所有 key 参与 LRU
    VOLATILE_LRU,       // 仅有过期时间的 key
    ALLKEYS_RANDOM,
    VOLATILE_RANDOM,
    ALLKEYS_LFU,
    VOLATILE_LFU,
};

class EvictionManager {
public:
    EvictionManager(Dict& dict, EvictionPolicy policy, size_t max_memory);

    // 需要淘汰时调用，尝试释放空间
    // 返回 true 表示释放了一些空间
    bool evictIfNeeded();

    // 更新 key 的 LRU/LFU 信息
    void touchEntry(Dict::Entry* entry);

private:
    Dict&           dict_;
    EvictionPolicy  policy_;
    size_t          max_memory_;
    uint32_t        lru_clock_;     // 全局 LRU 时钟 (每 100ms 递增)

    // 候选 key 池 (类似 Redis 的 eviction pool)
    // 存放最近采样到的候选淘汰 key
    struct EvictionCandidate {
        std::string key;
        uint64_t    idle_or_freq;  // LRU: 空闲秒数, LFU: 频率计数
    };
    std::vector<EvictionCandidate> pool_;
};
```

### 4.6 持久化 — AOF + RDB

**AOF Writer**：

```cpp
// persistence/aof_writer.h
class AofWriter {
public:
    AofWriter(const std::string& path);

    // 追加命令 (存储线程调用，极快)
    void append(const std::string& resp_cmd);

    // 后台 fsync 线程
    void startFsyncThread();
    void stopFsyncThread();

    // AOF 重写 (BGREWRITEAOF)
    void rewriteAof(Dict& dict);

    // 配置
    enum FsyncMode {
        FSYNC_ALWAYS,    // 每条 fsync
        FSYNC_EVERYSEC,  // 每秒 fsync (推荐)
        FSYNC_NO,        // 操作系统控制
    };

private:
    int fd_;
    std::string path_;
    FsyncMode fsync_mode_ = FSYNC_EVERYSEC;

    // 后台 fsync
    std::thread fsync_thread_;
    std::atomic<bool> running_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::string> pending_buffers_;
    std::vector<std::string> flush_buffers_;  // double buffer
};
```

### 4.7 线程间通信 — MPSC 命令队列

这是整个架构的核心数据通道。设计一个**批处理友好的 MPSC 队列**：

```cpp
// server/command_queue.h

struct PendingCommand {
    ClientContext* client;              // 归属客户端
    std::vector<std::string_view> args; // 命令参数 (零拷贝视图)
    ByteBuffer*     response_buf;       // 回复写入目标
};

// MPSC 队列: 多个 IO 线程 → 单个存储线程
// 基于链表，无锁 push，单消费端 pop 无需 CAS
class CommandQueue {
public:
    CommandQueue();

    // IO 线程调用 (多生产者，lock-free)
    void push(PendingCommand cmd);

    // 存储线程调用 (单消费者，无需同步)
    // 返回一批命令，减少函数调用开销
    std::vector<PendingCommand> drain();

    // 非阻塞检查
    bool empty() const;

private:
    struct Node {
        PendingCommand cmd;
        std::atomic<Node*> next{nullptr};
    };

    alignas(64) std::atomic<Node*> head_;  // 消费者端
    alignas(64) std::atomic<Node*> tail_;  // 生产者端
    Node* stub_;                           // 哨兵节点

    // 对象池，避免频繁 new/delete
    NodePool node_pool_;
};
```

### 4.8 客户端上下文

```cpp
// server/client_context.h
struct ClientContext {
    Socket::ptr  sock;
    SocketStream stream;
    RespParser   parser;
    RespWriter   writer;

    // 双缓冲区
    ByteBuffer   read_buf;     // 网络读缓冲
    ByteBuffer   write_buf;    // 回复写缓冲 (存储线程写入 → IO 线程 flush)

    // 当前正在解析的命令
    std::vector<std::string_view> args;

    // 所属 IO 线程
    int io_thread_id;

    // 事务状态
    bool in_multi = false;
    std::vector<PendingCommand> multi_queue;

    // Pub/Sub 订阅集合
    std::unordered_set<std::string> channels;
    std::unordered_set<std::string> patterns;

    // 统计
    uint64_t total_commands = 0;
    time_t   connected_at;
};
```

### 4.9 IO Worker — 每线程事件循环

```cpp
// server/io_worker.h
class IoWorker {
public:
    IoWorker(int id, CommandQueue& cmd_queue, StorageWorker& storage);

    void run();
    void stop();

private:
    int id_;
    CommandQueue& cmd_queue_;
    StorageWorker& storage_;

    // 接受连接
    void onAccept(Socket::ptr sock);

    // 处理一个客户端的可读事件 (在 fiber 中)
    void handleClient(ClientContext* client);

    // 刷新写缓冲 (可写事件或批量)
    void flushWriteBuf(ClientContext* client);
};
```

**IO Worker 的处理流程**：

```
1. TcpServer accept → 创建 ClientContext → 注册到当前 reactor
2. 为每个 ClientContext 创建一个 fiber
3. Fiber 循环:
   a. stream.read() → read_buf  (hook 自动 yield on EAGAIN)
   b. parser.feed(read_buf) → 解析命令
   c. 如果 NEED_MORE → 回到步骤 a
   d. 如果 OK:
      - 构造 PendingCommand{client, args_view, &write_buf}
      - cmd_queue.push(cmd)
      - 通知存储线程 (eventfd)
      - fiber 进入等待 → 等待存储线程完成
   e. 收到回复通知 → 检查 write_buf
   f. stream.writeFixed(write_buf) → flush 回复
   g. 回到步骤 a
```

**关键点**：IO 线程的 fiber 在等待存储线程回复时应该 **yield**，不占着线程不放。zero 的 hook 机制天然支持这个模式——或者用 `FiberMutex` / `WaitGroup` 实现同步等待。

### 4.10 Storage Worker — 事件循环

```cpp
// server/storage_worker.h
class StorageWorker {
public:
    StorageWorker();

    void run();
    void stop();

    // IO 线程通知有新命令
    void notify();

    // 注册 IO worker 的命令队列
    void registerQueue(CommandQueue* queue);

private:
    void processBatch(const std::vector<PendingCommand>& batch);
    void periodicTasks();

    StorageEngine   engine_;
    ExpireManager   expire_mgr_;
    EvictionManager eviction_mgr_;
    AofWriter       aof_;
    PubSubManager   pubsub_;

    // 所有 IO worker 的队列
    std::vector<CommandQueue*> io_queues_;

    int event_fd_;  // eventfd 用于唤醒

    // 统计
    uint64_t total_processed_ = 0;
    uint64_t total_usecs_ = 0;
};

void StorageWorker::run() {
    while (!stopping_) {
        // 1. 从所有 IO 队列收集命令 (round-robin, 批处理)
        std::vector<PendingCommand> batch;
        for (auto* q : io_queues_) {
            auto cmds = q->drain();
            batch.insert(batch.end(),
                         std::make_move_iterator(cmds.begin()),
                         std::make_move_iterator(cmds.end()));
        }

        // 2. 如果没命令，epoll_wait event_fd (等待通知)
        if (batch.empty()) {
            epoll_wait(epoll_fd_, &ev, 1, getNextTimeout());
            continue;
        }

        // 3. 批量处理
        processBatch(batch);

        // 4. 周期性任务
        periodicTasks();
    }
}

void StorageWorker::processBatch(const std::vector<PendingCommand>& batch) {
    for (const auto& cmd : batch) {
        // 查找命令表
        auto* info = CmdDispatcher::lookup(cmd.args[0]);
        if (!info) {
            RespWriter::writeError(*cmd.response_buf, "ERR unknown command");
            cmd.client->notify();  // 通知 IO fiber
            continue;
        }

        // 执行
        CmdContext ctx{cmd.client, cmd.args, cmd.response_buf};
        auto start = now_us();
        info->handler(ctx);
        info->usecs += (now_us() - start);
        info->calls++;

        // 通知 IO fiber: 回复已写入 buffer
        cmd.client->notify();
    }
}
```

---

## 五、关键数据流

### 5.1 SET key value 的完整路径

```
Client:  *3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n
    │
    ▼
[IO Thread N]
  1. Fiber: stream.read() → read_buf
  2. RespParser.feed(read_buf) → OK, args = ["SET","foo","bar"]
  3. 构造 PendingCommand{client, args, write_buf}
  4. cmd_queue[N].push(cmd)
  5. write(event_fd, &wake, 8)  // 唤醒存储线程
  6. Fiber: wait on fiber_mutex (挂起)
    │
    ▼  (MPSC queue → Storage Thread)
    │
    ▼
[Storage Thread]
  7. epoll_wait 返回 event_fd 可读
  8. batch = drain_all_queues()
  9. info = lookup("SET") → cmd_string_set
  10. cmd_string_set(ctx):
      a. Value v = Value::createString("bar")
      b. expire_mgr_.checkExpired("foo")  // 惰性删除旧值
      c. dict_.insert("foo", std::move(v))
      d. aof_.append("*3\r\n$3\r\nSET\r\n...")  // 追加 AOF
      e. ctx.replyOK()
  11. client->notify() → unlock fiber_mutex / eventfd
    │
    ▼  (通知回到 IO Thread)
    │
    ▼
[IO Thread N]
  12. Fiber 被唤醒
  13. stream.writeFixed(write_buf)  → "+OK\r\n" 发送到客户端
  14. 回到步骤 1，继续读下一个请求
```

### 5.2 Pipeline 优化

Redis 客户端常使用 pipeline 批量发送命令：

```
Client → *3\r\n$3\r\nSET\r\n...*2\r\n$3\r\nGET\r\n...
                ↑ 命令1                    ↑ 命令2 (同一 TCP 段)
```

IO 线程中 RespParser 在一次 `feed()` 中可能解析出多个命令。
批量提交到存储线程，减少跨线程通知次数：

```cpp
void IoWorker::handleClient(ClientContext* c) {
    while (true) {
        // 读数据
        ssize_t n = c->stream.read(c->read_buf);
        if (n <= 0) { closeClient(c); return; }

        // 批量解析
        while (true) {
            size_t consumed;
            auto result = c->parser.feed(data, len, consumed);
            if (result == RespParser::NEED_MORE) break;
            if (result == RespParser::PROTO_ERR) { replyError(c); return; }

            // 累积到批量
            batch_.push_back({c, c->parser.args(), &c->write_buf});
        }

        // 一次性提交
        if (!batch_.empty()) {
            cmd_queue_.pushBatch(batch_);
            notifyStorage();
            batch_.clear();
        }

        // 等待所有回复完成，批量 flush
        waitForReplies(c);
        c->stream.writeFixed(c->write_buf);
    }
}
```

### 5.3 Pub/Sub 路径

```
Publisher:
  Client-A → PUBLISH channel msg
  [IO Thread X] → parse → [Storage Thread]
    → pubsub.publish("channel", "msg")
    → 遍历 channel 订阅者列表
    → 对每个订阅者 client:
        → client->write_buf 写入 "*3\r\n$7\r\nmessage\r\n$7\r\nchannel\r\n$3\r\nmsg\r\n"
        → client->notify() 唤醒对应的 IO fiber

Subscriber:
  Client-B → SUBSCRIBE channel
  [Storage Thread]: pubsub.add("channel", client_B)
  → reply: "*3\r\n$9\r\nsubscribe\r\n$7\r\nchannel\r\n:1\r\n"
```

---

## 六、性能优化关键点

### 6.1 零拷贝路径

| 阶段 | 优化 |
|------|------|
| 网络读取 | `readv()` + 预分配 64KB buffer，减少系统调用 |
| RESP 解析 | string_view 零拷贝引用原始 buffer |
| 命令传递 | PendingCommand 只传指针和视图，不拷贝字符串 |
| 存储查找 | 完美哈希 + 预计算哈希值（解析阶段计算） |
| RESP 写回 | iovec scatter/gather，批量 writev |
| AOF 写入 | writev 聚合多条，批 fsync (每秒) |

### 6.2 内存管理

| 对象 | 池化策略 |
|------|---------|
| `ClientContext` | 对象池 (连接池)，复用 |
| `PendingCommand` | 固定大小数组池 |
| `RespParser` | 嵌入 ClientContext，无需额外分配 |
| `ByteBuffer::Node` | zero 自带的 BlockPool |
| `Dict::Entry` | 自定义 allocator，批量分配 |
| 小 Value (≤44B) | embstr 编码，嵌入 Entry 旁边，减少 malloc |

### 6.3 批处理

- **存储线程**: 一次 drain 所有队列，批量处理 → 减少函数调用和分支预测失败
- **IO 线程**: 累积多个解析结果，一次 pushBatch → 减少 MPSC 队列操作
- **AOF**: 多条命令一次 writev → 减少系统调用
- **网络写入**: write_buf 积累到阈值或命令完成再 flush → 减少小包

### 6.4 无竞争快路径

```
读操作 (GET, HGET, ZSCORE...):
  当前版本: IO Thread → Storage Thread → Dict::find() → 回复
  未来优化: IO Thread 直接 RCU 读 Dict (需要无锁读保护)

写操作 (SET, DEL, LPUSH...):
  必须经过 Storage Thread 串行化

Pub/Sub:
  publish 在 Storage Thread 写入各订阅者的 write_buf
  subscribe/unsubscribe 更新订阅表 (Storage Thread)
```

---

## 七、配置设计

```yaml
# ledis.conf
server:
  port: 6379
  bind: "0.0.0.0"
  io_threads: 3              # IO 线程数 (不含存储线程)
  tcp_backlog: 511
  timeout: 0                 # 客户端空闲超时 (0=不超时)
  max_clients: 10000

storage:
  max_memory: "4GB"          # 最大内存
  eviction: "allkeys-lru"    # 淘汰策略
  databases: 16              # 数据库数量

persistence:
  aof_enabled: true
  aof_path: "./ledis.aof"
  aof_fsync: "everysec"      # always / everysec / no
  rdb_enabled: false
  rdb_path: "./ledis.rdb"
  rdb_save:                  # save <seconds> <changes>
    - [900, 1]
    - [300, 10]
    - [60, 10000]

slowlog:
  slower_than_us: 10000      # 记录超过 10ms 的命令
  max_len: 128
```

---

## 八、实施计划

| 阶段 | 内容 | 产出 |
|------|------|------|
| **Phase 1** | RESP 协议层 + 命令表框架 | 能 ping-pong |
| **Phase 2** | Storage Engine (Dict + Value + String) | GET/SET/DEL 可用 |
| **Phase 3** | 线程模型 (IO Workers + Storage Worker + MPSC) | 多线程跑通 |
| **Phase 4** | 全命令集 (Hash/List/Set/ZSet) | redis-cli 可直接使用 |
| **Phase 5** | 过期管理 + 淘汰策略 | 缓存语义完整 |
| **Phase 6** | AOF 持久化 | 可重启恢复 |
| **Phase 7** | Pub/Sub + 事务 (MULTI/EXEC) + Lua (可选) | 高级特性 |
| **Phase 8** | RDB + 主从复制 (可选) | 生产可用 |
| **Phase 9** | 压测 + 调优 + benchmark 报告 | 100K+ QPS |

---

## 九、架构终裁决策

| # | 决策点 | 裁定 | 理由 |
|---|--------|------|------|
| 1 | **线程模型** | N IO + 1 Storage | IO 解析可并行，存储必须串行 |
| 2 | **IO-Storage 通信** | MPSC 链表队列 + eventfd 通知 | 无锁 push，批 drain |
| 3 | **IO 线程数** | N = CPU 核心数 - 1 | 留一个核给存储线程 |
| 4 | **网络模型** | SO_REUSEPORT + per-thread accept | 连接分散到各 IO 线程 |
| 5 | **协议** | RESP2 全兼容 + RESP3 可选 | redis-cli 直连 |
| 6 | **DICT** | 链式哈希 + 渐进式 rehash | Redis 验证过的成熟方案 |
| 7 | **List 编码** | quicklist (ziplist 组成的链表) | 内存紧凑 + 操作高效 |
| 8 | **Hash 编码** | ziplist → hashtable 自适应 | 小 hash 节省内存 |
| 9 | **Set 编码** | intset → hashtable 自适应 | 纯整数集二分查找 |
| 10 | **ZSet 编码** | skiplist + dict 双索引 | lstl::skip_list |
| 11 | **过期** | 惰性 + 主动采样 (时间轮定期触发) | Redis 经典方案 |
| 12 | **淘汰** | 近似 LRU (24bit 时钟) + LFU (8bit 对数计数) | 内存开销仅 per-key 4 字节 |
| 13 | **持久化** | AOF everysec ± RDB | AOF 优先，RDB 可选 |
| 14 | **构建系统** | CMake, C++17 | 与 zero 一致 |
| 15 | **命名空间** | `ledis` | 独立于 `zero` |
