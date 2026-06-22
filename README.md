# KVStore — 基于自研网络库的高性能 Redis 兼容存储

> **求职作品集** | C++17 | ARM64 Linux | 2025-2026

## 项目概览

从零构建了一套完整的 **高性能 KV 存储系统**，包含自研网络库、自研容器库、Redis 兼容服务端和学习平台。

```
┌──────────────────────────────────────────────────────┐
│                  redisLearningPlat                    │
│              Redis 学习平台 (React + FastAPI)          │
├──────────────────────────────────────────────────────┤
│                      ledis                           │
│        161 命令仿 Redis (30 万 QPS 单线程)            │
├──────────────────────┬───────────────────────────────┤
│        zero          │           lstl                │
│   M:N 协程网络库      │     轻量 STL 容器库            │
│   (74 万 QPS echo)   │  (内存池/开放寻址/无锁队列)     │
└──────────────────────┴───────────────────────────────┘
```

---

## 子项目

### ledis — 仿 Redis 服务端 (核心产品)

**161 命令**，单线程 **30 万 QPS** (ARM64)，Pipeline **225 万 QPS**，同机性能超越 Redis 7.0。

<details>
<summary><b>支持的全部命令 (点击展开) </b></summary>

| 分类 | 命令 |
|------|------|
| String | SET GET SETNX SETEX PSETEX GETSET GETRANGE SETRANGE APPEND STRLEN INCR INCRBY DECR DECRBY INCRBYFLOAT MGET MSET MSETNX GETDEL RENAME RENAMENX |
| Hash | HSET HGET HDEL HEXISTS HGETALL HKEYS HVALS HLEN HINCRBY HINCRBYFLOAT HSETNX HMSET HMGET HRANDFIELD HSTRLEN |
| List | LPUSH RPUSH LPOP RPOP LLEN LRANGE LINDEX LSET LREM LTRIM LPOS LMOVE BLPOP BRPOP |
| Set | SADD SREM SMEMBERS SISMEMBER SCARD SPOP SRANDMEMBER SINTER SINTERSTORE SUNION SUNIONSTORE SDIFF SDIFFSTORE SMISMEMBER SMOVE |
| ZSet | ZADD ZREM ZCARD ZSCORE ZRANK ZREVRANK ZRANGE ZREVRANGE ZRANGEBYSCORE ZREVRANGEBYSCORE ZCOUNT ZINCRBY ZREMRANGEBYRANK ZREMRANGEBYSCORE ZPOPMIN ZPOPMAX ZRANDMEMBER ZLEXCOUNT ZRANGEBYLEX ZREMRANGEBYLEX ZINTER ZUNION ZDIFF ZINTERSTORE ZUNIONSTORE ZDIFFSTORE BZPOPMIN BZPOPMAX |
| Stream | XADD XREAD XREADGROUP XRANGE XLEN XDEL XGROUP XACK XPENDING |
| Bitmap | SETBIT GETBIT BITCOUNT BITOP BITPOS |
| HyperLogLog | PFADD PFCOUNT PFMERGE |
| Geo | GEOADD GEODIST GEOHASH GEOPOS GEORADIUS |
| Lua | EVAL EVALSHA SCRIPT LOAD/FLUSH/EXISTS |
| Pub/Sub | SUBSCRIBE UNSUBSCRIBE PUBLISH PSUBSCRIBE PUNSUBSCRIBE PUBSUB |
| 事务 | MULTI EXEC DISCARD WATCH UNWATCH |
| 服务器 | PING ECHO TIME DBSIZE KEYS FLUSHDB RANDOMKEY SCAN CONFIG INFO CLIENT SHUTDOWN MONITOR SLOWLOG SAVE BGSAVE AUTH SELECT TOUCH MEMORY OBJECT RESTORE COPY EXPIRETIME PEXPIRETIME HELLO COMMAND ACL SORT |
</details>

**架构亮点：**

