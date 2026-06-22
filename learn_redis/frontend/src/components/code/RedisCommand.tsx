import React, { useState, useCallback } from 'react'
import { Copy, Check, Terminal } from 'lucide-react'
import { cn } from '@/lib/utils'

interface RedisCommandProps {
  command: string
  description?: string
}

export function RedisCommand({ command, description }: RedisCommandProps) {
  const [copied, setCopied] = useState(false)

  const handleCopy = useCallback(async () => {
    try {
      await navigator.clipboard.writeText(command)
      setCopied(true)
      setTimeout(() => setCopied(false), 2000)
    } catch {
      const textarea = document.createElement('textarea')
      textarea.value = command
      textarea.style.position = 'fixed'
      textarea.style.opacity = '0'
      document.body.appendChild(textarea)
      textarea.select()
      document.execCommand('copy')
      document.body.removeChild(textarea)
      setCopied(true)
      setTimeout(() => setCopied(false), 2000)
    }
  }, [command])

  return (
    <div className="rounded-lg border border-rose-200 dark:border-rose-800 overflow-hidden my-4">
      {/* Command header */}
      <div className="flex items-center justify-between bg-rose-50 dark:bg-rose-950/50 px-4 py-2 border-b border-rose-200 dark:border-rose-800">
        <div className="flex items-center gap-2">
          <Terminal className="h-3.5 w-3.5 text-rose-600 dark:text-rose-400" />
          <span className="text-xs font-medium text-rose-700 dark:text-rose-300">
            REDIS
          </span>
        </div>

        <button
          onClick={handleCopy}
          className={cn(
            'flex items-center gap-1.5 text-xs transition-colors rounded px-2 py-1',
            copied
              ? 'text-emerald-600 dark:text-emerald-400 bg-emerald-100 dark:bg-emerald-900/20'
              : 'text-rose-600 dark:text-rose-400 hover:bg-rose-100 dark:hover:bg-rose-900/20',
          )}
          aria-label={copied ? '已复制' : '复制命令'}
        >
          {copied ? (
            <>
              <Check className="h-3 w-3" />
              <span>已复制</span>
            </>
          ) : (
            <>
              <Copy className="h-3 w-3" />
              <span>复制</span>
            </>
          )}
        </button>
      </div>

      {/* Command content */}
      <div className="bg-rose-100/30 dark:bg-rose-950/20 px-4 py-3">
        <code className="text-sm font-mono text-rose-700 dark:text-rose-300">
          <span className="select-none text-rose-400 dark:text-rose-600 mr-2">
            &gt;
          </span>
          {command}
        </code>
        {description && (
          <p className="mt-1.5 text-xs text-rose-600/80 dark:text-rose-400/80">
            {description}
          </p>
        )}
      </div>
    </div>
  )
}
