import { useState, useEffect, useCallback } from 'react'
import { cn } from '@/lib/utils'
import { useRedisStore } from '@/stores/redisStore'
import { redisService } from '@/services/redisService'
import { Button } from '@/components/ui/button'
import { Card } from '@/components/ui/card'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs'
import { Separator } from '@/components/ui/separator'
import { Skeleton } from '@/components/ui/skeleton'
import { RefreshCw, Activity, Database, Users, Cpu, HardDrive, Clock } from 'lucide-react'
import type { ServerInfo, SlowLogEntry } from '@/types/redis'

function formatUptime(seconds: number): string {
  const d = Math.floor(seconds / 86400)
  const h = Math.floor((seconds % 86400) / 3600)
  const m = Math.floor((seconds % 3600) / 60)
  if (d > 0) return `${d}d ${h}h ${m}m`
  if (h > 0) return `${h}h ${m}m`
  return `${m}m`
}

function MetricCard({
  icon,
  label,
  value,
  sublabel,
}: {
  icon: React.ReactNode
  label: string
  value: string
  sublabel?: string
}) {
  return (
    <Card className="p-4 flex flex-col gap-1">
      <div className="flex items-center gap-2 text-muted-foreground">
        {icon}
        <span className="text-xs font-medium">{label}</span>
      </div>
      <span className="text-xl font-bold font-mono">{value}</span>
      {sublabel && <span className="text-[10px] text-muted-foreground">{sublabel}</span>}
    </Card>
  )
}

