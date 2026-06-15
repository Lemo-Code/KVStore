# Redis Lab Studio

> AI + Redis + 即时通讯 一体化智能学习平台前端 UI

## 技术选型

| 层级 | 技术 | 说明 |
|------|------|------|
| 框架 | React 19 + TypeScript | 组件化、类型安全 |
| 路由 | React Router 7 | 多页面导航与路由守卫 |
| 构建 | Vite 6 | 极速 HMR 开发体验 |
| 样式 | Tailwind CSS 4 | 原子化 CSS + 设计令牌 |
| 布局 | react-resizable-panels | Navicat 风格可拖拽分栏 |
| 编辑器 | Monaco Editor | VS Code 级命令编辑体验 |
| 动画 | Framer Motion | 面板展开、消息入场动画 |
| 状态 | Zustand + persist | Mock 状态 + 登录持久化 |
| 图标 | Lucide React | 统一线性图标体系 |

## 页面全景

### 公开页面
| 路由 | 页面 | 说明 |
|------|------|------|
| `/` | 落地页 | 产品介绍、功能亮点、学员评价 |
| `/login` | 登录 | 邮箱密码、社交登录、记住我 |
| `/register` | 注册 | 用户名/邮箱/密码、服务条款 |
| `/forgot-password` | 找回密码 | 邮件发送与成功反馈 |
| `/reset-password` | 重置密码 | 新密码设置 |

### 认证流程
| 路由 | 页面 | 说明 |
|------|------|------|
| `/verify-email` | 邮箱验证 | 6 位验证码输入 |
| `/onboarding` | 新手引导 | 学习目标/兴趣/环境 三步向导 |

### 主应用（需登录）
| 路由 | 页面 | 说明 |
|------|------|------|
| `/dashboard` | 仪表盘 | 学习统计、快捷入口、最近动态 |
| `/workspace` | Redis 工作台 | Navicat 风格全屏工作区 |
| `/connections` | 连接管理 | 新建/测试/连接 Redis 实例 |
| `/chat` | 协作通讯 | 学习房间列表 + 即时群聊 |
| `/ai` | AI 导师 | 多模式对话 + 推荐提问 |
| `/learning` | 学习中心 | 课程模块、进度追踪 |
| `/learning/:id` | 课程详情 | 课时目录 + 课程内容 |
| `/profile` | 个人资料 | 头像、成就徽章、信息编辑 |
| `/settings` | 设置 | 通用/外观/通知/安全 |
| `/notifications` | 通知中心 | 协作/AI/课程/系统通知 |

## 快速开始

```bash
cd 前端
npm install
npm run dev
```

访问 http://localhost:5173

**体验路径：**
1. 落地页 → 注册 → 邮箱验证 → 新手引导 → 仪表盘
2. 或直接登录（任意邮箱密码）→ 新手引导 → 进入平台

## 目录结构

```
src/
├── pages/
│   ├── auth/          # 登录、注册、找回/重置密码、邮箱验证
│   ├── landing/       # 落地页
│   ├── onboarding/    # 新手引导
│   ├── dashboard/     # 仪表盘
│   ├── workspace/     # Redis 工作台
│   ├── connections/   # 连接管理
│   ├── chat/          # 协作通讯
│   ├── ai/            # AI 导师
│   ├── learning/      # 学习中心 + 课程详情
│   ├── profile/       # 个人资料
│   ├── settings/      # 设置
│   └── notifications/ # 通知中心
├── components/
│   ├── auth/          # 认证布局、路由守卫
│   ├── layout/        # 主布局、工作台壳
│   ├── ai/ chat/ database/ learning/ ui/
├── stores/            # authStore + appStore
├── router/            # 路由配置
└── types/             # 类型定义
```
