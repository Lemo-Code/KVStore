import { useEffect, useState } from 'react'
import { cn } from '@/lib/utils'
import { useRedisStore } from '@/stores/redisStore'
import { redisService } from '@/services/redisService'
import { Separator } from '@/components/ui/separator'
import { Skeleton } from '@/components/ui/skeleton'
import StringEditor from './StringEditor'
import HashEditor from './HashEditor'
import ListEditor from './ListEditor'
import SetEditor from './SetEditor'
import ZSetEditor from './ZSetEditor'
import StreamEditor from './StreamEditor'
import JsonEditor from './JsonEditor'
import type { RedisValue, StringValue, HashValue, ListValue, SetValue, ZSetValue, StreamValue } from '@/types/redis'

const typeLabels: Record<string, string> = {
  string: 'String',
  hash: 'Hash',
  list: 'List',
  set: 'Set',
  zset: 'ZSet',
  stream: 'Stream',
}

const typeColors: Record<string, string> = {
  string: 'bg-emerald-100 text-emerald-700',
  hash: 'bg-blue-100 text-blue-700',
  list: 'bg-orange-100 text-orange-700',
  set: 'bg-purple-100 text-purple-700',
  zset: 'bg-pink-100 text-pink-700',
  stream: 'bg-teal-100 text-teal-700',
}

function formatBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`
}

function formatTTL(ttl: number): string {
  if (ttl === -2) return '已过期'
  if (ttl === -1) return '永不过期'
  if (ttl < 60) return `${ttl}s`
  if (ttl < 3600) return `${Math.floor(ttl / 60)}m`
  if (ttl < 86400) return `${Math.floor(ttl / 3600)}h`
  return `${Math.floor(ttl / 86400)}d`
}

export default function DataViewer() {
  const selectedKey = useRedisStore((s) => s.selectedKey)
  const keys = useRedisStore((s) => s.keys)
  const currentValue = useRedisStore((s) => s.currentValue)
  const setCurrentValue = useRedisStore((s) => s.setCurrentValue)
  const activeConnectionId = useRedisStore((s) => s.activeConnectionId)

  const [isLoading, setIsLoading] = useState(false)
  const [error, setError] = useState<string | null>(null)

  useEffect(() => {
    if (!selectedKey || !activeConnectionId) {
      setCurrentValue(null)
      return
    }

    let cancelled = false
    setIsLoading(true)
    setError(null)

    redisService
      .getValue(selectedKey)
      .then((val) => {
        if (!cancelled) {
          setCurrentValue(val)
          setIsLoading(false)
        }
      })
      .catch((err) => {
        if (!cancelled) {
          setError(err.message || '获取数据失败')
          setIsLoading(false)
        }
      })

    return () => {
      cancelled = true
    }
  }, [selectedKey, activeConnectionId, setCurrentValue])

  const selectedKeyMeta = keys.find((k) => k.name === selectedKey)

  // Empty state: no key selected
  if (!selectedKey) {
    return (
      <div className="flex h-full flex-col items-center justify-center text-muted-foreground">
        <div className="text-center max-w-sm">
          <span className="text-4xl block mb-3 opacity-30">&#128269;</span>
          <h3 className="text-sm font-medium mb-1">选择 Key 查看数据</h3>
          <p className="text-xs">从左侧 Key 树中选择一个 Key 来查看其数据内容</p>
        </div>
      </div>
    )
  }

  // Loading state
  if (isLoading) {
    return (
      <div className="flex h-full flex-col">
        <div className="px-4 py-3 border-b">
          <Skeleton className="h-4 w-48 mb-2" />
          <div className="flex gap-2">
            <Skeleton className="h-5 w-16" />
            <Skeleton className="h-5 w-20" />
            <Skeleton className="h-5 w-24" />
          </div>
        </div>
        <div className="flex-1 p-4">
          <Skeleton className="h-full w-full" />
        </div>
      </div>
    )
  }

  // Error state
  if (error) {
    return (
      <div className="flex h-full flex-col items-center justify-center text-muted-foreground">
        <div className="text-center max-w-sm">
          <span className="text-3xl block mb-3 text-red-400">&#9888;</span>
          <h3 className="text-sm font-medium mb-1 text-red-500">加载失败</h3>
          <p className="text-xs">{error}</p>
        </div>
      </div>
    )
  }

  // No value found
  if (!currentValue) {
    return (
      <div className="flex h-full flex-col items-center justify-center text-muted-foreground">
        <div className="text-center max-w-sm">
          <h3 className="text-sm font-medium mb-1">无数据</h3>
          <p className="text-xs">Key 可能不存在或已被删除</p>
        </div>
      </div>
    )
  }

  // Render the appropriate editor based on type
  const renderEditor = () => {
    switch (currentValue.type) {
      case 'string': {
        const strVal = currentValue as StringValue
        // Check if it's JSON
        try {
          JSON.parse(strVal.value)
          return <JsonEditor value={strVal} />
        } catch {
          return (
            <StringEditor
              value={strVal}
              onSave={async (newVal) => {
                await redisService.setStringValue(strVal.key, newVal)
                setCurrentValue({ ...strVal, value: newVal })
              }}
            />
          )
        }
      }
      case 'hash':
        return (
          <HashEditor
            value={currentValue as HashValue}
            onAddField={async (field, value) => {
              await redisService.setHashField(currentValue.key, field, value)
            }}
            onEditField={async (field, value) => {
              await redisService.setHashField(currentValue.key, field, value)
            }}
            onDeleteField={async (field) => {
              await redisService.deleteHashField(currentValue.key, field)
            }}
          />
        )
      case 'list':
        return (
          <ListEditor
            value={currentValue as ListValue}
            onPush={async (value, direction) => {
              await redisService.listPush(currentValue.key, value, direction)
            }}
            onRemove={async (index) => {
              await redisService.listRemove(currentValue.key, index)
            }}
          />
        )
      case 'set':
        return (
          <SetEditor
            value={currentValue as SetValue}
            onAdd={async (member) => {
              await redisService.setAdd(currentValue.key, member)
            }}
            onRemove={async (member) => {
              await redisService.setRemove(currentValue.key, member)
            }}
          />
        )
      case 'zset':
        return (
          <ZSetEditor
            value={currentValue as ZSetValue}
            onAdd={async (member, score) => {
              await redisService.zsetAdd(currentValue.key, member, score)
            }}
            onUpdateScore={async (member, score) => {
              await redisService.zsetAdd(currentValue.key, member, score)
            }}
            onRemove={async (member) => {
              await redisService.zsetRemove(currentValue.key, member)
            }}
          />
        )
      case 'stream':
        return <StreamEditor value={currentValue as StreamValue} />
      default:
        return <div className="p-4 text-sm text-muted-foreground">不支持的数据类型</div>
    }
  }

  return (
    <div className="flex h-full flex-col">
      {/* Key metadata bar */}
      <div className="flex items-center gap-3 px-3 py-2 border-b bg-muted/20">
        <span className="text-sm font-mono font-medium truncate max-w-[300px]" title={selectedKey}>
          {selectedKey}
        </span>

        <span
          className={cn(
            'px-1.5 py-0.5 rounded text-[10px] font-bold uppercase',
            typeColors[currentValue.type] || 'bg-gray-100 text-gray-600'
          )}
        >
          {typeLabels[currentValue.type] || currentValue.type}
        </span>

        {(currentValue as any).ttl !== undefined && (
          <span className="text-xs text-muted-foreground">
            TTL: <span className="font-mono">{formatTTL((currentValue as any).ttl)}</span>
          </span>
        )}

        {selectedKeyMeta && (
          <span className="text-xs text-muted-foreground">
            大小: <span className="font-mono">{formatBytes(selectedKeyMeta.size)}</span>
          </span>
        )}

        {currentValue.type === 'string' && (
          <span className="text-xs text-muted-foreground">
            编码: <span className="font-mono">{(currentValue as StringValue).encoding}</span>
          </span>
        )}

        {(currentValue.type === 'hash' || currentValue.type === 'list' || currentValue.type === 'set' || currentValue.type === 'zset' || currentValue.type === 'stream') && (
          <span className="text-xs text-muted-foreground">
            条目: <span className="font-mono">{(currentValue as any).length}</span>
          </span>
        )}
      </div>

      {/* Editor area */}
      <div className="flex-1 overflow-hidden">{renderEditor()}</div>
    </div>
  )
}
