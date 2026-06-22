import React, { useMemo, useCallback, useState } from 'react'
import ReactMarkdown from 'react-markdown'
import remarkGfm from 'remark-gfm'
import type { Components } from 'react-markdown'
import { Copy, Check } from 'lucide-react'
import { CodeBlock } from '@/components/code/CodeBlock'
import { StreamingText } from '@/components/ai/StreamingText'
import { cn } from '@/lib/utils'

interface AIResponseProps {
  content: string
  isStreaming?: boolean
}

export function AIResponse({ content, isStreaming = false }: AIResponseProps) {
  const [copied, setCopied] = useState(false)

  const handleCopy = useCallback(async () => {
    try {
      await navigator.clipboard.writeText(content)
      setCopied(true)
      setTimeout(() => setCopied(false), 2000)
    } catch {
      const textarea = document.createElement('textarea')
      textarea.value = content
      textarea.style.position = 'fixed'
      textarea.style.opacity = '0'
      document.body.appendChild(textarea)
      textarea.select()
      document.execCommand('copy')
      document.body.removeChild(textarea)
      setCopied(true)
      setTimeout(() => setCopied(false), 2000)
    }
  }, [content])

  const markdownComponents: Components = useMemo(
    () => ({
      code({ className, children, node, ...props }) {
        const match = /language-(\w+)/.exec(className || '')
        const isInline = !match && !String(children).includes('\n')

        if (isInline) {
          return (
            <code
              className="px-1.5 py-0.5 rounded bg-muted text-sm font-mono text-rose-600 dark:text-rose-400"
              {...props}
            >
              {children}
            </code>
          )
        }

        const language = match?.[1] || 'text'
        const code = String(children).replace(/\n$/, '')

        return <CodeBlock code={code} language={language} />
      },
      pre({ children, node, ...props }) {
        return <>{children}</>
      },
      a({ href, children, node, ...props }) {
        return (
          <a
            href={href}
            target="_blank"
            rel="noopener noreferrer"
            className="text-primary underline underline-offset-4 hover:text-primary/80"
            {...props}
          >
            {children}
          </a>
        )
      },
      p({ children, node, ...props }) {
        return (
          <p className="leading-7 mb-3 last:mb-0" {...props}>
            {children}
          </p>
        )
      },
      ul({ children, node, ...props }) {
        return (
          <ul className="list-disc list-inside mb-3 space-y-1" {...props}>
            {children}
          </ul>
        )
      },
      ol({ children, node, ...props }) {
        return (
          <ol className="list-decimal list-inside mb-3 space-y-1" {...props}>
            {children}
          </ol>
        )
      },
      blockquote({ children, node, ...props }) {
        return (
          <blockquote
            className="border-l-4 border-primary/30 pl-4 my-3 text-muted-foreground italic"
            {...props}
          >
            {children}
          </blockquote>
        )
      },
      h1({ children, node, ...props }) {
        return (
          <h1 className="text-xl font-bold mt-6 mb-3" {...props}>
            {children}
          </h1>
        )
      },
      h2({ children, node, ...props }) {
        return (
          <h2 className="text-lg font-semibold mt-5 mb-2" {...props}>
            {children}
          </h2>
        )
      },
      h3({ children, node, ...props }) {
        return (
          <h3 className="text-base font-semibold mt-4 mb-2" {...props}>
            {children}
          </h3>
        )
      },
      table({ children, node, ...props }) {
        return (
          <div className="overflow-x-auto mb-3">
            <table
              className="w-full border-collapse border border-border"
              {...props}
            >
              {children}
            </table>
          </div>
        )
      },
      th({ children, node, ...props }) {
        return (
          <th
            className="border border-border bg-muted px-3 py-1.5 text-left text-xs font-semibold"
            {...props}
          >
            {children}
          </th>
        )
      },
      td({ children, node, ...props }) {
        return (
          <td className="border border-border px-3 py-1.5 text-xs" {...props}>
            {children}
          </td>
        )
      },
    }),
    [],
  )

  return (
    <div className="pb-2">
      <div className="prose-content text-sm leading-relaxed">
        {isStreaming && !content ? (
          <span className="inline-flex items-center gap-1 text-muted-foreground">
            <span>AI 正在思考</span>
            <span className="inline-block animate-pulse">...</span>
          </span>
        ) : (
          <ReactMarkdown remarkPlugins={[remarkGfm]} components={markdownComponents}>
            {content}
          </ReactMarkdown>
        )}
      </div>

      {/* Copy button */}
      {content && !isStreaming && (
        <div className="flex justify-end mt-2">
          <button
            onClick={handleCopy}
            className={cn(
              'flex items-center gap-1.5 text-xs transition-colors rounded px-2 py-1',
              copied
                ? 'text-emerald-600 dark:text-emerald-400'
                : 'text-muted-foreground hover:text-foreground hover:bg-accent',
            )}
            aria-label={copied ? '已复制' : '复制回复'}
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
      )}
    </div>
  )
}
