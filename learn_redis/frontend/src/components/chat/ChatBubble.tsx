import React from 'react'
import { cn, formatTime } from '@/lib/utils'
import { Check, CheckCheck, Clock, AlertCircle } from 'lucide-react'
import { Avatar, AvatarFallback } from '@/components/ui/avatar'
import { MessageStatus } from '@/components/chat/MessageStatus'

interface ChatBubbleProps {
  variant: 'self' | 'other' | 'system'
  content: string
  timestamp: string
  userName?: string
  userAvatar?: string
  status?: 'sending' | 'delivered' | 'read' | 'failed'
}

function getInitials(name: string): string {
  if (!name) return '?'
  return name.charAt(0).toUpperCase()
}

export function ChatBubble({
  variant,
  content,
  timestamp,
  userName,
  userAvatar,
  status,
}: ChatBubbleProps) {
  if (variant === 'system') {
    return (
      <div className="flex justify-center py-2">
        <span className="text-xs text-muted-foreground px-3 py-1 rounded-full bg-muted/50">
          {content}
        </span>
      </div>
    )
  }

  const isSelf = variant === 'self'

  return (
    <div
      className={cn(
        'flex gap-2 mb-4 px-4',
        isSelf ? 'flex-row-reverse' : 'flex-row',
      )}
    >
      {/* Avatar for other */}
      {!isSelf && (
        <Avatar className="h-8 w-8 flex-shrink-0">
          {userAvatar ? (
            <img src={userAvatar} alt={userName} className="aspect-square h-full w-full rounded-full object-cover" />
          ) : null}
          <AvatarFallback className="text-xs">
            {userName ? getInitials(userName) : '?'}
          </AvatarFallback>
        </Avatar>
      )}

      <div
        className={cn(
          'flex flex-col max-w-[75%]',
          isSelf ? 'items-end' : 'items-start',
        )}
      >
        {/* User name for "other" variant */}
        {!isSelf && userName && (
          <span className="text-xs text-muted-foreground mb-1 ml-1">
            {userName}
          </span>
        )}

        {/* Message bubble */}
        <div
          className={cn(
            'px-4 py-2 text-sm leading-relaxed break-words',
            isSelf
              ? 'bg-primary text-primary-foreground rounded-2xl rounded-br-md'
              : 'bg-muted rounded-2xl rounded-bl-md',
          )}
        >
          {content}
        </div>

        {/* Timestamp and status */}
        <div
          className={cn(
            'flex items-center gap-1.5 mt-1',
            isSelf ? 'flex-row-reverse' : 'flex-row',
          )}
        >
          <span className="text-xs text-muted-foreground">
            {formatTime(timestamp)}
          </span>
          {isSelf && status && <MessageStatus status={status} />}
        </div>
      </div>
    </div>
  )
}
