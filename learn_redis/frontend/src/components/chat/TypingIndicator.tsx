import React from 'react'
import { cn } from '@/lib/utils'

interface TypingIndicatorProps {
  users: { userId: string; userName: string }[]
}

function getTypingText(users: { userId: string; userName: string }[]): string {
  if (users.length === 0) return ''
  if (users.length === 1) {
    return `${users[0].userName} 正在输入...`
  }
  if (users.length === 2) {
    return `${users[0].userName} 和 ${users[1].userName} 正在输入...`
  }
  return '多人正在输入...'
}

export function TypingIndicator({ users }: TypingIndicatorProps) {
  if (users.length === 0) return null

  const typingText = getTypingText(users)

  return (
    <div className="flex items-center gap-2 px-4 py-2">
      {/* Animated dots */}
      <div className="flex items-center gap-1">
        <span
          className="h-2 w-2 rounded-full bg-muted-foreground/60 animate-bounce"
          style={{ animationDelay: '0ms', animationDuration: '600ms' }}
        />
        <span
          className="h-2 w-2 rounded-full bg-muted-foreground/60 animate-bounce"
          style={{ animationDelay: '150ms', animationDuration: '600ms' }}
        />
        <span
          className="h-2 w-2 rounded-full bg-muted-foreground/60 animate-bounce"
          style={{ animationDelay: '300ms', animationDuration: '600ms' }}
        />
      </div>

      <span className="text-xs text-muted-foreground">{typingText}</span>
    </div>
  )
}
