import { useState, useEffect, useCallback } from 'react'
import { Search, RefreshCw, Clock, Eye, Trash2, Copy, Key as KeyIcon } from 'lucide-react'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Badge } from '@/components/ui/badge'
import { Separator } from '@/components/ui/separator'
import { EmptyState } from '@/components/shared/EmptyState'
import { useRedisStore } from '@/stores/redisStore'
import { redisService } from '@/services/redisService'
import { cn } from '@/lib/utils'
import type { RedisKey, RedisValue, HashField, ZSetMember } from '@/types/redis'

const typeBadge: Record<string, { label: string; color: string }> = {
  string: { label: 'String', color: 'bg-blue-50 text-blue-700 border-blue-200' },
  hash: { label: 'Hash', color: 'bg-amber-50 text-amber-700 border-amber-200' },
  list: { label: 'List', color: 'bg-emerald-50 text-emerald-700 border-emerald-200' },
  set: { label: 'Set', color: 'bg-violet-50 text-violet-700 border-violet-200' },
  zset: { label: 'ZSet', color: 'bg-rose-50 text-rose-700 border-rose-200' },
  stream: { label: 'Stream', color: 'bg-cyan-50 text-cyan-700 border-cyan-200' },
}

export default function KeyBrowser() {
  const keys = useRedisStore((s) => s.keys)
  const selected = useRedisStore((s) => s.selectedKey)
  const selectKey = useRedisStore((s) => s.selectKey)
  const setKeys = useRedisStore((s) => s.setKeys)
  const [search, setSearch] = useState('')
  const [loading, setLoading] = useState(false)

  const loadKeys = useCallback(async () => {
    setLoading(true)
    try { const d = await redisService.getKeys(search || undefined); setKeys(d) } catch { setKeys([]) }
    setLoading(false)
  }, [search, setKeys])

  useEffect(() => { loadKeys() }, [])

  const filtered = keys.filter((k) => !search || k.name.toLowerCase().includes(search.toLowerCase()))

  return (
    <div className="flex h-full">
      {/* Key List (280px) */}
      <div className="flex w-[280px] flex-shrink-0 flex-col border-r">
        <div className="border-b p-3 space-y-2">
          <div className="relative">
            <Search className="absolute left-2.5 top-2 h-3.5 w-3.5 text-muted-foreground" />
            <Input placeholder="搜索 Key..." value={search}
              onChange={(e) => setSearch(e.target.value)}
              className="h-8 pl-8 text-xs" />
          </div>
          <div className="flex items-center gap-2">
            <Button variant="ghost" size="icon-sm" className="h-7 w-7" onClick={loadKeys} disabled={loading}>
              <RefreshCw className={cn('h-3.5 w-3.5', loading && 'animate-spin')} />
            </Button>
            <span className="text-[11px] text-muted-foreground">{filtered.length} keys</span>
          </div>
        </div>
        <ScrollArea className="flex-1">
          {filtered.map((k) => (
            <button key={k.name}
              onClick={() => selectKey(k.name)}
              className={cn(
                'flex w-full items-center gap-2 px-3 py-1.5 text-left transition-colors hover:bg-muted/50',
                selected === k.name && 'bg-primary/10 border-l-[3px] border-l-primary',
              )}>
              <span className={cn('flex h-4 w-4 items-center justify-center rounded text-[9px] font-bold shrink-0 border', typeBadge[k.type]?.color)}>
                {k.type[0]?.toUpperCase()}
              </span>
              <span className="truncate text-[13px] font-mono">{k.name.split(':').pop()}</span>
              {k.ttl > 0 && <span className="ml-auto shrink-0 text-[10px] text-muted-foreground">{fmtTTL(k.ttl)}</span>}
            </button>
          ))}
          {filtered.length === 0 && <div className="p-4 text-center text-xs text-muted-foreground">暂无 Key</div>}
        </ScrollArea>
      </div>

      {/* Data Viewer (flex-1) */}
      <div className="flex-1 overflow-hidden">
        {selected ? <DataPanel /> : <EmptyState icon={KeyIcon} title="选择 Key" description="从左侧选择一个 Key 查看数据" />}
      </div>
    </div>
  )
}

