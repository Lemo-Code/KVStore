# 内存池设计方案（v1.0 — 从 0 到 1，独立组件）

> **状态：蓝图 + 已实现。** 多线程实现见 `module/kv_pool/`；单线程 LIGHT 池见 `module/lstl/memory/pool_single.h`。  
> **实现与优化总结（详细）**：[`../memory_pool_summary.md`](../memory_pool_summary.md)。  
> LSTL 核心仅提供 [`allocator_design.md`](allocator_design.md) 中的策略接口；本文描述 **客户端可选** 的池化组件演进蓝图。

> 目标：为 KVStore / Redis 类负载设计可演进的内存池，在 **C++11** 约束下达到「同线程接近 tcmalloc、多线程可扩展、可观测、可回收入驻内存」。

---

## 1. 背景与定位

| 维度 | 说明 |
|------|------|
| 产品 | KVStore 目录下的 **LSTL** 子项目，提供 `lstl::alloc` / `pool_alloc` |
| 负载 | 大量 **8~128B** 小对象（dictEntry、redisObject、SDS 头）、中等 query buffer、偶发大 value |
| 线程模型 | **每连接/每 IO 线程** 热路径 alloc+free；存在 **跨线程释放**（对象从逻辑线程交给 IO 线程回收） |
| 非目标 | 不替代系统 `malloc` 管理超大变长分配；不做通用 GC |

---

## 2. 设计原则

1. **热路径无锁**：同线程 alloc/free 仅触碰 `thread_local` tcache。
2. **共享状态分片**：多线程竞争通过 **多 arena + per-bin 分片** 摊薄，避免全局锁。
3. **跨线程释放批量化**：远程 free 进 **无锁 MPSC 队列**，由 owner arena 批量合并，禁止「每 free 一把 bin 锁」。
4. **元数据 O(1)**：通过 **span / chunk 页表** 反查 `arena_id + size_class`，禁止链表扫描。
5. **大小分级清晰**：Small（≤128B）/ Large（129B~阈值）/ Huge（超阈值直接系统分配）。
6. **可观测**：分层统计（tcache / arena / extent），支持 trim、purge、debug 模式。
7. **单线程可降级**：编译期关闭 `LSTL_THREAD_SAFE` 时退化为 **无锁 SGI 二级池**（当前 `pool_single` 语义）。

---

## 3. 需求规格

### 3.1 功能需求

| ID | 需求 |
|----|------|
| F1 | `allocate(n)` / `deallocate(p, n)`，`n==0` 返回 `nullptr` |
| F2 | 小块对齐到 **8B**，最大小块 **128B**（可配置 `LSTL_MAX_BYTES`） |
| F3 | 支持 **同线程** 高频 alloc/free |
| F4 | 支持 **跨线程 free**（正确回收，无 UAF） |
| F5 | 线程退出时 **自动 flush** tcache，不泄漏到进程级 |
| F6 | `trim_thread_cache()` / `purge_idle_memory()` 归还空闲内存 |
| F7 | OOM：异常或 handler 重试（与现 `oom.h` 一致） |
| F8 | 与 `simple_alloc<T, Alloc>`、`allocator` 适配 |

### 3.2 非功能需求

| ID | 指标（Release，x86_64 参考） |
|----|------------------------------|
| NF1 | 同线程 32B alloc/free **≥ malloc 的 80%**（Mops/s） |
| NF2 | 8 线程同尺寸 alloc/free **≥ malloc 的 50%**（首版），长期 **≥70%** |
| NF3 | 跨线程 free 批量后，远程路径 **不得** 在热路径上每次获取 arena bin 互斥锁 |
| NF4 | 元数据开销：小块 **≤ 12.5%**（128B 块用 16B 头为上限） |
| NF5 | 无死锁；析构顺序明确；C++11 无 `thread_local` 以外的 C++17 依赖 |

---

## 4. 总体架构

```
                    ┌─────────────────────────────────────┐
                    │  pool_alloc (public facade)         │
                    │  allocate / deallocate / stats      │
                    └─────────────────┬───────────────────┘
                                      │
              ┌───────────────────────┼───────────────────────┐
              │ n ≤ LSTL_MAX_BYTES    │  Large                │  Huge
              ▼                       ▼                       ▼
     ┌────────────────┐      ┌────────────────┐      ┌────────────────┐
     │ TCache (TLS)   │      │ LargeAllocator │      │ HugeAllocator  │
     │  per size idx  │      │  size + header │      │  malloc/mmap   │
     └───────┬────────┘      └────────────────┘      └────────────────┘
             │ miss / flush
             ▼
     ┌────────────────┐
     │ Arena[N]         │  ← 分片共享层
     │  bin[sc]         │     mutex 或 lock-free stack
     │  run cache       │
     │  remote queue    │  ← MPSC 跨线程 free
     └───────┬────────┘
             │ new run
             ▼
     ┌────────────────┐
     │ Extent / Span  │  ← 页对齐 backing store
     │  page map      │
     └────────────────┘
```

