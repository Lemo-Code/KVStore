import { useAppStore } from '@/stores/appStore'
import { cn } from '@/lib/utils'
import {
  BarChart3,
  Gauge,
  LineChart,
  Server,
} from 'lucide-react'

const metrics = [
  { label: 'OPS/s', value: '12,438', trend: '+5.2%', icon: Gauge, color: 'text-accent-teal' },
  { label: '命中率', value: '98.7%', trend: '+0.3%', icon: BarChart3, color: 'text-success' },
  { label: '连接数', value: '24', trend: '稳定', icon: Server, color: 'text-accent-blue' },
  { label: '网络 I/O', value: '1.2 MB/s', trend: '-2.1%', icon: LineChart, color: 'text-accent-amber' },
]

const slowQueries = [
  { cmd: 'KEYS user:*', duration: '142ms', time: '10:28:15' },
  { cmd: 'HGETALL config:app', duration: '23ms', time: '10:25:03' },
  { cmd: 'ZRANGE leaderboard:game 0 -1', duration: '18ms', time: '10:22:41' },
]

export function MonitorPanel() {
  const { bottomPanelOpen } = useAppStore()

  if (!bottomPanelOpen) return null

  return (
    <div className="h-36 shrink-0 border-t border-border-subtle bg-surface-1">
      <div className="flex h-full">
        {/* Metrics */}
        <div className="flex-1 grid grid-cols-4 gap-px bg-border-subtle">
          {metrics.map((m) => (
            <div key={m.label} className="bg-surface-1 px-4 py-3 flex flex-col justify-center">
              <div className="flex items-center gap-1.5 mb-1">
                <m.icon size={12} className={m.color} />
                <span className="text-[10px] text-text-muted uppercase tracking-wider">{m.label}</span>
              </div>
              <div className="text-lg font-bold text-text-primary font-mono">{m.value}</div>
              <div className={cn('text-[9px] mt-0.5', m.trend.startsWith('+') ? 'text-success' : m.trend.startsWith('-') ? 'text-danger' : 'text-text-muted')}>
                {m.trend}
              </div>
            </div>
          ))}
        </div>

        {/* Slow queries */}
        <div className="w-72 shrink-0 border-l border-border-subtle flex flex-col">
          <div className="px-3 py-1.5 border-b border-border-subtle text-[10px] font-semibold text-text-muted uppercase tracking-wider">
            慢查询日志
          </div>
          <div className="flex-1 overflow-y-auto">
            {slowQueries.map((q, i) => (
              <div
                key={i}
                className="flex items-center justify-between px-3 py-1.5 border-b border-border-subtle/50 hover:bg-surface-2 text-[10px]"
              >
                <span className="font-mono text-text-primary truncate flex-1">{q.cmd}</span>
                <span className="font-mono text-warning ml-2 shrink-0">{q.duration}</span>
                <span className="text-text-muted ml-2 shrink-0">{q.time}</span>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  )
}
