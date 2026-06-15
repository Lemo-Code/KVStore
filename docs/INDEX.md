# 📚 KVStore 项目文档索引

> **index_docs** — 以 Key-Value 模式组织的文档检索系统
>
> 每个文档按其主题/功能作为 Key 索引，Value 包含路径、描述、关联关系。
> 形如：`Key(模块.主题) → Value(路径 + 摘要 + 上下游依赖)`

---

## 一、文档图谱总览

```
KVStore/
│
├── 📘 ARCHITECTURE.md          ← 顶层架构设计 (Zero 网络库)
├── 📘 index.md                 ← 项目入口
│
├── 📂 docs/                    ← 本目录: 文档索引中枢
│   └── 📋 INDEX.md             ← [本文档] 全局 K-V 文档索引
│
├── 📂 newFolder/lstl/          ← lstl 基础库 (已完成)
│   ├── 📂 docs/
│   │   ├── API_REFERENCE.md    ← lstl API 参考手册
│   │   └── PROJECT_RETROSPECTIVE.md ← lstl 项目复盘 (秋招版)
│   ├── 📂 bench/               ← 性能基准测试
│   └── 📂 tests/               ← 15 个单元测试
│
├── 📂 lrpc/                    ← RPC 框架 (规划中)
│   └── 📂 docs/
│       ├── 01-wire-protocol.md ← 线协议设计
│       ├── 02-codec.md         ← 编解码层
│       ├── 03-transport.md     ← 传输层
│       ├── 04-interceptor.md   ← 拦截器链
│       └── 05-multiplex.md     ← 多路复用
│
└── 📂 ledis/                   ← 缓存服务器 (原有)
```

---

## 二、K-V 文档索引

### 2.1 架构设计层

| Key | Value |
|-----|-------|
| `arch.overview` | **文件**: `../ARCHITECTURE.md` |
| | **描述**: Zero 高性能网络库总体架构设计，含分层图、M:N Fiber 调度、Reactor + epoll、协程上下文切换、内存池策略、Hook 系统调用劫持等核心设计。 |
| | **状态**: 设计文档，待实现 |
| | **关联**: 依赖 `lstl` 基础库 |
| `arch.network` | **同上** — 覆盖 Socket / Stream / Buffer / Hook / Reactor / Scheduler / Fiber / Timer 模块设计 |
| `arch.decisions` | **同上第 8 节** — 终裁决策表：命名/C++标准/SSL/构建/Channel/Fiber 栈/HTTP 等 8 项关键决策及理由 |
| `arch.compare` | **同上第 1.2 节** — 与 Go netpoller 的对比分析 |

### 2.2 lstl 基础库层

| Key | Value |
|-----|-------|
| `lstl.api` | **文件**: `../newFolder/lstl/docs/API_REFERENCE.md` |
| | **描述**: lstl 完整 API 参考手册 — memory/ 子系统 (9 个头文件) + container/ 子系统 (30 个头文件)，含内存池架构图、构建指南、使用示例、已知限制。 |
| | **状态**: ✅ 已完成，15/15 测试通过 |
| | **关联**: 上层依赖库 `lstl` |
| `lstl.retrospect` | **文件**: `../newFolder/lstl/docs/PROJECT_RETROSPECTIVE.md` |
| | **描述**: 🔑 秋招项目复盘 — 为什么要做 lstl / 为什么不用 STL / 4 项性能优化及量化数据 / 架构设计决策 / 技术难点分析 / 面试追问准备 / 后续规划。 |
| | **状态**: ✅ 已完成 |
| | **受众**: 面试官 / 技术答辩 |
| `lstl.memory.pool` | **文件**: `../newFolder/lstl/include/lstl/memory/pool.h` |
| | **描述**: jemalloc 风格多级内存池实现 — 28 个 size class + per-class freelist + O(1) alloc/free。单线程性能比 malloc 快 40-71%。 |
| | **关联**: `lstl.memory.allocator`, `lstl.memory.alloc` |
| `lstl.memory.allocator` | **文件**: `../newFolder/lstl/include/lstl/memory/allocator.h` |
| | **描述**: 标准分配器接口 + `allocator_traits` + `simple_alloc` 适配器。 |
| `lstl.memory.construct` | **文件**: `../newFolder/lstl/include/lstl/memory/construct.h` |
| | **描述**: placement new 构造 / 显式析构 / 平凡析构类型优化 (no-op dispatch)。 |
| `lstl.memory.uninitialized` | **文件**: `../newFolder/lstl/include/lstl/memory/uninitialized.h` |
| | **描述**: 未初始化内存算法 — POD 路径 (memmove bulk copy) + 非 POD 路径 (逐元素构造 + 异常安全回滚)。 |
| `lstl.memory.utility` | **文件**: `../newFolder/lstl/include/lstl/memory/utility.h` |
| | **描述**: pair / swap / move / forward / integer_sequence 等基础工具。 |
| `lstl.memory.traits` | **文件**: `../newFolder/lstl/include/lstl/memory/type_traits.h` |
| | **描述**: is_pod / has_trivial_* / conditional / enable_if / decay 等编译期类型检测。 |
| `lstl.memory.functional` | **文件**: `../newFolder/lstl/include/lstl/memory/functional.h` |
| | **描述**: FNV-1a 哈希 / identity/select1st 键提取 / less/greater/equal_to 比较算子。 |

