import React from 'react'
import { cn } from '@/lib/utils'

interface StreamingTextProps {
  text: string
  isStreaming: boolean
}

export function StreamingText({ text, isStreaming }: StreamingTextProps) {
  return (
    <span className="inline">
      {text}
      {isStreaming && (
        <span
          className={cn(
            'inline-block w-0.5 h-4 ml-0.5 bg-primary',
            'animate-pulse',
          )}
          style={{ animationDuration: '800ms' }}
          aria-hidden="true"
        />
      )}
    </span>
  )
}