### 4.1 三层内存分类（对齐 jemalloc 术语）

| 级别 | 大小 | 后端 | 元数据 |
|------|------|------|--------|
| **Small** | 1~128B（8B 对齐 size class） | Arena run + tcache | Span 头在页首 |
| **Large** | 129B ~ `LSTL_LARGE_MAX`（如 32KB） | 独立 extent + 16B header | LargeHeader |
| **Huge** | > `LSTL_LARGE_MAX` | `malloc` / `mmap` | 可选不纳入池统计 |

---

## 5. 核心数据结构

### 5.1 Size class（小块）

```text
index = round_up(n, 8) / 8 - 1          // 0..15，共 16 档
block_size = (index + 1) * 8
```

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `LSTL_ALIGN` | 8 | 最小对齐 |
| `LSTL_MAX_BYTES` | 128 | 小块上限 |
| `tcache_max[idx]` | 8~32 | 每档 tcache 深度上限 |
| `batch_size[idx]` | 8~64 | 从 arena  refill 批量 |

**演进**：可增加 **pseudo size class**（如 24B、40B）减少内部碎片，首版保持 8B 阶梯。

### 5.2 Span / Chunk（页对齐 slab）

```cpp
// 每 extent 首页头部（16B，与现 ChunkHeader 兼容思路）
struct SpanHeader {
  uint32_t magic;           // 'SPAN'
  uint16_t arena_id;
  uint16_t size_class;      // 0xFFFF = large run 内嵌对象
  uint16_t page_count;
  uint16_t flags;           // MMAP | IN_USE | ...
  std::atomic<uint32_t> refcnt_or_live;  // purge 判定
};
// 用户区起始于 header 之后（+16B）
```

**页大小**：4KB（`LSTL_PAGE_SHIFT=12`）。小块 run 默认 **1 页** 或按 `batch * block_size * 2` 取整。

**反查**：`ptr → align_down(ptr, PAGE) → SpanHeader*`，校验 magic 与范围（**禁止** 全局 chunk 链表扫描）。

### 5.3 Arena

```cpp
struct ArenaBin {
  alignas(64) std::atomic<FreeNode*> head;  // lock-free stack（首版可用 mutex + 明确迁移路径）
  uint32_t push_count;                       // 统计
};

struct Arena {
  uint32_t id;
  ArenaBin bins[LSTL_FREELISTS];
  RunCache run_cache;           // 未切分的部分页
  RemoteFreeQueue remote;       // MPSC：{size_class, ptr} 批量节点
  char* bump; char* bump_end;   // 当前 run 游标（仅 arena 线程或 bin 锁内访问）
  size_t mapped_bytes;
};
```

**Arena 个数** `N`：

- `LSTL_ARENA_COUNT > 0` → 固定 N
- 否则 `N = min(4 * hardware_concurrency(), 64)`
- **线程绑定**：`arena_id = hash(tid) % N` 或 **首轮分配时 round-robin + 粘滞**（避免同进程内频繁换 arena）

### 5.4 TCache（thread_local）

```cpp
struct TCacheBin {
  FreeNode* head;
  uint16_t count;
  uint16_t max_count;
  uint16_t flush_defer;   // 延迟 flush 计数，减少 arena 争用
};

struct TCache {
  uint32_t arena_id;      // 粘滞绑定
  uint32_t slot_id;       // 注册表下标
  TCacheBin bins[16];
};
```

**线程生命周期**：

- 构造：绑定 arena，向全局 `TCacheRegistry` 注册槽位
- 析构：`flush_all()` → 归还 arena bin + drain remote 队列中本 arena 项

### 5.5 远程释放（跨线程）

```text
deallocate(p):
  owner = span_lookup(p).arena_id
  if owner == tls.arena_id:
      tcache_push(idx, p)                    // 快路径
  else:
      remote_queue[owner].push_batch(idx, p) // 无锁 MPSC
```

**Arena 侧合并**（慢路径，在 alloc miss 或 periodic flush 时）：

```text
drain_remote(arena):
  batch = remote.pop_all()
  merge into arena.bins[] or tcache refill list
```

