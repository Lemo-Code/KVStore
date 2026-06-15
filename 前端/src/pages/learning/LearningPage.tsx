import { PageContent, PageHeader } from '@/components/layout/MainLayout'
import { Badge } from '@/components/ui/Badge'
import { Button } from '@/components/ui/Button'
import { Card, CardContent, StatCard } from '@/components/ui/Card'
import { EmptyState } from '@/components/ui/EmptyState'
import { Tooltip } from '@/components/ui/Tooltip'
import { useToast } from '@/components/ui/Toast'
import { mockLearningModules } from '@/stores/appStore'
import { cn } from '@/lib/utils'
import { motion } from 'framer-motion'
import { Link, useNavigate } from 'react-router-dom'
import {
  Award,
  BookOpen,
  Calendar,
  ChevronRight,
  Clock,
  Flame,
  Play,
  Search,
  Star,
  Target,
  Trophy,
  Users,
} from 'lucide-react'
import { useState } from 'react'

const difficultyConfig = {
  beginner: { label: '入门', color: 'text-success', bg: 'bg-success/10 border-success/20' },
  intermediate: { label: '进阶', color: 'text-accent-amber', bg: 'bg-accent-amber/10 border-accent-amber/20' },
  advanced: { label: '高级', color: 'text-accent-red', bg: 'bg-accent-red/10 border-accent-red/20' },
}

const learningStats = [
  { icon: Trophy, label: '总进度', value: '47%', color: 'amber' as const, trend: { value: 5, positive: true } },
  { icon: Flame, label: '连续学习', value: '7 天', color: 'red' as const },
  { icon: Award, label: '获得徽章', value: '8', color: 'purple' as const, trend: { value: 2, positive: true } },
  { icon: Star, label: '完成课时', value: '18/38', color: 'teal' as const },
]

