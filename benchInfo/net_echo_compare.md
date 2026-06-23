# TCP Echo QPS: Epoll vs Libevent

单位: req/s | CPU: 5 | 线程轴: 1, 2, 4

| 对比项 | t=1 | t=2 | t=4 |
|---|---:|---:|---:|
| 01 raw epoll   | 27.13K | 54.69K | 137.32K |
| 02 libevent     | 32.15K | 51.30K | 90.50K |
