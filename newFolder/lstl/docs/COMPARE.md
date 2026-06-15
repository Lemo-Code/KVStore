# lstl vs STL 真实并发对比报告

> 测试环境: Linux x86_64, GCC 13.2, -O2 -DNDEBUG, Intel i7-13700K
> 测试日期: 2025-06-15
> 说明: 所有数据为 3 次运行取中位数，单位微秒 (us)

---

## 一、Vector 对比

### 1.1 单线程性能 (n=500,000)

| 操作 | lstl::vector | std::vector | 对比 | 优胜 |
|------|-------------|-------------|------|------|
| push_back (with reserve) | 1,157 us | 947 us | -18% | std |
| push_back (no reserve) | 1,430 us | 1,093 us | -24% | std |
| random access read | 262 us | 271 us | +3% | ≈ |
| iteration | 767 us | 752 us | -2% | ≈ |
| insert at front (n=10000) | 3,365 us | 3,117 us | -7% | std |

**分析**: lstl::vector 在 push_back 上慢了 18-24%，主要原因是内存池的锁开销（每次扩容需要 `lock_guard<recursive_mutex>`）。std::vector 使用全局 `::operator new`，在单线程下没有锁竞争。随机访问和迭代性能几乎持平。

**并发读测试** (4 线程, 每个 500k 次随机读):

| 操作 | lstl::vector | std::vector | 对比 |
|------|-------------|-------------|------|
| 4-thread random read | 298 us | 285 us | -4% |

并发读性能持平——两者都是连续内存，cache-friendly。

---

## 二、Map 对比

### 2.1 单线程性能 (n=200,000)

| 操作 | lstl::map | std::map | 对比 | 优胜 |
|------|----------|----------|------|------|
| insert 200k | 30,340 us | 27,347 us | -10% | std |
| **find (hit)** | **6,726 us** | **7,543 us** | **+11%** | **lstl** |
| find (miss) | ~0 us | ~0 us | ≈ | ≈ |
| iteration | 730 us | 662 us | -9% | std |
| erase half | 4,783 us | 4,557 us | -5% | std |

**分析**: lstl::map 的 find 比 std::map 快 11%！这是因为 lstl 使用迭代式（非递归）RB 树遍历。Insert 慢 10% 是因为节点分配走内存池多了一层间接。如果改用直接 `new`，差距可以缩小。

**并发读测试** (4 线程, 每个 50k 次 find):

| 操作 | lstl::map | std::map | 对比 |
|------|----------|----------|------|
| 4-thread find (hit) | 8,102 us | 7,915 us | -2% |

并发读持平——两者都是纯读操作，无锁。

---

## 三、Memory Pool 对比 (核心优势)

### 3.1 单线程 (n=500,000)

| 场景 | lstl::default_alloc | malloc/free | 对比 | 优胜 |
|------|-------------------|-------------|------|------|
| 64B 单尺寸 | 13,068 us | 17,715 us | **+26%** | **lstl** |
| **混合尺寸** | **79,113 us** | **246,820 us** | **+68%** | **lstl** |
| 快速重复周期 (50k×10) | 5,283 us | 7,893 us | **+33%** | **lstl** |

**核心发现**: 混合尺寸场景 lstl 比 malloc 快 **3.1 倍**！这是因为内存池的 freelist 复用避免了 malloc 的内存碎片整理和系统调用开销。在快速重复分配场景（模拟高频网络 IO），lstl 快 33%。

### 3.2 并发 (4 线程, 每个 50k×10 rounds)

| 操作 | lstl::default_alloc | malloc/free | 对比 |
|------|-------------------|-------------|------|
| 4-thread 分配/释放 | 107,546 us | 25,783 us | -76% |

**⚠️ 并发瓶颈**: lstl 使用全局 `recursive_mutex`，4 线程竞争下反而比 malloc 慢 4 倍。glibc 的 ptmalloc2 使用 per-thread arena，有效地减少了锁竞争。

**这是设计取舍**: lstl 的设计哲学是 per-fiber 内存池（每个协程一个 `pool_single` 实例），而非全局线程安全池。在协程网络库中，每个 fiber 使用自己的 `pool_single`，完全没有锁开销。

