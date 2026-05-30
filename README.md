# KVStore

本仓库实现 **KVStore** 分层组件库，底层为 **LSTL**（Light STL）——与 SGI/STL 同构的 C++11 空间配置与容器子集，向上提供存储引擎、内存池与网络基础设施。

## 分层架构

```
lstl (memory + container)
  ├── storage/lsmtree     LSM 存储引擎
  ├── kv_pool             多线程二级内存池
  └── net                 基础设施（base / runtime / log）
```

## 目录结构

```
KVStore/
├── cmake/
│   ├── KVStoreHelpers.cmake    # 统一测试注册与构建辅助
│   └── LSTLHelpers.cmake       # 兼容入口
├── module/
│   ├── lstl/                   # 层 1：STL 子集（header-only）
│   │   ├── memory/
│   │   └── container/
│   ├── storage/                # 层 2：存储引擎
│   │   └── lsmtree/
│   ├── kv_pool/                # 层 3：多线程内存池
│   └── net/                    # 层 4：基础设施
│       ├── common/ config/ thread/   # base
│       ├── fiber/                    # runtime
│       └── log/                      # log
├── tests/
│   ├── common/                 # 公共测试头
│   ├── lstl/{memory,container}/
│   ├── storage/lsmtree/
│   ├── kv_pool/
│   └── net/
├── docs/
└── third_party/sylars/         # 参考实现（独立构建）
```

## 构建与测试

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
  -DLSTL_BUILD_KV_POOL=ON \
  -DLSTL_BUILD_NET_LOG=ON
make -j$(nproc)
ctest --output-on-failure
```

常用 target：

| target | 说明 |
|--------|------|
| `run_tests` | 运行全部测试 |
| `run_tests_lstl` | 仅 lstl 层（`lstl.memory.*` / `lstl.container.*`） |
| `run_tests_memory` | 仅 `lstl.memory.*`（兼容旧名） |

可选：`-DLSTL_BUILD_BENCH=ON` 构建压测。

## 使用示例

### LSTL 分配器

```cpp
#include "alloc.h"
lstl::simple_alloc<Obj> alloc;
```

### LSM 树

```cpp
#include "lsmtree.h"
lstl::lsm::LsmTree<int, int> db;
db.put(1, 100);
```

### 自定义内存池注入

```cpp
#define LSTL_USER_ALLOC kv::pool_alloc
#include "memory.h"
```

## 文档

| 文档 | 说明 |
|------|------|
| [`module/README.md`](module/README.md) | 模块分层与 include 约定 |
| [`docs/lstl/spatial_allocator_summary.md`](docs/lstl/spatial_allocator_summary.md) | 空间配置模块设计 |
| [`docs/memory_pool_summary.md`](docs/memory_pool_summary.md) | 内存池实现总结 |