**禁止**：当前 v0.4 在远程路径上对 `ArenaBin::mu` **每块**加锁。

### 5.6 Large / Huge

| 类型 | 实现 |
|------|------|
| Large | `LargeHeader { size, magic }` + `malloc`；单独 `bytes_in_use` 统计 |
| Huge | 直接 `malloc`/`mmap`，可选不经过 pool |

---

## 6. 关键路径算法

### 6.1 Small allocate（同线程）

```text
1. idx = size_class(n)
2. if p = tcache_pop(idx): return p
3. batch = arena_fetch_batch(arena_id, idx)   // 含 drain_remote
4. tcache_push_rest(batch.tail)
5. return batch.head
```

### 6.2 Small deallocate

```text
1. meta = span_lookup(p); 失败 → large_dealloc 或 abort(debug)
2. if meta.arena_id == tls.arena_id:
      tcache_push(idx, p)
      if tcache.count > max: arena_bin_push_list(idx, overflow)
   else:
      remote[meta.arena_id].enqueue(idx, p)
```

### 6.3 Arena refill（慢路径）

```text
1. drain_remote_locked_or_lockfree(arena)
2. pop arena.bin[idx]
3. if empty: slice from bump run
4. if bump empty: map_new_span(arena_id, idx)
5. return batch
```

### 6.4 Purge（归还 OS）

**条件**（全部满足才可 `munmap`/`free` span）：

- span 内 `live_count == 0`
- 不在任何 tcache / arena.bin / remote 队列中
- bump 游标不在该 span 内

**遍历**：仅遍历 **SpanRegistry**（按 arena 分桶的 span 列表），不扫描线程。

---

## 7. 并发与锁策略

| 组件 | 策略 | 说明 |
|------|------|------|
| TCache | 仅 TLS | 无共享 |
| Arena bin | `atomic` LIFO stack **或** per-bin `spin_mutex` | 缓存行对齐，减少 false sharing |
| Remote queue | **MPSC 无锁队列**（节点预分配或块内嵌入） | 跨线程主路径 |
| SpanRegistry | 读写：`mutex`（低频）/ 分 arena 锁 | 仅 map/unmap/purge |
| 统计 | `atomic<uint64_t>` relaxed | 可 `LSTL_POOL_DISABLE_STATS` 关闭 |

**8 线程性能差的根因（现 v0.4）**：远程 free / flush 与 refill 争抢 **同一 arena 的 bin mutex**。v1.0 必须用 **remote MPSC + 批量 merge** 解决。

---

## 8. 对外 API

```cpp
struct pool_alloc {
  static void* allocate(size_t n);
  static void  deallocate(void* p, size_t n);

  // 观测
  static uint64_t tcache_hit_alloc();
  static uint64_t tcache_hit_free();
  static uint64_t tcache_miss();
  static uint64_t remote_enqueue_count();
  static uint64_t remote_merge_count();
  static size_t   small_mapped_bytes();
  static size_t   large_bytes_in_use();
  static uint32_t arena_count();

  // 控制
  static void trim_thread_cache();      // 当前线程 tcache → arena
  static size_t purge_idle_memory();    // 全局 purge
  static void reset_stats();            // 测试用
};
```

**编译开关**：

| 宏 | 效果 |
|----|------|
| `LSTL_THREAD_SAFE` | 启用 arena+tcache；否则 `pool_single` |
| `LSTL_USE_MALLOC_ALLOC` | 全部走 malloc |
| `LSTL_ARENA_COUNT` | arena 个数 |
| `LSTL_CHUNK_USE_MMAP` | span 用 mmap |
| `LSTL_POOL_DISABLE_STATS` | 去掉原子统计 |

---

## 9. 目录与模块划分（建议重构）

```text
module/lstl/
  alloc.h                    # 入口 typedef
  config.h
  memory/
    pool.h                   # 门面（仅转发）
    pool_single.h            # 单线程 SGI（保留）
    size_class.h             # 尺寸与索引
    freelist.h               # 链表原语
    span.h / span.cc         # 页对齐 + 反查（替代 chunk 命名）
    span_registry.h / .cc    # 登记与 purge 遍历
    arena.h / arena.cc       # 共享层
    tcache.h / tcache.cc     # TLS 层
    remote_queue.h           # MPSC（header-only 或 .cc）
    large.h / large.cc
    malloc_alloc.h / oom.h
    purge.h / reset.h
  internal/detail/           # 迭代器等
```

**删除/合并**：`thread_heap.*`（兼容别名可保留一版）、`chunk_*` 更名为 `span_*` 统一语义。

---

## 10. 可观测性与调试