### 2.3 容器子系统

| Key | Value |
|-----|-------|
| `lstl.container.vector` | **文件**: `../newFolder/lstl/include/lstl/container/vector.h` |
| | **描述**: 动态数组，2x 扩容因子，POD bulk copy 优化。bench: push_back 比 std::vector 快 34-37%。 |
| `lstl.container.list` | **文件**: `../newFolder/lstl/include/lstl/container/list.h` |
| | **描述**: 双向链表，哨兵节点设计，O(1) insert/erase，支持 splice/reverse/unique/remove。 |
| `lstl.container.deque` | **文件**: `../newFolder/lstl/include/lstl/container/deque.h` |
| | **描述**: 分段数组，64 元素/Buffer，O(1) 随机访问 + 两端操作。 |
| `lstl.container.map` | **文件**: `../newFolder/lstl/include/lstl/container/map.h` |
| | **描述**: RB-tree 有序映射，O(log n)，支持 operator[]/at/lower_bound/upper_bound。bench: insert 比 std::map 慢 35% (优化空间中)。 |
| `lstl.container.set` | **文件**: `../newFolder/lstl/include/lstl/container/set.h` |
| | **描述**: RB-tree 有序集合。 |
| `lstl.container.unordered_map` | **文件**: `../newFolder/lstl/include/lstl/container/unordered_map.h` |
| | **描述**: 哈希表 (分离链接法 + FNV-1a)，质数桶，负载因子 1.0 触发 rehash。 |
| `lstl.container.skip_map` | **文件**: `../newFolder/lstl/include/lstl/container/skip_map.h` |
| | **描述**: 跳表有序映射，32 层 1/4 概率，期望 O(log n)，并发友好。 |
| `lstl.container.bmap` | **文件**: `../newFolder/lstl/include/lstl/container/bmap.h` |
| | **描述**: B+ 树映射，256 Order (~4KB 叶子)，叶节点链表支持 O(1) 范围遍历。⚠️ 分裂未完整实现。 |
| `lstl.container.stack` | **文件**: `../newFolder/lstl/include/lstl/container/stack.h` |
| | **描述**: LIFO 栈适配器，默认基于 deque。 |
| `lstl.container.queue` | **文件**: `../newFolder/lstl/include/lstl/container/queue.h` |
| | **描述**: FIFO 队列适配器，默认基于 deque。 |
| `lstl.container.priority_queue` | **文件**: `../newFolder/lstl/include/lstl/container/priority_queue.h` |
| | **描述**: 最大堆优先队列，基于 vector + heap 算法。 |

### 2.4 内部实现层 (detail)

