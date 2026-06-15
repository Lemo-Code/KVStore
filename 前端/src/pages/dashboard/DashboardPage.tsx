import { PageContent, PageHeader } from '@/components/layout/MainLayout'
import { Button } from '@/components/ui/Button'
import { Card, CardContent, InfoCard, StatCard } from '@/components/ui/Card'
import { Tooltip, TooltipIcon } from '@/components/ui/Tooltip'
import { mockLearningModules } from '@/stores/appStore'
import { useAuthStore } from '@/stores/authStore'
import { cn } from '@/lib/utils'
import { motion } from 'framer-motion'
import { Link, useNavigate } from 'react-router-dom'
import {
  ArrowRight,
  Award,
  Bot,
  Calendar,
  Database,
  Flame,
  MessageCircle,
  Play,
  Target,
  Trophy,
  Zap,
} from 'lucide-react'

const quickActions = [
  { to: '/workspace', icon: Database, label: 'Redis 工作台', desc: '浏览键、执行命令', color: 'text-accent-red', bg: 'bg-accent-red/10' },
  { to: '/ai', icon: Bot, label: 'AI 导师', desc: '智能答疑与分析', color: 'text-accent-purple', bg: 'bg-accent-purple/10' },
  { to: '/chat', icon: MessageCircle, label: '协作室', desc: '3 条未读消息', color: 'text-accent-teal', bg: 'bg-accent-teal/10' },
  { to: '/learning', icon: Play, label: '继续学习', desc: 'Sorted Set 专题', color: 'text-accent-amber', bg: 'bg-accent-amber/10' },
]

const recentActivity = [
  { type: 'command', text: '执行 HGETALL user:1001:profile', time: '10 分钟前', icon: Database, detail: '耗时 2ms' },
  { type: 'ai', text: 'AI 导师解答了 ZSet 排行榜问题', time: '25 分钟前', icon: Bot, detail: '查看对话' },
  { type: 'chat', text: '张明 在协作室分享了代码片段', time: '1 小时前', icon: MessageCircle, detail: 'Sorted Set 专题' },
  { type: 'learn', text: '完成课程「Hash 数据结构」第 5 课', time: '2 小时前', icon: Trophy, detail: '获得 15 积分' },
]

const stats = [
  { label: '学习天数', value: '7', icon: Flame, color: 'red' as const, trend: { value: 40, positive: true } },
  { label: '完成课时', value: '18', icon: Target, color: 'amber' as const, trend: { value: 12, positive: true } },
  { label: '执行命令', value: '342', icon: Zap, color: 'teal' as const, trend: { value: 8, positive: true } },
  { label: 'AI 对话', value: '56', icon: Bot, color: 'purple' as const, trend: { value: 23, positive: true } },
]

const weeklyProgress = [
  { day: '周一', value: 85, label: '2h' },
  { day: '周二', value: 60, label: '1.5h' },
  { day: '周三', value: 100, label: '2.5h' },
  { day: '周四', value: 75, label: '1.8h' },
  { day: '周五', value: 90, label: '2.2h' },
  { day: '周六', value: 40, label: '1h' },
  { day: '周日', value: 70, label: '1.7h' },
]

