# LemoNettyCore — 高性能协程网络栈（核心半栈）

整合 netCore / netLemo 性能最优实现，**仅保留核心网络层**（不含 log / config）。

## 模块

| 层 | 内容 |
|----|------|
| 运行时 | `utils` `thread` `memory`（StackPool） |
| 协程 | `fiber`（Scheduler + scheduleNext 快路径 + idle spin 256） |
| IO | `io`（Reactor + IOManager + Hook + FdManager） |
| 传输 | `buffer`（RingBuffer） `socket`（Address + Socket） |

## 性能特性（来自 netLemo 优化）

- IO 就绪：`triggerEventRunnext` → `scheduleNext` 快路径
- Hook：无超时 `retry_fast` 快路径
- 调度：idle spin 256 轮
- stop：`cancelAllEvents` 跨线程安全唤醒

## 构建

```bash
cmake -B build -DLEMONETTYCORE_BUILD=ON \
  -DNETCORE_BUILD=OFF -DNETLEMO_BUILD=OFF -DLEMO_BUILD=OFF \
  -DLSTL_BUILD_NET_LOG=OFF
cmake --build build -j
```

## 终版测试

```bash
# 全套单元/集成测试（11 项）
cmake --build build --target run_tests_nettycore

# 或直接 ctest
ctest -R '^LemoNettyCore\.' --output-on-failure

# 性能 smoke
cmake --build build --target bench_echo_server_nettycore_quick

# 标准压测
cmake --build build --target bench_nettycore_perf
```

## 入口

```cpp
#include "lemo/nettycore.h"
#include "lemo/io/iomanager.h"

lemo::io::IOManager iom(4, true, "main");
```

## 与 netCore 关系

- **netCore** = 核心半栈 + log + config（全栈）
- **LemoNettyCore** = 核心半栈（本目录，终版推荐用于纯网络场景）

源码独立拷贝自 netCore 核心层，历史参考文件（legacy/epoll_context）已剔除。
