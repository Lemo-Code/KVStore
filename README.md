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
| 测试框架 | Google Test (gtest) via CMake FetchContent |
| 测试工具 | CTest, ASan, UBSan, TSan, gcov/lcov |
| 前端 | React 18, Vite 5, Tailwind CSS, Zustand |
| 后端 | FastAPI, SQLAlchemy, PostgreSQL |

---

## 🧪 测试体系

项目建立了完整的四层测试体系，覆盖所有核心组件。

### 测试架构

```
┌──────────────────────────────────────────────────────┐
│  Stress Tests (压力测试)                               │
│  并发压测 / 内存淘汰 / 大数据集 / 网络分区              │
├──────────────────────────────────────────────────────┤
│  Benchmark Tests (性能基准)                            │
│  QPS测量 / 延迟分布 / 可扩展性 / 对比基准              │
├──────────────────────────────────────────────────────┤
│  Integration Tests (集成测试)                          │
│  端到端 / 集群 / Pipeline / AOF回放                   │
├──────────────────────────────────────────────────────┤
│  Unit Tests (单元测试) — 86 test files                 │
│  zero (22) │ lstl (20) │ lrpc (6) │ ledis (24)        │
│  e2e (4)   │ bench (6) │ stress (4)                   │
└──────────────────────────────────────────────────────┘
```

### 测试覆盖矩阵

| 组件 | 单元测试 | 集成测试 | 性能基准 | 压力测试 | 覆盖目标 |
|------|---------|---------|---------|---------|---------|
| **zero** (网络库) | 22 files | — | 2 | — | 90%+ |
| **lstl** (容器库) | 20 files | — | — | — | 90%+ |
| **lrpc** (RPC) | 6 files | — | — | — | 85%+ |
| **ledis** (存储) | 24 files | 4 | 4 | 4 | 90%+ |
| **总计** | **72** | **4** | **6** | **4** | **86 files** |

### 测试组件说明

**zero 库测试 (22 files):**
- `test_base` — 基础工具 (endian, singleton, lexicalcast)
- `test_thread` — 线程原语 (SpinLock/Mutex/RWMutex/Semaphore)
- `test_fiber` / `test_fiber_context` / `test_fiber_pool` / `test_fiber_local` — 协程子系统
- `test_scheduler` / `test_work_stealing` — M:N 调度器 + Chase-Lev 队列
- `test_reactor` / `test_timer_wheel` / `test_fd_manager` / `test_hook` — epoll 事件循环 + 系统调用 hook
- `test_buffer` / `test_address` / `test_socket` / `test_stream` / `test_socket_stream` / `test_tcp_server` — 网络层
- `test_log` / `test_async_log` / `test_log_ringbuffer` / `test_log_config` — 日志系统
- `test_config` — YAML 配置系统

**ledis 库测试 (24 files):**
- `test_resp_parser` / `test_resp_writer` / `test_resp_types` — RESP2 协议层
- `test_value` / `test_dict` — 核心数据结构
- `test_storage_engine_*` (11 files) — 161 个 Redis 命令 (String/Hash/List/Set/ZSet/Stream/Bitmap/HyperLogLog/Geo/Keys/Transaction)
- `test_command` — 命令注册与分发
- `test_eviction` — 8 种淘汰策略 (LRU/LFU/TTL/Random)
- `test_lua_script` — LuaJIT 脚本引擎
- `test_session` / `test_blocking` / `test_pubsub` — 会话管理 / 阻塞操作 / 发布订阅
- `test_server` / `test_aof_writer` — 服务器生命周期 / AOF 持久化

**Bug 回归测试:** 14 个已知 Bug 均有对应回归测试用例，标记为 `BUG_REGRESSION`:
1. FiberPool::acquire bad_weak_ptr
2. FiberLocal reinterpret_cast UB
3. SpinLock ARM64 pause 缺失
4. ByteBuffer::toString nullptr UB
5. UringServer EAGAIN 数据丢失
6. BlockingManager 内存泄漏
7. PubSubManager 悬空指针
8. SETRANGE TOCTOU 竞态
9. Scan rehash 游标错误
10. Eviction 内存估算不准
11. LuaScriptEngine thread_local 竞态
12. TimerWheel cancelTimer O(N)
13. Cluster fail_votes_ 无 epoch 检查
14. ConfigVar listener 死锁

---

## 构建与运行