| Key | Value |
|-----|-------|
| `lstl.detail.rb_tree` | **文件**: `../newFolder/lstl/include/lstl/container/detail/rb_tree.h` |
| | **描述**: 迭代式红黑树 (CLRS 算法)，nullptr 叶子 + header 哨兵。insert_fixup/erase_fixup 处理 null→black 语义。 |
| `lstl.detail.hashtable` | **文件**: `../newFolder/lstl/include/lstl/container/detail/hashtable.h` |
| | **描述**: 分离链接法哈希表，质数桶 (SGI STL 质数表)，load factor 自动 rehash。 |
| `lstl.detail.skip_list` | **文件**: `../newFolder/lstl/include/lstl/container/detail/skip_list.h` |
| | **描述**: 概率跳表，内联 forward 数组分配 (malloc + placement new)。 |
| `lstl.detail.bplus_tree` | **文件**: `../newFolder/lstl/include/lstl/container/detail/bplus_tree.h` |
| | **描述**: 内存 B+ 树，内部节点仅存路由 Key，叶节点链表连接。 |
| `lstl.detail.heap` | **文件**: `../newFolder/lstl/include/lstl/container/detail/heap.h` |
| | **描述**: 二叉堆算法 — push_heap/pop_heap/make_heap/sort_heap (非递归 sift up/down)。 |
| `lstl.detail.iterators` | **文件**: `list_iterator.h` / `deque_iterator.h` / `reverse_iterator.h` / `slist_node.h` / `list_node.h` |
| | **描述**: 5 种迭代器 + 节点类型 — 双向链表迭代器、分段 deque 迭代器、反向迭代器适配器。 |

### 2.5 测试与基准

| Key | Value |
|-----|-------|
| `lstl.test.all` | **目录**: `../newFolder/lstl/tests/` |
| | **描述**: 15 个独立测试文件，覆盖所有容器和内存模块。`ctest` 一键运行。 |
| | **结果**: 15/15 PASS ✅ |
| `lstl.bench.vector` | **文件**: `../newFolder/lstl/bench/bench_vector.cpp` |
| | **描述**: vector vs std::vector 性能对比 — push_back / random access / iteration / insert。 |
| `lstl.bench.map` | **文件**: `../newFolder/lstl/bench/bench_map.cpp` |
| | **描述**: map vs std::map 性能对比 — insert / find(hit+miss) / iteration / erase。 |
| `lstl.bench.pool` | **文件**: `../newFolder/lstl/bench/bench_pool.cpp` |
| | **描述**: default_alloc vs malloc 性能对比 — single-size / mixed / rapid cycle。 |
| `lstl.bench.concurrent` | **文件**: `../newFolder/lstl/bench/bench_concurrent.cpp` |
| | **描述**: 多线程内存池压力测试 — 4/8 线程并发分配/释放。 |

### 2.6 网络库层 (zero)

| Key | Value |
|-----|-------|
| `zero.api` | **文件**: `../zero/docs/API.md` |
| | **描述**: Zero 网络库完整 API 参考 — Fiber/Scheduler/Reactor/Hook/Net/Log/Config/Thread 全部 8 个模块的使用示例和接口说明。 |
| | **性能**: Echo 35 万 QPS / P50=67μs / 日志 550 万 QPS |
| | **状态**: ✅ 已完成 |
| `zero.fiber` | **文件**: `../zero/fiber/fiber.h` |
| | **描述**: 有栈非对称协程 — ucontext 上下文切换 ~200ns, 128KB 栈 + guard page, FiberPool + StackPool。 |
| `zero.scheduler` | **文件**: `../zero/scheduler/scheduler.h` |
| | **描述**: M:N 协程调度器 — per-thread LIFO 队列 + Chase-Lev work-stealing deque + 全局 MPSC 队列。 |
| `zero.reactor` | **文件**: `../zero/scheduler/reactor.h` |
| | **描述**: Per-thread epoll — EPOLLONESHOT + eventfd 唤醒 + Timer Wheel 分层时间轮。 |
| `zero.hook` | **文件**: `../zero/scheduler/hook.h` |
| | **描述**: Syscall 劫持 — dlsym(RTLD_NEXT) 拦截 sleep/read/write/connect/send/recv 等，透明 fiber 化。 |
| `zero.net` | **文件**: `../zero/net/` |
| | **描述**: Socket/Address/Buffer(链式零拷贝)/Stream/SocketStream/TcpServer。 |
| `zero.log` | **文件**: `../zero/log/log.h` |
| | **描述**: 企业级日志 — 7级/层级Logger/ANSI颜色/MDC/限流/RollingFile/Async(RingBuffer)/YAML配置。 |
| `zero.config` | **文件**: `../zero/config/config.h` |
| | **描述**: ConfigVar<T> + YAML 加载 + 变更回调(热加载) + 容器类型支持。 |
| `zero.thread` | **文件**: `../zero/thread/mutex.h` |
| | **描述**: SpinLock(指数退避) + Mutex(pthread) + RWMutex(pthread_rwlock) + Semaphore + Thread。 |
| `zero.bench` | **文件**: `../zero/examples/bench_echo.cc` |
| | **描述**: Echo 基准测试 — 35 万 QPS, P50=67μs, 9.2 MB/s。 |

