# KVStore — High-Performance Distributed Key-Value Store

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-17-blue)](https://en.cppreference.com/w/cpp/17)
[![Platform](https://img.shields.io/badge/platform-Linux%20ARM64%20%7C%20x86__64-lightgrey)]()

**KVStore** 是一个从零构建的高性能分布式 KV 系统，包含自研的协程网络库、STL 容器库、Raft 共识引擎、以及兼容 Redis 的存储服务。全部 C++17 实现，~110,000 行代码，支持 ARM64 / x86-64。

```
  ╔═══════════════════════════════════════════════════╗
  ║              KVStore Architecture                 ║
  ╠═══════════════════════════════════════════════════╣
  ║  ┌─────────┐  ┌──────────┐  ┌──────────────────┐ ║
  ║  │  ledis   │  │ kvstore  │  │   bench tools    │ ║
  ║  │(Redis兼容)│  │(分布式KV) │  │  (压测+矩阵对比)   │ ║
  ║  └────┬─────┘  └────┬─────┘  └────────┬─────────┘ ║
  ║       ┌──────────────┼───────────────┐             ║
  ║       │         ┌────┴────┐          │             ║
  ║       │         │  lrpc   │          │             ║
  ║  ┌────┴────┐    │(RPC框架) │   ┌──────┴──────┐     ║
  ║  │  zero   │    └─────────┘   │    lstl     │     ║
  ║  │(网络库)  │                 │(STL容器库)   │     ║
  ║  └─────────┘                 └─────────────┘     ║
  ╚═══════════════════════════════════════════════════╝
```

---

## 目录

- [快速开始](#快速开始)
- [项目结构](#项目结构)
- [模块架构](#模块架构)
- [性能数据](#性能数据)
- [构建指南](#构建指南)
- [测试体系](#测试体系)
- [服务器环境](#服务器环境)
- [配置说明](#配置说明)
- [License](#license)

---

## 快速开始

```bash
# 1. 克隆项目
git clone https://github.com/yourname/KVStore.git
cd KVStore

# 2. 一键环境配置 + 编译 + 压测
bash shell/setup.sh

# 3. 启动服务
bin/ledis-server --port 6379

# 4. 用 redis-cli 连接
redis-cli -p 6379 PING
# → PONG
```

**前置依赖：** g++ (≥9.0), cmake (≥3.14), libyaml-cpp-dev, pthreads

**可选依赖：** redis-server (对比压测), libluajit-5.1-dev (Lua 脚本), liburing-dev (io_uring)

| 命令 | 功能 |
|------|------|
| `bash shell/setup.sh` | 一键全流程：环境检查→编译→功能验证→5组矩阵压测 |
| `bash shell/setup.sh --build-only` | 仅编译 |
| `bash shell/setup.sh --bench-only` | 仅压测（已有二进制） |
| `bash shell/setup.sh --clean` | 清理所有构建产物 |

---

## 项目结构

```
KVStore/
│
├── README.md                  ← 本文件
├── CMakeLists.txt             ← 根构建文件
├── .gitignore
│
├── SRC/                       ← 全部源代码 (~43K 行)
│   │
│   ├── zero/                  ★ 高性能协程网络库 (66 files, ~14K lines)
│   │   ├── include/zero/      # 头文件
│   │   │   ├── base/          # noncopyable, singleton, macro, endian, lexical_cast
│   │   │   ├── thread/        # Thread, Mutex, RWMutex, Semaphore, SpinLock, CPU affinity
│   │   │   ├── fiber/         # Fiber, FiberPool, StackPool, FiberLocal, Context (ARM64 asm)
│   │   │   ├── scheduler/     # Scheduler (work-stealing), Reactor (epoll), TimerWheel
│   │   │   │                  #   FDManager, WorkStealingQueue (Chase-Lev), Hook (dlsym)
│   │   │   ├── net/           # Buffer (ChainBuffer), Address, Socket, Stream, TcpServer
│   │   │   ├── log/           # 7-level Logger, AsyncLog, RingBuffer, Config
│   │   │   ├── config/        # ConfigVar<T>, YAML 配置系统
│   │   │   └── io/            # EpollEngine, IoUringEngine
│   │   ├── src/               # 实现文件 (fiber, scheduler, net, log, config, io)
│   │   └── zero.h             # 统一头文件
│   │
│   ├── zstl/                  ★ 自研 STL 容器库 (52 files, ~22K lines)
│   │   └── include/zstl/
│   │       ├── containers/    # vector, list, deque, map, set, unordered_map,
│   │       │                  #   skip_map, bplus_tree, bmap, priority_queue...
│   │       ├── memory/        # pool allocator (28 size classes), construct, uninitialized
│   │       ├── iterators/     # iterator traits, reverse, move, insert iterators
│   │       ├── algorithms/    # sort, find, merge, heap, partition...
│   │       ├── string/        # basic_string, string_view
│   │       └── thread/        # mutex, condition_variable, atomic extensions
│   │
│   ├── kvstore/               ★ 分布式 KV 系统 (67 files, ~4K lines) [WIP]
│   │   ├── common/            # kv_types, kv_error (Status), kv_utils (CRC16/CRC32C/Varint)
│   │   ├── config/            # KvConfig, YAML 加载
│   │   ├── protocol/          # 帧编码, KV/Raft/Admin 消息定义与序列化
│   │   ├── storage/           # IKvEngine 接口, MemoryEngine, WalWriter/Reader, SSTable
│   │   ├── raft/              # Raft 共识: 选举/复制/快照/成员变更/传输
│   │   ├── shard/             # 一致性哈希, 分片注册/控制/迁移
│   │   ├── api/               # KvApi (get/put/delete/scan/batch), TxnContext (乐观事务)
│   │   ├── server/            # KvServer (TCP accept), ClientSession, AdminCommands
│   │   ├── client/            # KvClient SDK (连接池+重试+自动发现)
│   │   └── monitor/           # HealthChecker (故障检测), MetricsCollector (计数器+直方图)
│   │
│   └── bench_now.cpp          # 独立性能基准测试
│
├── shell/                     ★ 一键脚本 (6 files)
│   ├── setup.sh               # 一键环境配置 + 编译 + 5组矩阵压测
│   ├── bench_log.sh           # ① zero_log vs spdlog 矩阵对比
│   ├── bench_pool.sh          # ② 内存池 vs malloc 矩阵对比
│   ├── bench_zstl.sh          # ③ zstl vs STL 矩阵对比
│   ├── bench_net.sh           # ④ zero网络库 vs libevent 矩阵对比
│   └── bench_kv.sh            # ⑤ ledis vs redis 矩阵对比
│
├── bin/                       ← 编译产物 (9 个二进制)
│   ├── ledis-server           # Redis 兼容 KV 服务 (161 命令)
│   ├── stress_log_matrix      # 日志矩阵压测
│   ├── stress_lstl_bench      # 容器矩阵压测
│   ├── stress_net_compare     # 网络矩阵压测
│   ├── stress_net_zero        # zero 网络库独立压测
│   ├── stress_ledis_redis_compare  # ledis vs redis 矩阵压测
│   ├── stress_ledis_concurrent     # ledis 并发正确性+性能
│   ├── bench_echo             # TCP echo 基准
│   └── echo_minimal           # 最小化 echo 服务器
│
├── benchmark/                 ← 压测报告输出目录
│   ├── SETUP_SUMMARY.txt      # setup.sh 汇总报告
│   ├── log_matrix.txt         # 日志对比数据
│   ├── pool_matrix.txt        # 内存池对比数据
│   ├── zstl_matrix.txt        # 容器对比数据
│   ├── net_matrix.txt         # 网络对比数据
│   └── kv_matrix.txt          # KV 对比数据
│
├── cmake/                     ← CMake 模块
│   ├── gtest.cmake            # Google Test 自动下载
│   ├── coverage.cmake         # gcov/lcov 覆盖率
│   └── TestUtils.cmake        # add_zero_test / add_lstl_test / add_ledis_test ...
│
├── build-stress/              ← 压测构建目录
├── build-res-zero/            ← zero 库独立构建
└── build-res-zstl/            ← zstl 库独立构建
```

---

## 模块架构

### 1. zero — 高性能协程网络库

自研的 M:N 协程网络框架，是整个项目的网络基础设施。

| 模块 | 功能 | 关键技术 |
|------|------|----------|
| **fiber** | 有栈非对称协程 (stackful asymmetric) | ARM64 汇编上下文切换 (~10ns)，6 状态 FSM，StackPool (mmap + guard page) |
| **scheduler** | M:N 调度器 (M 协程:N 线程) | Work-Stealing (Chase-Lev 无锁双端队列)，Per-thread Reactor |
| **reactor** | Per-thread epoll 事件循环 | Edge-triggered，eventfd 跨线程唤醒，FdContext 映射 |
| **timer** | 分层时间轮 (5-level) | O(1) tick/insert，1ms 精度，~49 天覆盖 |
| **hook** | 透明 syscall 拦截 | dlsym(RTLD_NEXT)，自动将阻塞 I/O 转为异步 |
| **net** | TCP/UDP 网络栈 | ChainBuffer (零拷贝 iovec)，Address (IPv4/IPv6/Unix)，非阻塞 Socket |
| **log** | 7 级异步日志 | RingBuffer (无锁 SPSC)，Console/File/Syslog Appender，MDC |
| **config** | YAML 配置系统 | ConfigVar<T>，变更监听回调，热加载 |
| **thread** | 线程原语 | Thread (pthread 封装)，SpinLock，Mutex，RWMutex，Semaphore，CPU Affinity |

**设计特点：**
- 协程堆栈受 guard page 保护，溢出触发 SIGSEGV 而非静默损坏
- 系统调用 hook 透明转换同步→异步，现有代码无需修改
- 时间轮跨线程无锁（每线程独立实例）
- Chase-Lev 队列底部 LIFO (cache hot)，顶部 FIFO (steal fairness)

### 2. zstl — 自研 STL 容器库

header-only，零依赖标准库容器的独立实现。

| 模块 | 内容 |
|------|------|
| **containers** | vector, list, slist, deque, map, set, multimap, multiset, unordered_map, unordered_set, **skip_map**, **bplus_tree**, bmap, bset, stack, queue, priority_queue |
| **memory** | pool allocator (28 大小类: 8B~7KB)，construct/destroy，type_traits |
| **algorithms** | sort (内省排序), find, merge, heap, partition, next_permutation... |
| **iterators** | iterator_traits, reverse_iterator, move_iterator, insert_iterator |
| **string** | basic_string, string_view (C++17 兼容) |
| **thread** | mutex, condition_variable, call_once, atomic 扩展 |

**与 STL 的关键差异：**
- POD 类型 vector::push_back 使用 memcpy 批量优化
- 开放寻址 unordered_map (FNV-1a 哈希，线性探测)
- 内存池分配器减少 malloc 调用
- skip_map 提供 O(log N) 有序操作 + 无锁读

### 3. kvstore — 分布式 KV 系统

基于 zero + zstl 构建的分布式 KV 存储，多 Raft 架构。

```
                     Client Request (key="foo")
                            │
                            ▼
               ┌────────────────────────┐
               │   ConsistentHash       │
               │   CRC16("foo") % 16384  │
               └────────────────────────┘
                            │
              ┌─────────────┼─────────────┐
              │             │             │
         Shard 0        Shard 1       Shard N
      (slots 0-5460)  (5461-10922)  (10923-16383)
              │             │             │
         RaftGroup 0   RaftGroup 1   RaftGroup N
        [N1, N2, N3]  [N2, N3, N4]  [N1, N4, N5]
```

| 模块 | 功能 |
|------|------|
| **storage** | IKvEngine 抽象接口，MemoryEngine (std::map + unordered_map 双索引)，WAL 持久化，SSTable (LSM 风格) |
| **raft** | 完整 Raft 实现：Leader Election (pre-vote)，Log Replication (pipeline batch)，Snapshot (chunked transfer)，Membership Change (joint consensus) |
| **shard** | 一致性哈希 (CRC16, 16384 slots)，分片注册/控制/迁移 |
| **protocol** | 二进制帧协议 (magic + CRC32C)，KV/Raft/Admin 三类消息 |
| **api** | KvApi (Get/Put/Delete/Scan/Batch)，TxnContext (乐观并发事务) |
| **server** | TCP accept loop (select-based)，请求分发，帧解析，MOVED 重定向 |

### 4. ledis — Redis 兼容服务

兼容 Redis 协议的 KV 服务，支持 161 个命令，集群模式。

**架构演进：**
- v1: 单线程 epoll
- v2: Fiber 协程 (M:N 调度) ← 当前默认
- v5: 多线程 shard + epoll

**集群功能：** CLUSTER NODES / SLOTS / INFO，MOVED 重定向，Gossip 协议，自动故障转移

---

## 性能数据

> **测试环境:** Apple M-series / ARM64 (aarch64) · 5 核 · 6 GB RAM
> **系统:** Ubuntu 24.04 LTS · Kernel 6.8.0 · GCC 13.3.0 · `-O3 -march=native`

---

### ① ledis vs redis — 网络 QPS（真实 TCP 路径, redis-benchmark）


#### 多线程/多客户端对比 (t = 并发客户端数)

---

### ② ledis 引擎层极限吞吐（绕过网络, 直接 API 调用）


---

### ③ zero 网络库 vs libevent — TCP Echo


---

### ④ zstl vs STL — 容器 (ops/s, 4 线程)


---

### ⑤ 日志吞吐 — zero_log (lines/s)

---

### 性能总览


---

## 构建指南

### 环境要求

| 组件 | 最低版本 | 说明 |
|------|----------|------|
| **OS** | Linux (Ubuntu 20.04+) | ARM64 或 x86-64 |
| **GCC** | 9.0+ | C++17 完整支持 |
| **CMake** | 3.14+ | 构建系统 |
| **yaml-cpp** | 0.6+ | YAML 配置解析 |
| **pthread** | — | 线程库 (系统自带) |

### 安装依赖

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y g++ cmake libyaml-cpp-dev

# 可选依赖
sudo apt install -y redis-server          # redis 对比测试
sudo apt install -y libluajit-5.1-dev     # Lua 脚本支持
sudo apt install -y liburing-dev          # io_uring 支持
```

### 编译

```bash
# 方式 1: 一键脚本 (推荐)
bash shell/setup.sh --build-only

# 方式 2: 手动编译 zero + ledis + 压测工具
cmake -B build-stress -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_STRESS_TESTS=ON -DBUILD_TESTS=OFF
cmake --build build-stress -j$(nproc)

# 方式 3: 仅编译 zero 库
cmake -B build-res-zero SRC/zero -DCMAKE_BUILD_TYPE=Release
cmake --build build-res-zero -j$(nproc)

# 方式 4: 仅编译 zstl 测试
cmake -B build-zstl-test SRC/zstl -DCMAKE_BUILD_TYPE=Release
cmake --build build-zstl-test -j$(nproc)
```

### 运行

```bash
# 启动 ledis 服务 (单机)
bin/ledis-server --port 6379

# 启动 3 节点集群
bin/ledis-server --port 7000 --cluster-enabled --cluster-port 17000 &
bin/ledis-server --port 7001 --cluster-enabled --cluster-port 17001 \
                 --cluster-seeds 127.0.0.1:17000 &
bin/ledis-server --port 7002 --cluster-enabled --cluster-port 17002 \
                 --cluster-seeds 127.0.0.1:17000 &

# 用 redis-cli 连接
redis-cli -p 6379 SET foo bar
redis-cli -p 6379 GET foo
redis-cli -c -p 7000 SET cluster_key value  # 集群模式自动跳转
```

---

## 测试体系

### 单元测试

```bash
# zstl 测试 (40+ 测试文件)
cmake -B build-zstl-test SRC/zstl -DBUILD_TESTS=ON
cmake --build build-zstl-test -j$(nproc)
cd build-zstl-test && ctest --output-on-failure

# kvstore 测试
cmake -B SRC/kvstore/build-check SRC/kvstore -DBUILD_KVSTORE_TESTS=ON
cmake --build SRC/kvstore/build-check
./SRC/kvstore/build-check/kvstore-test
```

### 矩阵压测

```bash
# 5 组独立压测
bash shell/bench_log.sh    # 日志矩阵
bash shell/bench_pool.sh   # 内存池矩阵
bash shell/bench_zstl.sh   # 容器矩阵
bash shell/bench_net.sh    # 网络矩阵
bash shell/bench_kv.sh     # KV 矩阵

# 一键运行全部
bash shell/setup.sh --bench-only

# 查看结果
cat benchmark/SETUP_SUMMARY.txt
```

### 并发正确性测试

```bash
# ledis INCR 并发正确性 (2/4/8 线程, mutex 串行化)
bin/stress_ledis_concurrent benchmark/
```

**测试结果:**

| 线程数 | 每线程操作 | 期望值 | 实际值 | 耗时 | 结果 |
|:---:|-----:|-----:|-----:|----:|:--:|
| 2 | INCR×100,000 | 200,000 | 200,000 | 34ms | ✅ |
| 4 | INCR×100,000 | 400,000 | 400,000 | 62ms | ✅ |
| 8 | INCR×50,000 | 400,000 | 400,000 | 68ms | ✅ |

### 集群功能测试

| 功能 | 结果 |
|------|:--:|
| 3 节点组网 | ✅ |
| CLUSTER NODES 发现 | ✅ |
| CLUSTER SLOTS 分配 (16384 slots) | ✅ |
| MOVED 重定向 (正确返回目标节点) | ✅ |
| redis-cli -c 自动跳转 | ✅ |
| 20 keys 批量读写完整性 | ✅ |

---

## 服务器环境

### 推荐配置

| 级别 | CPU | 内存 | 适用场景 |
|------|-----|------|----------|
| **开发** | 2 核 | 2 GB | 编译 + 单元测试 |
| **压测** | 4 核 | 8 GB | 全量基准测试 |
| **生产** | 8 核+ | 32 GB+ | 集群部署 |

### 支持架构

| 架构 | 状态 | 备注 |
|------|------|------|
| **ARM64 (aarch64)** | ✅ 完整支持 | 汇编级协程上下文切换优化 |
| **x86-64** | ✅ 完整支持 | 汇编级协程上下文切换优化 |

### 关键环境变量

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `LEDIS_PORT` | 16379 | ledis 服务端口 |
| `REDIS_PORT` | 6379 | Redis 对比端口 |
| `ECHO_PORT` | 18900 | Echo 服务端口 |
| `CMAKE` | `/usr/bin/cmake` | CMake 路径 |

---

## 配置说明

### YAML 配置示例

```yaml
# ledis 配置
port: 6379
bind: "0.0.0.0"
loglevel: "INFO"
tcp_nodelay: true

# 持久化
aof: "/var/lib/ledis/appendonly.aof"
aof_mode: "everysec"

# 集群
cluster_enabled: true
cluster_port: 16379
cluster_replicas: 1

# zero 网络库配置
zero:
  fiber_stack_size: 131072    # 128KB
  fiber_pool_size: 1024
  scheduler_threads: 4
  socket_recv_timeout_ms: 5000
  socket_tcp_no_delay: true
```

---

## License

MIT License

---

## 参考

- [Raft 共识算法论文](https://raft.github.io/raft.pdf)
- [Chase-Lev Work-Stealing Deque](https://www.di.ens.fr/~zappa/readings/ppopp13.pdf)
- [Redis 集群规范](https://redis.io/docs/reference/cluster-spec/)
- [Linux epoll](https://man7.org/linux/man-pages/man7/epoll.7.html)
- [ARM64 AAPCS64 调用约定](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst)
