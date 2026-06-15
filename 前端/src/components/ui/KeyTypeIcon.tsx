import { cn } from '@/lib/utils'
import type { RedisKeyType } from '@/types'
import {
  Braces,
  Hash,
  Layers,
  ListOrdered,
  Radio,
  Type,
} from 'lucide-react'

const typeConfig: Record<
  RedisKeyType,
  { icon: typeof Type; color: string; label: string }
> = {
  string: { icon: Type, color: 'text-accent-blue', label: 'String' },
  hash: { icon: Hash, color: 'text-accent-teal', label: 'Hash' },
  list: { icon: ListOrdered, color: 'text-accent-amber', label: 'List' },
  set: { icon: Braces, color: 'text-accent-purple', label: 'Set' },
  zset: { icon: Layers, color: 'text-accent-red', label: 'ZSet' },
  stream: { icon: Radio, color: 'text-success', label: 'Stream' },
}

interface KeyTypeIconProps {
  type: RedisKeyType
  size?: number
  showLabel?: boolean
  className?: string
}

export function KeyTypeIcon({ type, size = 14, showLabel, className }: KeyTypeIconProps) {
  const config = typeConfig[type]
  const Icon = config.icon

  return (
    <span className={cn('inline-flex items-center gap-1', className)}>
      <Icon size={size} className={config.color} strokeWidth={2} />
      {showLabel && (
        <span className={cn('text-[10px] font-medium', config.color)}>{config.label}</span>
      )}
    </span>
  )
}

export function getTypeBadgeVariant(type: RedisKeyType) {
  const map: Record<RedisKeyType, 'default' | 'type' | 'success' | 'warning' | 'purple'> = {
    string: 'default',
    hash: 'success',
    list: 'warning',
    set: 'purple',
    zset: 'type',
    stream: 'success',
  }
  return map[type]
}