export default function ServerMonitor() {
  const activeConnectionId = useRedisStore((s) => s.activeConnectionId)
  const serverInfo = useRedisStore((s) => s.serverInfo)
  const slowLog = useRedisStore((s) => s.slowLog)
  const setServerInfo = useRedisStore((s) => s.setServerInfo)
  const setSlowLog = useRedisStore((s) => s.setSlowLog)

  const [isLoading, setIsLoading] = useState(false)
  const [autoRefresh, setAutoRefresh] = useState(false)

  const loadData = useCallback(async () => {
    if (!activeConnectionId) return
    setIsLoading(true)
    try {
      const [info, log] = await Promise.all([
        redisService.getServerInfo(activeConnectionId),
        redisService.getSlowLog(activeConnectionId, 10),
      ])
      if (info) setServerInfo(info)
      setSlowLog(log)
    } finally {
      setIsLoading(false)
    }
  }, [activeConnectionId, setServerInfo, setSlowLog])

  useEffect(() => {
    loadData()
  }, [loadData])

  // Auto-refresh every 5 seconds
  useEffect(() => {
    if (!autoRefresh) return
    const interval = setInterval(loadData, 5000)
    return () => clearInterval(interval)
  }, [autoRefresh, loadData])

  if (!activeConnectionId) {
    return (
      <div className="flex items-center justify-center h-full text-muted-foreground">
        <div className="text-center">
          <Activity className="h-8 w-8 mx-auto mb-2 opacity-40" />
          <p className="text-sm">请先选择 Redis 连接</p>
        </div>
      </div>
    )
  }

  if (isLoading && !serverInfo) {
    return (
      <div className="p-4 space-y-4">
        <Skeleton className="h-8 w-48" />
        <div className="grid grid-cols-3 gap-4">
          {[...Array(6)].map((_, i) => (
            <Skeleton key={i} className="h-24" />
          ))}
        </div>
      </div>
    )
  }

  if (!serverInfo) {
    return (
      <div className="flex items-center justify-center h-full">
        <Button variant="outline" onClick={loadData}>
          <RefreshCw className="h-4 w-4 mr-2" />
          加载服务器信息
        </Button>
      </div>
    )
  }

  const uptime = parseInt(serverInfo.server.uptime_in_seconds || '0', 10)
  const opsPerSec = serverInfo.stats.instantaneous_ops_per_sec || '0'
  const hits = parseInt(serverInfo.stats.keyspace_hits || '0', 10)
  const misses = parseInt(serverInfo.stats.keyspace_misses || '0', 10)
  const hitRateRaw = hits + misses > 0 ? (hits / (hits + misses)) * 100 : 100
  const hitRate = `${hitRateRaw.toFixed(1)}%`
  const connectedClients = serverInfo.clients.connected_clients || '0'
  const usedMemory = serverInfo.memory.used_memory_human || '0'
  const maxMemory = serverInfo.memory.maxmemory_human || '0'
  const totalKeys = Object.values(serverInfo.keyspace).reduce((sum, ks) => {
    const match = ks.match(/keys=(\d+)/)
    return sum + (match ? parseInt(match[1], 10) : 0)
  }, 0)

  return (
    <div className="flex flex-col h-full">
      {/* Toolbar */}
      <div className="flex items-center gap-2 px-3 py-2 border-b bg-muted/30">
        <Activity className="h-4 w-4 text-muted-foreground" />
        <span className="text-sm font-semibold">服务器监控</span>
        <span className="text-xs text-muted-foreground">
          {serverInfo.server.redis_version} | {serverInfo.server.redis_mode}
        </span>
        <div className="flex-1" />
        <div className="flex items-center gap-2">
          <label className="flex items-center gap-1.5 text-xs cursor-pointer select-none">
            <span className="text-muted-foreground">自动刷新</span>
            <input
              type="checkbox"
              checked={autoRefresh}
              onChange={(e) => setAutoRefresh(e.target.checked)}
              className="rounded"
            />
          </label>
          <Button
            variant="outline"
            size="sm"
            className="h-7 text-xs"
            onClick={loadData}
            disabled={isLoading}
          >
            <RefreshCw className={cn('h-3.5 w-3.5 mr-1', isLoading && 'animate-spin')} />
            刷新
          </Button>
        </div>
      </div>

      <ScrollArea className="flex-1">
        <Tabs defaultValue="overview" className="p-4">
          <TabsList className="mb-4">
            <TabsTrigger value="overview" className="text-xs">概览</TabsTrigger>
            <TabsTrigger value="memory" className="text-xs">内存</TabsTrigger>
            <TabsTrigger value="clients" className="text-xs">客户端</TabsTrigger>
            <TabsTrigger value="slowlog" className="text-xs">慢查询</TabsTrigger>
            <TabsTrigger value="replication" className="text-xs">复制</TabsTrigger>
          </TabsList>

          {/* Overview Tab */}
          <TabsContent value="overview" className="space-y-4">
            <div className="grid grid-cols-2 md:grid-cols-3 gap-3">
              <MetricCard
                icon={<Clock className="h-4 w-4" />}
                label="运行时间"
                value={formatUptime(uptime)}
                sublabel={serverInfo.server.os}
              />
              <MetricCard
                icon={<Activity className="h-4 w-4" />}
                label="QPS"
                value={parseInt(opsPerSec).toLocaleString()}
                sublabel="每秒操作数"
              />
              <MetricCard
                icon={<Database className="h-4 w-4" />}
                label="命中率"
                value={hitRate}
                sublabel={`Hits: ${hits.toLocaleString()} | Miss: ${misses.toLocaleString()}`}
              />
              <MetricCard
                icon={<Users className="h-4 w-4" />}
                label="连接客户端"
                value={connectedClients}
                sublabel={`最大: ${serverInfo.clients.maxclients}`}
              />
              <MetricCard
                icon={<HardDrive className="h-4 w-4" />}
                label="内存使用"
                value={usedMemory}
                sublabel={`最大: ${maxMemory}`}
              />
              <MetricCard
                icon={<Database className="h-4 w-4" />}
                label="Key 数量"
                value={String(totalKeys)}
                sublabel={`DB 数量: ${Object.keys(serverInfo.keyspace).length}`}
              />
            </div>

            <Separator />

            {/* Additional Stats */}
            <div className="grid grid-cols-2 gap-3">
              <Card className="p-3">
                <span className="text-xs font-semibold text-muted-foreground">命令统计</span>
                <div className="mt-2 space-y-1 text-xs">
                  <div className="flex justify-between">
                    <span>处理命令总数</span>
                    <span className="font-mono">{parseInt(serverInfo.stats.total_commands_processed || '0').toLocaleString()}</span>
                  </div>
                  <div className="flex justify-between">
                    <span>连接总数</span>
                    <span className="font-mono">{parseInt(serverInfo.stats.total_connections_received || '0').toLocaleString()}</span>
                  </div>
                  <div className="flex justify-between">
                    <span>过期 Key</span>
                    <span className="font-mono">{parseInt(serverInfo.stats.expired_keys || '0').toLocaleString()}</span>
                  </div>
                  <div className="flex justify-between">
                    <span>淘汰 Key</span>
                    <span className="font-mono">{parseInt(serverInfo.stats.evicted_keys || '0').toLocaleString()}</span>
                  </div>
                </div>
              </Card>

              <Card className="p-3">
                <span className="text-xs font-semibold text-muted-foreground">CPU</span>
                <div className="mt-2 space-y-1 text-xs">
                  <div className="flex justify-between">
                    <span>CPU User</span>
                    <span className="font-mono">{parseFloat(serverInfo.cpu.used_cpu_user || '0').toLocaleString()}s</span>
                  </div>
                  <div className="flex justify-between">
                    <span>CPU Sys</span>
                    <span className="font-mono">{parseFloat(serverInfo.cpu.used_cpu_sys || '0').toLocaleString()}s</span>
                  </div>
                  <div className="flex justify-between">
                    <span>进程 ID</span>
                    <span className="font-mono">{serverInfo.server.process_id}</span>
                  </div>
                  <div className="flex justify-between">
                    <span>TCP Port</span>
                    <span className="font-mono">{serverInfo.server.tcp_port}</span>
                  </div>
                </div>
              </Card>
            </div>
          </TabsContent>

          {/* Memory Tab */}
          <TabsContent value="memory" className="space-y-3">
            <h4 className="text-sm font-semibold">内存统计</h4>
            <div className="grid grid-cols-2 gap-3">
              {Object.entries(serverInfo.memory).map(([key, val]) => (
                <Card key={key} className="p-3 flex justify-between items-center">
                  <span className="text-xs text-muted-foreground">{key}</span>
                  <span className="text-sm font-mono font-semibold">{val}</span>
                </Card>
              ))}
            </div>
          </TabsContent>

          {/* Clients Tab */}
          <TabsContent value="clients" className="space-y-3">
            <h4 className="text-sm font-semibold">客户端连接信息</h4>
            <table className="w-full text-sm border rounded-md">
              <thead className="bg-muted/50">
                <tr className="border-b">
                  <th className="px-3 py-2 text-left text-xs font-medium text-muted-foreground">属性</th>
                  <th className="px-3 py-2 text-left text-xs font-medium text-muted-foreground">值</th>
                </tr>
              </thead>
              <tbody>
                {Object.entries(serverInfo.clients).map(([key, val], idx) => (
                  <tr key={key} className={cn('border-b', idx % 2 === 0 && 'bg-muted/10')}>
                    <td className="px-3 py-1.5 text-xs font-medium">{key}</td>
                    <td className="px-3 py-1.5 text-xs font-mono">{val}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </TabsContent>

          {/* Slowlog Tab */}
          <TabsContent value="slowlog" className="space-y-3">
            <h4 className="text-sm font-semibold">慢查询日志 (Top 10)</h4>
            {slowLog.length === 0 ? (
              <p className="text-xs text-muted-foreground">暂无慢查询记录</p>
            ) : (
              <table className="w-full text-sm border rounded-md">
                <thead className="bg-muted/50">
                  <tr className="border-b">
                    <th className="px-3 py-2 text-left text-xs font-medium text-muted-foreground w-12">ID</th>
                    <th className="px-3 py-2 text-left text-xs font-medium text-muted-foreground">时间</th>
                    <th className="px-3 py-2 text-right text-xs font-medium text-muted-foreground w-20">耗时</th>
                    <th className="px-3 py-2 text-left text-xs font-medium text-muted-foreground">命令</th>
                    <th className="px-3 py-2 text-left text-xs font-medium text-muted-foreground w-40">客户端</th>
                  </tr>
                </thead>
                <tbody>
                  {slowLog.map((entry, idx) => (
                    <tr key={entry.id} className={cn('border-b', idx % 2 === 0 && 'bg-muted/10')}>
                      <td className="px-3 py-1.5 text-xs font-mono tabular-nums">{entry.id}</td>
                      <td className="px-3 py-1.5 text-xs text-muted-foreground">
                        {new Date(entry.timestamp).toLocaleString()}
                      </td>
                      <td className="px-3 py-1.5 text-xs font-mono text-right">
                        <span
                          className={cn(
                            'tabular-nums',
                            entry.duration > 100000 ? 'text-red-600 font-semibold' : 'text-orange-500'
                          )}
                        >
                          {(entry.duration / 1000).toFixed(1)}ms
                        </span>
                      </td>
                      <td className="px-3 py-1.5">
                        <span className="text-xs font-mono truncate block max-w-[400px]">
                          {entry.command}
                        </span>
                      </td>
                      <td className="px-3 py-1.5 text-xs font-mono text-muted-foreground">
                        {entry.clientIp}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            )}
          </TabsContent>

          {/* Replication Tab */}
          <TabsContent value="replication" className="space-y-3">
            <h4 className="text-sm font-semibold">复制状态</h4>
            <div className="grid grid-cols-2 gap-3 mb-4">
              <Card className="p-4">
                <span className="text-xs text-muted-foreground">角色</span>
                <p className="text-lg font-bold mt-1">{serverInfo.replication.role}</p>
              </Card>
              <Card className="p-4">
                <span className="text-xs text-muted-foreground">已连接从节点</span>
                <p className="text-lg font-bold mt-1">{serverInfo.replication.connected_slaves || '0'}</p>
              </Card>
            </div>

            {Object.entries(serverInfo.replication)
              .filter(([k]) => k.startsWith('slave'))
              .map(([key, val]) => (
                <Card key={key} className="p-3 mb-2">
                  <span className="text-xs font-semibold text-muted-foreground">{key}</span>
                  <div className="mt-1 flex flex-wrap gap-2">
                    {val.split(',').map((part, i) => (
                      <span key={i} className="text-xs font-mono bg-muted px-2 py-0.5 rounded">
                        {part.trim()}
                      </span>
                    ))}
                  </div>
                </Card>
              ))}

            {!Object.keys(serverInfo.replication).some((k) => k.startsWith('slave')) && (
              <p className="text-xs text-muted-foreground">无已连接的从节点</p>
            )}
          </TabsContent>
        </Tabs>
      </ScrollArea>
    </div>
  )
}
