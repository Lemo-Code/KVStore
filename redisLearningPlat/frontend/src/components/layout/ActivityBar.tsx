import { useNavigate, useLocation } from 'react-router-dom'
import { Database, MessageSquare, Bot, Settings, LogOut } from 'lucide-react'
import { Tooltip, TooltipTrigger, TooltipContent } from '@/components/ui/tooltip'
import { useUIStore, type ModuleType } from '@/stores/uiStore'
import { useAuthStore } from '@/stores/authStore'
import { cn } from '@/lib/utils'

const items: { id: ModuleType; path: string; icon: typeof Database; label: string }[] = [
  { id: 'redis', path: '/redis', icon: Database, label: 'Redis 管理' },
  { id: 'chat', path: '/chat', icon: MessageSquare, label: '学习交流' },
  { id: 'ai', path: '/ai', icon: Bot, label: 'AI 助手' },
  { id: 'settings', path: '/settings', icon: Settings, label: '设置' },
]

export function ActivityBar() {
  const navigate = useNavigate()
  const location = useLocation()
  const setActive = useUIStore((s) => s.setActiveModule)
  const logout = useAuthStore((s) => s.logout)

  // Derive active module from current route
  const currentPath = location.pathname
  const activeModule = items.find((i) => currentPath.startsWith(i.path))?.id

  const handleClick = (item: (typeof items)[0]) => {
    setActive(item.id)
    navigate(item.path)
  }

  return (
    <div className="flex w-12 flex-col items-center border-r bg-muted/50 py-3">
      {items.map((m) => {
        const isActive = activeModule === m.id
        return (
          <Tooltip key={m.id}>
            <TooltipTrigger asChild>
              <button
                onClick={() => handleClick(m)}
                className={cn(
                  'relative mb-1 flex h-10 w-10 items-center justify-center rounded-lg transition-colors',
                  isActive
                    ? 'text-primary'
                    : 'text-muted-foreground hover:bg-muted hover:text-foreground',
                )}
              >
                {/* Blue active indicator bar */}
                {isActive && (
                  <div className="absolute left-0 top-2 h-6 w-0.5 rounded-r-full bg-primary" />
                )}
                <m.icon className="h-5 w-5" />
              </button>
            </TooltipTrigger>
            <TooltipContent side="right" sideOffset={8}>
              <p className="text-xs">{m.label}</p>
            </TooltipContent>
          </Tooltip>
        )
      })}
      <div className="mt-auto">
        <Tooltip>
          <TooltipTrigger asChild>
            <button
              onClick={logout}
              className="flex h-10 w-10 items-center justify-center rounded-lg text-muted-foreground transition-colors hover:bg-muted hover:text-destructive"
            >
              <LogOut className="h-4 w-4" />
            </button>
          </TooltipTrigger>
          <TooltipContent side="right" sideOffset={8}>
            <p className="text-xs">退出</p>
          </TooltipContent>
        </Tooltip>
      </div>
    </div>
  )
}
