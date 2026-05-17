# KVStore 两种内存池：完整实现与优化总结（非常详细版）

> **文档版本**：与当前 `module/lstl`、`module/kv_pool` 源码一致  
> **相关文档**：[`lstl/spatial_allocator_summary.md`](lstl/spatial_allocator_summary.md)（空间配置模块）、[`lstl/allocator_design.md`](lstl/allocator_design.md)、[`lstl/memory_pool_design.md`](lstl/memory_pool_design.md)

---

## 目录

1. [文档目的与阅读指引](#1-文档目的与阅读指引)  
2. [为什么需要两种池](#2-为什么需要两种池)  
3. [理论基础：SGI 二级空间配置器](#3-理论基础sgi-二级空间配置器)  
4. [实现一：lstl::pool（单线程）](#4-实现一lstlpool单线程)  
5. [实现二：kv::pool（多线程）](#5-实现二kvpool多线程)  
6. [早期架构与问题清单](#6-早期架构与问题清单)  
7. [优化历程（分阶段详解）](#7-优化历程分阶段详解)  
8. [压测方法与结果](#8-压测方法与结果)  
9. [热路径 vs 慢路径开销分析](#9-热路径-vs-慢路径开销分析)  
10. [线程安全模型与使用约束](#10-线程安全模型与使用约束)  
11. [配置宏与调参](#11-配置宏与调参)  
12. [源码地图与测试](#12-源码地图与测试)  
13. [与 LSTL 集成](#13-与-lstl-集成)  
14. [选型决策树](#14-选型决策树)  
15. [附录：时序与对比表](#15-附录时序与对比表)

---

## 1. 文档目的与阅读指引

本文档回答四个问题：

1. **两种池分别是什么、放在哪、解决什么问题？**  
2. **各自内部如何实现（数据结构、分配/释放路径）？**  
3. **kv::pool 经历了哪些优化、每步解决什么、效果如何？**  
4. **在生产中如何选型、如何接入 LSTL？**

| 读者目标 | 建议阅读章节 |
|----------|--------------|
| 快速选型 | §2、§14 |
| 理解 SGI 二级池 | §3 |
| 单线程极致性能 | §4 |
| 多线程 / Redis 跨线程 free | §5、§10 |
| 性能调优与排障 | §7、§8、§9、§11 |

---

## 2. 为什么需要两种池

### 2.1 矛盾本质

| 需求 | 单线程最优解 | 多线程最优解 |
|------|--------------|--------------|
| alloc/free 热路径 | 全局静态池、零检查、无元数据 | 每线程本地池、无锁 |
| 跨线程归还 | 不需要 | 必须路由到归属线程/Arena |
| 代码复杂度 | 极低（≈ SGI 200 行核心） | 需 TLS、中央结构、MPSC |

**同一条 API** 若既要 SGI 级单线程零开销，又要 Redis 式跨线程 free，必然在热路径上保留「是否为本地块」的判断与块归属元数据——这是 **架构取舍**，不是实现粗糙。

### 2.2 仓库内的分工

```
module/lstl/memory/pool_single.h   →  lstl::pool_alloc_t   →  单线程、LSTL 默认 alloc
module/kv_pool/                    →  kv::pool_alloc        →  多线程、独立模块注入
```

| 维度 | lstl::pool | kv::pool |
|------|------------|----------|
| 路径 | `module/lstl/memory/pool_single.h` | `module/kv_pool/` |
| 线程 | 单线程 only | 多线程 + 跨线程 free |
| 全局状态 | `static PoolSingle` | `thread_local` + 多 Arena |
| 对标 | SGI `__default_alloc` LIGHT | Redis：主线程 free / worker alloc |
| 单线程性能 | ~490–520 Mops/s | ~470–500 Mops/s（≈97% lstl） |
| 8 线程同线程 | 不可用 | ~1100–1200 Mops/s 聚合 |

---

## 3. 理论基础：SGI 二级空间配置器

### 3.1 二级池在 STL 中的位置

```
应用 allocate(n 字节)
    → 一级配置器 malloc_alloc（原样 malloc/free）
    → 二级配置器 default_alloc（小块池化 + 大块委托一级）
```

本仓库两种池都是 **二级配置器** 实现：小块池化，大块与超大块委托 `malloc`。

### 3.2 小块 size class（两者相同）

- 对齐：`ALIGN = 8`  
- 上限：`MAX_BYTES = 128`  
- 档位数：`FREELISTS = 16`  
- 索引：`index = align_up(bytes) / 8 - 1`  

| 请求（示例） | align_up | index | 实际 slot |
|--------------|----------|-------|-----------|
| 1–8 B       | 8        | 0     | 8 B       |
| 9–16 B      | 16       | 1     | 16 B      |
| 17–24 B     | 24       | 2     | 24 B      |
| …           | …        | …     | …         |
| 121–128 B   | 128      | 15    | 128 B     |

### 3.3 三级分流

```
                    allocate(n)
                         │
         ┌───────────────┼───────────────┐
         ▼               ▼               ▼
    n ≤ 128B      128B < n ≤ 32KB    n > 32KB
    二级池         large_*            malloc_alloc
    freelist+bump  带 LargeHeader     直接 malloc
```

### 3.4 侵入式 freelist

```cpp
union FreeNode { FreeNode* next; };
```

- **分配**：`pop` 链表头，返回的指针即用户可用内存。  
- **释放**：`push` 把头指针设为该块，**不单独存 size**（调用方必须传对 `n` 以算 index）。  
- **无 per-object 头**：释放后 `next` 覆盖原用户数据前几个字节。

### 3.5 chunk_alloc 五步（SGI 经典）

1. 若 `bump_end - bump_start >= need`：从 bump 切出，返回。  
2. 若剩余 ≥ 一块但 < need：减少 `nobjs`，切出可用块数。  
3. 若 bump 有碎片：按碎片大小挂到对应 freelist，清空 bump。  
4. 计算 `bytes_to_get = 2 * total_bytes + heap_size >> 4`，向系统要新内存。  
5. 设置新 bump 区间，递归调用 `chunk_alloc`。

### 3.6 refill

中央 freelist 为空时，从 chunk 切 `REFILL_BATCH` 块（lstl 默认 20，kv 默认 128），第一块返回调用方，其余串成链表挂回 freelist。

---

## 4. 实现一：lstl::pool（单线程）

### 4.1 模块结构

```
pool_alloc_t（memory/pool.h）     门面：静态方法
        │
        ▼
PoolSingle（memory/pool_single.h）  唯一实现体
        │
        ├── free_list[16]
        ├── bump_start, bump_end
        └── heap_size
```

### 4.2 allocate 路径（小块）

```text
pool_alloc_t::allocate(n)
  → is_large? → large_allocate
  → !is_small? → malloc_alloc
  → PoolSingle::instance()
  → bytes = align_up(n), index = size_class_index(bytes)
  → node = freelist_pop(&free_list[index])
  → if (!node) node = refill(bytes, index)
  → return node
```

**热路径指令级特征**：一次 TLS 无（全局静态引用）、一次 pop、无分支检查块归属。

### 4.3 deallocate 路径（小块）

```text
  → bytes, index
  → freelist_push(&free_list[index], p)
```

**零元数据**：不检查指针来源、不更新 span、不加锁。

### 4.4 LIGHT 模式 chunk 获取（默认）

```cpp
raw = malloc(bytes_to_get);
bump_start = raw;
bump_end = raw + bytes_to_get;
```

- 无 `posix_memalign`、无 4KB 对齐要求。  
- 无 `ChunkHeader` / `SpanHeader` 在热路径。

### 4.5 重型 Span 模式（`LSTL_POOL_LIGHT=0`）

- chunk 通过 `posix_memalign(4096)` 或 malloc+对齐获得。  
- 每 span 有 `SpanHeader`：magic、size_class、block_count、free_count。  
- free 时更新 span 计数；`purge_idle_memory` 可归还空闲 span。  
- **更慢**，适合需要内存追踪与 purge 的场景。

### 4.6 性能与限制

| 指标 | 典型值（Release，median×5） |
|------|------------------------------|
| 8–128B alloc/free 单线程 | **490–520 Mops/s** |
| vs malloc 单线程 | **约 2.5–2.7× 吞吐** |
| 多线程并发 | **不支持**（数据竞争） |

### 4.7 适用场景

- LSTL 默认 `alloc`（未定义 `LSTL_USE_MALLOC_ALLOC`）  
- 纯单线程业务、压测对标 SGI  
- 不需要跨线程 `deallocate`

---

## 5. 实现二：kv::pool（多线程）

### 5.1 设计目标

1. **同线程** alloc/free 尽量与 lstl/SGI 相同（freelist + bump）。  
2. **跨线程** free 正确且无需全局锁（per-thread 热路径无锁）。  
3. **可观测**：`remote_enqueue_count`、`arena_count`、`trim_thread_cache`。

### 5.2 总体架构图

```text
                    kv::pool_alloc::allocate / deallocate
                              (内联于 pool.h)
                                    │
                    thread_local TlsFastCtx
                    (继承 ThreadLocalPool)
                                    │
         ┌──────────────────────────┼──────────────────────────┐
         │ HOT: 同线程               │ COLD: 跨线程                │
         ▼                          ▼                          │
  freelist_pop/push            owns_pointer == false             │
  bump / chunk_alloc           find_chunk_global(p)              │
                               thread_tag != self                │
                               remote_enqueue → MPSC            │
         │                          │                          │
         └──────────────────────────┼──────────────────────────┘
                                    ▼
                         Arena[arena_id]  (shard)
                         ├── AtomicFreelist bins_[16]
                         ├── RemoteFreeQueue
                         └── atomic chunk_head_ → ChunkHeader 链表
```

### 5.3 文件职责表

| 文件 | 职责 |
|------|------|
| `pool.h` | 对外 API；**内联** alloc/free 热路径 |
| `pool_detail.h` | `PoolState`、TLS、`remote_enqueue`、`find_chunk_global` |
| `thread_pool.h` | `ThreadLocalPool`：freelist、bump、chunk、flush |
| `arena.h` | 中央 bin、MPSC drain、chunk 注册/查找 |
| `chunk.h` | `ChunkHeader`、区间判断 |
| `atomic_freelist.h` | 无锁链表栈（CAS） |
| `remote_queue.h` | `RemoteBatch`、`RemoteFreeQueue` |
| `freelist.h` / `size_class.h` / `large.h` | 与 lstl 同构工具 |
| `config.h` | 宏配置 |

### 5.4 ThreadLocalPool 内存布局

**设计决策**：`free_list_[16]` 置于结构体**首字段**，与 `PoolSingle::free_list` 布局一致，便于编译器生成相近的 load/store 偏移。

```text
ThreadLocalPool / TlsFastCtx 首部：
  free_list_[16]     ← 热路径直接 &pool.free_list_[index]
  bump_start_, bump_end_, heap_size_
  has_local_range_, local_min_, local_max_
  arena_, arena_id_, thread_tag_, ...
TlsFastCtx 额外：
  remote_partials[64]   ← 每 Arena 一批未提交的跨线程指针
```

### 5.5 Chunk 内存布局（块尾元数据）

```text
malloc(raw_bytes) 其中 raw_bytes = bytes_to_get + sizeof(ChunkHeader)

低地址 ┌──────────────────────────────────────┐ 高地址
       │  user area (bump 在此分配)              │ ChunkHeader │
       │  user_start = raw                      │ magic       │
       │  user_end = raw + bytes_to_get         │ arena_id    │
       └──────────────────────────────────────┘ thread_tag  │
                                                next_registry (延迟注册链表)
```

**对比早期方案**：

| 方案 | 问题 |
|------|------|
| 块头 4B 标签 + 内部 size | 16B 请求变 32B slot；freelist `next` 与标签冲突 |
| 头部 ChunkHeader | 用户指针偏移，与 SGI 不一致 |
| **块尾 Header（现方案）** | 用户指针 = bump 起点；仅多 16B/chunk 尾部 |

### 5.6 allocate 热路径（逐步）

```text
1. large / !small 分流（同 lstl）
2. pool = tls_pool()                    // thread_local，构造时已 init
3. index = size_class_index(align_up(n))
4. node = freelist_pop(&pool.free_list_[index])
5. if (!node) node = pool.refill_on_miss(index, bytes)
       refill:
         a. batch = arena->withdraw_bin(index)  // 原子 pop_all 中央链
         b. if (batch) 拆链，adopt 扩展 local_min/max
         c. else chunk_alloc (malloc + 尾部 Header，延迟 register)
6. return node
```

### 5.7 deallocate 双路径（逐步）

**默认**（`KV_POOL_FAST_SAME_THREAD=0`）：

```text
1. index = size_class_index(align_up(n))
2. if (pool.owns_pointer(p))           // local_min <= p < local_max
       → pool.push(index, node)       // 热路径：无锁
3. else header = find_chunk_global(p) // 遍历所有 Arena 的 chunk 链表
4. if (header && header->thread_tag != pool.thread_tag())
       → remote_enqueue(arena_id, index, p)
5. else pool.push(index, node)        // 本线程块但从中央 adopt 来的
```

**FAST 模式**：跳过 2–4，直接 push（**仅当保证同线程 free**）。

### 5.8 remote_enqueue 与 MPSC

```text
remote_enqueue(arena_id, size_class, ptr):
  batch = tls.remote_partials[arena_id]
  if (!batch) batch = malloc(sizeof RemoteBatch)
  batch->items[count++] = { ptr, size_class }
  remote_enqueues++
  if (count >= KV_POOL_REMOTE_BATCH)    // 默认 64
    arena[arena_id].remote().push(batch)  // MPSC 头插 CAS
    batch = nullptr
```

**消费**：`Arena::withdraw_bin` 或 `flush_remote` 时 `drain_remote`：

```text
drain_all MPSC 链表
  for each item in batch:
    bins_[size_class].push_one(ptr)   // 原子 CAS 入中央 bin
  free(batch)
```

### 5.9 AtomicFreelist 语义

| 操作 | 实现 | 用途 |
|------|------|------|
| `pop_all` | `exchange(0, acquire)` | 线程取走整条中央链，本地拆分 |
| `push_all` | CAS 把链挂到 head | 线程 flush 本地 freelist 到中央 |
| `push_one` | CAS 单节点入栈 | drain_remote 逐块归还 |

### 5.10 Arena 分片

```text
thread_tag = 每线程唯一递增 ID (thread_local 首次分配)
arena_id = thread_tag % arena_count
arena_count = KV_POOL_ARENA_COUNT 或 default: hw_concurrency * 4 (max 64)
```

减少不同线程集中竞争同一 Arena 的中央 bin 与 MPSC 头。

### 5.11 延迟 chunk 注册

**热路径**（`chunk_alloc`）：只把 `ChunkHeader` 挂到 `pending_registry_head_`，**不**调用 `arena->register_chunk`（无 CAS）。

**冷路径**（`flush_all` / `trim_thread_cache` / 线程析构）：

```text
register_pending_chunks():
  while (pending) { arena->register_chunk(h); h = h->next_registry; }
```

减少新 chunk 分配时的原子竞争。

### 5.12 trim_thread_cache

每线程应在线程退出前或阶段结束时调用：

```text
flush_remote_partials()     // 未满的 RemoteBatch 推入 MPSC
arena->flush_remote()       // drain MPSC → bins_
pool.flush_all()            // 注册 pending chunk；本地 freelist → 中央
```

---

## 6. 早期架构与问题清单

### 6.1 早期 kv::pool 架构（已废弃）

```text
多线程共享 Arena
  ├── mutex 保护 bump          ← 热路径加锁
  ├── 4KB posix_memalign span
  ├── 块头 4B 标签
  └── find_chunk 仅当前 arena
```

### 6.2 Bug 与性能问题对照表

| # | 问题 | 后果 | 修复方向 |
|---|------|------|----------|
| 1 | Arena 共享 bump + mutex | MT 吞吐差、锁竞争 | 每线程 TLS bump |
| 2 | 4KB 强对齐 + SpanHeader | 分配慢、元数据大 | malloc chunk + 块尾 Header |
| 3 | 块头标签导致内部 32B class | index 越界、浪费 | 取消标签，slot=SGI |
| 4 | 标签与 freelist `next` 共用 | 路由/计数错误 | 块尾 Header，不用块内 tag |
| 5 | find_chunk 只查本 Arena | 跨 shard 误判 | find_chunk_global |
| 6 | LOCAL_MAX=2048 | 热路径 flush | LOCAL_MAX=0 |
| 7 | pool.cc 非内联 | ST ~0.47× SGI | 热路径迁入 pool.h |
| 8 | 每轮 ensure_tls 分支 | 多余开销 | TLS 构造时 init |

---

## 7. 优化历程（分阶段详解）

### 7.1 性能演进总表

| 阶段 | 关键改动 | kv ST (16B) | kv MT 8T (16B) | 跨线程测试 |
|------|----------|-------------|----------------|------------|
| 0 早期 | 共享 bump+锁、span、标签 | ~250 Mops/s | ~600 量级 | 不稳定 |
| 1 双路径 | TLS+Arena+MPSC、SGI slot | ~250–295 | ~620–790 | PASS |
| 2 无 LOCAL_MAX | 去掉 local_count flush | ~294 | ~760–790 | PASS |
| 3 内联+TLS | pool.h 内联、Tls 构造 init | **~470** | **~1400+** | PASS |
| 4 块尾+延迟注册 | Header 尾部、pending 链 | **~475–499** | **~1160** | PASS |
| 5 FAST_SAME_THREAD | 跳过 owns_pointer | ~295（无提升） | — | 不可用 MT |

*压测环境：Release，median×5，具体数值随 CPU 略有浮动。*

### 阶段 0：基线问题

- 对标对象错误：曾与裸 malloc 比，而非 **SGI 二级池**。  
- 单线程只有 SGI 的 **~0.39–0.47×**。

### 阶段 1：双路径架构重构

**改动**：

- `ThreadLocalPool`：每线程 freelist + bump。  
- `Arena`：仅 `withdraw_bin` / `deposit_bin` / `remote`。  
- 跨线程：`remote_enqueue` + MPSC。  
- slot 与 SGI 一致。

**效果**：

- MT 8 线程超过 SGI **单线程** 吞吐。  
- ST 仍约 **50% SGI**（尚未内联）。

### 阶段 2：去掉 LOCAL_MAX

**改动**：

- `KV_POOL_LOCAL_MAX=0`。  
- `push` 不再计数、不再触发 `flush_bin`。

**效果**：

- 对齐 SGI「本地 freelist 无上限」；ST 小幅提升。

### 阶段 3：内联与 TLS 初始化（最关键）

**改动**：

- 删除 `ensure_tls()` 每轮分支。  
- `TlsFastCtx` 在 `thread_local` 构造时 `init(arena_id, tag, arena)`。  
- `allocate`/`deallocate` **全部迁入 `pool.h` 内联**。  
- `freelist_pop(&pool.free_list_[index])` 与 lstl 同写法。

**原因**：

- lstl 为 header-only，编译器将 pop/push **展开进测试循环**。  
- kv 曾在 `pool.cc` 中，每轮 4 次函数调用边界，ST 腰斩。

**效果**：

- ST：**~250 → ~470 Mops/s**（≈ **96% lstl**）。  
- MT 8T：**~760 → ~1400+ Mops/s**。

### 阶段 4：元数据与注册优化

**改动**：

- ChunkHeader 移到 **chunk 尾部**；`user_start = raw`。  
- **延迟** `register_chunk` 到 flush/析构。  
- `local_min` / `local_max` 区间快速判断。  
- `__builtin_expect(owns_pointer, 1)`。

**效果**：

- ST 稳定在 lstl 的 **94–98%**。  
- `test_cross_thread`（10000 次跨线程 free）稳定 PASS。

### 阶段 5：FAST_SAME_THREAD 实验

**改动**：`KV_POOL_FAST_SAME_THREAD=1` 跳过 owns_pointer 与 find_chunk。

**结论**：ST **几乎无提升**（~295 vs ~295）→ 瓶颈已不在检查，而在 TLS、尾部 Header、编译器内联质量等固定成本。

---

## 8. 压测方法与结果

### 8.1 工具

- 可执行文件：`bin/kv_pool/bench_kv_pool`（需 `-DLSTL_BUILD_BENCH=ON`）  
- 对比项：`malloc/free`、`lstl::pool_alloc_t`、`kv::pool_alloc`  
- 方法：每项 **median of 5 runs**；`volatile sink` 防止分配被删。

### 8.2 单线程用例

| 大小 | 迭代次数 |
|------|----------|
| 8B   | 2,000,000 |
| 16B  | 2,000,000 |
| 32B  | 1,500,000 |
| 64B  | 1,500,000 |
| 128B | 1,000,000 |

### 8.3 多线程用例

- 8 线程，每线程 200,000 轮 alloc/free（同 size），**同线程归还**。  
- 总 ops = 1,600,000 / 档位。

### 8.4 Redis 模式用例

- worker 线程 `allocate` 50 万个 32B，主线程统一 `deallocate`。  
- 统计 `remote_enqueue_count`（应等于 500000）。

### 8.5 结果汇总（参考值）

**单线程 Mops/s**

| 大小 | malloc | lstl | kv | kv/lstl |
|------|--------|------|-----|---------|
| 8B   | ~190   | ~490 | ~475 | 0.97× |
| 16B  | ~191   | ~510 | ~499 | 0.98× |
| 32B  | ~197   | ~514 | ~489 | 0.95× |
| 64B  | ~197   | ~512 | ~488 | 0.95× |
| 128B | ~196   | ~517 | ~486 | 0.94× |

**8 线程同线程 Mops/s**

| 大小 | malloc | kv | kv/malloc |
|------|--------|-----|-----------|
| 16B  | ~520   | ~1160 | 2.2× |
| 32B  | ~570   | ~1160 | 2.0× |
| 64B  | ~525   | ~1180 | 2.3× |

**对比 SGI 单线程（~500 Mops/s）**：kv 8 线程同线程约 **2.3× SGI 单线程**。

---

## 9. 热路径 vs 慢路径开销分析

### 9.1 lstl 单线程热路径（基准）

| 步骤 | 成本 |
|------|------|
| `PoolSingle::instance()` | 全局静态引用 |
| `freelist_pop` | 1 次 load + store |
| `freelist_push` | 1 次 load + store |
| 新 chunk | malloc + bump 设置 |

### 9.2 kv 同线程额外成本（仍保持 ~97% lstl）

| 步骤 | 相对 lstl |
|------|-----------|
| `tls_pool()` | thread_local 访问（vs 全局 static） |
| `owns_pointer` | 2 次指针比较 + 分支 |
| 新 chunk | +16B Header、pending 链（无热路径 CAS） |
| refill 中央 | 偶尔 `AtomicFreelist::pop_all` |

### 9.3 kv 跨线程慢路径

| 步骤 | 成本 |
|------|------|
| `find_chunk_global` | O(arena × chunks) 链表扫描 |
| `remote_enqueue` | 写 batch + 可能 MPSC CAS |
| 原线程 `drain_remote` | 批量 CAS 入 bin |

Redis 模式吞吐 **10–20 Mops/s** 属正常（慢路径主导）。

---

## 10. 线程安全模型与使用约束

### 10.1 安全保证

| 场景 | lstl::pool | kv::pool |
|------|------------|----------|
| 多线程同时 alloc/free 同一池 | ❌ UB | ✅（TLS 隔离） |
| 线程 A alloc，B free | ❌ | ✅（remote 路径） |
| 大块 malloc | ✅（libc） | ✅ |
| double free | ❌ UB | ❌ UB |

### 10.2 使用约束

1. **lstl**：仅单线程调用 `pool_alloc_t`。  
2. **kv**：每使用线程应 `trim_thread_cache()`（退出前）。  
3. **勿**在 MT 生产环境设 `KV_POOL_FAST_SAME_THREAD=1`。  
4. `deallocate(p, n)` 的 `n` 必须与 `allocate` 一致（决定 size class）。

---

## 11. 配置宏与调参

### 11.1 lstl（`module/lstl/config.h`）

| 宏 | 默认 | 说明 |
|----|------|------|
| `LSTL_POOL_LIGHT` | 1 | LIGHT vs Span |
| `LSTL_USE_MALLOC_ALLOC` | 0 | 1=全局 malloc |
| `LSTL_POOL_MAX_BYTES` | 128 | 小块上限 |
| `LSTL_POOL_REFILL_BATCH` | 20 | refill 块数 |
| `LSTL_LARGE_MAX` | 32768 | 大块上限 |

### 11.2 kv_pool（`module/kv_pool/config.h`）

| 宏 | 默认 | 说明 |
|----|------|------|
| `KV_POOL_REFILL_BATCH` | 128 | 中央 refill |
| `KV_POOL_LOCAL_MAX` | 0 | 0=无本地上限 |
| `KV_POOL_REMOTE_BATCH` | 64 | 跨线程批大小 |
| `KV_POOL_FAST_SAME_THREAD` | 0 | 调试/纯 ST 可用 |
| `KV_POOL_DEFER_CHUNK_REGISTER` | 1 | 延迟注册 |
| `KV_POOL_ARENA_COUNT` | 0 | 0=自动 hw×4 |

### 11.3 调参建议

| 目标 | 建议 |
|------|------|
| 提高 MT 中央供给 | 略增 `REFILL_BATCH` |
| 降低跨线程 CAS 频率 | 增 `REMOTE_BATCH` |
| 减少 Arena 竞争 | 显式设 `ARENA_COUNT` 为 2× 线程数 |
| 极致 ST、无跨线程 | 用 lstl，不必调 kv |

---

## 12. 源码地图与测试

### 12.1 目录

```text
module/lstl/memory/
  pool.h              pool_alloc_t 门面
  pool_single.h       PoolSingle 实现
  freelist.h          size_class.h  large.h

module/kv_pool/
  kv_pool.h           #include "memory/pool.h"
  config.h
  memory/
    pool.h            内联 API
    pool_detail.h     TLS / PoolState / remote
    thread_pool.h     ThreadLocalPool
    arena.h           Arena
    chunk.h           ChunkHeader
    atomic_freelist.h remote_queue.h
    pool.cc           占位 TU（链接用）

tests/memory/test_pool_single.cc
tests/kv_pool/test_pool_mt.cc
tests/kv_pool/test_cross_thread.cc
tests/kv_pool/benchmarks/bench_kv_pool.cc
```

### 12.2 测试矩阵

| 测试 | 池 | 验证 |
|------|-----|------|
| test_pool_single | lstl | 小块/大块/统计 |
| test_pool_mt | kv | 8 线程同线程 |
| test_cross_thread | kv | 跨线程 free + remote>0 |
| bench_kv_pool | 三者 | 性能对比 |

### 12.3 构建命令

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
  -DLSTL_BUILD_KV_POOL=ON \
  -DLSTL_BUILD_BENCH=ON
make -j$(nproc)
ctest -R '^(memory|kv_pool)\.' --output-on-failure
./bin/kv_pool/bench_kv_pool
```

---

## 13. 与 LSTL 集成

```cpp
// 全局替换（需 kv::pool_alloc 满足静态 allocate/deallocate 字节接口）
#define LSTL_USER_ALLOC kv::pool_alloc
#include "alloc.h"

// 局部使用
lstl::simple_alloc<MyObj, kv::pool_alloc> a;
MyObj* p = a.allocate(10);
```

**注意**：`kv::pool_alloc` 与 `lstl::pool_alloc_t` 是不同类型；多线程场景用 kv，单线程极致用 lstl。

---

## 14. 选型决策树

```text
是否多线程调用 alloc/free？
├─ 否 → 是否要用 LSTL 默认 alloc？
│        ├─ 是 → lstl::pool_alloc_t（最快）
│        └─ 否 → LSTL_USE_MALLOC_ALLOC 或自定义
└─ 是 → 是否存在「A 线程 alloc，B 线程 free」？
         ├─ 否 → kv::pool（每线程热路径无锁）
         └─ 是 → kv::pool + trim_thread_cache（Redis 模式）
```

**混合架构（推荐）**：

- 主线程热路径对象：lstl 或 kv 同线程路径。  
- 异步 worker 归还主线程：kv + remote_enqueue。  
- 不必强行「全进程一个池」。

---

## 15. 附录：时序与对比表

### 15.1 跨线程 free 时序（Redis 模式）

```text
Worker                           Main
  │                                │
  ├─ allocate(32) → TLS pool       │
  ├─ trim_thread_cache (optional)  │
  │                                ├─ deallocate(p, 32)
  │                                ├─ owns_pointer? NO
  │                                ├─ find_chunk_global
  │                                ├─ remote_enqueue
  │                                │     → remote_partials[arena]
  │                                │     → MPSC push (batch full)
  ├─ allocate (later)              │
  ├─ withdraw_bin                  │
  │     └─ drain_remote → bins_    │
  └─ pop from local freelist       │
```

### 15.2 lstl vs kv 特性矩阵

| 特性 | lstl::pool | kv::pool |
|------|------------|----------|
| SGI 二级小块 | ✅ | ✅ |
| 单线程性能 | ★★★★★ | ★★★★☆ |
| 多线程同线程 | ❌ | ★★★★★ |
| 跨线程 free | ❌ | ★★★★☆ |
| 实现复杂度 | 低 | 中高 |
| 代码位置 | lstl 内 | 独立模块 |
| 默认 LSTL alloc | 是 | 需 USER_ALLOC |

### 15.3 优化原则回顾

1. **热路径=S GI**：freelist + bump + malloc chunk，无锁。  
2. **慢路径隔离**：跨线程才走 find_chunk + MPSC。  
3. **编译器友好**：内联 + `free_list_` 首字段 + 少分支。  
4. **元数据最便宜**：块尾 Header、延迟 CAS 注册。  
5. **测量对标 SGI**：不是只对 malloc。

---

## 文档修订说明

| 版本 | 内容 |
|------|------|
| 初版 | 两种池总览、优化阶段、压测表 |
| **非常详细版（本文）** | 完整路径逐步、早期问题表、演进数据、时序、选型树、源码地图 |

**一句话**：`lstl::pool` 是 SGI 单线程终极形态；`kv::pool` 在其热路径上增加 TLS 与块归属，用 MPSC 解决跨线程归还；经去锁、内联、无 LOCAL_MAX、块尾 Header 等优化后，**ST≈97% lstl、MT≈2.3× malloc**，且 Redis 跨线程场景可用。
