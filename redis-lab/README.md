# RedisLab - Redis 协作学习平台

集 **Redis 可视化管理 + 实时协作交流 + AI 辅助学习** 三位一体的学习平台。

## 架构概览

```
Browser (React) ──→ Go Backend (Gin) ──→ Ledis (Redis-compatible)
                       │
                       ├── Multi-tenant namespace isolation (u:{userID}: prefix)
                       ├── Resource quota enforcement (keys, memory, rate)
                       ├── Real-time chat (WebSocket Hub)
                       └── AI tutor (Claude API SSE streaming)
```

### 核心特性

1. **沙箱化 Redis 环境** — 每个用户的 Key 自动加上命名空间前缀 `u:{userID}:`，多用户共享同一个 Ledis 实例而互不干扰
2. **资源配额管控** — 每用户限制：Key 数量、Value 大小、内存使用、命令频率
3. **Navicat 式 Redis 客户端** — Key 树浏览器、Monaco Editor 命令终端、多类型结果展示
4. **实时聊天室** — WebSocket 驱动的多房间聊天系统，边学边交流
5. **AI Redis 助手** — 基于 Claude API 的流式对话，上下文感知（当前连接状态、最近命令）

## 快速开始

### 前置条件

- Go 1.21+
- Node.js 20+
- Ledis 服务器运行在 `localhost:6379`
- (可选) Claude API Key 用于 AI 功能

### 开发模式

```bash
# 1. 启动 Ledis (在另一个终端)
cd ../ledis/build && ./ledis-server

# 2. 启动 Go 后端
cd server
go run .

# 3. 启动前端
cd web
npm install
npm run dev
```

打开浏览器访问 `http://localhost:3000`

### Docker Compose

```bash
# 设置 AI API Key (可选)
export AI_API_KEY=your-claude-api-key

docker-compose up -d
```

访问 `http://localhost`

## 项目结构

```
redis-lab/
├── docker-compose.yml          # 三容器编排 (Ledis + Backend + Frontend)
├── Makefile
├── README.md
├── nginx/
│   └── nginx.conf              # 前端静态文件 + API/WS 反向代理
├── server/                     # Go 后端
│   ├── main.go                 # 入口
│   ├── config/config.go        # 环境变量配置
│   ├── database/database.go    # GORM + SQLite + 种子数据
│   ├── models/                 # 数据模型
│   │   ├── user.go
│   │   ├── connection.go       # Ledis 连接配置
│   │   ├── quota.go            # 用户资源配额
│   │   ├── room.go             # 聊天室
│   │   ├── message.go          # 聊天消息
│   │   └── conversation.go     # AI 对话
│   ├── services/               # 核心业务逻辑
│   │   ├── auth_service.go     # JWT 认证
│   │   ├── redis_service.go    # Redis 代理 (命名空间 + 配额)
│   │   ├── quota_service.go    # 资源配额管控
│   │   ├── chat_service.go     # WebSocket Hub
│   │   └── ai_service.go       # Claude API 流式调用
│   ├── handlers/               # HTTP/WS 处理器
│   │   ├── auth.go
│   │   ├── connection.go
│   │   ├── redis_exec.go
│   │   ├── quota.go
│   │   ├── chat.go
│   │   └── ai.go
│   ├── middleware/             # JWT + CORS
│   ├── router/router.go       # 路由注册
│   └── Dockerfile
└── web/                        # React 前端
    ├── src/
    │   ├── api/                # Axios API 层
    │   │   ├── client.ts       # JWT 拦截器
    │   │   ├── auth.ts
    │   │   ├── connections.ts
    │   │   ├── redis.ts
    │   │   ├── quota.ts
    │   │   ├── chat.ts
    │   │   └── ai.ts
    │   ├── stores/             # Zustand 状态管理
    │   │   ├── authStore.ts
    │   │   ├── connectionStore.ts
    │   │   ├── chatStore.ts
    │   │   └── aiStore.ts
    │   ├── components/
    │   │   ├── redis/          # KeyExplorer, CommandTerminal, ResultPanel, QuotaBar
    │   │   ├── chat/           # ChatPanel, RoomList, MessageList, MessageInput
    │   │   └── ai/             # AiPanel, AiMessageList, AiInput
    │   ├── pages/              # WorkspacePage, ChatPage, AiPage, LoginPage
    │   └── types/index.ts
    └── Dockerfile
```

## API 文档

### 认证
| Method | Path | Description |
|--------|------|-------------|
| POST | `/api/auth/register` | 注册 |
| POST | `/api/auth/login` | 登录 → JWT |
| GET  | `/api/auth/me` | 当前用户 |

### 配额
| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/quota` | 资源使用状态 |

### 连接管理
| Method | Path | Description |
|--------|------|-------------|
| GET/POST | `/api/connections` | 列表/创建 |
| PUT/DELETE | `/api/connections/:id` | 更新/删除 |
| POST | `/api/connections/:id/test` | 测试连接 |

### Redis 操作
| Method | Path | Description |
|--------|------|-------------|
| POST | `/api/connections/:id/exec` | 执行命令 |
| GET | `/api/connections/:id/keys` | Key 列表 |
| GET | `/api/connections/:id/keys/*` | Key 详情 |
| DELETE | `/api/connections/:id/keys/*` | 删除 Key |
| POST | `/api/connections/:id/flush` | 清空用户命名空间 |

### 聊天
| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/rooms` | 房间列表 |
| GET | `/api/rooms/:id/messages` | 历史消息 |
| WS | `/api/ws/chat` | WebSocket |

### AI 助手
| Method | Path | Description |
|--------|------|-------------|
| POST | `/api/ai/chat` | 发送消息 (SSE) |
| GET | `/api/ai/conversations` | 会话列表 |
| GET | `/api/ai/conversations/:id` | 会话详情 |
| DELETE | `/api/ai/conversations/:id` | 删除会话 |

## 资源配额设计

每个用户默认配额：

| 资源 | 限制 | 说明 |
|------|------|------|
| Max Keys | 100 | 最大 Key 存储数量 |
| Max Value Size | 1 MB | 单个 Value 最大字节数 |
| Max Memory | 100 MB | 估算总内存使用 |
| Rate Limit | 60/min | 每分钟最大命令数 |

`FLUSHDB` 被拦截并替换为仅清空用户自己的命名空间。

危险命令 (`FLUSHALL`, `CONFIG`, `SHUTDOWN`, `DEBUG`, `KEYS *` 全局版等) 在沙箱中被阻止。

## 环境变量

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `SERVER_PORT` | 8080 | 后端端口 |
| `DB_PATH` | ./data/redislab.db | SQLite 路径 |
| `JWT_SECRET` | (内置默认) | JWT 签名密钥 |
| `AI_API_KEY` | (空) | Claude API Key |
| `AI_BASE_URL` | https://api.anthropic.com | AI API 地址 |
| `AI_MODEL` | claude-sonnet-4-6 | 模型名称 |

## 技术栈

| 层次 | 技术 |
|------|------|
| 前端 | React 18 + TypeScript + Ant Design 5 + Monaco Editor + Zustand |
| 后端 | Go + Gin + GORM + SQLite + gorilla/websocket |
| Redis 客户端 | go-redis/v9 |
| AI | Claude Messages API (SSE 流式) |
| 部署 | Docker Compose (Ledis + Go Backend + Nginx Frontend) |
