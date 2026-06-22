import { useState, useEffect } from 'react'
import { useNavigate, useLocation } from 'react-router-dom'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Button } from '@/components/ui/button'
import { Badge } from '@/components/ui/badge'
import { useRedisStore } from '@/stores/redisStore'
import { redisService } from '@/services/redisService'
import ConnectionForm from '@/components/redis/ConnectionForm'
import { Plus, CheckCircle2, XCircle, MessageSquare, Bot } from 'lucide-react'
import { cn } from '@/lib/utils'

/* ─── Redis Sidebar ─── */

function RedisSidebar() {
  const navigate = useNavigate()
  const location = useLocation()
  const connections = useRedisStore((s) => s.connections)
  const activeId = useRedisStore((s) => s.activeConnectionId)
  const setActive = useRedisStore((s) => s.setActiveConnection)
  const setConnections = useRedisStore((s) => s.setConnections)
  const [show, setShow] = useState(false)

  useEffect(() => {
    redisService.getConnections().then(setConnections)
  }, [setConnections])

  const handleSelectConnection = (id: string) => {
    setActive(id)
    if (location.pathname !== '/redis') navigate('/redis')
  }

  return (
    <div className="flex h-full flex-col">
      <div className="flex items-center justify-between border-b px-4 py-2.5">
        <span className="text-xs font-semibold uppercase tracking-wider text-muted-foreground">
          连接
        </span>
        <Button
          variant="ghost"
          size="icon-sm"
          className="h-6 w-6"
          onClick={() => setShow(true)}
        >
          <Plus className="h-3.5 w-3.5" />
        </Button>
      </div>
      <ScrollArea className="flex-1">
        {connections.map((c) => (
          <button
            key={c.id}
            onClick={() => handleSelectConnection(c.id)}
            className={cn(
              'flex w-full items-center gap-2.5 px-4 py-2.5 text-left text-sm transition-colors hover:bg-muted/50',
              activeId === c.id && 'bg-muted',
            )}
          >
            {c.status === 'connected' ? (
              <CheckCircle2 className="h-3 w-3 shrink-0 text-emerald-500" />
            ) : (
              <XCircle className="h-3 w-3 shrink-0 text-muted-foreground/30" />
            )}
            <div className="min-w-0 flex-1">
              <div className="truncate text-sm">{c.name}</div>
              <div className="text-xs text-muted-foreground">
                {c.host}:{c.port}
              </div>
            </div>
          </button>
        ))}
      </ScrollArea>
      <ConnectionForm
        open={show}
        onClose={() => setShow(false)}
        onSave={(conn) => {
          useRedisStore.getState().addConnection(conn)
          setShow(false)
        }}
      />
    </div>
  )
}

/* ─── Chat Sidebar ─── */

const rooms = [
  {
    id: '1',
    name: 'Redis 新手互助',
    unread: 3,
    last: '有人知道 RDB 和 AOF 的区别吗？',
  },
  {
    id: '2',
    name: '缓存策略讨论组',
    unread: 0,
    last: '推荐看缓存穿透解决方案',
  },
  {
    id: '3',
    name: '集群运维交流',
    unread: 5,
    last: 'Cluster 扩容最佳实践?',
  },
]

function ChatSidebar() {
  const [active, setActive] = useState('1')

  return (
    <div className="flex h-full flex-col">
      <div className="border-b px-4 py-2.5">
        <span className="text-xs font-semibold uppercase tracking-wider text-muted-foreground">
          聊天
        </span>
      </div>
      <ScrollArea className="flex-1">
        {rooms.map((r) => (
          <button
            key={r.id}
            onClick={() => setActive(r.id)}
            className={cn(
              'flex w-full items-start gap-2.5 px-4 py-2.5 text-left transition-colors hover:bg-muted/50',
              active === r.id && 'bg-muted',
            )}
          >
            <div className="mt-0.5 flex h-7 w-7 shrink-0 items-center justify-center rounded-full bg-muted">
              <MessageSquare className="h-3.5 w-3.5 text-muted-foreground" />
            </div>
            <div className="min-w-0 flex-1">
              <div className="flex items-center justify-between">
                <span className="truncate text-sm font-medium">{r.name}</span>
                {r.unread > 0 && (
                  <Badge
                    variant="default"
                    className="ml-1 h-4 min-w-4 px-1 text-[10px]"
                  >
                    {r.unread}
                  </Badge>
                )}
              </div>
              <div className="truncate text-xs text-muted-foreground">{r.last}</div>
            </div>
          </button>
        ))}
      </ScrollArea>
    </div>
  )
}

/* ─── AI Sidebar ─── */

function AISidebar() {
  return (
    <div className="flex h-full flex-col">
      <div className="flex items-center justify-between border-b px-4 py-2.5">
        <span className="text-xs font-semibold uppercase tracking-wider text-muted-foreground">
          对话
        </span>
        <Button variant="ghost" size="icon-sm" className="h-6 w-6">
          <Plus className="h-3.5 w-3.5" />
        </Button>
      </div>
      <ScrollArea className="flex-1">
        <button className="flex w-full items-center gap-2.5 px-4 py-2.5 text-left bg-muted">
          <Bot className="h-4 w-4 shrink-0 text-muted-foreground" />
          <div className="min-w-0">
            <div className="truncate text-sm">Redis 持久化</div>
            <div className="text-xs text-muted-foreground">今天</div>
          </div>
        </button>
      </ScrollArea>
    </div>
  )
}

/* ─── Settings Sidebar ─── */

function SettingsSidebar() {
  return (
    <div className="flex h-full flex-col">
      <div className="border-b px-4 py-2.5">
        <span className="text-xs font-semibold uppercase tracking-wider text-muted-foreground">
          设置
        </span>
      </div>
      <div className="p-3">
        <button className="flex w-full items-center gap-2 rounded-md px-2 py-1.5 text-sm bg-muted">
          通用
        </button>
      </div>
    </div>
  )
}

/* ─── SidebarPanel ─── */

export function SidebarPanel() {
  const location = useLocation()

  // Derive active module from route
  const getModule = () => {
    if (location.pathname.startsWith('/redis')) return 'redis'
    if (location.pathname.startsWith('/chat')) return 'chat'
    if (location.pathname.startsWith('/ai')) return 'ai'
    if (location.pathname.startsWith('/settings')) return 'settings'
    return 'redis' // default
  }

  const mod = getModule()

  return (
    <div className="flex h-full w-60 flex-col border-r bg-card">
      {mod === 'redis' && <RedisSidebar />}
      {mod === 'chat' && <ChatSidebar />}
      {mod === 'ai' && <AISidebar />}
      {mod === 'settings' && <SettingsSidebar />}
    </div>
  )
}
