# KVStore / LSTL

本仓库目录名为 **KVStore**，当前实现 **LSTL**（Light STL）——与 SGI/STL 同构的 **C++11 空间配置模块**（构造、未初始化算法、`allocator`、traits 等）。

**不包含内置内存池。** 默认分配策略为 `malloc`/`free`；池化由客户端自选实现并注入（见 `module/kv_pool/`）。

## 目录结构

```
KVStore/
├── module/
│   ├── lstl/
│   │   ├── memory/           # 空间配置模块（header-only）
│   │   └── container/        # 容器（vector 等）
│   ├── kv_pool/              # 多线程内存池
│   │   ├── kv_pool.h
│   │   ├── config.h
│   │   └── memory/
│   └── net/                  # 日志模块
│       ├── log.h
│       └── log/
├── tests/
│   ├── memory/               # lstl 空间配置测试
│   ├── container/            # lstl 容器测试
│   ├── kv_pool/
│   └── net/
├── docs/lstl/                # 设计文档（含 spatial_allocator_summary.md）
└── cmake/                    # CMake 辅助
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

可选：`-DLSTL_BUILD_BENCH=ON` 构建 `bin/memory/bench_alloc_compare`、`bin/kv_pool/bench_kv_pool`。

## 使用自定义分配器

```cpp
namespace my {
struct kv_pool {
  static void* allocate(size_t n);
  static void deallocate(void* p, size_t n);
};
}

#define LSTL_USER_ALLOC my::kv_pool
#include "memory.h"

lstl::simple_alloc<Obj, my::kv_pool> alloc;
```

或仅对某容器指定：`simple_alloc<T, my::kv_pool>`。

## 文档

| 文档 | 说明 |
|------|------|
| [`docs/lstl/allocator_design.md`](docs/lstl/allocator_design.md) | 空间配置器架构（v2.0 边界） |
| [`docs/lstl/spatial_allocator_summary.md`](docs/lstl/spatial_allocator_summary.md) | **空间配置模块完整设计总结（非常详细）** |
| [`docs/lstl/memory_pool_design.md`](docs/lstl/memory_pool_design.md) | 内存池蓝图 |
| [`docs/memory_pool_summary.md`](docs/memory_pool_summary.md) | 两种内存池实现与优化总结（详细） |
| [`module/README.md`](module/README.md) | 模块目录说明 |
