import React from 'react'
import { BookOpen, FileText, X } from 'lucide-react'
import { cn } from '@/lib/utils'

interface ContextChipProps {
  label: string
  type: 'article' | 'page'
  onRemove: () => void
}

export function ContextChip({ label, type, onRemove }: ContextChipProps) {
  const Icon = type === 'article' ? BookOpen : FileText

  return (
    <span
      className={cn(
        'inline-flex items-center gap-1.5 px-2.5 py-1 rounded-full text-xs',
        'bg-primary/10 text-primary border border-primary/20',
        'transition-colors',
      )}
    >
      <Icon className="h-3 w-3" />
      <span className="max-w-32 truncate">{label}</span>
      <button
        onClick={onRemove}
        className={cn(
          'ml-0.5 rounded-full p-0.5',
          'hover:bg-primary/20 transition-colors',
        )}
        aria-label={`移除上下文: ${label}`}
      >
        <X className="h-3 w-3" />
      </button>
    </span>
  )
}
