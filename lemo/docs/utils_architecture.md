# Lemo Utils 架构

## 定位

`lemo::utils` 是 lemo 的**基础工具层**，供 config / log / 后续 runtime 复用，不依赖业务模块。

## 设计原则

- **零业务依赖**：utils 不 include config / log
- **C++11 + 无第三方库**：仅标准库 + pthread（线程 ID）
- **header 优先**：轻量工具放头文件；涉及系统调用放 `.cc`

## 模块划分

```
include/lemo/utils/
├── noncopyable.h      禁止拷贝/移动基类
├── lexical_cast.h     字符串 ↔ 标量（类似 boost::lexical_cast）
├── string_util.h      trim / split / to_lower
├── thread_util.h      线程 ID / 线程名
├── time_util.h        单调时钟、进程 elapsed
└── utils.h            统一入口
```

## 依赖

```
config → utils
log    → utils (+ config)
runtime(未来) → utils
fiber → thread → utils（已实现，见 fiber_architecture.md）
```

## 与 log/util 的关系

原 `log/util.cc` 中的 `GetThreadId` / `GetElapse` 等迁入 `utils`；`lemo/log/log.h` 保留同名 inline 转发，兼容现有宏。