export default function LearningPage() {
  const [filter, setFilter] = useState<'all' | 'beginner' | 'intermediate' | 'advanced'>('all')
  const [searchQuery, setSearchQuery] = useState('')
  const navigate = useNavigate()
  const toast = useToast()
  const resumeModule =
    mockLearningModules.find((m) => m.progress > 0 && m.progress < 100) ?? mockLearningModules[0]
  const totalProgress = Math.round(
    mockLearningModules.reduce((a, m) => a + m.progress, 0) / mockLearningModules.length,
  )

  const filtered = mockLearningModules
    .filter((m) => filter === 'all' || m.difficulty === filter)
    .filter((m) => !searchQuery || m.title.toLowerCase().includes(searchQuery.toLowerCase()) || m.description.toLowerCase().includes(searchQuery.toLowerCase()))

  const completedCount = mockLearningModules.filter((m) => m.progress === 100).length
  const inProgressCount = mockLearningModules.filter((m) => m.progress > 0 && m.progress < 100).length

  return (
    <>
      <PageHeader
        title="学习中心"
        subtitle="系统化 Redis 课程，从基础到生产实践"
        actions={
          <div className="flex items-center gap-2">
            <Button
              variant="outline"
              size="sm"
              onClick={() => toast.info('学习日历即将上线', '敬请期待打卡与计划功能')}
            >
              <Calendar size={14} />
              学习日历
            </Button>
            <Button
              variant="accent"
              size="sm"
              onClick={() => navigate(`/learning/${resumeModule.id}`)}
            >
              <Play size={14} />
              继续上次学习
            </Button>
          </div>
        }
      />
      <PageContent>
        {/* Overview cards */}
        <div className="grid grid-cols-2 lg:grid-cols-4 gap-3 mb-8">
          {learningStats.map((s, i) => (
            <motion.div
              key={s.label}
              initial={{ opacity: 0, y: 8 }}
              animate={{ opacity: 1, y: 0 }}
              transition={{ delay: i * 0.05 }}
            >
              <StatCard
                value={s.value}
                label={s.label}
                icon={s.icon}
                color={s.color}
                trend={s.trend}
              />
            </motion.div>
          ))}
        </div>

        {/* Today's goal */}
        <Card className="mb-8 border-accent-teal/20 bg-accent-teal/5">
          <CardContent className="flex items-center gap-4 py-5">
            <div className="flex h-14 w-14 items-center justify-center rounded-2xl bg-accent-teal/15">
              <BookOpen size={26} className="text-accent-teal" />
            </div>
            <div className="flex-1">
              <div className="flex items-center gap-2">
                <span className="text-sm font-semibold text-accent-teal">今日学习目标</span>
                <Badge variant="success">进行中</Badge>
              </div>
              <div className="text-sm text-text-secondary mt-1">
                完成「数据结构深度解析」第 9 课：Sorted Set 排行榜实战
              </div>
            </div>
            <div className="text-right">
              <div className="flex items-center gap-1 text-[10px] text-text-muted justify-end">
                <Clock size={11} />
                预计 25 分钟
              </div>
              <Link to="/learning/mod2">
                <Button variant="accent" size="sm" className="mt-2 !bg-accent-teal hover:!bg-accent-teal/80">
                  开始学习
                </Button>
              </Link>
            </div>
          </CardContent>
        </Card>

        {/* Learning path progress */}
        <div className="mb-8">
          <div className="flex items-center justify-between mb-3">
            <h3 className="text-sm font-semibold text-text-secondary">学习路径</h3>
            <span className="text-xs text-text-muted">总进度 {totalProgress}% · {completedCount} 已完成 · {inProgressCount} 进行中</span>
          </div>
          <div className="flex gap-2">
            {mockLearningModules.slice(0, 6).map((mod, i) => (
              <Tooltip key={i} content={`${mod.title} · ${mod.progress}%`} placement="top">
                <div className="flex-1 h-2 rounded-full bg-surface-4 overflow-hidden cursor-pointer">
                  <div
                    className={cn(
                      'h-full transition-all',
                      mod.progress === 100 ? 'bg-success' : mod.progress > 0 ? 'bg-accent-amber' : 'bg-surface-hover'
                    )}
                    style={{ width: `${mod.progress}%` }}
                  />
                </div>
              </Tooltip>
            ))}
          </div>
        </div>

        {/* Filter & Search */}
        <div className="flex flex-wrap items-center gap-2 mb-4">
          <div className="flex gap-1">
            {(['all', 'beginner', 'intermediate', 'advanced'] as const).map((f) => (
              <button
                key={f}
                onClick={() => setFilter(f)}
                className={cn(
                  'rounded-full px-3 py-1 text-xs font-medium transition-all',
                  filter === f
                    ? 'bg-accent-red/15 text-accent-red ring-1 ring-accent-red/20'
                    : 'text-text-muted hover:text-text-secondary hover:bg-surface-2',
                )}
              >
                {f === 'all' ? '全部' : difficultyConfig[f].label}
              </button>
            ))}
          </div>
          <div className="flex-1" />
          <div className="relative w-64">
            <Search size={13} className="absolute left-3 top-1/2 -translate-y-1/2 text-text-muted" />
            <input
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="搜索课程..."
              className="w-full rounded-lg border border-border-subtle bg-surface-0 py-1.5 pl-9 pr-3 text-xs focus:outline-none focus:border-accent-red/40"
            />
          </div>
        </div>

        {/* Course grid */}
        <div className="grid md:grid-cols-2 gap-4">
          {filtered.length === 0 ? (
            <EmptyState
              className="col-span-2"
              icon={Search}
              title="未找到匹配的课程"
              description={searchQuery ? `没有与「${searchQuery}」匹配的课程，换个关键词试试。` : '当前筛选下暂无课程。'}
              action={{
                label: '清除筛选',
                onClick: () => {
                  setSearchQuery('')
                  setFilter('all')
                },
              }}
            />
          ) : (
            filtered.map((mod, i) => {
              const diff = difficultyConfig[mod.difficulty]
              const isCompleted = mod.progress === 100
              return (
                <motion.div
                  key={mod.id}
                  initial={{ opacity: 0, y: 12 }}
                  animate={{ opacity: 1, y: 0 }}
                  transition={{ delay: i * 0.04 }}
                >
                  <Link
                    to={`/learning/${mod.id}`}
                    className="block rounded-xl border border-border-subtle bg-surface-1 p-5 hover:border-border hover:bg-surface-2 transition-all group"
                  >
                    <div className="flex items-start justify-between mb-3">
                      <div className="flex-1 min-w-0">
                        <div className="flex items-center gap-2">
                          <h3 className="text-sm font-semibold group-hover:text-accent-red transition-colors truncate">
                            {mod.title}
                          </h3>
                          {isCompleted && <Trophy size={14} className="text-success shrink-0" />}
                        </div>
                        <p className="text-xs text-text-muted mt-1 line-clamp-2">{mod.description}</p>
                      </div>
                      <ChevronRight size={16} className="text-text-muted group-hover:text-text-secondary shrink-0 ml-2" />
                    </div>

                    <div className="flex items-center gap-2 mb-3">
                      <span className={cn('rounded border px-1.5 py-0.5 text-[9px] font-semibold', diff.bg, diff.color)}>
                        {diff.label}
                      </span>
                      <span className="text-[10px] text-text-muted flex items-center gap-1">
                        <Target size={10} /> {mod.completedLessons}/{mod.lessons} 课时
                      </span>
                      <span className="text-[10px] text-text-muted flex items-center gap-1">
                        <Users size={10} /> {Math.floor(Math.random() * 500) + 100} 学员
                      </span>
                    </div>

                    <div className="flex items-center gap-2">
                      <div className="flex-1 h-1.5 rounded-full bg-surface-4 overflow-hidden">
                        <div
                          className={cn(
                            'h-full rounded-full transition-all',
                            isCompleted ? 'bg-success' : mod.progress > 0 ? 'bg-gradient-to-r from-accent-amber to-accent-red' : 'bg-surface-hover',
                          )}
                          style={{ width: `${mod.progress}%` }}
                        />
                      </div>
                      <span className="text-xs font-mono font-bold w-10 text-right tabular-nums">{mod.progress}%</span>
                    </div>

                    <div className="flex flex-wrap gap-1 mt-3">
                      {mod.tags.map((tag) => (
                        <Badge key={tag}>{tag}</Badge>
                      ))}
                    </div>
                  </Link>
                </motion.div>
              )
            })
          )}
        </div>
      </PageContent>
    </>
  )
}