| 能力 | 实现 |
|------|------|
| 统计 | `PoolStats` 原子计数 + 按 arena 聚合（可选） |
| Debug 模式 | `LSTL_POOL_DEBUG`：double free 检测、magic、泄漏 trace |
| 压测 | `bench_alloc_compare`：malloc / stl / lstl，单/多线程 |
| 单测 | alloc、pool_v2、cross_thread、purge、stress |

**验收基准**（`BENCH_FOCUS=1`）：

- 单线程：`focus_small_32B` lstl/malloc ≥ **0.85**
- 8 线程：`mt_small_16B` lstl/malloc ≥ **0.50**（首版），≥ **0.70**（优化后）

---

## 11. 分阶段实施路线

### Phase 0 — 基线冻结（1 天）

- [ ] 锁定现有测试 + `bench_alloc_compare` 为回归基线
- [ ] 文档化现 v0.4 性能数据（单线程 OK / 8 线程差）

### Phase 1 — 元数据与反查（3~5 天）

- [ ] 实现 `SpanHeader` + `span_lookup`（页对齐反查）
- [ ] `SpanRegistry` 按 arena 分桶；去掉全局链表 O(n) 查找
- [ ] 单测：owner_arena 正确性、非法指针拒绝

### Phase 2 — Arena 核心（5~7 天）

- [ ] 重写 `arena_fetch_batch` / `arena_bin_push_list`
- [ ] 线程粘滞 arena 绑定
- [ ] 单线程对接 tcache，通过 `test_pool_v2`

### Phase 3 — 远程释放（5 天）

- [ ] `RemoteFreeQueue` MPSC
- [ ] `deallocate` 跨线程路径
- [ ] `test_cross_thread_deallocate` + 8 线程 bench 达标 NF2 首版

### Phase 4 — 回收与抛光（3~5 天）

- [ ] `purge_idle_memory` 基于 span live 计数
- [ ] `trim` / 线程析构 flush
- [ ] 统计 API、debug 模式
- [ ] 删除废弃代码（`thread_heap` 实现体、重复 freelist）

### Phase 5 — 可选进阶

- [ ] Per-CPU arena 亲和（`sched_getcpu`）
- [ ] 无锁 arena bin（Treiber stack + ABA tag）
- [ ] 大小类调优表（Redis 对象尺寸分布驱动）

---

## 12. 与当前实现的关系及迁移

| 现 v0.4 | v1.0 设计 |
|---------|-----------|
| `ChunkHeader.owner_arena` | → `SpanHeader.arena_id` + `size_class` |
| `ArenaBin::mutex` 全路径 | → bin 锁仅慢路径；远程走 MPSC |
| `chunk_registry` 全局链表 | → `SpanRegistry` per-arena |
| `g_tcaches[]` 悬垂风险 | → `TCacheRegistry` 注册/注销（已部分修复） |
| `pool_single` | **保留**，作为 `!LSTL_THREAD_SAFE` 后端 |

**迁移策略**：按 Phase 增量替换；`pool_alloc` 对外 API **不变**。

---

## 13. Redis / KV 场景映射

| 场景 | 推荐块大小 | 路径 | 注意 |
|------|------------|------|------|
| dictEntry / robj | 16~48B | Small + tcache | 同连接线程 alloc/free |
| SDS 头+短串 | 24~128B | Small | 注意对齐 |
| query buffer | 1~4KB | Large | 连接级复用 |
| 大 value | 64KB+ | Huge | 不走 small pool |
| IO 线程释放主线程对象 | 任意 small | Remote queue | 业务层可批量化更好 |
| 连接关闭 | — | `trim` + purge | 释放囤积 |

---

## 14. 风险与决策记录

| 风险 | 缓解 |
|------|------|
| MPSC 实现复杂 | 首版可用带锁 batch 队列（每 arena 一条），每批 ≥32 节点 |
| 无锁栈 ABA | 使用 tagged pointer 或暂用 mutex |
| 碎片 | 限制 tcache_max；定期 purge；按 workload 调 size class |
| C++11 无线程局部 CPU id | 用 `hash<thread::id>` 粘滞 arena |

---

## 15. 总结

v1.0 设计的核心是 **三层分离**：

1. **TCache** — 同线程零锁；
2. **Arena** — 分片共享 + **远程 MPSC**；
3. **Span** — O(1) 元数据与可回收页。

按本文 Phase 1→4 实施，可在保持 `lstl::alloc` API 稳定的前提下，系统性解决当前 **8 线程小块远低于 malloc** 的问题，并建立可长期演进的 KV 专用内存池基座。
