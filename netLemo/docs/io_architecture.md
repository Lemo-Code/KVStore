# IO 模块

`lemo::io` 提供 epoll Reactor 与 `IOManager`（Scheduler + Reactor）。

## 组件

| 类 | 说明 |
|----|------|
| `Reactor` | epoll + pipe tickle，独立事件循环 |
| `IOManager` | 继承 `fiber::Scheduler`，`idle()` 中 `reactor.poll()` |
| `FdContext` / `FdManager` | fd 元数据（socket 识别、超时），供后续 hook 使用 |

## 用法

```cpp
#include "lemo/io/module.h"

lemo::io::IOManager iom(4, true, "main");
// 构造后已 start()

iom.schedule([]() {
  // addEvent(fd, READ) + YieldToHold() ...
});

iom.stop();
```

## 测试

```bash
make run_tests_lemo_io
```

## 后续

- `hook`：阻塞 syscall 自动 yield 到 Reactor
- `Runtime` 门面：Scheduler + Reactor 组合（替代直接继承）
