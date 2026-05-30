# Net 日志模块

工业级异步日志实现，参考 Sylar API/格式，独立架构。

## 架构概览

```
业务线程 (多)
    │
    ├─ FormatLine ─┐
    │               │
    ├─ FormatLine ─┼── thread_local buf ── std::string (payload)
    │               │
    └─ FormatLine ─┘
                   │
                   ▼
         enqueue(AsyncLogRecord)
                   │
                   ▼
    ┌─────────────────────────────┐
    │   LockFreeMpscQueue<string> │  ◄── 全局单队列
    └─────────────────────────────┘
                   │
                   ▼ (worker 单线程 drain)
         FileWriter::append ── ByteBuffer ── write(fd)
```

## 核心优化

| 优化项 | 实现 |
|--------|------|
| **FormatItem 直写 string** | `appendTo()` 替代每项 `ostringstream` |
| **LogMessageStream 快速路径** | `char*`/`string`/整数直接 append，仅浮点/自定义类型用 `ostringstream` |
| **FormatLine 零拷贝返回** | `thread_local buf` + `std::move` 返回 |
| **全局单队列** | 避免按日切文件时 channel map 无限增长 |
| **MPSC 消费串行化** | `drain_mtx_` 保证单消费者约束 |
| **ByteBuffer 批量写** | 256KB 缓冲，64KB 阈值刷盘 |

## 配置 (config.h)

- `NET_LOG_ASYNC_BUF_BYTES` = 256KB（写缓冲容量）
- `NET_LOG_ASYNC_FLUSH_THRESHOLD` = 64KB（刷盘阈值）
- `NET_LOG_ASYNC_FLUSH_MS` = 800ms（后台轮询周期）
- `NET_LOG_DEGRADE_MODE` = 0/1（软上限丢弃开关）

## Sink 方案

| 方案 | 类/工厂 | 说明 |
|------|---------|------|
| 固定单文件 | `FixedFile` | 始终追加同一文件 |
| 按日切分 | `TimeRotate` | `base.YYYY-MM-DD` 顺序生成 |
| 链式轮转 | `SizeChain` | `path` → `path.1` → `path.2` |
| 环形 N 槽 | `CircularRing` | 写满槽 i 切 i+1，回 0 truncate 覆盖 |

## 边界保证

- **同步**：持锁内完成轮转判断 + 写入，单条日志不跨文件
- **异步**：入队前按 `event->getTime()` 解析目标路径，后台仅追加
- **轮转/truncate**：`flushFile()` 排空该路径队列 + 刷缓冲，`reopenFile()` 重开 fd

## 性能分析

多线程压测方法、瓶颈定位（Logger 锁 / MPSC / 单 worker）、shared 与 sharded 对照数据见：

- [PERFORMANCE_ANALYSIS.md](./PERFORMANCE_ANALYSIS.md)
- 压测工具：`tests/net/bench_log_perf.cc`（`bin/net/bench_log_perf`）