// ========= Data Panel =========
function DataPanel() {
  const key = useRedisStore((s) => s.selectedKey)!
  const [value, setValue] = useState<RedisValue | null>(null)
  const [loading, setLoading] = useState(false)

  useEffect(() => {
    setLoading(true)
    redisService.getValue(key).then((v) => { setValue(v); setLoading(false) })
  }, [key])

  if (loading) return <div className="flex h-full items-center justify-center text-sm text-muted-foreground">加载中...</div>
  if (!value) return <EmptyState icon={Eye} title="无数据" />

  const info = typeBadge[value.type] || typeBadge.string

  return (
    <ScrollArea className="h-full">
      <div className="p-6 space-y-6">
        {/* Metadata bar */}
        <div className="flex items-center gap-3 flex-wrap">
          <h2 className="text-sm font-semibold font-mono truncate max-w-[400px]" title={key}>{key}</h2>
          <Badge variant="outline" className={cn('text-[11px]', info.color)}>{info.label}</Badge>
          {'ttl' in value && value.ttl > 0 && (
            <span className="flex items-center gap-1 text-xs text-muted-foreground"><Clock className="h-3 w-3" />TTL: {fmtTTL(value.ttl)}</span>
          )}
          {'ttl' in value && value.ttl === -1 && <span className="text-xs text-muted-foreground">永不过期</span>}
          {'length' in value && <span className="text-xs text-muted-foreground">{value.length} 条</span>}
        </div>
        <Separator />
        {/* Render by type */}
        <RenderValue value={value} />
      </div>
    </ScrollArea>
  )
}

function RenderValue({ value }: { value: RedisValue }) {
  switch (value.type) {
    case 'string': return <StringDisplay value={value.value} />
    case 'hash': return <HashTable fields={value.fields} />
    case 'list': return <ListTable items={value.values} />
    case 'set': return <SetGrid members={value.members} />
    case 'zset': return <ZSetTable members={value.members} />
    case 'stream': return <StreamCards messages={value.messages} groups={'consumerGroups' in value ? value.consumerGroups : []} />
    default: return <pre className="text-xs font-mono">{JSON.stringify(value, null, 2)}</pre>
  }
}

function StringDisplay({ value }: { value: string }) {
  const isJson = (value.startsWith('{') || value.startsWith('['))
  return (
    <div className="rounded-lg border bg-muted/30 p-4">
      <pre className="text-[13px] font-mono whitespace-pre-wrap break-all">
        {isJson ? (() => { try { return JSON.stringify(JSON.parse(value), null, 2) } catch { return value } })() : value}
      </pre>
    </div>
  )
}

function HashTable({ fields }: { fields: HashField[] }) {
  if (!fields.length) return <p className="text-sm text-muted-foreground">空 Hash</p>
  return (
    <div className="rounded-lg border overflow-hidden">
      <table className="w-full text-[13px]">
        <thead><tr className="bg-muted/50 border-b"><th className="text-left px-4 py-2 text-[11px] font-medium text-muted-foreground uppercase tracking-wider">Field</th><th className="text-left px-4 py-2 text-[11px] font-medium text-muted-foreground uppercase tracking-wider">Value</th></tr></thead>
        <tbody>{fields.map((f, i) => <tr key={i} className="border-b last:border-0 hover:bg-muted/30"><td className="px-4 py-2 font-mono text-xs font-medium">{f.field}</td><td className="px-4 py-2 font-mono text-xs max-w-[500px] truncate">{f.value}</td></tr>)}</tbody>
      </table>
    </div>
  )
}

