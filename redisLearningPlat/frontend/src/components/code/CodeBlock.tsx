import React, { useState, useCallback } from 'react'
import { Copy, Check } from 'lucide-react'
import { cn } from '@/lib/utils'

interface CodeBlockProps {
  code: string
  language?: string
  title?: string
}

export function CodeBlock({ code, language = 'bash', title }: CodeBlockProps) {
  const [copied, setCopied] = useState(false)

  const handleCopy = useCallback(async () => {
    try {
      await navigator.clipboard.writeText(code)
      setCopied(true)
      setTimeout(() => setCopied(false), 2000)
    } catch {
      // Fallback for older browsers
      const textarea = document.createElement('textarea')
      textarea.value = code
      textarea.style.position = 'fixed'
      textarea.style.opacity = '0'
      document.body.appendChild(textarea)
      textarea.select()
      document.execCommand('copy')
      document.body.removeChild(textarea)
      setCopied(true)
      setTimeout(() => setCopied(false), 2000)
    }
  }, [code])

  const lines = code.split('\n')
  const lineCount = lines.length
  const paddedLineNumbers = String(lineCount).length

  return (
    <div className="rounded-lg overflow-hidden border border-zinc-800 my-4">
      {/* Header bar */}
      <div className="flex items-center justify-between bg-zinc-900 px-4 py-2 border-b border-zinc-800">
        <div className="flex items-center gap-3">
          <span className="text-xs text-zinc-400 font-mono uppercase tracking-wider">
            {language}
          </span>
          {title && (
            <>
              <span className="text-zinc-600">|</span>
              <span className="text-xs text-zinc-300">{title}</span>
            </>
          )}
        </div>

        <button
          onClick={handleCopy}
          className={cn(
            'flex items-center gap-1.5 text-xs transition-colors rounded px-2 py-1',
            copied
              ? 'text-emerald-400 bg-emerald-500/10'
              : 'text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800',
          )}
          aria-label={copied ? '已复制' : '复制代码'}
        >
          {copied ? (
            <>
              <Check className="h-3.5 w-3.5" />
              <span>Copied!</span>
            </>
          ) : (
            <>
              <Copy className="h-3.5 w-3.5" />
              <span>Copy</span>
            </>
          )}
        </button>
      </div>

      {/* Code content */}
      <div className="bg-zinc-950 overflow-x-auto">
        <pre className="p-4 text-sm leading-relaxed">
          <code className={`language-${language} font-mono text-zinc-100`}>
            {lines.map((line, i) => (
              <div key={i} className="flex">
                {/* Line number */}
                <span
                  className="select-none text-right text-zinc-600 mr-4 flex-shrink-0"
                  style={{ minWidth: `${paddedLineNumbers}ch` }}
                >
                  {String(i + 1).padStart(paddedLineNumbers, ' ')}
                </span>
                {/* Line content */}
                <span className="flex-1">{line || ' '}</span>
              </div>
            ))}
          </code>
        </pre>
      </div>
    </div>
  )
}
