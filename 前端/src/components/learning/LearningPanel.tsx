import { Badge } from '@/components/ui/Badge'
import { Button } from '@/components/ui/Button'
import { mockLearningModules } from '@/stores/appStore'
import { cn } from '@/lib/utils'
import type { LearningModule } from '@/types'
import { motion } from 'framer-motion'
import {
  Award,
  BookOpen,
  ChevronRight,
  Clock,
  Flame,
  Star,
  Target,
  Trophy,
} from 'lucide-react'

const difficultyConfig = {
  beginner: { label: '入门', color: 'text-success', bg: 'bg-success/10 border-success/20' },
  intermediate: { label: '进阶', color: 'text-accent-amber', bg: 'bg-accent-amber/10 border-accent-amber/20' },
  advanced: { label: '高级', color: 'text-accent-red', bg: 'bg-accent-red/10 border-accent-red/20' },
}

function ModuleCard({ module, index }: { module: LearningModule; index: number }) {
  const diff = difficultyConfig[module.difficulty]

  return (
    <motion.div
      initial={{ opacity: 0, y: 12 }}
      animate={{ opacity: 1, y: 0 }}
      transition={{ delay: index * 0.08 }}
      className="rounded-lg border border-border-subtle bg-surface-2 p-3 hover:border-border transition-colors cursor-pointer group"
    >
      <div className="flex items-start justify-between mb-2">
        <div className="flex-1 min-w-0">
          <h4 className="text-xs font-semibold text-text-primary truncate">{module.title}</h4>
          <p className="text-[10px] text-text-muted mt-0.5 line-clamp-2">{module.description}</p>
        </div>
        <ChevronRight
          size={14}
          className="text-text-muted group-hover:text-text-secondary shrink-0 mt-0.5 transition-colors"
        />
      </div>

      <div className="flex items-center gap-2 mb-2.5">
        <span className={cn('rounded border px-1.5 py-0.5 text-[9px] font-semibold', diff.bg, diff.color)}>
          {diff.label}
        </span>
        <span className="text-[9px] text-text-muted flex items-center gap-0.5">
          <BookOpen size={9} />
          {module.completedLessons}/{module.lessons} 课时
        </span>
      </div>

      {/* Progress bar */}
      <div className="flex items-center gap-2">
        <div className="flex-1 h-1.5 rounded-full bg-surface-4 overflow-hidden">
          <div
            className={cn(
              'h-full rounded-full transition-all duration-500',
              module.progress === 100
                ? 'bg-success'
                : module.progress > 0
                  ? 'bg-accent-amber'
                  : 'bg-surface-hover',
            )}
            style={{ width: `${module.progress}%` }}
          />
        </div>
        <span className="text-[10px] font-mono font-semibold text-text-secondary w-8 text-right">
          {module.progress}%
        </span>
      </div>

      <div className="flex flex-wrap gap-1 mt-2">
        {module.tags.map((tag) => (
          <span
            key={tag}
            className="rounded bg-surface-3 px-1.5 py-0.5 text-[9px] font-mono text-text-muted"
          >
            {tag}
          </span>
        ))}
      </div>
    </motion.div>
  )
}

export function LearningPanel() {
  const totalProgress = Math.round(
    mockLearningModules.reduce((a, m) => a + m.progress, 0) / mockLearningModules.length,
  )

  return (
    <div className="flex h-full flex-col">
      {/* Header stats */}
      <div className="border-b border-border-subtle px-4 py-3">
        <div className="flex items-center gap-2 mb-3">
          <Trophy size={14} className="text-accent-amber" />
          <span className="text-sm font-semibold">学习进度</span>
        </div>

        <div className="grid grid-cols-3 gap-2">
          {[
            { icon: Target, label: '总进度', value: `${totalProgress}%`, color: 'text-accent-amber' },
            { icon: Flame, label: '连续学习', value: '7 天', color: 'text-accent-red' },
            { icon: Star, label: '获得徽章', value: '3 枚', color: 'text-accent-purple' },
          ].map(({ icon: Icon, label, value, color }) => (
            <div
              key={label}
              className="rounded-lg bg-surface-2 border border-border-subtle p-2 text-center"
            >
              <Icon size={14} className={cn('mx-auto mb-1', color)} />
              <div className="text-sm font-bold text-text-primary">{value}</div>
              <div className="text-[9px] text-text-muted">{label}</div>
            </div>
          ))}
        </div>
      </div>

      {/* Today's goal */}
      <div className="mx-4 my-3 rounded-lg border border-accent-teal/20 bg-accent-teal/5 p-3">
        <div className="flex items-center gap-2 mb-1.5">
          <Award size={13} className="text-accent-teal" />
          <span className="text-xs font-semibold text-accent-teal">今日学习目标</span>
        </div>
        <p className="text-[10px] text-text-secondary leading-relaxed">
          完成「数据结构深度解析」第 9 课：Sorted Set 排行榜实战
        </p>
        <div className="flex items-center gap-2 mt-2">
          <Clock size={10} className="text-text-muted" />
          <span className="text-[9px] text-text-muted">预计 25 分钟</span>
          <Button variant="accent" size="sm" className="ml-auto !h-6 !text-[10px] !bg-accent-teal hover:!bg-accent-teal/80 shadow-accent-teal/20">
            开始学习
          </Button>
        </div>
      </div>

      {/* Modules */}
      <div className="flex-1 overflow-y-auto px-4 pb-4 space-y-2">
        <div className="flex items-center justify-between mb-1">
          <span className="text-[10px] font-semibold text-text-muted uppercase tracking-wider">
            课程模块
          </span>
          <Badge>{mockLearningModules.length} 个模块</Badge>
        </div>
        {mockLearningModules.map((mod, i) => (
          <ModuleCard key={mod.id} module={mod} index={i} />
        ))}
      </div>
    </div>
  )
}
