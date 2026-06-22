import React from 'react'
import { Clock, Check, CheckCheck, AlertCircle } from 'lucide-react'
import { cn } from '@/lib/utils'

interface MessageStatusProps {
  status: 'sending' | 'delivered' | 'read' | 'failed'
}

export function MessageStatus({ status }: MessageStatusProps) {
  const statusConfig = {
    sending: {
      icon: Clock,
      className: 'text-muted-foreground',
      label: '发送中',
    },
    delivered: {
      icon: Check,
      className: 'text-muted-foreground',
      label: '已送达',
    },
    read: {
      icon: CheckCheck,
      className: 'text-primary',
      label: '已读',
    },
    failed: {
      icon: AlertCircle,
      className: 'text-destructive',
      label: '发送失败',
    },
  } as const

  const config = statusConfig[status]
  const Icon = config.icon

  return (
    <span
      className={cn('inline-flex items-center gap-0.5 text-xs', config.className)}
      title={config.label}
      aria-label={config.label}
    >
      <Icon className="h-3.5 w-3.5" />
    </span>
  )
}
