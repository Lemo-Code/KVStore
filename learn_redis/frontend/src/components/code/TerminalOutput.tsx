import React, { useState, useCallback } from 'react'
import { Copy, Check } from 'lucide-react'
import { cn } from '@/lib/utils'

interface TerminalOutputProps {
  output: string
  prompt?: string
}

export function TerminalOutput({ output, prompt = '$' }: TerminalOutputProps) {
  const [copied, setCopied] = useState(false)

  const handleCopy = useCallback(async () => {
    const fullText = `${prompt} ${output}`
    try {
      await navigator.clipboard.writeText(fullText)
      setCopied(true)
      setTimeout(() => setCopied(false), 2000)
    } catch {
      const textarea = document.createElement('textarea')
      textarea.value = fullText
      textarea.style.position = 'fixed'
      textarea.style.opacity = '0'
      document.body.appendChild(textarea)
      textarea.select()
      document.execCommand('copy')
      document.body.removeChild(textarea)
      setCopied(true)
      setTimeout(() => setCopied(false), 2000)
    }
  }, [output, prompt])

  const lines = output.split('\n')

  return (
    <div className="rounded-lg overflow-hidden border border-zinc-700 my-4">
      {/* Terminal header bar */}
      <div className="flex items-center justify-between bg-zinc-800 px-4 py-2 border-b border-zinc-700">
        <div className="flex items-center gap-1.5">
          <span className="h-3 w-3 rounded-full bg-red-500" />
          <span className="h-3 w-3 rounded-full bg-yellow-500" />
          <span className="h-3 w-3 rounded-full bg-green-500" />
        </div>

        <button
          onClick={handleCopy}
          className={cn(
            'flex items-center gap-1.5 text-xs transition-colors rounded px-2 py-1',
            copied
              ? 'text-green-400 bg-green-500/10'
              : 'text-zinc-400 hover:text-zinc-200 hover:bg-zinc-700',
          )}
          aria-label={copied ? '已复制' : '复制输出'}
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

      {/* Terminal output */}
      <div className="bg-zinc-950 px-4 py-3 overflow-x-auto">
        <pre className="text-sm leading-relaxed">
          <code className="font-mono">
            {lines.map((line, i) => (
              <div key={i} className="flex">
                {/* Show prompt only on first line */}
                {i === 0 && (
                  <span className="text-green-400 select-none mr-2">{prompt}</span>
                )}
                {i !== 0 && <span className="mr-2">&nbsp;</span>}
                <span className="text-green-200/80">{line || ' '}</span>
              </div>
            ))}
          </code>
        </pre>
      </div>
    </div>
  )
}
