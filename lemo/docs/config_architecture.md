# Lemo Config 架构

## 定位

`lemo::config` 提供**约定优于配置**的运行时配置中心，支持从 `.conf` 属性文件加载，并驱动 log 等模块装配。

## 设计原则（对齐 sylar / net）

1. **先声明后加载**：只有 `ConfigCenter::Lookup` 注册过的 key 才会被文件覆盖
2. **key 小写化**：`Log.Level` 与 `log.level` 等价
3. **无 YAML 依赖**：v1 使用 `key = value` 属性文件，`.` 表示层级
4. **类型安全**：`ConfigVar<T>` + `LexicalCast` 做序列化

## 分层

```
┌─────────────────────────────────────┐
│  log_config          日志装配桥接    │
├─────────────────────────────────────┤
│  config_center       注册 / 加载     │
├─────────────────────────────────────┤
│  property_loader     解析 .conf      │
├─────────────────────────────────────┤
│  config_var          类型化配置项    │
└─────────────────────────────────────┘
         ↓ 依赖
      lemo::utils
```

## 属性文件格式

```ini
# lemo.conf
log.level = INFO
log.pattern = %d{%Y-%m-%d %H:%M:%S} [%p] %m%n
log.appender = file
log.file.path = /tmp/lemo.log

log.logger.com.example.level = DEBUG
log.logger.com.example.additive = false
```

## 日志配置 key 约定

| Key | 类型 | 说明 |
|-----|------|------|
| `log.level` | string | root 有效级别 |
| `log.pattern` | string | root PatternLayout |
| `log.appender` | string | `console` / `file` / `rolling_file` / `async` |
| `log.file.path` | string | file 路径 |
| `log.file.max_bytes` | uint64 | rolling 阈值 |
| `log.file.max_files` | uint32 | rolling 保留数 |
| `log.file.roll_interval` | string | none / day / hour |
| `log.async.delegate` | string | async 内层 appender |
| `log.logger.<name>.level` | string | 命名 logger 级别 |
| `log.logger.<name>.additive` | bool | 是否继承父 appender |

## 加载流程

```
LoadFromFile(path)
  → PropertyLoader 解析为 flat map
  → 对每个 (key,value)：lookupBase(key)->fromString(value)
  → LogConfig::Apply() 读取 log.* ConfigVar，装配 LoggerRepository
```

## 依赖规则

```
log_config → config_center → property_loader → config_var → utils
log 模块在 Apply 阶段调用 log_config，config 不反向依赖 log 实现细节（仅 Apply 时 link）
```
