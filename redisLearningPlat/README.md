# Learn Redis - Redis 智能学习平台

融合 **IM 即时通讯 + 知识导航 + AI 智能助手** 的 Redis 学习平台。

## 技术栈

| 层级 | 技术 |
|------|------|
| 前端框架 | React 18 + TypeScript + Vite 5 |
| UI 组件库 | shadcn/ui (Radix primitives) + Tailwind CSS |
| 状态管理 | Zustand 4 |
| 后端框架 | Python FastAPI |
| 数据库 | SQLite (开发) / PostgreSQL (生产) |
| AI | OpenAI 兼容 API (多 LLM 支持) |
| 实时通信 | WebSocket (IM) + SSE (AI 流式) |

## 快速开始

```bash
# 安装前端依赖
cd frontend && pnpm install

# 安装后端依赖
cd backend && pip install -r requirements.txt

# 初始化数据库
cd backend && alembic upgrade head

# 启动开发服务器
# 终端1: 后端
cd backend && uvicorn app.main:app --reload --port 8000

# 终端2: 前端
cd frontend && pnpm dev

# 或使用 Docker Compose
docker-compose up
```

## 默认测试账号

- 用户名: `admin`
- 密码: `admin123`

## 项目结构

```
learn_redis/
├── frontend/          # React 前端
├── backend/           # Python FastAPI 后端
├── shared/            # 共享类型定义
└── docker-compose.yml # 容器化部署
```
