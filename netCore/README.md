# netCore — 统一协程网络栈（整合 lemo / netLemo / sylar 最优实现）

自包含实现：**utils / thread / log / config / memory / fiber / io / buffer / socket**。

## 设计（统一版）

- `IOManager` = `Scheduler` + `Reactor`（epoll + pipe tickle）
- IO 就绪走 `scheduleNext` 快路径（netLemo 优化）
- hook 无超时 `retry_fast` 快路径
- idle spin 256 轮（netLemo 优化）
- `FdManager` 独立管理 fd 元数据（超时、socket 标记）
- **唯一推荐构建目标**，替代 lemo / netLemo / module/net 并行版本

```
utils → thread → memory → fiber → io → buffer/socket
                    ↘ log → config
```

## 构建

```bash
# 仅构建统一版 netCore（推荐）
cmake -B build -DNETCORE_BUILD=ON -DNETLEMO_BUILD=OFF -DLEMO_BUILD=OFF -DLSTL_BUILD_NET_LOG=OFF
cmake --build build -j
cmake --build build --target run_tests_netcore_io
cmake --build build --target bench_netcore_perf
```

## 性能测试

```bash
# 快速 smoke
bin/netCore/bench_echo_server --quick

# 标准压测（4 线程 / 64 连接 / 128B / 1000 消息）
bin/netCore/bench_echo_server --mode local --threads 4 --connections 64 --payload 128 --messages 1000

# 全套
cmake --build build --target bench_netcore_perf
```

## 历史文件（不参与编译，仅参考）

| 文件 | 说明 |
|------|------|
| `src/io/iomanager_legacy.cc` | 旧版内联 epoll（有性能回归） |
| `src/io/iomanager_reactor_wrapper.cc` | 过渡版 |
| `src/io/epoll_context.cc` | 已合并到 reactor.cc |
| `src/io/iomanager_inlined.cc.bak` | 内联尝试备份 |

```cpp
#include "lemo/netcore.h"
#include "lemo/io/iomanager.h"
```