```bash
# === 构建 ledis (仅库和可执行文件) ===
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 启动服务
./ledis/ledis-server --port 6379

# === 构建并运行所有测试 ===
cd build
cmake .. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

# 运行全部测试
ctest --output-on-failure -j4

# 按组件运行
ctest -R zero.     # zero 库测试 (22)
ctest -R lstl.     # lstl 容器测试 (20)
ctest -R lrpc.     # lrpc RPC 测试 (6)
ctest -R ledis.    # ledis 存储测试 (24)
ctest -R integration.  # 集成测试 (4)
ctest -R stress.   # 压力测试 (4)

# 运行特定测试 (支持通配符)
ctest -R test_dict
ctest -R test_storage_engine_string

# === Sanitizer 构建 ===

# Address + Undefined Behavior Sanitizer
cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
         -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
ctest --output-on-failure

# Thread Sanitizer (需单独构建，与 ASan 不兼容)
cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" \
         -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
ctest --output-on-failure -j1  # TSan 建议单线程运行

# === 代码覆盖率 ===
cmake .. -DCMAKE_CXX_FLAGS="--coverage -O0" -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
ctest --output-on-failure
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/gtest/*' --output-file coverage_filtered.info
genhtml coverage_filtered.info --output-directory coverage_report
# 打开 coverage_report/index.html 查看详细覆盖率报告

# === 性能基准测试 ===
cmake .. -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 原始引擎压测
./tests/benchmark/bench_ledis_raw

# 服务器压测 (redis-benchmark 兼容输出)
./tests/benchmark/bench_ledis_server

# Dict 对比 std::unordered_map
./tests/benchmark/bench_dict_vs_std

# Dict 并发可扩展性
./tests/benchmark/bench_dict_concurrent

# zero echo 吞吐量
./tests/benchmark/bench_zero_echo

# 调度器延迟
./tests/benchmark/bench_zero_scheduler

# === 压力测试 ===
cmake .. -DBUILD_STRESS_TESTS=ON -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 并发客户端压测
./tests/stress/stress_ledis_concurrent

# 内存淘汰压测
./tests/stress/stress_ledis_memory

# 大数据集压测
./tests/stress/stress_ledis_large_keys

# === 原有构建方式 (兼容) ===
cd ledis/build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./ledis-server --port 6379

# 压测
redis-benchmark -p 6379 -t set,get -n 200000 -c 50 --csv
redis-benchmark -p 6379 -t set -n 500000 -c 50 -P 16 --csv
```

---

## 已知问题 (Bug Tracker)

以下问题在代码审查中发现，均有对应的回归测试 (`BUG_REGRESSION` 标记)：

| # | 严重度 | 组件 | 问题描述 |
|---|--------|------|---------|
| 1 | 🔴 Critical | FiberPool | `acquire()` 返回裸指针构造的 shared_ptr，调用 `shared_from_this()` 会抛 `bad_weak_ptr` |
| 2 | 🔴 Critical | FiberLocal | `reinterpret_cast` 在 shared_ptr 类型间转换，存在严格别名违例 UB |
| 3 | 🟡 Medium | SpinLock | ARM64 平台缺少 `__yield` 内联汇编，自旋锁性能次优 |
| 4 | 🟡 Medium | ByteBuffer | `toString()` 传递 `nullptr` 给 `std::string` 构造函数，实现定义行为 |
| 5 | 🔴 Critical | UringServer | 发送 EAGAIN 时静默丢弃响应，可能导致数据丢失 |
| 6 | 🟡 Medium | BlockingManager | 原始 `new`/`delete` 管理 Waiter 生命周期，异常路径存在泄漏风险 |
| 7 | 🔴 Critical | PubSubManager | 存储 `Session*` 裸指针，会话销毁时若未调用 `cleanup()` 则产生悬空指针 |
| 8 | 🟡 Medium | StorageEngine | `SETRANGE` 对同一 key 执行两次 `dict_.find()`，多线程下存在 TOCTOU 竞态 |
| 9 | 🟡 Medium | StorageEngine | SCAN 游标在 Dict resize 时可能丢失或重复 key |
| 10 | 🟢 Low | EvictionManager | 内存估算使用粗略公式 (`capacity*96 + size*64`)，未考虑复合类型实际大小 |
| 11 | 🟡 Medium | LuaScriptEngine | `thread_local lua_args_` 与共享 `lua_State*` 搭配，多线程存在数据竞态 |
| 12 | 🟢 Low | TimerWheel | `cancelTimer()` 惰性删除为 O(N)，大量取消时性能下降 |
| 13 | 🟡 Medium | ClusterGossip | `fail_votes_` 无 epoch 检查，旧 epoch 投票可能影响新一轮故障检测 |
| 14 | 🟡 Medium | ConfigVar | 变更监听器中回调同一 ConfigVar 的 `setValue()` 可能导致死锁 |

> 所有问题均通过 `tests/` 中的回归测试追踪。运行 `ctest -R BUG_REGRESSION` 可执行所有 Bug 回归测试。

---

## 作者

LemoDis | 2026-2027 | [GitHub](https://github.com/Lemo-Code/KVStore)
