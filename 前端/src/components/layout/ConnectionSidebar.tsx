import { Badge } from '@/components/ui/Badge'
import { Button } from '@/components/ui/Button'
import { KeyTypeIcon } from '@/components/ui/KeyTypeIcon'
import { cn, formatBytes, formatTTL } from '@/lib/utils'
import {
  mockConnection,
  mockDatabases,
  mockKeys,
  useAppStore,
} from '@/stores/appStore'
import type { RedisKeyType } from '@/types'
import { AnimatePresence, motion } from 'framer-motion'
import {
  ChevronDown,
  ChevronRight,
  Database,
  Filter,
  FolderTree,
  Plus,
  RefreshCw,
  Search,
  Server,
} from 'lucide-react'
import { useState } from 'react'

const typeFilters: { value: RedisKeyType | 'all'; label: string }[] = [
  { value: 'all', label: '全部' },
  { value: 'string', label: 'String' },
  { value: 'hash', label: 'Hash' },
  { value: 'list', label: 'List' },
  { value: 'set', label: 'Set' },
  { value: 'zset', label: 'ZSet' },
  { value: 'stream', label: 'Stream' },
]

export function ConnectionSidebar() {
  const { activeDb, selectedKey, searchQuery, setActiveDb, setSelectedKey, setSearchQuery } =
    useAppStore()
  const [expandedDbs, setExpandedDbs] = useState<Set<number>>(new Set([0]))
  const [typeFilter, setTypeFilter] = useState<RedisKeyType | 'all'>('all')
  const conn = mockConnection

  const filteredKeys = mockKeys.filter((k) => {
    const matchSearch = k.name.toLowerCase().includes(searchQuery.toLowerCase())
    const matchType = typeFilter === 'all' || k.type === typeFilter
    return matchSearch && matchType
  })

  const toggleDb = (id: number) => {
    setExpandedDbs((prev) => {
      const next = new Set(prev)
      if (next.has(id)) next.delete(id)
      else next.add(id)
      return next
    })
  }

  return (
    <div className="flex h-full flex-col bg-surface-1">
      {/* Header */}
      <div className="flex items-center justify-between border-b border-border-subtle px-3 py-2">
        <div className="flex items-center gap-2">
          <FolderTree size={14} className="text-accent-red" />
          <span className="text-xs font-semibold text-text-secondary uppercase tracking-wider">
            连接管理
          </span>
        </div>
        <div className="flex gap-0.5">
          <Button variant="ghost" size="icon" className="h-6 w-6">
            <Plus size={13} />
          </Button>
          <Button variant="ghost" size="icon" className="h-6 w-6">
            <RefreshCw size={13} />
          </Button>
        </div>
      </div>

      {/* Connection node */}
      <div className="flex-1 overflow-y-auto">
        <div className="px-2 py-2">
          <div className="flex items-center gap-1.5 rounded-md px-2 py-1.5 text-xs">
            <Server size={14} className="text-accent-red shrink-0" />
            <span className="font-medium truncate">{conn.name}</span>
            <Badge variant="success" className="ml-auto shrink-0">
              v{conn.version}
            </Badge>
          </div>

          {/* Stats row */}
          <div className="mx-2 mb-2 grid grid-cols-3 gap-1 rounded-md bg-surface-2 p-2 border border-border-subtle">
            {[
              { label: '内存', value: conn.memory },
              { label: '键数', value: String(conn.keys) },
              { label: '运行', value: conn.uptime },
            ].map((s) => (
              <div key={s.label} className="text-center">
                <div className="text-[10px] text-text-muted">{s.label}</div>
                <div className="text-[11px] font-semibold text-text-primary">{s.value}</div>
              </div>
            ))}
          </div>

          {/* Databases */}
          {mockDatabases.map((db) => {
            const isExpanded = expandedDbs.has(db.id)
            const isActive = activeDb === db.id

            return (
              <div key={db.id}>
                <button
                  onClick={() => {
                    toggleDb(db.id)
                    setActiveDb(db.id)
                  }}
                  className={cn(
                    'flex w-full items-center gap-1.5 rounded-md px-2 py-1.5 text-xs transition-colors',
                    isActive ? 'bg-surface-3 text-text-primary' : 'hover:bg-surface-2 text-text-secondary',
                  )}
                >
                  {isExpanded ? (
                    <ChevronDown size={12} className="shrink-0 text-text-muted" />
                  ) : (
                    <ChevronRight size={12} className="shrink-0 text-text-muted" />
                  )}
                  <Database size={13} className="shrink-0 text-accent-amber" />
                  <span className="font-medium">{db.name}</span>
                  <span className="ml-auto text-[10px] text-text-muted">{db.keyCount}</span>
                </button>

                <AnimatePresence>
                  {isExpanded && isActive && (
                    <motion.div
                      initial={{ height: 0, opacity: 0 }}
                      animate={{ height: 'auto', opacity: 1 }}
                      exit={{ height: 0, opacity: 0 }}
                      transition={{ duration: 0.15 }}
                      className="overflow-hidden"
                    >
                      {/* Search */}
                      <div className="px-3 py-1.5">
                        <div className="relative">
                          <Search
                            size={12}
                            className="absolute left-2 top-1/2 -translate-y-1/2 text-text-muted"
                          />
                          <input
                            value={searchQuery}
                            onChange={(e) => setSearchQuery(e.target.value)}
                            placeholder="搜索键名..."
                            className="w-full rounded-md border border-border-subtle bg-surface-0 py-1 pl-7 pr-2 text-[11px] text-text-primary placeholder:text-text-muted focus:border-accent-red/40 focus:outline-none focus:ring-1 focus:ring-accent-red/20"
                          />
                        </div>
                      </div>

                      {/* Type filter */}
                      <div className="flex items-center gap-1 px-3 pb-1.5 overflow-x-auto">
                        <Filter size={10} className="text-text-muted shrink-0" />
                        {typeFilters.map((f) => (
                          <button
                            key={f.value}
                            onClick={() => setTypeFilter(f.value)}
                            className={cn(
                              'shrink-0 rounded px-1.5 py-0.5 text-[10px] font-medium transition-colors',
                              typeFilter === f.value
                                ? 'bg-accent-red/15 text-accent-red'
                                : 'text-text-muted hover:text-text-secondary',
                            )}
                          >
                            {f.label}
                          </button>
                        ))}
                      </div>

                      {/* Keys list */}
                      <div className="px-1 pb-2">
                        {filteredKeys.map((key) => (
                          <button
                            key={key.name}
                            onClick={() => setSelectedKey(key)}
                            className={cn(
                              'flex w-full items-center gap-2 rounded-md px-2 py-1.5 text-left transition-all',
                              selectedKey?.name === key.name
                                ? 'bg-accent-red/10 border border-accent-red/20 text-text-primary'
                                : 'hover:bg-surface-2 text-text-secondary border border-transparent',
                            )}
                          >
                            <KeyTypeIcon type={key.type} size={12} />
                            <span className="flex-1 truncate font-mono text-[11px]">{key.name}</span>
                            {key.ttl > 0 && (
                              <span className="shrink-0 text-[9px] text-warning font-mono">
                                {formatTTL(key.ttl)}
                              </span>
                            )}
                          </button>
                        ))}
                        {filteredKeys.length === 0 && (
                          <div className="px-2 py-4 text-center text-[11px] text-text-muted">
                            未找到匹配的键
                          </div>
                        )}
                      </div>
                    </motion.div>
                  )}
                </AnimatePresence>
              </div>
            )
          })}
        </div>
      </div>

      {/* Footer */}
      <div className="border-t border-border-subtle px-3 py-2">
        <div className="flex items-center justify-between text-[10px] text-text-muted">
          <span>{filteredKeys.length} 个键</span>
          <span className="font-mono">
            总大小 {formatBytes(mockKeys.reduce((a, k) => a + k.size, 0))}
          </span>
        </div>
      </div>
    </div>
  )
}
