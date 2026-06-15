import { Badge } from '@/components/ui/Badge'
import { Button } from '@/components/ui/Button'
import { KeyTypeIcon, getTypeBadgeVariant } from '@/components/ui/KeyTypeIcon'
import { cn, formatBytes, formatTTL } from '@/lib/utils'
import {
  mockHashData,
  mockListData,
  mockSetData,
  mockZSetData,
  useAppStore,
} from '@/stores/appStore'
import type { RedisKeyType } from '@/types'
import {
  Clock,
  Edit3,
  Plus,
  RefreshCw,
  Trash2,
} from 'lucide-react'

function HashViewer() {
  return (
    <table className="w-full text-xs">
      <thead>
        <tr className="border-b border-border-subtle text-left">
          <th className="px-4 py-2 font-semibold text-text-muted w-1/3">Field</th>
          <th className="px-4 py-2 font-semibold text-text-muted">Value</th>
          <th className="px-4 py-2 w-16" />
        </tr>
      </thead>
      <tbody>
        {mockHashData.map((row, i) => (
          <tr
            key={row.field}
            className={cn(
              'border-b border-border-subtle/50 hover:bg-surface-3/50 transition-colors group',
              i % 2 === 0 ? 'bg-surface-1' : 'bg-surface-2/30',
            )}
          >
            <td className="px-4 py-2 font-mono text-accent-amber">{row.field}</td>
            <td className="px-4 py-2 font-mono text-text-primary">{row.value}</td>
            <td className="px-2 opacity-0 group-hover:opacity-100 transition-opacity">
              <div className="flex gap-0.5">
                <button className="p-1 rounded hover:bg-surface-4 text-text-muted">
                  <Edit3 size={11} />
                </button>
                <button className="p-1 rounded hover:bg-danger/10 text-text-muted hover:text-danger">
                  <Trash2 size={11} />
                </button>
              </div>
            </td>
          </tr>
        ))}
      </tbody>
    </table>
  )
}

function ZSetViewer() {
  return (
    <table className="w-full text-xs">
      <thead>
        <tr className="border-b border-border-subtle text-left">
          <th className="px-4 py-2 font-semibold text-text-muted w-16">#</th>
          <th className="px-4 py-2 font-semibold text-text-muted">Member</th>
          <th className="px-4 py-2 font-semibold text-text-muted w-28 text-right">Score</th>
        </tr>
      </thead>
      <tbody>
        {mockZSetData.map((row, i) => (
          <tr
            key={row.member}
            className={cn(
              'border-b border-border-subtle/50 hover:bg-surface-3/50',
              i % 2 === 0 ? 'bg-surface-1' : 'bg-surface-2/30',
            )}
          >
            <td className="px-4 py-2 text-text-muted font-mono">{i + 1}</td>
            <td className="px-4 py-2 font-mono text-text-primary">{row.member}</td>
            <td className="px-4 py-2 font-mono text-accent-red text-right font-semibold">
              {row.score.toLocaleString()}
            </td>
          </tr>
        ))}
      </tbody>
    </table>
  )
}

function ListViewer() {
  return (
    <div className="divide-y divide-border-subtle/50">
      {mockListData.map((item, i) => (
        <div
          key={i}
          className={cn(
            'flex items-center gap-3 px-4 py-2.5 hover:bg-surface-3/50',
            i % 2 === 0 ? 'bg-surface-1' : 'bg-surface-2/30',
          )}
        >
          <span className="w-8 text-center font-mono text-[10px] text-text-muted">{i}</span>
          <span className="font-mono text-xs text-text-primary">{item}</span>
        </div>
      ))}
    </div>
  )
}

function SetViewer() {
  return (
    <div className="flex flex-wrap gap-2 p-4">
      {mockSetData.map((item) => (
        <span
          key={item}
          className="rounded-md border border-accent-purple/20 bg-accent-purple/10 px-2.5 py-1 font-mono text-xs text-accent-purple"
        >
          {item}
        </span>
      ))}
    </div>
  )
}

function StringViewer() {
  return (
    <div className="p-4">
      <pre className="rounded-lg border border-border-subtle bg-surface-0 p-4 font-mono text-sm text-text-primary leading-relaxed whitespace-pre-wrap">
        {`{"session_id":"abc123","user_id":1001,"expires":"2025-06-15T12:00:00Z","permissions":["read","write"]}`}
      </pre>
    </div>
  )
}

const viewers: Record<RedisKeyType, () => React.ReactNode> = {
  hash: HashViewer,
  zset: ZSetViewer,
  list: ListViewer,
  set: SetViewer,
  string: StringViewer,
  stream: () => (
    <div className="p-8 text-center text-text-muted text-xs">
      Stream 数据查看器 — 消息流可视化
    </div>
  ),
}

export function KeyDetailPanel() {
  const { selectedKey } = useAppStore()

  if (!selectedKey) {
    return (
      <div className="flex h-full items-center justify-center text-text-muted text-sm">
        从左侧选择一个键查看详情
      </div>
    )
  }

  const Viewer = viewers[selectedKey.type]

  return (
    <div className="flex h-full flex-col">
      {/* Key header */}
      <div className="flex items-center justify-between border-b border-border-subtle px-4 py-2.5 bg-surface-2/30">
        <div className="flex items-center gap-3 min-w-0">
          <KeyTypeIcon type={selectedKey.type} size={16} showLabel />
          <span className="font-mono text-sm font-medium truncate">{selectedKey.name}</span>
          <Badge variant={getTypeBadgeVariant(selectedKey.type)}>{selectedKey.type}</Badge>
        </div>
        <div className="flex items-center gap-3 shrink-0">
          <div className="flex items-center gap-1 text-[10px] text-text-muted">
            <Clock size={11} />
            TTL: <span className="font-mono text-warning">{formatTTL(selectedKey.ttl)}</span>
          </div>
          <div className="text-[10px] text-text-muted">
            大小: <span className="font-mono">{formatBytes(selectedKey.size)}</span>
          </div>
          {selectedKey.encoding && (
            <div className="text-[10px] text-text-muted">
              编码: <span className="font-mono">{selectedKey.encoding}</span>
            </div>
          )}
          <div className="flex gap-0.5 ml-2">
            <Button variant="ghost" size="icon" className="h-6 w-6">
              <RefreshCw size={12} />
            </Button>
            <Button variant="ghost" size="icon" className="h-6 w-6">
              <Plus size={12} />
            </Button>
            <Button variant="ghost" size="icon" className="h-6 w-6">
              <Edit3 size={12} />
            </Button>
            <Button variant="ghost" size="icon" className="h-6 w-6 text-danger hover:text-danger">
              <Trash2 size={12} />
            </Button>
          </div>
        </div>
      </div>

      {/* Data viewer */}
      <div className="flex-1 overflow-auto">
        <Viewer />
      </div>
    </div>
  )
}
