# Lemo

参考 **sylar** + **log4j**，C++11 分层架构。

## 模块

| 模块 | 说明 | 文档 |
|------|------|------|
| `utils` | 字符串/时间/线程/类型转换 | [docs/utils_architecture.md](docs/utils_architecture.md) |
| `thread` | pthread 封装（Thread/Mutex/Semaphore） | — |
| `fiber` | 协程 + GMP 调度器 + 时间轮 | [docs/fiber_architecture.md](docs/fiber_architecture.md) |
| `io` | Reactor(epoll) + IOManager | [docs/io_architecture.md](docs/io_architecture.md) |
| `log` | 日志核心 | [docs/log_architecture.md](docs/log_architecture.md) |
| `config` | 配置中心 + 属性文件 + 日志装配 | [docs/config_architecture.md](docs/config_architecture.md) |

## 依赖

```
utils
thread → utils
fiber  → thread → utils
io     → fiber → thread → utils
log    → fiber → thread → utils
config → log → …
```

## 总入口

```cpp
#include "lemo/lemo.h"
#include "lemo/fiber/module.h"

lemo::Init("lemo.conf");
lemo::fiber::Scheduler sch(4, true, "main");
sch.start();
LEMO_LOG_INFO(LEMO_LOG_ROOT()) << "started";
```

## Config

详细说明见 [docs/config_user_guide.md](docs/config_user_guide.md)。

完整测试配置：[`tests/lemo/config/fixtures/lemo_test.conf`](../tests/lemo/config/fixtures/lemo_test.conf)

**演示工具（带完整输出，推荐）：**

```bash
make config_demo
bin/lemo/config/config_demo tests/lemo/config/fixtures/lemo_test.conf
```

**静默加载也会输出摘要**：`LoadLogConfigFile` 默认向 stdout 打印生效配置，并写入 root logger（`[config]` 前缀，WARN 级别）。

```cpp
#include "lemo/config/config.h"
lemo::config::LoadLogConfigFile("lemo.conf");
```

**详细调试**：`lemo::config::LoadLogConfigFileVerbose("lemo.conf");`

## 测试

```bash
cmake .. -DLEMO_BUILD=ON
make run_tests_lemo              # 全部 lemo 测试
make run_tests_lemo_fiber        # 仅协程
make run_tests_lemo_config       # 仅配置
```

## 性能基准

```bash
bin/lemo/fiber/bench_scheduler --quick
bin/lemo/log/bench_log_mt_perf
```

## Log 示例

见 [docs/log_architecture.md](docs/log_architecture.md)。
