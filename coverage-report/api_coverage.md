# API 覆盖率报告

> 指标含义：**测试/压测代码中是否调用过该 API**，不是 gcov 行覆盖率。

## 分库汇总

| 库 | 已覆盖 | 总数 | 覆盖率 |
|----|--------|------|--------|
| ledis | 200 | 200 | 100.0% |
| lrpc | 10 | 10 | 100.0% |
| lstl | 29 | 29 | 100.0% |
| zero | 62 | 65 | 95.4% |

## 分模块明细

| 模块 | 已覆盖 | 总数 | 覆盖率 |
|------|--------|------|--------|
| ledis.commands | 161 | 161 | 100.0% |
| ledis.cluster | 16 | 16 | 100.0% |
| ledis.resp_writer | 18 | 18 | 100.0% |
| ledis.resp_parser | 5 | 5 | 100.0% |
| lstl.containers | 19 | 19 | 100.0% |
| lstl.memory | 10 | 10 | 100.0% |
| lrpc.protocol | 10 | 10 | 100.0% |
| zero.byte_buffer | 45 | 46 | 97.8% |
| zero.config | 4 | 6 | 66.7% |
| zero.endian | 4 | 4 | 100.0% |
| zero.timer_wheel | 6 | 6 | 100.0% |
| zero.util | 3 | 3 | 100.0% |
| **TOTAL** | **300** | **304** | **98.7%** |

## 未覆盖 — zero.byte_buffer

- `addCapacity`

## 未覆盖 — zero.config

- `LoadFromYaml`
- `LookupBase`
