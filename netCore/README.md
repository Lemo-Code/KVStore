# netCore — sylar 风格协程网络栈（独立目录）

自包含实现：**utils / thread / log / config / memory / fiber / io / buffer / socket**。

## 设计

- `IOManager` = `Scheduler` + epoll + pipe tickle（无 Reactor/Runtime）
- IO 就绪走 `scheduleNext` 快路径
- 模块划分与 sylar 一致

```
utils → thread → memory → fiber → io → buffer/socket
                    ↘ log → config
```

## 构建

```bash
cmake -B build-netcore -DNETCORE_BUILD=ON -DNETLEMO_BUILD=OFF
cmake --build build-netcore --target bench_netcore_perf -j
```

## 性能测试

```bash
# 快速 smoke
bin/netCore/bench_echo_server --quick

# 标准压测（4 线程 / 64 连接 / 128B / 1000 消息）
bin/netCore/bench_echo_server --mode local --threads 4 --connections 64 --payload 128 --messages 1000

# 全套
cmake --build build-netcore --target bench_netcore_perf
```

## 文件说明

| 文件 | 说明 |
|------|------|
| `src/io/iomanager.cc` | 当前实现：Reactor 逻辑内联，sylar 对外 API |
| `src/io/iomanager_legacy.cc` | 旧版合并实现（有性能回归，仅保留参考） |
| `src/io/reactor.cc` / `include/lemo/io/reactor.h` | 历史参考，不参与编译 |

```cpp
#include "lemo/netcore.h"
#include "lemo/io/iomanager.h"
```
