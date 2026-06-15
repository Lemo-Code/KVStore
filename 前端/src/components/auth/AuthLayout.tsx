import { cn } from '@/lib/utils'
import { motion } from 'framer-motion'
import {
  Bot,
  Database,
  MessageCircle,
  Sparkles,
  Zap,
} from 'lucide-react'
import type { ReactNode } from 'react'

interface AuthLayoutProps {
  children: ReactNode
  title: string
  subtitle: string
}

const features = [
  {
    icon: Database,
    title: '可视化 Redis 管理',
    desc: 'Navicat 级连接树、键浏览与命令控制台',
    color: 'text-accent-red',
    bg: 'bg-accent-red/10',
  },
  {
    icon: Bot,
    title: 'AI 智能导师',
    desc: '上下文感知的命令分析与个性化学习路径',
    color: 'text-accent-purple',
    bg: 'bg-accent-purple/10',
  },
  {
    icon: MessageCircle,
    title: '实时协作学习',
    desc: '学习室群聊、代码共享与导师在线答疑',
    color: 'text-accent-teal',
    bg: 'bg-accent-teal/10',
  },
]

export function AuthLayout({ children, title, subtitle }: AuthLayoutProps) {
  return (
    <div className="flex h-full min-h-0">
      {/* Left brand panel */}
      <div className="hidden lg:flex lg:w-[480px] xl:w-[540px] shrink-0 flex-col justify-between relative overflow-hidden bg-surface-1 border-r border-border-subtle">
        <div className="absolute inset-0 bg-gradient-to-br from-accent-red/8 via-transparent to-accent-purple/8" />
        <div className="absolute top-0 right-0 w-96 h-96 bg-accent-red/5 rounded-full blur-3xl -translate-y-1/2 translate-x-1/2" />
        <div className="absolute bottom-0 left-0 w-80 h-80 bg-accent-purple/5 rounded-full blur-3xl translate-y-1/2 -translate-x-1/2" />

        <div className="relative p-10">
          <div className="flex items-center gap-3 mb-16">
            <div className="flex h-10 w-10 items-center justify-center rounded-xl bg-gradient-to-br from-accent-red to-accent-red-dim shadow-lg shadow-accent-red/30">
              <Sparkles size={20} className="text-white" />
            </div>
            <div>
              <div className="text-lg font-bold tracking-tight">
                Redis Lab <span className="text-accent-red">Studio</span>
              </div>
              <div className="text-[11px] text-text-muted">AI + Redis + 即时通讯 学习平台</div>
            </div>
          </div>

          <div className="space-y-6">
            {features.map((f, i) => (
              <motion.div
                key={f.title}
                initial={{ opacity: 0, x: -20 }}
                animate={{ opacity: 1, x: 0 }}
                transition={{ delay: 0.1 + i * 0.1 }}
                className="flex gap-4"
              >
                <div className={cn('flex h-10 w-10 shrink-0 items-center justify-center rounded-lg', f.bg)}>
                  <f.icon size={18} className={f.color} />
                </div>
                <div>
                  <h3 className="text-sm font-semibold text-text-primary">{f.title}</h3>
                  <p className="text-xs text-text-muted mt-0.5 leading-relaxed">{f.desc}</p>
                </div>
              </motion.div>
            ))}
          </div>
        </div>

        <div className="relative p-10">
          <div className="rounded-xl border border-border-subtle bg-surface-2/50 p-4 backdrop-blur-sm">
            <div className="flex items-center gap-2 mb-2">
              <Zap size={14} className="text-accent-amber" />
              <span className="text-xs font-semibold text-accent-amber">平台数据</span>
            </div>
            <div className="grid grid-cols-3 gap-4">
              {[
                { label: '注册学员', value: '12,480+' },
                { label: 'Redis 实验', value: '86 万+' },
                { label: 'AI 答疑', value: '240 万+' },
              ].map((s) => (
                <div key={s.label}>
                  <div className="text-lg font-bold text-text-primary">{s.value}</div>
                  <div className="text-[10px] text-text-muted">{s.label}</div>
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>

      {/* Right form panel */}
      <div className="flex-1 flex flex-col min-w-0 overflow-y-auto">
        <div className="flex-1 flex items-center justify-center p-6 sm:p-10">
          <motion.div
            initial={{ opacity: 0, y: 16 }}
            animate={{ opacity: 1, y: 0 }}
            className="w-full max-w-md"
          >
            {/* Mobile brand */}
            <div className="lg:hidden flex items-center gap-2 mb-8">
              <div className="flex h-8 w-8 items-center justify-center rounded-lg bg-gradient-to-br from-accent-red to-accent-red-dim">
                <Sparkles size={14} className="text-white" />
              </div>
              <span className="font-bold">Redis Lab Studio</span>
            </div>

            <div className="mb-8">
              <h1 className="text-2xl font-bold tracking-tight text-text-primary">{title}</h1>
              <p className="text-sm text-text-muted mt-1.5">{subtitle}</p>
            </div>

            {children}
          </motion.div>
        </div>
      </div>
    </div>
  )
}

export function SocialLoginButtons() {
  return (
    <div className="space-y-3">
      <div className="relative">
        <div className="absolute inset-0 flex items-center">
          <div className="w-full border-t border-border-subtle" />
        </div>
        <div className="relative flex justify-center text-[10px] uppercase tracking-wider">
          <span className="bg-surface-0 px-3 text-text-muted">或使用以下方式</span>
        </div>
      </div>
      <div className="grid grid-cols-3 gap-2">
        {['GitHub', '微信', '企业微信'].map((provider) => (
          <button
            key={provider}
            type="button"
            className="flex items-center justify-center gap-1.5 rounded-lg border border-border-subtle bg-surface-1 py-2.5 text-xs text-text-secondary hover:bg-surface-2 hover:text-text-primary transition-colors"
          >
            {provider}
          </button>
        ))}
      </div>
    </div>
  )
}
