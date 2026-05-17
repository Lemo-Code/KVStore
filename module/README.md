# module/

业务与基础库源码统一放在此目录；单元测试仍在仓库根目录 `tests/<模块名>/`。

```
module/
├── lstl/
│   ├── memory/     # 空间配置器（header-only，C++11 STL/SGI 同构）
│   └── container/  # 容器（vector 等，基于 memory）
├── kv_pool/        # 多线程二级内存池（TCache + Arena + MPSC）
└── net/            # 异步日志（log 子模块）
```

| 模块 | 构建开关 | 测试目录 | 产物目录 |
|------|----------|----------|----------|
| lstl memory | 默认开启 | `tests/memory/` | `bin/memory/` |
| lstl container | `LSTL_BUILD_CONTAINER` | `tests/container/` | `bin/container/` |
| kv_pool | `LSTL_BUILD_KV_POOL` | `tests/kv_pool/` | `bin/kv_pool/` |
| net | `LSTL_BUILD_NET_LOG` | `tests/net/` | `bin/net/` |

`lstl_memory` 在根 `CMakeLists.txt` 中导出 memory 与 container 的 include 路径；`kv_pool`、`net` 由 `module/CMakeLists.txt` 按选项 `add_subdirectory`。

设计文档：

- [`docs/lstl/spatial_allocator_summary.md`](../docs/lstl/spatial_allocator_summary.md) — 空间配置模块完整设计总结  
- [`docs/memory_pool_summary.md`](../docs/memory_pool_summary.md) — 两种内存池实现与优化总结  
