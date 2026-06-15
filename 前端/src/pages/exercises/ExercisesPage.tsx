import { PageContent, PageHeader } from '@/components/layout/MainLayout'
import { Badge } from '@/components/ui/Badge'
import { Button } from '@/components/ui/Button'
import { EmptyState } from '@/components/ui/EmptyState'
import { usePlatformStore } from '@/stores/platformStore'
import { cn } from '@/lib/utils'
import { motion } from 'framer-motion'
import { Link } from 'react-router-dom'
import {
  Bug,
  CheckCircle2,
  ChevronRight,
  Code2,
  Filter,
  Target,
} from 'lucide-react'
import { useState } from 'react'

const difficultyMap = {
  easy: { label: '简单', variant: 'success' as const },
  medium: { label: '中等', variant: 'warning' as const },
  hard: { label: '困难', variant: 'type' as const },
}

const typeMap = {
  command: { label: '命令练习', icon: Code2, color: 'text-accent-teal' },
  design: { label: '设计题', icon: Target, color: 'text-accent-purple' },
  debug: { label: '调试题', icon: Bug, color: 'text-accent-red' },
}

export default function ExercisesPage() {
  const exercises = usePlatformStore((s) => s.exercises)
  const [filter, setFilter] = useState<'all' | 'pending' | 'done'>('all')
  const [difficulty, setDifficulty] = useState<string>('all')

  const filtered = exercises.filter((e) => {
    if (filter === 'pending' && e.completed) return false
    if (filter === 'done' && !e.completed) return false
    if (difficulty !== 'all' && e.difficulty !== difficulty) return false
    return true
  })

  const completed = exercises.filter((e) => e.completed).length

  return (
    <>
      <PageHeader
        title="练习场"
        subtitle={`${completed}/${exercises.length} 已完成 · 动手实践巩固 Redis 技能`}
        actions={
          <Link to="/workspace">
            <Button variant="outline" size="sm">
              <Code2 size={14} />
              打开工作台
            </Button>
          </Link>
        }
      />
      <PageContent>
        <div className="flex flex-wrap items-center gap-2 mb-6">
          {(['all', 'pending', 'done'] as const).map((f) => (
            <button
              key={f}
              onClick={() => setFilter(f)}
              className={cn(
                'rounded-full px-3 py-1 text-xs font-medium transition-colors',
                filter === f ? 'bg-accent-red/15 text-accent-red' : 'text-text-muted hover:text-text-secondary',
              )}
            >
              {f === 'all' ? '全部' : f === 'pending' ? '待完成' : '已完成'}
            </button>
          ))}
          <Filter size={12} className="text-text-muted ml-2" />
          {['all', 'easy', 'medium', 'hard'].map((d) => (
            <button
              key={d}
              onClick={() => setDifficulty(d)}
              className={cn(
                'rounded-full px-2.5 py-1 text-[10px] font-medium transition-colors',
                difficulty === d ? 'bg-surface-4 text-text-primary' : 'text-text-muted',
              )}
            >
              {d === 'all' ? '全部难度' : difficultyMap[d as keyof typeof difficultyMap]?.label}
            </button>
          ))}
        </div>

        {filtered.length === 0 ? (
          <EmptyState
            icon={Filter}
            title="没有符合条件的练习"
            description="试试切换筛选条件或难度，查看更多练习题。"
            action={{
              label: '重置筛选',
              onClick: () => {
                setFilter('all')
                setDifficulty('all')
              },
            }}
          />
        ) : (
        <div className="grid md:grid-cols-2 gap-4">
          {filtered.map((ex, i) => {
            const diff = difficultyMap[ex.difficulty]
            const type = typeMap[ex.type]
            const TypeIcon = type.icon
            return (
              <motion.div
                key={ex.id}
                initial={{ opacity: 0, y: 10 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ delay: i * 0.04 }}
              >
                <Link
                  to={`/exercises/${ex.id}`}
                  className="block rounded-xl border border-border-subtle bg-surface-1 p-5 hover:border-border hover:bg-surface-2 transition-all group"
                >
                  <div className="flex items-start justify-between mb-3">
                    <div className="flex items-center gap-2">
                      <div className="flex h-9 w-9 items-center justify-center rounded-lg bg-surface-3">
                        <TypeIcon size={16} className={type.color} />
                      </div>
                      <div>
                        <div className="flex items-center gap-2">
                          <span className="text-sm font-semibold group-hover:text-accent-red transition-colors">
                            {ex.title}
                          </span>
                          {ex.completed && <CheckCircle2 size={14} className="text-success" />}
                        </div>
                        <span className="text-[10px] text-text-muted">{type.label}</span>
                      </div>
                    </div>
                    <ChevronRight size={16} className="text-text-muted group-hover:text-text-secondary" />
                  </div>
                  <p className="text-xs text-text-muted leading-relaxed mb-3">{ex.description}</p>
                  <div className="flex items-center gap-2">
                    <Badge variant={diff.variant}>{diff.label}</Badge>
                    {ex.tags.map((t) => (
                      <span key={t} className="text-[9px] font-mono text-text-muted bg-surface-3 px-1.5 py-0.5 rounded">
                        {t}
                      </span>
                    ))}
                  </div>
                </Link>
              </motion.div>
            )
          })}
        </div>
        )}
      </PageContent>
    </>
  )
}
