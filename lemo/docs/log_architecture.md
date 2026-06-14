# Lemo Log 架构

## 设计目标

- **分层清晰**：core → layout → appenders → spi → manager，单向依赖
- **开闭原则**：新增 Appender 只注册工厂，不改 Logger / Dispatcher
- **组合优于继承**：异步 = `AsyncAppender` 包装任意 Appender，不在 Logger 上挂 async 标志
- **log4j 语义**：Logger 层级、EffectiveLevel、Additivity、MDC
- **sylar 兼容**：Pattern 格式、LogEventWrap RAII、宏用法不变

## 分层

```
┌─────────────────────────────────────────┐
│  log.h          宏 / 便捷入口            │
├─────────────────────────────────────────┤
│  manager        LoggerRepository        │
├─────────────────────────────────────────┤
│  spi            AppenderRegistry        │
├─────────────────────────────────────────┤
│  appenders      Console / File / Rolling│
│                 AsyncAppender(装饰器)    │
├─────────────────────────────────────────┤
│  async          Dispatcher 刷盘线程      │
├─────────────────────────────────────────┤
│  layout         PatternLayout           │
├─────────────────────────────────────────┤
│  core           Level Record Appender   │
│                 Logger Filter           │
└─────────────────────────────────────────┘
```

## 依赖规则

```
manager → spi → appenders → async → layout → core
mdc → core
禁止：core/layout 依赖 appenders/manager
```

## 扩展 Appender

```cpp
// 1. 实现 Appender 子类
class MyAppender : public Appender { ... };

// 2. 注册（静态初始化或模块 init）
AppenderRegistry::Instance().Register(
    "my", [](const AppenderConfig& cfg) {
      return Appender::ptr(new MyAppender(cfg.Get("path")));
    });

// 3. 配置或代码创建
auto a = AppenderRegistry::Instance().Create("my", {{"path", "/tmp/x.log"}});
logger->AddAppender(a);
```

## 异步模型

同步路径：`Logger → Appender::Append → IO`

异步路径：`Logger → AsyncAppender::Append → 队列 → Dispatcher 线程 → delegate->Append`

Logger **不感知** sync/async；由装配阶段决定。

## 落盘与缓冲（Server 场景）

| 路径 | 行为 | 内存 |
|------|------|------|
| **sync + FileAppender** | `ofstream` 用户态缓冲（通常几 KB），满则 `write` 进内核；**不**每条 `flush()` | 有界，不会随日志条数线性涨 |
| **显式 `Flush()`** | 把用户态 + 尽量推进内核缓冲，便于排查或优雅退出 | — |
| **async** | caller 侧 TLS 批量入队 → 全局队列 → worker 写 delegate | 队列可积压：生产远快于写盘时 **deque 会涨**，需控制速率或定期 `Flush()` / 限流 |

要点：

- **落盘 ≠ 每条 flush**：多数场景依赖 stream 缓冲 + OS page cache；crash 可能丢最近未 flush 的几 KB。
- **每条 flush 的问题在 CPU/syscall**，不是省内存；Server 高 QPS 下应避免。
- **Rolling** 仅在 roll 前 `flush()`（保证 rename 前数据落盘），日常 Append 不 flush。

推荐：线上 file/sync 按周期或按条数批量 flush（后续可配置）；调试/退出前 `logger->Flush()`。
