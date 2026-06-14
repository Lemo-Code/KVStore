# Lemo 配置使用说明

## 一句话

**先声明、后加载、再装配**：代码里用 `Lookup` 注册配置项，`.conf` 文件只覆盖已注册的 key，最后 `log.*` 自动装配到 Logger。

## 加载流程

```
lemo.conf (key = value)
    │
    ▼
PropertyLoader 解析 ──► flat map
    │
    ▼
ConfigCenter::LoadFlatMap
    ├─ 已 Lookup 的 key  → 写入 ConfigVar（生效）
    └─ 未声明的 key      → 忽略（如 ignored.key）
    │
    ▼
LogConfig::Apply
    ├─ root: level / pattern / appender
    └─ log.logger.xxx.*: 命名 logger
```

## 配置文件格式

```ini
# 注释行以 # 开头
log.level = WARN
log.pattern = %m
log.appender = file
log.file.path = /tmp/app.log

# 点分 logger 名
log.logger.com.example.level = DEBUG
log.logger.com.example.additive = false
```

## 加载后输出

`LoadLogConfigFile` / `LoadLogConfigString` 装配完成后会：

1. **stdout** 打印生效配置列表（测试运行时直接可见）
2. **root logger** 写入 `[config] ...` 摘要（输出到 `log.file.path` 或 console）

## 代码用法

```cpp
#include "lemo/config/config.h"

// 常规加载：控制台摘要 + 日志记录
lemo::config::LoadLogConfigFile("lemo.conf");

// 详细调试：逐步打印解析 / 统计 / 装配
lemo::config::LoadLogConfigFileVerbose("lemo.conf");
```

## 演示工具（推荐先看这个）

```bash
cd build
make config_demo
bin/lemo/config/config_demo ../tests/lemo/config/fixtures/lemo_test.conf
```

输出包含 5 步：
1. 文件里解析出哪些 key
2. 哪些生效、哪些被忽略
3. 当前所有 ConfigVar 的值
4. Logger 装配结果
5. 试写一条日志

## 常见 key

| Key | 说明 |
|-----|------|
| `log.level` | root 日志级别 |
| `log.pattern` | 格式，如 `%m` |
| `log.appender` | `console` / `file` / `rolling_file` / `async` |
| `log.file.path` | 文件路径 |
| `log.logger.<name>.level` | 子 logger 级别 |
| `log.logger.<name>.additive` | 是否向父 logger 传播 |

## 自定义业务配置

```cpp
// 1. 先声明
auto port = lemo::config::ConfigCenter::Lookup<int>("server.port", 8080);

// 2. 再加载文件（只覆盖 server.port 等已声明项）
lemo::config::ConfigCenter::LoadFromFile("lemo.conf");

// 3. 使用
int p = port->GetValue();
```

未 `Lookup` 的 key **不会**生效，这是刻意设计（约定优于配置）。
