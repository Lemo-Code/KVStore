import { PageContent, PageHeader } from '@/components/layout/MainLayout'
import { Button } from '@/components/ui/Button'
import { cn } from '@/lib/utils'
import { motion } from 'framer-motion'
import {
  Bell,
  Bot,
  CheckCheck,
  MessageCircle,
  Settings,
  Trophy,
} from 'lucide-react'
import { useState } from 'react'

type NotifType = 'chat' | 'ai' | 'learn' | 'system'

interface Notification {
  id: string
  type: NotifType
  title: string
  content: string
  time: string
  read: boolean
}

const mockNotifications: Notification[] = [
  { id: '1', type: 'chat', title: '协作室新消息', content: '张明在 Sorted Set 专题讨论中 @了你', time: '5 分钟前', read: false },
  { id: '2', type: 'ai', title: 'AI 导师回复', content: '你关于排行榜设计的问题已收到详细解答', time: '30 分钟前', read: false },
  { id: '3', type: 'learn', title: '课程提醒', content: '今日学习目标：Sorted Set 排行榜实战（剩余 25 分钟）', time: '1 小时前', read: false },
  { id: '4', type: 'chat', title: '新成员加入', content: '王浩 加入了 Sorted Set 专题讨论', time: '2 小时前', read: true },
  { id: '5', type: 'learn', title: '成就解锁', content: '恭喜获得「连续学习 7 天」徽章！', time: '昨天', read: true },
  { id: '6', type: 'system', title: '系统更新', content: 'Redis Lab Studio v1.1 已发布，新增 Stream 数据查看器', time: '2 天前', read: true },
  { id: '7', type: 'ai', title: 'AI 学习建议', content: '根据你的学习进度，建议开始「高级特性」模块', time: '3 天前', read: true },
]

const typeConfig: Record<NotifType, { icon: typeof Bell; color: string }> = {
  chat: { icon: MessageCircle, color: 'text-accent-teal' },
  ai: { icon: Bot, color: 'text-accent-purple' },
  learn: { icon: Trophy, color: 'text-accent-amber' },
  system: { icon: Settings, color: 'text-text-muted' },
}

export default function NotificationsPage() {
  const [notifications, setNotifications] = useState(mockNotifications)
  const [filter, setFilter] = useState<'all' | 'unread'>('all')

  const filtered = filter === 'unread' ? notifications.filter((n) => !n.read) : notifications
  const unreadCount = notifications.filter((n) => !n.read).length

  const markAllRead = () => {
    setNotifications((prev) => prev.map((n) => ({ ...n, read: true })))
  }

  return (
    <>
      <PageHeader
        title="通知中心"
        subtitle={unreadCount > 0 ? `${unreadCount} 条未读通知` : '所有通知已读'}
        actions={
          unreadCount > 0 ? (
            <Button variant="ghost" size="sm" onClick={markAllRead}>
              <CheckCheck size={14} />
              全部标为已读
            </Button>
          ) : undefined
        }
      />
      <PageContent>
        <div className="max-w-2xl mx-auto">
          <div className="flex gap-2 mb-4">
            {(['all', 'unread'] as const).map((f) => (
              <button
                key={f}
                onClick={() => setFilter(f)}
                className={cn(
                  'rounded-full px-3 py-1 text-xs font-medium transition-colors',
                  filter === f ? 'bg-accent-red/15 text-accent-red' : 'text-text-muted hover:text-text-secondary',
                )}
              >
                {f === 'all' ? '全部' : `未读 (${unreadCount})`}
              </button>
            ))}
          </div>

          <div className="rounded-xl border border-border-subtle bg-surface-1 divide-y divide-border-subtle overflow-hidden">
            {filtered.length === 0 ? (
              <div className="py-12 text-center text-sm text-text-muted">
                <Bell size={32} className="mx-auto mb-3 opacity-30" />
                暂无通知
              </div>
            ) : (
              filtered.map((notif, i) => {
                const config = typeConfig[notif.type]
                const Icon = config.icon
                return (
                  <motion.div
                    key={notif.id}
                    initial={{ opacity: 0, x: -8 }}
                    animate={{ opacity: 1, x: 0 }}
                    transition={{ delay: i * 0.04 }}
                    className={cn(
                      'flex items-start gap-3 px-4 py-3.5 hover:bg-surface-2 transition-colors cursor-pointer',
                      !notif.read && 'bg-accent-red/[0.03]',
                    )}
                  >
                    <div className={cn('flex h-9 w-9 shrink-0 items-center justify-center rounded-lg bg-surface-3')}>
                      <Icon size={16} className={config.color} />
                    </div>
                    <div className="flex-1 min-w-0">
                      <div className="flex items-center gap-2">
                        <span className={cn('text-xs font-semibold', !notif.read && 'text-text-primary')}>
                          {notif.title}
                        </span>
                        {!notif.read && <span className="h-1.5 w-1.5 rounded-full bg-accent-red" />}
                      </div>
                      <p className="text-[11px] text-text-muted mt-0.5 leading-relaxed">{notif.content}</p>
                      <span className="text-[10px] text-text-muted/60 mt-1 block">{notif.time}</span>
                    </div>
                  </motion.div>
                )
              })
            )}
          </div>
        </div>
      </PageContent>
    </>
  )
}
