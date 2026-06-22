import { memo } from 'react'
import { cn } from '@/lib/utils'
import { ChevronRight, ChevronDown, Folder, FolderOpen, Type, Hash, List, Braces, Layers, Database } from 'lucide-react'
import type { RedisDataType } from '@/types/redis'

interface KeyTreeItemProps {
  name: string
  isLeaf: boolean
  type?: RedisDataType
  ttl?: number
  count?: number
  depth: number
  isSelected: boolean
  isExpanded: boolean
  onToggle: () => void
  onSelect: () => void
  onContextMenu?: (e: React.MouseEvent) => void
}

const typeIconMap: Record<string, React.FC<{ className?: string }>> = {
  string: Type,
  hash: Hash,
  list: List,
  set: Braces,
  zset: Layers,
  stream: Database,
}

const typeLabel: Record<string, string> = {
  string: 'S',
  hash: 'H',
  list: 'L',
  set: 'S',
  zset: 'Z',
  stream: 'X',
}

const typeColor: Record<string, string> = {
  string: 'text-emerald-500',
  hash: 'text-blue-500',
  list: 'text-orange-500',
  set: 'text-purple-500',
  zset: 'text-pink-500',
  stream: 'text-teal-500',
}

function KeyTreeItem({
  name,
  isLeaf,
  type,
  ttl,
  count,
  depth,
  isSelected,
  isExpanded,
  onToggle,
  onSelect,
  onContextMenu,
}: KeyTreeItemProps) {
  const hasExpiry = ttl !== undefined && ttl > 0
  const indentStyle = { paddingLeft: `${depth * 16 + 8}px` }
  const TypeIcon = type ? typeIconMap[type] : null

  return (
    <div
      className={cn(
        'group relative flex cursor-pointer items-center gap-1 rounded-md py-1 pr-2 transition-colors',
        isSelected ? 'bg-primary/10 text-primary' : 'hover:bg-accent/50 text-foreground'
      )}
      style={indentStyle}
      onClick={() => {
        if (isLeaf) {
          onSelect()
        } else {
          onToggle()
        }
      }}
      onContextMenu={(e) => {
        e.preventDefault()
        onContextMenu?.(e)
      }}
    >
      {/* Expand/Collapse chevron for non-leaf nodes */}
      {!isLeaf && (
        <button
          className="flex h-4 w-4 items-center justify-center flex-shrink-0 text-muted-foreground hover:text-foreground"
          onClick={(e) => {
            e.stopPropagation()
            onToggle()
          }}
        >
          {isExpanded ? (
            <ChevronDown className="h-3.5 w-3.5" />
          ) : (
            <ChevronRight className="h-3.5 w-3.5" />
          )}
        </button>
      )}

      {/* Spacer for leaf nodes without chevron */}
      {isLeaf && <span className="w-4 flex-shrink-0" />}

      {/* Folder or type icon */}
      {isLeaf && TypeIcon ? (
        <TypeIcon className={cn('h-3.5 w-3.5 flex-shrink-0', typeColor[type!])} />
      ) : !isLeaf ? (
        isExpanded ? (
          <FolderOpen className="h-3.5 w-3.5 flex-shrink-0 text-yellow-500" />
        ) : (
          <Folder className="h-3.5 w-3.5 flex-shrink-0 text-yellow-500" />
        )
      ) : (
        <Type className="h-3.5 w-3.5 flex-shrink-0 text-muted-foreground" />
      )}

      {/* Key name text */}
      <span className="text-xs truncate flex-1 min-w-0 font-medium">{name}</span>

      {/* Type letter badge */}
      {isLeaf && type && (
        <span
          className={cn(
            'flex-shrink-0 text-[10px] font-bold px-1 rounded',
            typeColor[type],
            'bg-muted'
          )}
        >
          {typeLabel[type]}
        </span>
      )}

      {/* TTL badge */}
      {hasExpiry && (
        <span className="flex-shrink-0 text-[9px] text-muted-foreground bg-muted px-1 rounded">
          {formatTTL(ttl!)}
        </span>
      )}

      {/* Count badge for branches */}
      {!isLeaf && count !== undefined && count > 0 && (
        <span className="flex-shrink-0 text-[9px] text-muted-foreground bg-muted px-1 rounded tabular-nums">
          {count}
        </span>
      )}
    </div>
  )
}

function formatTTL(ttl: number): string {
  if (ttl <= 0) return 'exp'
  if (ttl < 60) return `${ttl}s`
  if (ttl < 3600) return `${Math.floor(ttl / 60)}m`
  if (ttl < 86400) return `${Math.floor(ttl / 3600)}h`
  return `${Math.floor(ttl / 86400)}d`
}

export default memo(KeyTreeItem)