### 2.7 RPC 框架层 (lrpc)

| Key | Value |
|-----|-------|
| `lrpc.wire` | **文件**: `../lrpc/docs/01-wire-protocol.md` |
| | **描述**: RPC 线协议设计 — 消息格式、帧界定、魔数/版本/序列化方式。 |
| `lrpc.codec` | **文件**: `../lrpc/docs/02-codec.md` |
| | **描述**: 编解码层 — 序列化/反序列化、压缩选项、类型系统映射。 |
| `lrpc.transport` | **文件**: `../lrpc/docs/03-transport.md` |
| | **描述**: 传输层 — TCP/Unix Socket、连接管理、心跳、重连策略。 |
| `lrpc.interceptor` | **文件**: `../lrpc/docs/04-interceptor.md` |
| | **描述**: 拦截器链 — 鉴权/限流/日志/监控等横切关注点的插件化。 |
| `lrpc.multiplex` | **文件**: `../lrpc/docs/05-multiplex.md` |
| | **描述**: 多路复用 — 单连接多请求复用、请求 ID 映射、背压控制。 |

---

## 三、快速导航

### 按受众导航

| 受众 | 推荐阅读顺序 |
|------|-------------|
| **面试官** | `lstl.retrospect` → `lstl.api` → `lstl.bench.*` |
| **新加入开发者** | `arch.overview` → `lstl.api` → `lstl.test.all` |
| **代码审查者** | `lstl.api` → 源代码 (header-only, 直接看 `include/`) |
| **性能评测** | `lstl.bench.*` → `lstl.retrospect` §5 |

### 按模块导航

| 模块 | 入口 Key | 状态 |
|------|---------|------|
| 架构设计 | `arch.overview` | 📝 设计阶段 |
| lstl 基础库 | `lstl.api` | ✅ 完成 |
| lstl 复盘 | `lstl.retrospect` | ✅ 完成 |
| 内存子系统 | `lstl.memory.*` | ✅ 完成 |
| 容器子系统 | `lstl.container.*` | ✅ 完成 |
| RPC 框架 | `lrpc.*` | 📝 设计阶段 |
| Zero 网络库 | `zero.*` | ✅ 完成 |
| lstl 复盘 | `lstl.retrospect` | ✅ 完成 |
| RPC 框架 | `lrpc.*` | 📝 设计阶段 |

### 按状态导航

| 状态 | Key 列表 |
|------|---------|
| ✅ 已完成 | `lstl.*` (全部 24 个 Keys) |
| 📝 设计阶段 | `arch.*`, `lrpc.*` |
| ⏳ 规划中 | 网络库实现、缓存服务器实现 |

---

## 四、依赖关系图

```
Architecture (ARCHITECTURE.md)
    │
    ├── lstl (基础库) ←────── 已实现 ✅
    │   ├── memory/     (9 文件)
    │   ├── container/  (30 文件)
    │   ├── tests/      (15 测试)
    │   └── bench/      (4 基准)
    │
    ├── Zero 网络库 ←──────── 已实现 ✅
    │   ├── fiber       (协程) → 34万 QPS / P50=67μs
    │   ├── scheduler   (M:N 调度) → work-stealing
    │   ├── reactor     (epoll) → EPOLLONESHOT + eventfd
    │   ├── hook        (syscall 劫持) → 透明异步化
    │   ├── net         (socket/stream/buffer) → 链式零拷贝
    │   ├── log         (异步日志) → 550万 QPS
    │   └── config      (RCU 配置) → YAML + 热加载
    │
    ├── lrpc (RPC 框架) ←───── 设计阶段
    │   └── docs/ (5 文档)
    │
    └── ledis (缓存服务器) ←── 原有项目
```

---

> **索引维护规则**: 新文档加入时，在本文件中添加一条 K-V 记录，Key 格式为 `模块.子模块.主题`，Value 包含文件路径 + 一句话描述 + 状态 + 关联 Key。