export default function DashboardPage() {
  const user = useAuthStore((s) => s.user)
  const navigate = useNavigate()
  const currentModule = mockLearningModules.find((m) => m.progress > 0 && m.progress < 100)
  const maxWeekly = Math.max(...weeklyProgress.map((d) => d.value))

  return (
    <>
      <PageHeader
        title={`你好，${user?.username ?? '学员'} 👋`}
        subtitle="欢迎回到 Redis Lab Studio，继续你的学习之旅"
        actions={
          <div className="flex items-center gap-2 text-xs text-text-muted">
            <Calendar size={14} />
            <span>今天是 {new Date().toLocaleDateString('zh-CN', { month: 'long', day: 'numeric' })}</span>
          </div>
        }
      />
      <PageContent>
        {/* Welcome banner */}
        <div className="mb-6 rounded-2xl border border-accent-red/20 bg-gradient-to-r from-accent-red/5 via-accent-purple/5 to-transparent p-5 flex items-center justify-between">
          <div>
            <div className="flex items-center gap-2">
              <Award className="text-accent-red" size={20} />
              <span className="font-semibold">连续学习 7 天 · 保持势头！</span>
            </div>
            <p className="text-xs text-text-muted mt-1">距离下个里程碑还差 3 天，加油！</p>
          </div>
          <Link to="/learning">
            <Button variant="accent" size="sm">
              继续学习 <ArrowRight size={14} />
            </Button>
          </Link>
        </div>

        {/* Stats */}
        <div className="grid grid-cols-2 lg:grid-cols-4 gap-3 mb-6">
          {stats.map((s, i) => (
            <motion.div
              key={s.label}
              initial={{ opacity: 0, y: 12 }}
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

        <div className="grid lg:grid-cols-3 gap-6">
          {/* Quick actions */}
          <div className="lg:col-span-2 space-y-6">
            <div>
              <div className="flex items-center justify-between mb-3">
                <h2 className="text-sm font-semibold text-text-secondary">快捷入口</h2>
                <Tooltip content="点击任意卡片快速跳转">
                  <span className="text-[10px] text-text-muted cursor-help">提示</span>
                </Tooltip>
              </div>
              <div className="grid sm:grid-cols-2 gap-3">
                {quickActions.map((action, i) => (
                  <motion.div
                    key={action.to}
                    initial={{ opacity: 0, x: -12 }}
                    animate={{ opacity: 1, x: 0 }}
                    transition={{ delay: 0.1 + i * 0.05 }}
                  >
                    <Link
                      to={action.to}
                      className="flex items-center gap-3 rounded-xl border border-border-subtle bg-surface-1 p-4 hover:border-border hover:bg-surface-2 transition-all group"
                    >
                      <div className={cn('flex h-10 w-10 items-center justify-center rounded-lg', action.bg)}>
                        <action.icon size={18} className={action.color} />
                      </div>
                      <div className="flex-1 min-w-0">
                        <div className="text-sm font-semibold">{action.label}</div>
                        <div className="text-[11px] text-text-muted">{action.desc}</div>
                      </div>
                      <ArrowRight size={14} className="text-text-muted group-hover:text-text-secondary transition-colors" />
                    </Link>
                  </motion.div>
                ))}
              </div>
            </div>

            {/* Weekly activity chart */}
            <div>
              <div className="flex items-center justify-between mb-3">
                <h2 className="text-sm font-semibold text-text-secondary">本周学习时长</h2>
                <span className="text-xs text-text-muted">总计 12.7 小时</span>
              </div>
              <Card className="p-5">
                <div className="flex items-end justify-between gap-2 h-32">
                  {weeklyProgress.map((day, i) => (
                    <div key={i} className="flex-1 flex flex-col items-center gap-2 group">
                      <div className="relative w-full flex justify-center">
                        <div
                          className="w-full max-w-[28px] rounded-t-lg bg-gradient-to-t from-accent-red/60 to-accent-red transition-all group-hover:from-accent-red/80 group-hover:to-accent-red"
                          style={{ height: `${(day.value / maxWeekly) * 100}px` }}
                        />
                        <div className="absolute -top-6 text-[10px] text-text-muted opacity-0 group-hover:opacity-100 transition-opacity">
                          {day.label}
                        </div>
                      </div>
                      <span className="text-[10px] text-text-muted">{day.day}</span>
                    </div>
                  ))}
                </div>
              </Card>
            </div>

            {/* Recent activity */}
            <div>
              <h2 className="text-sm font-semibold text-text-secondary mb-3">最近动态</h2>
              <Card variant="gradient" className="divide-y divide-border-subtle">
                {recentActivity.map((item, i) => (
                  <div key={i} className="flex items-center gap-3 px-4 py-3 hover:bg-surface-2/50 transition-colors group">
                    <div className="flex h-9 w-9 items-center justify-center rounded-lg bg-surface-3 group-hover:bg-surface-4 transition-colors">
                      <item.icon size={15} className="text-text-muted" />
                    </div>
                    <div className="flex-1 min-w-0">
                      <div className="text-xs text-text-primary truncate">{item.text}</div>
                      <div className="flex items-center gap-2 mt-0.5">
                        <span className="text-[10px] text-text-muted">{item.time}</span>
                        <span className="text-[10px] text-accent-teal/70">· {item.detail}</span>
                      </div>
                    </div>
                    <TooltipIcon icon={ArrowRight} content="查看详情" />
                  </div>
                ))}
              </Card>
            </div>
          </div>

          {/* Learning progress */}
          <div className="space-y-4">
            <h2 className="text-sm font-semibold text-text-secondary">学习进度</h2>
            {currentModule && (
              <Card hover>
                <CardContent>
                  <div className="flex items-center gap-2 mb-4">
                    <div className="flex h-9 w-9 items-center justify-center rounded-lg bg-accent-amber/10">
                      <Trophy size={18} className="text-accent-amber" />
                    </div>
                    <div>
                      <div className="text-sm font-semibold">{currentModule.title}</div>
                      <div className="text-[10px] text-text-muted">进行中</div>
                    </div>
                  </div>
                  <div className="flex items-center gap-2 mb-2">
                    <div className="flex-1 h-2 rounded-full bg-surface-4 overflow-hidden">
                      <div
                        className="h-full rounded-full bg-gradient-to-r from-accent-amber to-accent-red transition-all"
                        style={{ width: `${currentModule.progress}%` }}
                      />
                    </div>
                    <span className="text-xs font-mono font-bold tabular-nums w-10 text-right">{currentModule.progress}%</span>
                  </div>
                  <p className="text-[11px] text-text-muted mb-4">
                    {currentModule.completedLessons}/{currentModule.lessons} 课时已完成
                  </p>
                  <Link to="/learning">
                    <Button variant="accent" size="sm" className="w-full">
                      继续学习 <ArrowRight size={14} />
                    </Button>
                  </Link>
                </CardContent>
              </Card>
            )}

            <InfoCard
              title="AI 学习建议"
              description="你最近在练习 Sorted Set，建议完成「排行榜实战」练习后，尝试用 AI 导师分析你的 leaderboard:game 键结构。"
              icon={Bot}
              color="purple"
              action={{ label: '咨询 AI 导师', onClick: () => navigate('/ai') }}
            />

            <Card>
              <CardContent className="text-center py-4">
                <div className="text-xs text-text-muted mb-1">本周排名</div>
                <div className="text-3xl font-bold text-accent-red">#42</div>
                <div className="text-[11px] text-success mt-1">↑ 8 名</div>
              </CardContent>
            </Card>
          </div>
        </div>
      </PageContent>
    </>
  )
}
