import { PageContent, PageHeader } from '@/components/layout/MainLayout'
import { Badge } from '@/components/ui/Badge'
import { Button } from '@/components/ui/Button'
import { mockLearningModules } from '@/stores/appStore'
import { cn } from '@/lib/utils'
import { motion } from 'framer-motion'
import { Link, useParams } from 'react-router-dom'
import {
  ArrowLeft,
  BookOpen,
  CheckCircle2,
  Circle,
  Clock,
  Lock,
  Play,
} from 'lucide-react'

const lessons = [
  { id: 1, title: 'String 基础操作', duration: '15min', done: true },
  { id: 2, title: 'Key 的命名规范', duration: '10min', done: true },
  { id: 3, title: 'TTL 与过期策略', duration: '20min', done: true },
  { id: 4, title: 'Hash 数据结构入门', duration: '18min', done: true },
  { id: 5, title: 'Hash vs String 选择', duration: '15min', done: true },
  { id: 6, title: 'List 队列实战', duration: '22min', done: true },
  { id: 7, title: 'Set 去重与交集', duration: '18min', done: true },
  { id: 8, title: 'ZSet 基础操作', duration: '20min', done: true },
  { id: 9, title: 'Sorted Set 排行榜实战', duration: '25min', done: false, current: true },
  { id: 10, title: '数据结构综合练习', duration: '30min', done: false, locked: true },
  { id: 11, title: 'Stream 消息流', duration: '25min', done: false, locked: true },
  { id: 12, title: '期末测验', duration: '40min', done: false, locked: true },
]

export default function CourseDetailPage() {
  const { courseId } = useParams()
  const course = mockLearningModules.find((m) => m.id === courseId) ?? mockLearningModules[1]

  return (
    <>
      <PageHeader
        title={course.title}
        subtitle={course.description}
        actions={
          <Link to="/learning">
            <Button variant="ghost" size="sm">
              <ArrowLeft size={14} />
              返回课程列表
            </Button>
          </Link>
        }
      />
      <PageContent className="!p-0">
        <div className="flex h-full">
          {/* Lesson sidebar */}
          <div className="w-80 shrink-0 border-r border-border-subtle bg-surface-1 overflow-y-auto">
            <div className="p-4 border-b border-border-subtle">
              <div className="flex items-center gap-2 mb-2">
                <BookOpen size={14} className="text-accent-amber" />
                <span className="text-xs font-semibold">课程目录</span>
              </div>
              <div className="flex items-center gap-2">
                <div className="flex-1 h-1.5 rounded-full bg-surface-4 overflow-hidden">
                  <div className="h-full rounded-full bg-accent-amber" style={{ width: `${course.progress}%` }} />
                </div>
                <span className="text-[10px] font-mono font-bold">{course.progress}%</span>
              </div>
            </div>

            <div className="py-2">
              {lessons.map((lesson, i) => (
                <motion.button
                  key={lesson.id}
                  initial={{ opacity: 0, x: -8 }}
                  animate={{ opacity: 1, x: 0 }}
                  transition={{ delay: i * 0.03 }}
                  disabled={lesson.locked}
                  className={cn(
                    'w-full flex items-center gap-3 px-4 py-2.5 text-left transition-colors',
                    lesson.current && 'bg-accent-amber/10 border-r-2 border-accent-amber',
                    lesson.locked && 'opacity-40 cursor-not-allowed',
                    !lesson.current && !lesson.locked && 'hover:bg-surface-2',
                  )}
                >
                  {lesson.done ? (
                    <CheckCircle2 size={16} className="text-success shrink-0" />
                  ) : lesson.locked ? (
                    <Lock size={16} className="text-text-muted shrink-0" />
                  ) : lesson.current ? (
                    <Play size={16} className="text-accent-amber shrink-0" />
                  ) : (
                    <Circle size={16} className="text-text-muted shrink-0" />
                  )}
                  <div className="flex-1 min-w-0">
                    <div className={cn('text-xs font-medium truncate', lesson.current && 'text-accent-amber')}>
                      {lesson.id}. {lesson.title}
                    </div>
                    <div className="flex items-center gap-1 text-[10px] text-text-muted mt-0.5">
                      <Clock size={9} />
                      {lesson.duration}
                    </div>
                  </div>
                </motion.button>
              ))}
            </div>
          </div>

          {/* Lesson content */}
          <div className="flex-1 overflow-y-auto p-8">
            <Badge variant="warning" className="mb-4">第 9 课</Badge>
            <h2 className="text-2xl font-bold mb-2">Sorted Set 排行榜实战</h2>
            <p className="text-sm text-text-muted mb-6 leading-relaxed">
              本课将带你使用 Redis Sorted Set 实现一个完整的实时游戏排行榜系统，涵盖分数更新、Top N 查询、排名查询等核心场景。
            </p>

            <div className="rounded-xl border border-border-subtle bg-surface-1 p-6 mb-6">
              <h3 className="text-sm font-semibold mb-3">学习目标</h3>
              <ul className="space-y-2 text-sm text-text-secondary">
                {[
                  '理解 Sorted Set 的有序特性与时间复杂度',
                  '掌握 ZADD、ZINCRBY、ZREVRANGE 等核心命令',
                  '实现分数增量更新与 Top N 排行榜查询',
                  '使用 ZREVRANK 查询玩家实时排名',
                ].map((item) => (
                  <li key={item} className="flex items-start gap-2">
                    <CheckCircle2 size={14} className="text-success shrink-0 mt-0.5" />
                    {item}
                  </li>
                ))}
              </ul>
            </div>

            <div className="rounded-xl border border-border bg-surface-0 overflow-hidden mb-6">
              <div className="px-4 py-2 bg-surface-2 border-b border-border-subtle text-[10px] font-mono text-text-muted">
                示例命令
              </div>
              <pre className="p-4 font-mono text-sm text-accent-teal leading-relaxed">
{`# 初始化排行榜
ZADD leaderboard:game 9850 "player_alpha" 8720 "player_beta"

# 获取 Top 10
ZREVRANGE leaderboard:game 0 9 WITHSCORES

# 增量更新分数
ZINCRBY leaderboard:game 100 "player_alpha"

# 查询排名
ZREVRANK leaderboard:game "player_alpha"`}
              </pre>
            </div>

            <div className="flex gap-3">
              <Button variant="accent" className="h-10 px-6">
                <Play size={16} />
                开始练习
              </Button>
              <Link to="/workspace">
                <Button variant="outline" className="h-10 px-6">
                  打开 Redis 工作台
                </Button>
              </Link>
              <Link to="/ai">
                <Button variant="ghost" className="h-10 px-6">
                  咨询 AI 导师
                </Button>
              </Link>
            </div>
          </div>
        </div>
      </PageContent>
    </>
  )
}
