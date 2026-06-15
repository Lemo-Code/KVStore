import { CommandPalette } from '@/components/layout/CommandPalette'
import { cn } from '@/lib/utils'
import { useAuthStore } from '@/stores/authStore'
import { usePlatformStore } from '@/stores/platformStore'
import { motion } from 'framer-motion'
import {
  Bell,
  BookOpen,
  Bot,
  Database,
  Dumbbell,
  HelpCircle,
  LayoutDashboard,
  Loader2,
  LogOut,
  Menu,
  MessageCircle,
  Plug,
  Search,
  Settings,
  Shield,
  Sparkles,
  User,
} from 'lucide-react'
import { Suspense, useEffect, useState } from 'react'
import { NavLink, Navigate, Outlet, useLocation, useNavigate } from 'react-router-dom'

const navItems = [
  { to: '/dashboard', icon: LayoutDashboard, label: '仪表盘' },
  { to: '/workspace', icon: Database, label: 'Redis 工作台' },
  { to: '/connections', icon: Plug, label: '连接管理' },
  { to: '/chat', icon: MessageCircle, label: '协作通讯', badge: 3 },
  { to: '/ai', icon: Bot, label: 'AI 导师' },
  { to: '/learning', icon: BookOpen, label: '学习中心' },
  { to: '/exercises', icon: Dumbbell, label: '练习题库' },
]

const bottomItems = [
  { to: '/notifications', icon: Bell, label: '通知', badge: 5 },
  { to: '/profile', icon: User, label: '个人资料' },
  { to: '/settings', icon: Settings, label: '设置' },
  { to: '/help', icon: HelpCircle, label: '帮助中心' },
]

