import { Button } from '@/components/ui/Button'
import { mockConnection } from '@/stores/appStore'
import { useAppStore } from '@/stores/appStore'
import {
  BookOpen,
  Bot,
  ChevronDown,
  MessageCircle,
  PanelRightOpen,
  Play,
  Plus,
  Settings,
  Sparkles,
  Wifi,
} from 'lucide-react'

export function TopBar() {
  const { rightPanel, setRightPanel } = useAppStore()
  const conn = mockConnection

  return (
    <header className="flex h-11 shrink-0 items-center justify-between border-b border-border-subtle bg-surface-1 px-3">
      {/* Brand */}
      <div className="flex items-center gap-3">
        <div className="flex items-center gap-2">
          <div className="flex h-7 w-7 items-center justify-center rounded-lg bg-gradient-to-br from-accent-red to-accent-red-dim shadow-md shadow-accent-red/25">
            <Sparkles size={14} className="text-white" />
          </div>
          <div className="flex flex-col">
            <span className="text-sm font-bold tracking-tight leading-none">
              Redis Lab <span className="text-accent-red">Studio</span>
            </span>
            <span className="text-[10px] text-text-muted leading-none mt-0.5">
              智能学习平台
            </span>
          </div>
        </div>

        <div className="mx-2 h-5 w-px bg-border" />

        {/* Connection status */}
        <button className="flex items-center gap-2 rounded-md px-2 py-1 hover:bg-surface-3 transition-colors">
          <span className="relative flex h-2 w-2">
            <span className="absolute inline-flex h-full w-full animate-ping rounded-full bg-success opacity-40" />
            <span className="relative inline-flex h-2 w-2 rounded-full bg-success" />
          </span>
          <Wifi size={13} className="text-text-secondary" />
          <span className="text-xs text-text-secondary">{conn.name}</span>
          <span className="font-mono text-[10px] text-text-muted">
            {conn.host}:{conn.port}
          </span>
          <ChevronDown size={12} className="text-text-muted" />
        </button>
      </div>

      {/* Center actions */}
      <div className="flex items-center gap-1.5">
        <Button variant="ghost" size="sm">
          <Play size={13} />
          运行
        </Button>
        <Button variant="ghost" size="sm">
          <Plus size={13} />
          新建查询
        </Button>
      </div>

      {/* Right panel toggles */}
      <div className="flex items-center gap-1">
        <div className="flex items-center rounded-lg bg-surface-2 p-0.5 border border-border-subtle">
          {(
            [
              { id: 'ai' as const, icon: Bot, label: 'AI 导师' },
              { id: 'chat' as const, icon: MessageCircle, label: '协作' },
              { id: 'learning' as const, icon: BookOpen, label: '课程' },
            ] as const
          ).map(({ id, icon: Icon, label }) => (
            <button
              key={id}
              onClick={() => setRightPanel(id)}
              className={`flex items-center gap-1.5 rounded-md px-2.5 py-1 text-xs font-medium transition-all ${
                rightPanel === id
                  ? 'bg-surface-4 text-text-primary shadow-sm'
                  : 'text-text-muted hover:text-text-secondary'
              }`}
            >
              <Icon size={13} />
              {label}
            </button>
          ))}
        </div>

        <div className="mx-1 h-5 w-px bg-border" />

        <Button variant="ghost" size="icon">
          <Settings size={15} />
        </Button>
        <Button variant="ghost" size="icon">
          <PanelRightOpen size={15} />
        </Button>
      </div>
    </header>
  )
}
