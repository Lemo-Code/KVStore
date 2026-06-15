import { AIAssistantPanel } from '@/components/ai/AIAssistantPanel'
import { PageHeader } from '@/components/layout/MainLayout'
import { Card, CardContent } from '@/components/ui/Card'
import { cn } from '@/lib/utils'
import { motion } from 'framer-motion'
import {
  BookOpen,
  Bug,
  Code2,
  History,
  Lightbulb,
  Sparkles,
  Target,
  Zap,
} from 'lucide-react'
import { useState } from 'react'

const aiModes = [
  { id: 'tutor', label: '学习辅导', icon: BookOpen, desc: '概念讲解与知识梳理', color: 'text-accent-purple', bg: 'bg-accent-purple/10' },
  { id: 'debug', label: '命令调试', icon: Bug, desc: '分析命令错误与优化', color: 'text-accent-red', bg: 'bg-accent-red/10' },
  { id: 'design', label: '架构设计', icon: Target, desc: '数据结构与方案建议', color: 'text-accent-teal', bg: 'bg-accent-teal/10' },
  { id: 'practice', label: '练习生成', icon: Code2, desc: '自动生成练习题', color: 'text-accent-amber', bg: 'bg-accent-amber/10' },
]

const prompts = [
  { icon: Lightbulb, text: '解释 Redis 持久化 RDB 和 AOF 的区别', category: '基础' },
  { icon: Zap, text: '如何用 Redis 实现分布式锁？', category: '实战' },
  { icon: Code2, text: '分析我的 leaderboard:game 键是否设计合理', category: '分析' },
  { icon: Sparkles, text: '生成 5 道 Hash 数据结构练习题', category: '练习' },
  { icon: Target, text: 'Sorted Set 如何实现实时排行榜？', category: '实战' },
  { icon: BookOpen, text: 'Redis 集群与哨兵的区别是什么？', category: '架构' },
]

const recentSessions = [
  { id: 1, title: 'ZSet 排行榜设计', time: '今天 14:30', mode: 'design' },
  { id: 2, title: 'HGETALL 命令错误排查', time: '昨天 10:15', mode: 'debug' },
  { id: 3, title: '持久化机制对比', time: '2 天前', mode: 'tutor' },
]

export default function AIPage() {
  const [mode, setMode] = useState('tutor')
  const [showHistory, setShowHistory] = useState(false)

  return (
    <div className="flex h-full flex-col">
      <PageHeader
        title="AI Redis 导师"
        subtitle="上下文感知的智能辅导，基于你的 Redis 环境提供个性化建议"
        actions={
          <button
            onClick={() => setShowHistory(!showHistory)}
            className="flex items-center gap-1.5 text-xs text-text-muted hover:text-text-secondary transition-colors"
          >
            <History size={14} /> 历史对话
          </button>
        }
      />
      <div className="flex flex-1 min-h-0">
        {/* Left sidebar */}
        <div className="w-64 shrink-0 border-r border-border-subtle bg-surface-1 flex flex-col">
          <div className="p-3 border-b border-border-subtle">
            <div className="text-[10px] font-semibold text-text-muted uppercase tracking-wider mb-2">
              对话模式
            </div>
            <div className="space-y-1">
              {aiModes.map((m) => (
                <button
                  key={m.id}
                  onClick={() => setMode(m.id)}
                  className={cn(
                    'w-full flex items-center gap-2.5 rounded-lg px-2.5 py-2 text-left transition-all group',
                    mode === m.id
                      ? 'bg-accent-purple/10 border border-accent-purple/20'
                      : 'hover:bg-surface-2 border border-transparent',
                  )}
                >
                  <div className={cn('flex h-8 w-8 items-center justify-center rounded-lg', m.bg)}>
                    <m.icon size={15} className={m.color} />
                  </div>
                  <div className="flex-1 min-w-0">
                    <div className="text-xs font-medium">{m.label}</div>
                    <div className="text-[9px] text-text-muted truncate">{m.desc}</div>
                  </div>
                </button>
              ))}
            </div>
          </div>

          <div className="p-3 flex-1 overflow-y-auto">
            <div className="text-[10px] font-semibold text-text-muted uppercase tracking-wider mb-2 flex items-center justify-between">
              推荐提问
              <span className="text-accent-purple text-[9px]">基于你的学习</span>
            </div>
            <div className="space-y-1.5">
              {prompts.map((p, i) => (
                <motion.button
                  key={i}
                  whileHover={{ x: 2 }}
                  className="w-full flex items-start gap-2 rounded-lg border border-border-subtle bg-surface-2 px-2.5 py-2 text-left hover:border-accent-purple/30 transition-colors group"
                >
                  <div className="flex h-6 w-6 items-center justify-center rounded bg-surface-3 shrink-0 mt-0.5">
                    <p.icon size={12} className="text-accent-purple" />
                  </div>
                  <div className="flex-1 min-w-0">
                    <span className="text-[11px] text-text-secondary leading-relaxed block">{p.text}</span>
                    <span className="text-[9px] text-text-muted mt-0.5 block">{p.category}</span>
                  </div>
                </motion.button>
              ))}
            </div>
          </div>

          <div className="border-t border-border-subtle p-3">
            <Card>
              <CardContent className="p-3 text-center">
                <div className="text-[10px] text-text-muted">今日剩余</div>
                <div className="flex items-baseline justify-center gap-1 mt-1">
                  <span className="text-2xl font-bold text-accent-purple">48</span>
                  <span className="text-xs text-text-muted">/ 50</span>
                </div>
                <div className="text-[9px] text-text-muted mt-0.5">次 AI 对话</div>
                <div className="mt-2 h-1 bg-surface-3 rounded-full overflow-hidden">
                  <div className="h-full w-[96%] bg-accent-purple rounded-full" />
                </div>
              </CardContent>
            </Card>
          </div>
        </div>

        {/* Chat */}
        <div className="flex-1 min-w-0">
          <AIAssistantPanel />
        </div>

        {/* Right sidebar - Recent sessions */}
        {showHistory && (
          <div className="w-64 shrink-0 border-l border-border-subtle bg-surface-1 p-3 overflow-y-auto">
            <div className="text-[10px] font-semibold text-text-muted uppercase tracking-wider mb-3">
              最近对话
            </div>
            <div className="space-y-1.5">
              {recentSessions.map((session) => {
                const modeInfo = aiModes.find((m) => m.id === session.mode)!
                return (
                  <button
                    key={session.id}
                    className="w-full text-left rounded-lg border border-border-subtle bg-surface-2 p-3 hover:border-border transition-colors"
                  >
                    <div className="flex items-center gap-2 mb-1.5">
                      <modeInfo.icon size={13} className={modeInfo.color} />
                      <span className="text-xs font-medium truncate">{session.title}</span>
                    </div>
                    <div className="text-[10px] text-text-muted">{session.time}</div>
                  </button>
                )
              })}
            </div>
            <button className="mt-3 w-full text-xs text-accent-purple hover:underline">
              查看全部对话记录 →
            </button>
          </div>
        )}
      </div>
    </div>
  )
}