export function MainLayout() {
  const user = useAuthStore((s) => s.user)
  const hasOnboarded = useAuthStore((s) => s.hasOnboarded)
  const logout = useAuthStore((s) => s.logout)
  const openCommandPalette = usePlatformStore((s) => s.setCommandPaletteOpen)
  const navigate = useNavigate()
  const location = useLocation()
  const [sidebarOpen, setSidebarOpen] = useState(false)

  useEffect(() => {
    setSidebarOpen(false)
  }, [location.pathname])

  if (!hasOnboarded) {
    return <Navigate to="/onboarding" replace />
  }

  const handleLogout = () => {
    logout()
    navigate('/login')
  }

  return (
    <div className="flex h-full">
      {/* Mobile drawer overlay */}
      {sidebarOpen && (
        <div
          onClick={() => setSidebarOpen(false)}
          className="fixed inset-0 z-40 bg-black/50 backdrop-blur-sm lg:hidden"
        />
      )}

      {/* Sidebar */}
      <aside
        className={cn(
          'w-[220px] shrink-0 flex flex-col border-r border-border-subtle bg-surface-1',
          'max-lg:fixed max-lg:inset-y-0 max-lg:left-0 max-lg:z-50 max-lg:transition-transform max-lg:duration-200',
          sidebarOpen ? 'max-lg:translate-x-0' : 'max-lg:-translate-x-full',
        )}
      >
        {/* Brand */}
        <div className="flex items-center gap-2.5 px-4 h-12 border-b border-border-subtle">
          <div className="flex h-7 w-7 items-center justify-center rounded-lg bg-gradient-to-br from-accent-red to-accent-red-dim">
            <Sparkles size={13} className="text-white" />
          </div>
          <div>
            <div className="text-xs font-bold leading-none">Redis Lab Studio</div>
            <div className="text-[9px] text-text-muted leading-none mt-0.5">智能学习平台</div>
          </div>
        </div>

        {/* Global search trigger */}
        <div className="px-2 pt-3">
          <button
            onClick={() => openCommandPalette(true)}
            className="flex w-full items-center gap-2 rounded-lg border border-border-subtle bg-surface-2 px-2.5 py-2 text-text-muted hover:border-border hover:text-text-secondary transition-all"
          >
            <Search size={14} />
            <span className="flex-1 text-left text-xs">搜索…</span>
            <kbd className="rounded border border-border-subtle bg-surface-3 px-1 text-[9px] font-medium">⌘K</kbd>
          </button>
        </div>

        {/* Nav */}
        <nav className="flex-1 px-2 py-3 space-y-0.5 overflow-y-auto">
          <div className="px-2 mb-2 text-[9px] font-semibold text-text-muted uppercase tracking-wider">
            主要功能
          </div>
          {navItems.map((item) => (
            <NavLink
              key={item.to}
              to={item.to}
              className={({ isActive }) =>
                cn(
                  'flex items-center gap-2.5 rounded-lg px-2.5 py-2 text-xs font-medium transition-all',
                  isActive
                    ? 'bg-accent-red/10 text-accent-red border border-accent-red/20'
                    : 'text-text-secondary hover:bg-surface-3 hover:text-text-primary',
                )
              }
            >
              <item.icon size={16} />
              <span className="flex-1">{item.label}</span>
              {item.badge && (
                <span className="flex h-4 min-w-4 items-center justify-center rounded-full bg-accent-red px-1 text-[9px] font-bold text-white">
                  {item.badge}
                </span>
              )}
            </NavLink>
          ))}

          <div className="px-2 mt-4 mb-2 text-[9px] font-semibold text-text-muted uppercase tracking-wider">
            账户
          </div>
          {bottomItems.map((item) => (
            <NavLink
              key={item.to}
              to={item.to}
              className={({ isActive }) =>
                cn(
                  'flex items-center gap-2.5 rounded-lg px-2.5 py-2 text-xs font-medium transition-all',
                  isActive
                    ? 'bg-surface-3 text-text-primary'
                    : 'text-text-secondary hover:bg-surface-3 hover:text-text-primary',
                )
              }
            >
              <item.icon size={16} />
              <span className="flex-1">{item.label}</span>
              {item.badge && (
                <span className="flex h-4 min-w-4 items-center justify-center rounded-full bg-accent-amber px-1 text-[9px] font-bold text-surface-0">
                  {item.badge}
                </span>
              )}
            </NavLink>
          ))}

          {user?.role === 'admin' && (
            <NavLink
              to="/admin"
              className={({ isActive }) =>
                cn(
                  'flex items-center gap-2.5 rounded-lg px-2.5 py-2 text-xs font-medium transition-all',
                  isActive
                    ? 'bg-accent-purple/10 text-accent-purple border border-accent-purple/20'
                    : 'text-text-secondary hover:bg-surface-3 hover:text-text-primary',
                )
              }
            >
              <Shield size={16} />
              <span className="flex-1">管理后台</span>
            </NavLink>
          )}
        </nav>

        {/* User card */}
        <div className="border-t border-border-subtle p-3">
          <div className="flex items-center gap-2.5 rounded-lg bg-surface-2 p-2.5 border border-border-subtle">
            <div className="flex h-8 w-8 shrink-0 items-center justify-center rounded-full bg-accent-red/15 text-xs font-bold text-accent-red">
              {user?.avatar ?? 'U'}
            </div>
            <div className="flex-1 min-w-0">
              <div className="text-xs font-semibold truncate">{user?.username ?? '用户'}</div>
              <div className="text-[10px] text-text-muted truncate">{user?.level ?? '学员'}</div>
            </div>
            <button
              onClick={handleLogout}
              className="p-1 rounded text-text-muted hover:text-danger transition-colors"
              title="退出登录"
            >
              <LogOut size={14} />
            </button>
          </div>
        </div>
      </aside>

      {/* Main content */}
      <div className="flex flex-1 min-w-0 flex-col overflow-hidden">
        {/* Mobile top bar */}
        <div className="lg:hidden flex items-center gap-3 h-12 px-4 border-b border-border-subtle bg-surface-1 shrink-0">
          <button
            onClick={() => setSidebarOpen(true)}
            className="-ml-1.5 rounded-lg p-1.5 text-text-secondary hover:bg-surface-3 hover:text-text-primary transition-colors"
            aria-label="打开菜单"
          >
            <Menu size={18} />
          </button>
          <div className="flex items-center gap-2">
            <div className="flex h-6 w-6 items-center justify-center rounded-md bg-gradient-to-br from-accent-red to-accent-red-dim">
              <Sparkles size={11} className="text-white" />
            </div>
            <span className="text-xs font-bold">Redis Lab Studio</span>
          </div>
          <button
            onClick={() => openCommandPalette(true)}
            className="ml-auto rounded-lg p-1.5 text-text-muted hover:bg-surface-3 hover:text-text-secondary transition-colors"
            aria-label="搜索"
          >
            <Search size={16} />
          </button>
        </div>

        <main className="flex-1 min-w-0 flex flex-col overflow-hidden">
          <Suspense fallback={<PageLoader />}>
            <Outlet />
          </Suspense>
        </main>
      </div>

      <CommandPalette />
    </div>
  )
}

/** Page header for non-workspace pages */
export function PageHeader({
  title,
  subtitle,
  actions,
}: {
  title: string
  subtitle?: string
  actions?: React.ReactNode
}) {
  return (
    <motion.div
      initial={{ opacity: 0, y: -8 }}
      animate={{ opacity: 1, y: 0 }}
      className="flex items-center justify-between px-6 py-4 border-b border-border-subtle bg-surface-1 shrink-0"
    >
      <div>
        <h1 className="text-lg font-bold">{title}</h1>
        {subtitle && <p className="text-xs text-text-muted mt-0.5">{subtitle}</p>}
      </div>
      {actions && <div className="flex items-center gap-2">{actions}</div>}
    </motion.div>
  )
}

export function PageContent({ children, className }: { children: React.ReactNode; className?: string }) {
  return (
    <div className={cn('flex-1 overflow-y-auto p-6', className)}>
      {children}
    </div>
  )
}

/** Fallback shown while a lazily-loaded route chunk is being fetched. */
function PageLoader() {
  return (
    <div className="flex flex-1 flex-col items-center justify-center gap-3 text-text-muted">
      <Loader2 size={24} className="animate-spin text-accent-red" />
      <span className="text-xs">加载中…</span>
    </div>
  )
}