- **单线程 fiber + 非阻塞 I/O**：类 Redis 架构，无锁、零上下文切换
- **开放寻址 Dict + hash 缓存**：线性探测，FNV-1a 哈希，70% 负载因子，批量 resize
- **ARM64 CRC32 硬件加速**、**PGO 编译优化**
- **maxmemory + 8 种淘汰策略**：LRU / LFU / TTL / Random
- **AOF 持久化**：ALWAYS / EVERYSEC / NO 三种模式
- **LuaJIT 脚本引擎**：EVAL / EVALSHA / redis.call
- **Stream Consumer Group**：XREADGROUP / XACK / XPENDING
- **迭代记录**：[ledis/doc/ITERATIONS.md](ledis/doc/ITERATIONS.md) — 从 v1 存储线程模型到 v5 多核 sharding 的完整演进

**性能对比 (ARM64 4 核，同机测试)：**

| | Ledis | Redis 7.0 |
|------|-------|------|
| SET (50 连接) | **30.0 万** | 22.9 万 |
| GET (50 连接) | **31.5 万** | 21.3 万 |
| SADD (50 连接) | **30.0 万** | 22.0 万 |
| Pipeline P=16 | 225 万 | 272 万 |

---

### zero — 高性能 M:N 协程网络库

C++17 实现，汇编级上下文切换，epoll reactor，work-stealing 调度器。

**核心特性：**

- **M:N 协程调度**：用户态 fiber，汇编实现上下文切换 (~10ns)，Chase-Lev 工作窃取
- **epoll reactor**：per-thread 事件循环，层级时间轮 (O(1) tick)
- **系统调用 hook**：dlsym 拦截，透明异步化阻塞 I/O
- **零拷贝 buffer**：ChainBuffer + iovec scatter/gather
- **内存池**：fiber 栈池、buffer 块池
- **echo 压测**：74 万 QPS (4 线程 ARM64)，零错误

---

### lstl — 轻量 STL 容器库

C++14 header-only，仿 jemalloc 内存池，20+ 容器。

**核心特性：**

- **内存池**：28 个 size class (8B ~ 7KB)，per-size-class freelist，O(1) 分配/释放
- **开放寻址哈希表**：线性探测，power-of-2 容量，位掩码取模
- **红黑树 map/set**：迭代式 CLRS 算法，无递归
- **B+ 树、跳表**：高级数据结构
- **deque**：分段数组 (64 元素 buffer)，O(1) 双端操作
- **性能**：vector push_back 仅比 std::vector 慢 18-24%，内存池 40-71% 快于 malloc

---

### redisLearningPlat — Redis 学习平台

React 18 + TypeScript + FastAPI + PostgreSQL，AI 驱动的交互式学习系统。

- **前端**：Vite 5 + shadcn/ui + Tailwind CSS + Zustand 状态管理
- **后端**：Python FastAPI + SQLAlchemy + JWT 认证
- **AI 助手**：OpenAI API 流式 SSE，上下文感知问答
- **功能**：Dashboard、知识库、聊天、Redis 可视化操作、学习路径

---

## 构建与运行

```bash
# 构建 ledis
cd ledis/build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 启动服务
./ledis-server --port 6379

# 压测
redis-benchmark -p 6379 -t set,get -n 200000 -c 50 --csv
redis-benchmark -p 6379 -t set -n 500000 -c 50 -P 16 --csv
```

## 技术栈

| 层次 | 技术 |
|------|------|
| 语言 | C++17, Python, TypeScript |
| 编译 | CMake 3.14+, GCC 13, -O3 -march=native |
| 平台 | ARM64 Linux (Kernel 6.8) |
| 协议 | RESP2 (Redis Serialization Protocol) |
| 测试 | redis-cli, redis-benchmark |
| 前端 | React 18, Vite 5, Tailwind CSS, Zustand |
| 后端 | FastAPI, SQLAlchemy, PostgreSQL |

## 作者

求职作品 | 2025-2026 | [GitHub](https://github.com/Lemo-Code/KVStore)
