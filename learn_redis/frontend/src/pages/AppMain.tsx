import { useState, useEffect, lazy, Suspense } from 'react'
import { MessageSquare, LogOut, Database, Key, Terminal, Activity, FileCode, Search, Plus, RefreshCw } from 'lucide-react'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs'
import { Separator } from '@/components/ui/separator'
import { Badge } from '@/components/ui/badge'
import { EmptyState } from '@/components/shared/EmptyState'
import { ErrorBoundary } from '@/components/shared/ErrorBoundary'
import { useRedisStore } from '@/stores/redisStore'
import { useAuthStore } from '@/stores/authStore'
import { redisService } from '@/services/redisService'
import ConnectionForm from '@/components/redis/ConnectionForm'
import { cn } from '@/lib/utils'

// Lazy sub-pages
const KeyBrowser = lazy(() => import('@/components/redis/KeyBrowser'))
const DataViewer = lazy(() => import('@/components/redis/DataViewer'))
const QueryEditor = lazy(() => import('@/components/redis/QueryEditor'))
const ServerMonitor = lazy(() => import('@/components/redis/ServerMonitor'))
const CliTerminal = lazy(() => import('@/components/redis/CliTerminal'))
import FloatingDock from '@/components/FloatingDock'

// ==================== Main App ====================
export default function AppMain() {
  const connections = useRedisStore((s) => s.connections)
  const activeId = useRedisStore((s) => s.activeConnectionId)
  const setActive = useRedisStore((s) => s.setActiveConnection)
  const setConnections = useRedisStore((s) => s.setConnections)
  const selectedKey = useRedisStore((s) => s.selectedKey)
  const logout = useAuthStore((s) => s.logout)
  const [showConnForm, setShowConnForm] = useState(false)
  const [activeTab, setActiveTab] = useState('browser')

  useEffect(() => { redisService.getConnections().then(setConnections) }, [setConnections])

  const activeConn = connections.find((c) => c.id === activeId)

  return (
    <div className="flex h-screen overflow-hidden bg-background">
      {/* ===== LEFT PANEL: Connection Tree (280px) ===== */}
      <div className="flex w-[280px] flex-shrink-0 flex-col border-r bg-card">
        {/* Header */}
        <div className="flex items-center justify-between border-b px-4 py-3">
          <div className="flex items-center gap-2.5">
            <div className="flex h-8 w-8 items-center justify-center rounded-lg bg-primary">
              <Database className="h-4 w-4 text-primary-foreground" />
            </div>
            <span className="text-sm font-semibold">Learn Redis</span>
          </div>
          <Button variant="ghost" size="icon-sm" onClick={() => setShowConnForm(true)}>
            <Plus className="h-4 w-4" />
          </Button>
        </div>

        {/* Connection list */}
        <ScrollArea className="flex-1">
          {connections.map((c) => (
            <button key={c.id}
              onClick={() => setActive(c.id)}
              className={cn(
                'flex w-full items-center gap-2.5 px-4 py-2.5 text-left transition-colors hover:bg-muted/50',
                activeId === c.id && 'bg-muted border-l-[3px] border-l-primary',
              )}>
              <span className={cn('h-2 w-2 rounded-full shrink-0', c.status === 'connected' ? 'bg-emerald-500' : 'bg-gray-300')} />
              <div className="min-w-0 flex-1">
                <div className="truncate text-[13px] font-medium">{c.name}</div>
                <div className="text-[11px] text-muted-foreground">{c.host}:{c.port}</div>
              </div>
            </button>
          ))}
          {connections.length === 0 && (
            <div className="px-4 py-8 text-center text-sm text-muted-foreground">
              暂无连接，点击 + 添加
            </div>
          )}
        </ScrollArea>

        {/* Bottom actions */}
        <div className="border-t p-2 flex items-center justify-end gap-1">
          <Button variant="ghost" size="icon-sm" className="h-8 w-8" title="退出" onClick={logout}>
            <LogOut className="h-4 w-4" />
          </Button>
        </div>
      </div>

      {/* ===== RIGHT: Workspace ===== */}
      <div className="flex flex-1 flex-col overflow-hidden">
        {/* Tab bar */}
        <div className="flex items-center border-b bg-card">
          <Tabs value={activeTab} onValueChange={setActiveTab} className="flex-1">
            <TabsList className="h-9 rounded-none border-b-0 bg-transparent p-0">
              {[
                { id: 'browser', icon: Key, label: 'Key 浏览器' },
                { id: 'query', icon: FileCode, label: '查询' },
                { id: 'cli', icon: Terminal, label: 'CLI' },
                { id: 'monitor', icon: Activity, label: '监控' },
              ].map((t) => (
                <TabsTrigger key={t.id} value={t.id}
                  className="h-9 gap-1.5 rounded-none border-b-2 border-transparent px-4 text-xs data-[state=active]:border-primary data-[state=active]:bg-transparent data-[state=active]:shadow-none">
                  <t.icon className="h-3.5 w-3.5" />{t.label}
                </TabsTrigger>
              ))}
            </TabsList>
          </Tabs>
          {/* Connection info */}
          {activeConn && (
            <div className="flex items-center gap-2 px-3 text-xs text-muted-foreground border-l py-2">
              <span className="h-1.5 w-1.5 rounded-full bg-emerald-500" />
              {activeConn.name} ({activeConn.host}:{activeConn.port})
            </div>
          )}
        </div>

        {/* Content area */}
        <div className="flex flex-1 overflow-hidden">
          {/* Main workspace */}
          <div className="flex-1 overflow-hidden">
            <ErrorBoundary>
              <Suspense fallback={<div className="p-6 text-sm text-muted-foreground">加载中...</div>}>
                {!activeId ? (
                  <div className="flex h-full items-center justify-center">
                    <EmptyState icon={Database} title="选择连接" description="从左侧选择一个 Redis 连接开始" />
                  </div>
                ) : (
                  <>
                    {activeTab === 'browser' && <KeyBrowser />}
                    {activeTab === 'query' && <QueryEditor />}
                    {activeTab === 'cli' && <CliTerminal />}
                    {activeTab === 'monitor' && <ServerMonitor />}
                  </>
                )}
              </Suspense>
            </ErrorBoundary>
          </div>

        </div>
      </div>

      {/* Connection Form Dialog */}
      <ConnectionForm open={showConnForm} onClose={() => setShowConnForm(false)}
        onSave={(conn) => { useRedisStore.getState().addConnection(conn); setShowConnForm(false) }} />

      <FloatingDock />
    </div>
  )
}