function ListTable({ items }: { items: { index: number; value: string }[] }) {
  if (!items.length) return <p className="text-sm text-muted-foreground">空 List</p>
  return (
    <div className="rounded-lg border overflow-hidden">
      <table className="w-full text-[13px]">
        <thead><tr className="bg-muted/50 border-b"><th className="text-left px-4 py-2 text-[11px] font-medium text-muted-foreground w-16">#</th><th className="text-left px-4 py-2 text-[11px] font-medium text-muted-foreground">Value</th></tr></thead>
        <tbody>{items.map((it) => <tr key={it.index} className="border-b last:border-0 hover:bg-muted/30"><td className="px-4 py-2 text-xs text-muted-foreground">{it.index}</td><td className="px-4 py-2 font-mono text-xs">{it.value}</td></tr>)}</tbody>
      </table>
    </div>
  )
}

function SetGrid({ members }: { members: string[] }) {
  if (!members.length) return <p className="text-sm text-muted-foreground">空 Set</p>
  return <div className="flex flex-wrap gap-2">{members.map((m, i) => <span key={i} className="rounded-lg border bg-muted/30 px-3 py-1.5 text-[13px] font-mono">{m}</span>)}</div>
}

function ZSetTable({ members }: { members: ZSetMember[] }) {
  if (!members.length) return <p className="text-sm text-muted-foreground">空 ZSet</p>
  const sorted = [...members].sort((a, b) => b.score - a.score)
  return (
    <div className="rounded-lg border overflow-hidden">
      <table className="w-full text-[13px]">
        <thead><tr className="bg-muted/50 border-b"><th className="text-left px-4 py-2 text-[11px] font-medium text-muted-foreground w-12">#</th><th className="text-left px-4 py-2 text-[11px] font-medium text-muted-foreground">Member</th><th className="text-right px-4 py-2 text-[11px] font-medium text-muted-foreground">Score</th></tr></thead>
        <tbody>{sorted.map((m, i) => <tr key={i} className="border-b last:border-0 hover:bg-muted/30"><td className="px-4 py-2 text-xs text-muted-foreground">{i + 1}</td><td className="px-4 py-2 font-mono text-xs">{m.member}</td><td className="px-4 py-2 text-xs text-right font-mono tabular-nums">{m.score.toLocaleString()}</td></tr>)}</tbody>
      </table>
    </div>
  )
}

function StreamCards({ messages, groups }: { messages: { id: string; fields: Record<string, string> }[]; groups: { name: string; consumers: number; pending: number }[] }) {
  if (!messages.length) return <p className="text-sm text-muted-foreground">空 Stream</p>
  return (
    <div className="space-y-4">
      {groups.length > 0 && (
        <div className="rounded-lg border p-4">
          <h3 className="text-xs font-semibold mb-2 uppercase tracking-wider text-muted-foreground">消费者组</h3>
          <div className="space-y-1">{groups.map((g) => <div key={g.name} className="flex items-center gap-4 text-xs"><span className="font-mono font-medium">{g.name}</span><span className="text-muted-foreground">{g.consumers} consumers</span><span className="text-muted-foreground">{g.pending} pending</span></div>)}</div>
        </div>
      )}
      {messages.map((m) => (
        <div key={m.id} className="rounded-lg border p-4">
          <div className="text-[11px] text-muted-foreground mb-2 font-mono">{m.id}</div>
          <div className="grid grid-cols-2 gap-x-4 gap-y-1">
            {Object.entries(m.fields).map(([k, v]) => <div key={k} className="text-xs"><span className="font-medium">{k}:</span> <span className="font-mono">{v}</span></div>)}
          </div>
        </div>
      ))}
    </div>
  )
}

function fmtTTL(s: number): string {
  if (s < 60) return `${s}s`
  if (s < 3600) return `${Math.floor(s / 60)}m`
  if (s < 86400) return `${Math.floor(s / 3600)}h`
  return `${Math.floor(s / 86400)}d`
}
