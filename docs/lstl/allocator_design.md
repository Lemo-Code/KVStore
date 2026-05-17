# LSTL 空间配置器设计（v2.0）

> **内存池不是本模块的一部分。** 池化分配由客户端自选实现，通过策略类型注入；本模块只提供与 SGI/STL 同构的空间配置能力。  
> **完整设计总结（非常详细）**：[`spatial_allocator_summary.md`](spatial_allocator_summary.md)。

---

## 1. 模块边界

```
┌─────────────────────────────────────────────────────────┐
│  LSTL 空间配置模块（module/lstl）                          │
│  ─────────────────────────────────────────────────────    │
│  • 策略接口约定 + 默认 malloc_alloc                       │
│  • construct / destroy / uninitialized_*                  │
│  • type_traits、allocator、allocator_traits               │
│  • temporary_buffer、simple_alloc                         │
│  ─────────────────────────────────────────────────────    │
│  ✗ 内存池 / arena / tcache（已移出，后续独立仓库或目录）    │
└─────────────────────────────────────────────────────────┘
          ▲
          │ 用户实现并注入
          │
┌─────────┴─────────┐
│ 客户端自选：         │
│ • malloc（默认）    │
│ • 自研 memory_pool  │
│ • jemalloc 封装     │
│ • std::allocator    │
└─────────────────────┘
```

**原则（与 STL 一致）**

1. 模块 **只负责** 在有效内存上构造/析构对象，**不拥有** 池化策略。
2. `allocate` 返回 **可供 T 使用的已分配存储**；`deallocate` 按 `(p, n)` 归还。
3. 换分配器 **不改** `vector` 预备逻辑（uninitialized_*、traits）的代码路径，只换模板参数 `Alloc`。

---

## 2. 目录结构

```text
module/lstl/
  config.h                 # 对齐、OOM、临时缓冲上限等
  alloc.h                  # 默认策略 typedef + simple_alloc
  allocator.h              # allocator<T>（默认绑定 alloc）
  allocator_traits.h
  construct.h
  uninitialized.h
  type_traits.h
  temporary_buffer.h
  memory.h                 # 统一 #include 入口
  memory/
    malloc_alloc.h         # 一级配置器（默认）
    oom.h                  # OOM 策略
  internal/detail/         # 迭代器等
```

**已移出（后续独立设计）**：原 `memory/pool*.h`、`arena`、`tcache`、`chunk` 等 → 见 `docs/lstl/memory_pool_design.md`（蓝图，非本模块实现）。

---

## 3. 配置器策略接口（C++11 概念性约定）

任意类型 `Alloc` 可作为 `simple_alloc<T, Alloc>` 的策略，需满足：

| 成员 | 签名 | 说明 |
|------|------|------|
| `allocate` | `static void* allocate(size_t n)` | `n` 为**字节数**；`n==0` 返回 `nullptr` |
| `deallocate` | `static void deallocate(void* p, size_t n)` | `p==nullptr` 或 `n==0` 无操作 |

可选（若策略支持 OOM 重试，可自行实现；默认路径用 `malloc_alloc`）：

| 成员 | 签名 |
|------|------|
| `set_malloc_handler` | `static void (*set_malloc_handler(void (*)()))()` |

**不要求** 线程安全、池统计、purge — 由具体策略自行保证。

---

## 4. 默认策略与切换方式

### 4.1 编译期全局默认（`alloc` typedef）

```cpp
// 默认
#include "alloc.h"
// lstl::alloc → malloc_alloc_t

// 使用自定义池（在 include alloc.h 之前）
#define LSTL_USER_ALLOC my::redis_pool_alloc
#include "alloc.h"
```

### 4.2 模板参数（推荐，最灵活）

```cpp
#include "alloc.h"

my::pool_alloc my_policy;

// 仅这一块用池
lstl::simple_alloc<RedisObject, my::pool_alloc> obj_alloc;

// 或 std 风格
lstl::allocator<RedisObject> a;  // 仍用默认 alloc，除非特化
```

### 4.3 与 `allocator<T>` 的关系

`allocator<T>` 内部调用 `alloc::allocate(n * sizeof(T))`，与 `std::allocator` 相同模式。需要全进程换策略时 `#define LSTL_USER_ALLOC` 或特化 `allocator`。

---

## 5. 组件依赖图

```
memory.h
  ├── alloc.h ──────────► malloc_alloc.h ──► oom.h
  ├── allocator.h ──────► alloc.h, construct.h
  ├── allocator_traits.h
  ├── construct.h ──────► internal/detail/iterator_facet.h
  ├── uninitialized.h
  ├── type_traits.h
  └── temporary_buffer.h ► malloc_alloc.h（临时缓冲固定走 malloc，避免池污染）
```

**刻意决策**：`get_temporary_buffer` **始终** 使用 `malloc_alloc_t`，不经过 `alloc` typedef，避免算法临时内存占用客户端池。

---

## 6. 配置宏（config.h）

| 宏 | 含义 |
|----|------|
| `LSTL_ALIGN` | 默认 8 |
| `LSTL_MAX_BYTES` | temporary_buffer 单次上限参考 |
| `LSTL_USER_ALLOC` | 可选，替换默认 `alloc` typedef |
| `LSTL_OOM_MODE_CERR` | OOM 时 cerr+abort |
| `LSTL_DEFAULT_USE_MALLOC_ALLOC` | 保留兼容，默认 1 |

**已移除（随内存池移出）**：`LSTL_THREAD_SAFE`、`LSTL_ARENA_COUNT`、`LSTL_CHUNK_USE_MMAP`。

---

## 7. 测试范围

| 测试 | 验证 |
|------|------|
| test_alloc | malloc、OOM handler、`simple_alloc`、默认 `alloc` |
| test_construct / uninitialized / allocator | 与分配策略解耦 |
| test_oom_policy | OOM 行为 |
| test_temporary_buffer | 临时缓冲 |
| test_memory_stress | 多线程 + `alloc`（malloc 后端） |

**不再包含**：`test_pool_v2`（池测试随实现移出后恢复）。

---

## 8. 客户端集成示例

```cpp
// 仅用 LSTL 算法 + 自己的池
namespace my {
struct kv_pool {
  static void* allocate(size_t n);
  static void deallocate(void* p, size_t n);
};
}

#define LSTL_USER_ALLOC my::kv_pool
#include "lstl/memory.h"

void foo() {
  lstl::simple_alloc<char, my::kv_pool>::allocate(128);
}
```

---

## 9. 与内存池蓝图的关系

| 文档 | 状态 |
|------|------|
| `allocator_design.md`（本文） | **当前模块** 权威设计 |
| `memory_pool_design.md` | **未来独立组件** 从 0 到 1 蓝图，实现时不修改 LSTL 核心 |

实现内存池见 `module/kv_pool/`，仅实现 `allocate`/`deallocate`，通过 `LSTL_USER_ALLOC` 或显式模板参数接入。

**两种池的完整实现说明、优化历程与压测**：见 [`../memory_pool_summary.md`](../memory_pool_summary.md)。
