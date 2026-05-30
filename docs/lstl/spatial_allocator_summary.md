# LSTL 空间配置模块 — 详细设计总结

> **模块路径**：`module/lstl/`  
> **权威设计**：[`allocator_design.md`](allocator_design.md)（v2.0 边界与接口）  
> **内存池（独立）**：[`../memory_pool_summary.md`](../memory_pool_summary.md)（lstl 单线程池 + kv 多线程池）  
> **池化蓝图**：[`memory_pool_design.md`](memory_pool_design.md)

---

## 目录

1. [模块定位与目标](#1-模块定位与目标)  
2. [模块边界：包含与不包含](#2-模块边界包含与不包含)  
3. [设计原则](#3-设计原则)  
4. [分层架构](#4-分层架构)  
5. [目录与文件职责](#5-目录与文件职责)  
6. [分配策略层](#6-分配策略层)  
7. [封装层：simple_alloc 与 allocator](#7-封装层simple_alloc-与-allocator)  
8. [对象构造与析构](#8-对象构造与析构)  
9. [未初始化内存算法](#9-未初始化内存算法)  
10. [类型特征与迭代器分派](#10-类型特征与迭代器分派)  
11. [临时缓冲](#11-临时缓冲)  
12. [单线程二级池（模块内可选后端）](#12-单线程二级池模块内可选后端)  
13. [策略注入与切换方式](#13-策略注入与切换方式)  
14. [配置宏完整说明](#14-配置宏完整说明)  
15. [依赖关系图](#15-依赖关系图)  
16. [测试体系](#16-测试体系)  
17. [构建与使用](#17-构建与使用)  
18. [与 kv_pool / 容器的关系](#18-与-kv_pool--容器的关系)  
19. [实现完成度清单](#19-实现完成度清单)  
20. [与 SGI/STL 对照表](#20-与-sgistl-对照表)

---

## 1. 模块定位与目标

### 1.1 是什么

**LSTL（Light STL）空间配置模块** 是 KVStore 仓库中的 **C++11 空间配置器子集**，对标 SGI STL / 标准库中负责「原始内存 + 对象构造」的那一层，而不是完整 STL 容器库。

核心能力：

- 按**字节**或**元素个数**分配/释放原始存储；
- 在已分配存储上 **placement new 构造**、显式 **析构**；
- 提供 **uninitialized_*** 算法，供未来 `vector` 等容器在扩容时使用；
- 通过 **可替换策略** 支持 `malloc`、内置单线程二级池、或用户自定义池（如 `kv::pool`）。

### 1.2 不是什么

- **不是** 完整 STL（无 `vector` / `deque` / `list` / `algorithm`）；
- **不是** 多线程内存池实现主体（多线程池在 `module/kv_pool`）；
- **不强制** 所有分配走池化（默认虽为二级池，可一键切回 `malloc`）。

### 1.3 产品目标

| 目标 | 说明 |
|------|------|
| 同构 SGI | 分层、命名、算法语义与《STL 源码剖析》空间配置章节一致 |
| C++11 可编译 | 无 C++14/17 依赖；可用 `throw()`、traits 分派 |
| 策略可注入 | 换分配器不改 `uninitialized_*` / `construct` 代码 |
| header-only 友好 | 核心为头文件；构建为 `lstl_memory` INTERFACE 库 |
| 为 KV/Redis 负载铺路 | 小块高频分配；池由独立模块提供 |

---

## 2. 模块边界：包含与不包含

```
┌─────────────────────────────────────────────────────────────┐
│  LSTL 空间配置模块（module/lstl）                              │
├─────────────────────────────────────────────────────────────┤
│  ✓ 策略接口约定（allocate / deallocate 字节语义）              │
│  ✓ malloc_alloc 一级配置器 + OOM handler                      │
│  ✓ pool_alloc_t 门面 → pool_single 单线程二级池（可选默认）    │
│  ✓ simple_alloc<T, Policy>                                   │
│  ✓ allocator<T>、allocator_traits                            │
│  ✓ construct / destroy（单对象与区间）                        │
│  ✓ uninitialized_copy / fill / fill_n                        │
│  ✓ type_traits（SGI 风格 __type_traits）                       │
│  ✓ temporary_buffer（固定 malloc）                           │
│  ✓ iterator_facet（算法分派）                                  │
├─────────────────────────────────────────────────────────────┤
│  ✗ 多线程 Arena / TCache / MPSC（→ module/kv_pool）           │
│  ✗ STL 容器 vector / deque / list                             │
│  ✗ 标准 algorithm / iterator 完整库                           │
└─────────────────────────────────────────────────────────────┘
          ▲
          │ LSTL_USER_ALLOC 或 simple_alloc<T, MyPool>
          │
┌─────────┴──────────┐
│ 客户端可选策略      │
│ • malloc           │
│ • lstl pool_single │
│ • kv::pool         │
│ • jemalloc 封装    │
└────────────────────┘
```

---

## 3. 设计原则

### 原则 1：只负责对象生命周期，不拥有池化策略

模块定义 **如何在有效内存上构造/析构**；**从哪块内存来** 由 `AllocPolicy` 决定。这与 STL 将空间配置器与容器分离的思想一致。

### 原则 2：字节语义与元素语义分层

| 层级 | API 风格 | 大小参数 |
|------|----------|----------|
| 策略 / `alloc` | `allocate(n)` | **字节数** `n` |
| `simple_alloc<T>` | `allocate(k)` | **元素个数** `k` → 内部 `k * sizeof(T)` 字节 |
| `allocator<T>` | `allocate(k)` | **元素个数** `k` → 内部 `k * sizeof(T)` 字节 |

### 原则 3：换分配器不改算法代码

`uninitialized_copy`、`construct` 等 **不直接调用** `malloc` 或池；容器层将来通过 `Allocator` 模板参数换策略，算法头文件保持稳定。

### 原则 4：临时内存与业务池隔离

`get_temporary_buffer` **硬编码** 走 `malloc_alloc_t`，避免排序/分区等算法的临时缓冲占用用户自定义池，防止池污染与容量抖动。

---

## 4. 分层架构

```
                    ┌─────────────────────────────────────┐
                    │  应用 / 未来容器（vector …）          │
                    └──────────────────┬──────────────────┘
                                       │
         ┌─────────────────────────────┼─────────────────────────────┐
         │                             │                             │
         ▼                             ▼                             ▼
┌─────────────────┐         ┌─────────────────┐         ┌─────────────────┐
│ uninitialized_* │         │ construct/      │         │ temporary_      │
│                 │         │ destroy         │         │ buffer          │
└────────┬────────┘         └────────┬────────┘         └────────┬────────┘
         │                           │                           │
         │         ┌─────────────────┴─────────────────┐         │
         │         │         type_traits               │         │
         │         │    iterator_facet（分派标签）       │         │
         │         └─────────────────┬─────────────────┘         │
         │                           │                           │
         ▼                           ▼                           ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  allocator<T>          allocator_traits<Alloc>                          │
│  simple_alloc<T, Policy>                                                  │
└────────────────────────────────┬────────────────────────────────────────┘
                                 │
                                 ▼
                    ┌────────────────────────┐
                    │  typedef alloc = …      │  ← 编译期默认策略
                    └────────────┬───────────┘
                                 │
              ┌──────────────────┼──────────────────┐
              ▼                  ▼                  ▼
       malloc_alloc_t      pool_alloc_t      LSTL_USER_ALLOC
       （一级）             （二级 LIGHT）      （用户 / kv_pool）
```

---

## 5. 目录与文件职责

```text
module/lstl/
├── config.h                      # 全局宏：对齐、池上限、LIGHT、malloc 开关
├── memory.h                      # 统一入口，include 下列所有公共头
├── alloc.h                       # alloc typedef + simple_alloc
├── allocator.h                   # allocator<T>、allocator<void>
├── allocator_traits.h            # allocator_traits 泛化 + allocator 特化
├── construct.h                   # construct / destroy（单对象与区间）
├── uninitialized.h               # uninitialized_copy / fill / fill_n
├── type_traits.h                 # __type_traits、POD 判定
├── temporary_buffer.h            # get/return_temporary_buffer
├── memory/
│   ├── malloc_alloc.h            # 一级配置器
│   ├── oom.h                     # OOM 失败策略
│   ├── pool.h                    # pool_alloc_t 门面
│   ├── pool_single.h             # 单线程二级池实现（SGI LIGHT）
│   ├── freelist.h                # 侵入式自由链表
│   ├── size_class.h              # 对齐、size class、small/large 判定
│   ├── large.h                   # 大块（带 LargeHeader）
│   ├── span.h                    # Span 模式（LIGHT=0）
│   └── span_registry.h           # Span 注册与 purge（LIGHT=0）
└── internal/detail/
    └── iterator_facet.h          # iterator_traits、迭代器 tag
```

| 文件 | 行数级职责 | 对外 API 要点 |
|------|------------|---------------|
| `memory.h` | 聚合 | 一行 include 引入全套 |
| `alloc.h` | 策略选择 | `alloc`、`simple_alloc` |
| `allocator.h` | std 风格 | `allocator<T>::allocate(n)` 元素个数 |
| `construct.h` | 构造析构 | `construct`、`destroy` 区间 |
| `uninitialized.h` | 容器预备 | 三个 uninitialized_* |
| `temporary_buffer.h` | 算法辅助 | 仅 malloc |

---

## 6. 分配策略层

### 6.1 策略接口（C++11 概念性约定）

任意类型可作为 `simple_alloc<T, Alloc>` 的策略，**最低要求**：

```cpp
struct MyAlloc {
  static void* allocate(size_t n);           // n = 字节数；n==0 → nullptr
  static void deallocate(void* p, size_t n); // p==nullptr 或 n==0 无操作
};
```

**可选**（`malloc_alloc` / `pool_alloc` 提供）：

```cpp
static void (*set_malloc_handler(void (*f)()))();
```

**不要求**：线程安全、统计、purge — 由具体策略自行保证。

### 6.2 一级配置器：`malloc_alloc_t`

**文件**：`memory/malloc_alloc.h`

| 行为 | 实现 |
|------|------|
| 分配 | `std::malloc(n)`，失败则循环调用 `oom_handler` 重试 |
| 释放 | `std::free(p)`，忽略 `n` |
| OOM | 无 handler 时 `detail::lstl_oom_fail()` |
| handler | 静态函数指针，可 `set_malloc_handler` 替换 |

**OOM 策略**（`memory/oom.h`）：

- 默认：`throw std::bad_alloc()`
- `LSTL_OOM_MODE_CERR`：`cerr` 输出后 `abort()`

### 6.3 默认策略选择：`alloc` typedef

**文件**：`alloc.h`

```cpp
#ifdef LSTL_USER_ALLOC
typedef LSTL_USER_ALLOC alloc;
#elif LSTL_USE_MALLOC_ALLOC
typedef malloc_alloc_t alloc;
#else
typedef pool_alloc_t alloc;    // 默认：单线程二级池
#endif
```

| 优先级 | 条件 | 结果 |
|--------|------|------|
| 1 | 定义 `LSTL_USER_ALLOC` | 用户类型 |
| 2 | `LSTL_USE_MALLOC_ALLOC=1` | malloc |
| 3 | 否则 | `pool_alloc_t` |

### 6.4 大块路径：`large.h`

当 `LSTL_POOL_MAX_BYTES < bytes <= LSTL_LARGE_MAX`（默认 128B～32KB）：

- `large_allocate`：`malloc(sizeof(LargeHeader) + bytes)`，返回 user 指针（header 在前）
- `large_deallocate`：根据 header 还原并 `free`
- magic：`LSTL_LARGE_MAGIC`（'LARG'）

超过 `LSTL_LARGE_MAX` 的策略层直接 `malloc_alloc`（在 `pool_single` 中）。

---

## 7. 封装层：simple_alloc 与 allocator

### 7.1 `simple_alloc<T, AllocPolicy>`

**设计**：SGI `simple_alloc` 同名语义 — 按**元素个数**分配。

```cpp
template <typename T, typename AllocPolicy = alloc>
struct simple_alloc {
  static T* allocate(size_type n);      // n 元素 → AllocPolicy::allocate(n * sizeof(T))
  static void deallocate(T* p, size_type n);
};
```

**用途**：容器内部、明确类型的批量分配；第二模板参数可指定 `my::pool` 而不影响全局 `alloc`。

### 7.2 `allocator<T>`

**设计**：贴近 C++98/03 `std::allocator` 接口（C++11 兼容写法）。

| 成员 | 行为 |
|------|------|
| `allocate(n)` | `alloc::allocate(n * sizeof(T))`；`n > max_size()` 抛 `bad_alloc` |
| `deallocate(p, n)` | `alloc::deallocate(p, n * sizeof(T))` |
| `construct(p, val)` | 转发 `lstl::construct` |
| `destroy(p)` | 转发 `lstl::destroy` |
| `rebind<U>::other` | `allocator<U>` |
| `operator==` | 恒 true（空分配器，无状态） |

**`allocator<void>`**：仅 typedef 与 `rebind`，无 `allocate`。

### 7.3 `allocator_traits<Alloc>`

**设计**：C++11 `allocator_traits` 的简化实现，统一容器对分配器的调用方式。

提供：`allocate`、`deallocate`、`construct`、`destroy`、`max_size`、`rebind_alloc`、`select_on_container_copy_construction`。

**特化** `allocator_traits<allocator<T>>`：补全 `void_pointer` 等成员（因 `allocator<void>` 特殊）。

---

## 8. 对象构造与析构

**文件**：`construct.h`

### 8.1 单对象

```cpp
template <typename T>
void construct(T* p);                    // 默认构造

template <typename T, typename V>
void construct(T* p, const V& value);    // 拷贝/转换构造

template <typename T>
void destroy(T* pointer);                // 显式析构，不释放内存
```

均使用 placement new / 显式析构，**不触碰分配器**。

### 8.2 区间析构 `destroy(first, last)`

**分派维度**（二维）：

1. **迭代器类别**：input → forward → bidirectional → random_access（逐级转发到 input 实现）
2. **POD 优化**：若 `__type_traits<value_type>::this_type_is_POD_type` 为 `__true_type`，**不调用析构**（空操作）

非 POD：循环 `destroy(&*first)`。

**设计意图**：与 SGI 一致，POD 数组销毁无需逐元素调用析构函数。

---

## 9. 未初始化内存算法

**文件**：`uninitialized.h`  
**用途**：容器 `vector` 扩容时，在新分配的 **未初始化** 存储上构造元素。

### 9.1 `uninitialized_copy(first, last, result)`

| 条件 | 实现 |
|------|------|
| 迭代器为 random_access **且** value_type 为 POD | `memcpy` 整块拷贝 |
| 否则 | 逐元素 `construct(&*result, *first)` |

**分派链**：

```
uninitialized_copy
  → uninitialized_copy_impl(..., iterator_category)
    → random_access + __type_traits::is_POD
      → true:  memcpy
      → false: 逐元素 construct
```

### 9.2 `uninitialized_fill(first, last, x)`

| POD | 非 POD |
|-----|--------|
| `*first = x` 赋值 | `construct(&*first, x)` |

### 9.3 `uninitialized_fill_n(first, n, x)`

与 `uninitialized_fill` 相同 POD 分支逻辑，循环 `n` 次。

### 9.4 与分配策略的关系

这些算法 **只操作已存在的指针区间**，不负责 `allocate`；容器代码应先通过 `Allocator` 取得存储，再调用本模块算法 — 保证 **算法与策略解耦**。

---

## 10. 类型特征与迭代器分派

### 10.1 `type_traits.h` — `__type_traits<T>`

SGI 风格，核心字段：

```cpp
typedef __true_type / __false_type this_type_is_POD_type;
// 以及 default/copy/assignment/destructor 是否具有
```

**已特化 POD 为 true 的类型**：算术类型、`char` 系列、指针等。

**作用**：

- `uninitialized_*` 选择 `memcpy` vs `construct`
- `destroy` 区间是否跳过析构

> 注：未实现完整 C++11 `std::is_trivially_destructible`，而是 SGI 式手工特化表。

### 10.2 `iterator_facet.h`

| 内容 | 说明 |
|------|------|
| `input_iterator_tag` … `random_access_iterator_tag` | 继承层次标签 |
| `iterator_traits<Iterator>` | 委托 `std::iterator_traits` 或指针特化 |
| `iterator_category(it)` | 编译期标签获取 |

**作用**：`uninitialized_copy`、`destroy` 区间按迭代器能力分派；指针视为 `random_access_iterator_tag`。

---

## 11. 临时缓冲

**文件**：`temporary_buffer.h`

### 11.1 API

```cpp
template <typename T>
std::pair<T*, ptrdiff_t> get_temporary_buffer(ptrdiff_t len);

template <typename T>
void return_temporary_buffer(T* p);
```

### 11.2 设计决策

| 决策 | 原因 |
|------|------|
| **固定 `malloc_alloc_t`** | 不经过 `alloc` typedef，避免占用用户池 |
| 长度上限 `LSTL_MAX_BYTES / sizeof(T)` | 防止单次申请过大 |
| `len <= 0` 返回 `(0, 0)` | 与 SGI 一致 |
| `return` 时 `deallocate(p, 0)` | malloc 路径忽略 n |

**典型用途**：归并排序、分区等算法的临时工作区（未来 algorithm 模块）。

---

## 12. 单线程二级池（模块内可选后端）

> 多线程池见 [`../memory_pool_summary.md`](../memory_pool_summary.md)；此处仅述 **LSTL 模块内** 的 `pool_single`。

### 12.1 门面 `pool_alloc_t`（`memory/pool.h`）

静态转发至 `detail::PoolSingle`：

- `allocate` / `deallocate`
- `set_malloc_handler`
- `purge_idle_memory`（LIGHT 模式为 no-op）
- `small_mapped_bytes`、可选统计 `small_alloc_count` 等

### 12.2 实现 `PoolSingle`（`memory/pool_single.h`）

**全局单例**：`static PoolSingle& instance()` — **仅单线程安全**。

**小块路径**（≤128B，8B 对齐，16 档）：

```
allocate → freelist_pop → miss → refill → chunk_alloc
deallocate → freelist_push（零检查）
```

**chunk_alloc**（SGI 经典）：

1. bump 剩余够 → 切片返回  
2. 部分剩余 → 调整块数  
3. 碎片挂到对应 size class freelist  
4. `malloc` 新 chunk：`2*need + heap_size/16`  
5. refill 批量切块入 freelist  

**三级分流**：

| 请求大小 | 路径 |
|----------|------|
| ≤ 128B | 二级池 freelist + bump |
| 128B < n ≤ 32KB | `large_allocate` |
| 更大 | `malloc_alloc` |

### 12.3 LIGHT vs Span 模式

| | `LSTL_POOL_LIGHT=1`（默认） | `LSTL_POOL_LIGHT=0` |
|--|---------------------------|----------------------|
| chunk | `malloc` 一整段 | `posix_memalign` 4KB + SpanHeader |
| free | 直接 push | span 计数 |
| purge | 无 | `span_registry::purge_idle` |
| 热路径 | 最快，对标 SGI | 可追踪、可归还系统 |

---

## 13. 策略注入与切换方式

### 方式 A：编译期全局替换

```cpp
#define LSTL_USER_ALLOC my::kv_pool_wrapper
#include "alloc.h"
// 此后 lstl::alloc 为用户类型
```

### 方式 B：模板参数（推荐）

```cpp
lstl::simple_alloc<RedisObject, kv::pool_alloc> alloc;
// allocator 仍用默认 alloc，除非特化
```

### 方式 C：强制 malloc

```bash
cmake .. -DLSTL_USE_MALLOC_ALLOC=ON
```

或代码中：

```cpp
// config 或编译选项 LSTL_USE_MALLOC_ALLOC=1
typedef malloc_alloc_t alloc;
```

### 方式 D：接入 kv_pool

`kv::pool_alloc` 满足 `allocate(void*, size_t)` 静态接口后：

```cpp
#define LSTL_USER_ALLOC kv::pool_alloc
#include "alloc.h"
```

---

## 14. 配置宏完整说明

| 宏 | 默认值 | 含义 |
|----|--------|------|
| `LSTL_ALIGN` | 8 | 小块对齐字节 |
| `LSTL_POOL_MAX_BYTES` | 128 | 二级池小块上限 |
| `LSTL_LARGE_MAX` | 32768 | 大块池上限 |
| `LSTL_FREELISTS` | 16 | 自由链表档位数（8～128B） |
| `LSTL_POOL_REFILL_BATCH` | 20 | refill 批量块数 |
| `LSTL_POOL_LIGHT` | 1 | 1=LIGHT 热路径 |
| `LSTL_PAGE_SHIFT` / `LSTL_PAGE_SIZE` | 12 / 4KB | Span 模式页大小 |
| `LSTL_MAX_BYTES` | 32768 | temporary_buffer 参考上限 |
| `LSTL_USE_MALLOC_ALLOC` | 0 | 1=全局 alloc 用 malloc |
| `LSTL_USER_ALLOC` | 未定义 | 用户策略类型名 |
| `LSTL_OOM_MODE_CERR` | 未定义 | OOM 时 cerr+abort |
| `LSTL_POOL_DISABLE_STATS` | 未定义 | 定义后关闭池统计 |

**已移除**（随多线程池移出 LSTL 核心）：`LSTL_THREAD_SAFE`、`LSTL_ARENA_COUNT`、`LSTL_CHUNK_USE_MMAP` 等。

---

## 15. 依赖关系图

```
memory.h
├── alloc.h
│   ├── config.h
│   ├── memory/malloc_alloc.h → oom.h
│   └── memory/pool.h → pool_single.h → freelist, size_class, large, [span]
├── allocator.h → alloc.h, construct.h
├── allocator_traits.h → construct.h
├── construct.h → iterator_facet.h, type_traits.h
├── uninitialized.h → construct.h, iterator_facet.h, type_traits.h
├── type_traits.h
└── temporary_buffer.h → config.h, malloc_alloc.h（仅 malloc，不经过 alloc）
```

**无循环依赖**；`pool_single` 不依赖 `allocator.h`。

---

## 16. 测试体系

**目录**：`tests/lstl/memory/`  
**CMake**：`lstl_add_module_tests(memory TESTS …)`  
**输出**：`bin/memory/<test_name>`

| 测试 | 验证内容 |
|------|----------|
| `test_alloc` | `malloc_alloc`、`set_malloc_handler`、`simple_alloc`、`alloc` 默认路径 |
| `test_construct` | 单对象/区间 construct、destroy |
| `test_uninitialized` | copy / fill / fill_n，POD 与非 POD |
| `test_allocator` | `allocator<T>` allocate/deallocate/construct/destroy |
| `test_temporary_buffer` | 申请与归还、长度截断 |
| `test_type_traits` | POD 分支行为 |
| `test_oom_policy` | 无 handler 抛异常或 abort |
| `test_pool_single` | 默认 `pool_alloc_t` 小块/大块/统计 |
| `test_memory_stress` | 多线程调用 `alloc`（malloc 后端压力） |

**不包含在本模块测试集**：`kv_pool` 多线程/跨线程（见 `tests/kv_pool/`）。

**运行**：

```bash
ctest -R '^memory\.' --output-on-failure
# 或
make run_tests_memory
```

---

## 17. 构建与使用

### 17.1 CMake

根 `CMakeLists.txt`：

```cmake
set(LSTL_ROOT "${CMAKE_SOURCE_DIR}/module/lstl")
add_library(lstl_memory INTERFACE)
target_include_directories(lstl_memory INTERFACE ${LSTL_ROOT})
```

测试 target 通过 `lstl_apply_target_options` 链接 `lstl_memory`。

### 17.2 最小使用

```cpp
#include "memory.h"

struct Foo { int x; };

int main() {
  // 默认 alloc = pool_alloc_t（单线程池）
  Foo* p = lstl::simple_alloc<Foo>::allocate(10);
  lstl::construct(p, Foo{42});
  lstl::destroy(p);
  lstl::simple_alloc<Foo>::deallocate(p, 10);
  return 0;
}
```

### 17.3 仅算法 + 自定义池

```cpp
namespace my {
struct pool {
  static void* allocate(size_t n);
  static void deallocate(void* p, size_t n);
};
}

#define LSTL_USER_ALLOC my::pool
#include "memory.h"
```

---

## 18. 与 kv_pool / 容器的关系

### 18.1 与 kv_pool

| 组件 | 关系 |
|------|------|
| LSTL 空间配置 | 定义接口与算法 |
| `module/kv_pool` | 实现多线程 `allocate`/`deallocate`，**不修改** LSTL 核心头文件 |
| 集成 | `#define LSTL_USER_ALLOC` 或 `simple_alloc<T, kv::pool_alloc>` |

详见 [`../memory_pool_summary.md`](../memory_pool_summary.md)。

### 18.2 与未来容器

本模块 **已具备** vector 所需底层能力：

- `allocator<T>` 分配元素存储  
- `uninitialized_copy` / `fill` 扩容构造  
- `destroy` 区间析构  
- `construct` 单元素插入  

**未实现**：`vector` 类模板本身、`iterator` 封装、异常安全插入删除策略。

---

## 19. 实现完成度清单

| 能力项 | 状态 | 备注 |
|--------|------|------|
| 策略接口 + malloc 一级 | ✅ | |
| OOM handler | ✅ | |
| simple_alloc | ✅ | |
| allocator / allocator_traits | ✅ | |
| construct / destroy | ✅ | POD 优化 |
| uninitialized_* | ✅ | POD memcpy |
| type_traits | ✅ | SGI 子集 |
| temporary_buffer | ✅ | 固定 malloc |
| iterator_facet | ✅ | |
| pool_alloc 单线程二级 | ✅ | 默认 alloc |
| pool Span 重型模式 | ✅ | 可选 LIGHT=0 |
| 多线程池 | ✅ | **独立** kv_pool |
| LSTL 容器 | ❌ | 未规划在本模块 v2.0 |
| C++11 allocator_traits 完整 | ⚠️ | 简化子集，够用 |

---

## 20. 与 SGI/STL 对照表

| SGI / STL 概念 | LSTL 对应 |
|----------------|-----------|
| `malloc_alloc` 一级 | `malloc_alloc_t` |
| `default_alloc` / 二级池 | `pool_alloc_t` → `PoolSingle` |
| `simple_alloc<T, Alloc>` | `simple_alloc<T, AllocPolicy>` |
| `allocator<T>` | `allocator<T>` |
| `construct` / `destroy` | 同名 |
| `uninitialized_copy` 等 | 同名，POD 优化 |
| `__type_traits` | `__type_traits` |
| `get_temporary_buffer` | 同名，固定 malloc |
| `vector` | **未实现** |
| 多线程池 | **kv_pool**（模块外） |

---

## 附录 A：相关文档索引

| 文档 | 内容 |
|------|------|
| [`allocator_design.md`](allocator_design.md) | v2.0 权威边界与接口 |
| [`memory_pool_design.md`](memory_pool_design.md) | 池化组件蓝图 |
| [`../memory_pool_summary.md`](../memory_pool_summary.md) | 两种内存池实现与优化 |
| [`../../module/README.md`](../../module/README.md) | 模块目录总览 |
| [`../../README.md`](../../README.md) | 仓库构建说明 |

---

## 附录 B：一句话总结

**LSTL 空间配置模块**设计并实现了：与 SGI/STL 同构的 **可插拔字节级分配策略**、**对象构造/析构**、**未初始化内存算法**、**allocator 封装** 与 **临时缓冲**；默认小块走 **单线程 SGI 二级池**，可切换 **malloc** 或 **注入 kv_pool**；**不包含** 多线程池内核与 STL 容器，只为上层容器与 KV 负载提供稳定、可替换的内存基础设施。
