# module/

业务源码按**分层依赖**组织；单元测试在仓库根目录 `tests/<层>/<模块>/`。

## 分层架构

```
                    ┌─────────────────┐
                    │  应用 / KVStore  │  （未来）
                    └────────┬────────┘
                             │
         ┌───────────────────┼───────────────────┐
         ▼                   ▼                   ▼
  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
  │  storage    │    │   kv_pool   │    │     net     │
  │  (lsmtree)  │    │  多线程池    │    │ 基础设施包   │
  └──────┬──────┘    └──────┬──────┘    └──────┬──────┘
         │                  │                   │
         └──────────────────┼───────────────────┘
                            ▼
                   ┌─────────────────┐
                   │      lstl       │
                   │ memory+container│
                   └─────────────────┘
```

## 目录

```
module/
├── lstl/                   # 层 1：STL 子集（header-only）
│   ├── memory/             #   空间配置器
│   └── container/          #   序列/关联容器
├── storage/                # 层 2：存储引擎（header-only）
│   └── lsmtree/            #   LSM 树
├── kv_pool/                # 层 3：多线程内存池（STATIC）
│   └── memory/
└── net/                    # 层 4：协程网络库（STATIC，见 docs/net/）
    ├── base 层             #   common/ config/ thread/
    ├── runtime/            #   目标：fiber/scheduler/timer（待迁入）
    ├── io/                 #   reactor/hook/fd_context
    ├── transport/          #   socket/stream/buffer（待迁入）
    ├── server/             #   acceptor/tcp_server（新建）
    ├── log/                #   异步日志（正交）
    ├── design/             #   接口骨架（不参与编译）
    └── fiber/ io/ socket/  #   旧实现，逐层替换中
```

> **协程网络库重构**：架构文档见 [`docs/net/`](../docs/net/README.md)；接口契约见 [`module/net/design/`](net/design/README.md)。

## 构建开关与测试

| 层 | 模块 | CMake 开关 | CMake 目标 | 测试目录 | 产物 |
|----|------|------------|------------|----------|------|
| lstl | memory | 默认 ON | `lstl::memory` | `tests/lstl/memory/` | `bin/lstl/memory/` |
| lstl | container | `LSTL_BUILD_CONTAINER` | `lstl::memory` | `tests/lstl/container/` | `bin/lstl/container/` |
| storage | lsmtree | `LSTL_BUILD_LSM_TREE` | `storage::lsmtree` | `tests/storage/lsmtree/` | `bin/storage/lsmtree/` |

**lsmtree 子模块**：

| 头文件 | 说明 |
|--------|------|
| `lsmtree.h` | 内存版 `LsmTree` |
| `lsmtree_persistent.h` | 持久化 `PersistentLsmTree`（WAL + 磁盘 SSTable + Bloom） |

持久化目录布局：`MANIFEST`、`wal.log`、`sst-<id>.sst`。
| — | kv_pool | `LSTL_BUILD_KV_POOL` | `kv_pool::kv_pool` | `tests/kv_pool/` | `bin/kv_pool/` |
| net | base/runtime/log | `LSTL_BUILD_NET_LOG` | `net::log` | `tests/net/` | `bin/net/` |

公共测试头：`tests/common/lstl_test_common.h`（lstl / storage 层共用）。

## include 约定

| 层 | include 根 | 示例 |
|----|-----------|------|
| lstl memory | `module/lstl/memory` | `#include "alloc.h"` |
| lstl container | `module/lstl/container` | `#include "container.h"` |
| storage | `module/storage/lsmtree` | `#include "lsmtree.h"` |
| kv_pool | `module/kv_pool` | `#include "kv_pool.h"` |
| net | `module/net` | `#include "log/log.h"` |

## 参考实现

`sylar/`（仓库根目录）为 Sylar 框架只读快照，用于 API 对照，**未接入**本仓库 CMake，不再维护。新实现以 `docs/net/architecture.md` 为准。