### 3.3 无锁场景: pool_single vs malloc

| 操作 | pool_single (无锁) | malloc/free | 对比 |
|------|-------------------|-------------|------|
| 单线程 64B×100k | 892 us | 3,543 us | **+75%** |

**per-fiber 模式下，pool_single 比 malloc 快 4 倍。**

---

## 四、Unordered Map 对比

### 4.1 单线程 (n=200,000)

| 操作 | lstl::unordered_map | std::unordered_map | 对比 |
|------|-------------------|-------------------|------|
| insert 200k | 22,104 us | 18,339 us | -17% |
| find (hit) | 5,201 us | 4,883 us | -6% |
| erase half | 3,912 us | 3,567 us | -9% |

**分析**: lstl 的 hashtable 比 std 慢约 10-17%，主要是因为：
1. FNV-1a hash vs std::hash (通常更优化)
2. 质数桶表查找（除法取模）vs std 的 2 的幂掩码
3. 迭代器的 bucket_index() 是 O(n) 扫描

---

## 五、综合对比表

| 维度 | lstl 优势 | STL 优势 | 持平 |
|------|----------|---------|------|
| **内存分配 (单线程)** | ✅ +26~68% | | |
| **内存分配 (per-fiber)** | ✅ +75% | | |
| **Vector push_back** | | ✅ +18~24% | |
| **Vector 随机访问** | | | ✅ |
| **Map find** | ✅ +11% | | |
| **Map insert** | | ✅ +10% | |
| **Map 迭代** | | | ✅ |
| **并发分配 (全局锁)** | | ✅ +76% | |
| **并发读** | | | ✅ |

---

## 六、并发模型深度分析

### 6.1 为什么 lstl 全局锁慢？

```
lstl 全局池模型:
  Thread-0 ──┐
  Thread-1 ──┼── Lock ── pool_impl ── freelist[0..27]
  Thread-2 ──┤    ↑
  Thread-3 ──┘    全局竞争点

glibc ptmalloc2 模型:
  Thread-0 ── arena[0] ── freelist (per-thread, 无锁快速路径)
  Thread-1 ── arena[1] ── freelist
  Thread-2 ── arena[2] ── freelist
  Thread-3 ── arena[3] ── freelist
```

### 6.2 正确使用方式: per-fiber pool_single

```cpp
// ❌ 错误: 全局共享池 (有锁竞争)
lstl::vector<int> v;  // 内部用 default_alloc (全局锁)

// ✅ 正确: Per-fiber 独立池 (零锁竞争)
thread_local lstl::pool_single tls_pool(4096, 64);
// 每个 fiber/thread 有自己的池，完全无锁
```

### 6.3 后续优化方向

1. **Thread-local cache**: 参考 jemalloc tcache，每个线程缓存一批小对象
2. **Lock-free freelist**: 使用 CAS 原子操作替代 mutex
3. **Hashtable 桶索引缓存**: 消除 O(n) bucket_index() 扫描
4. **2的幂桶数 + 位掩码**: 替代质数桶的除法取模

---

## 七、结论

### lstl 适合的场景

| 场景 | 原因 |
|------|------|
| **单线程 / per-fiber 高吞吐** | pool_single 比 malloc 快 75% |
| **高频小块分配** | freelist O(1) 复用，零系统调用 |
| **自定义内存策略** | 可注入自定义分配器 |
| **嵌入式 / 无依赖** | header-only，零外部依赖 |
| **学习 / 教学** | 代码清晰，注释完整 |

### STL 更适合的场景

| 场景 | 原因 |
|------|------|
| **通用多线程** | ptmalloc per-thread arena |
| **极致性能** | 20 年优化，编译器内置优化路径 |
| **完整 API** | emplace, try_emplace, merge, extract... |
| **生产环境** | 久经考验，边缘情况已覆盖 |

### 一句话总结

> **lstl 不是 STL 的替代品，而是特定场景（单线程/fiber-local 高吞吐 + 自定义内存策略）的优化选择。在正确使用 per-fiber pool_single 的前提下，内存分配性能可达 STL 的 4 倍。**
