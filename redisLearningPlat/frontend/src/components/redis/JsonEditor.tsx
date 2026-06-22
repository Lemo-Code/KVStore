import { useState, useMemo, useCallback } from 'react'
import { cn } from '@/lib/utils'
import { Button } from '@/components/ui/button'
import { ScrollArea } from '@/components/ui/scroll-area'
import { ChevronRight, ChevronDown, Braces, FileText, MinusSquare, PlusSquare } from 'lucide-react'
import type { StringValue } from '@/types/redis'

interface JsonEditorProps {
  value: StringValue
}

type JsonNodeType = 'object' | 'array' | 'string' | 'number' | 'boolean' | 'null'

interface JsonNode {
  key?: string
  value: any
  type: JsonNodeType
  path: string
  depth: number
}

function parseJsonNodes(obj: any, depth = 0, path = '$', key?: string): JsonNode[] {
  const nodes: JsonNode[] = []

  if (obj === null) {
    nodes.push({ key, value: 'null', type: 'null', path, depth })
    return nodes
  }

  const t = typeof obj

  if (t === 'string') {
    nodes.push({ key, value: obj, type: 'string', path, depth })
  } else if (t === 'number') {
    nodes.push({ key, value: obj, type: 'number', path, depth })
  } else if (t === 'boolean') {
    nodes.push({ key, value: obj, type: 'boolean', path, depth })
  } else if (Array.isArray(obj)) {
    nodes.push({ key, value: `Array[${obj.length}]`, type: 'array', path, depth })
    obj.forEach((item, i) => {
      nodes.push(...parseJsonNodes(item, depth + 1, `${path}[${i}]`, String(i)))
    })
  } else if (t === 'object') {
    const entries = Object.entries(obj as Record<string, any>)
    nodes.push({ key, value: `Object{${entries.length}}`, type: 'object', path, depth })
    for (const [k, v] of entries) {
      nodes.push(...parseJsonNodes(v, depth + 1, `${path}.${k}`, k))
    }
  }

  return nodes
}

const typeColors: Record<string, string> = {
  string: 'text-emerald-600',
  number: 'text-blue-600',
  boolean: 'text-orange-600',
  null: 'text-gray-400',
  object: 'text-purple-600',
  array: 'text-teal-600',
}

const typeBgColors: Record<string, string> = {
  string: 'bg-emerald-50',
  number: 'bg-blue-50',
  boolean: 'bg-orange-50',
  null: 'bg-gray-50',
}

function JsonNodeRow({
  node,
  isExpanded,
  onToggle,
  isCollapsible,
}: {
  node: JsonNode
  isExpanded: boolean
  onToggle: () => void
  isCollapsible: boolean
}) {
  const indent = node.depth * 20
  const isContainer = node.type === 'object' || node.type === 'array'

  return (
    <div
      className="flex items-center gap-1 py-0.5 hover:bg-muted/30 transition-colors rounded-sm group"
      style={{ paddingLeft: `${indent + 8}px` }}
    >
      {/* Expand/collapse toggle */}
      {isContainer && isCollapsible ? (
        <button
          className="flex h-4 w-4 items-center justify-center flex-shrink-0 text-muted-foreground hover:text-foreground"
          onClick={onToggle}
        >
          {isExpanded ? (
            <ChevronDown className="h-3 w-3" />
          ) : (
            <ChevronRight className="h-3 w-3" />
          )}
        </button>
      ) : (
        <span className="w-4 flex-shrink-0" />
      )}

      {/* Key */}
      {node.key !== undefined && (
        <span className="text-xs font-medium text-foreground flex-shrink-0">{node.key}</span>
      )}

      {/* Colon separator for key-value */}
      {node.key !== undefined && !isContainer && (
        <span className="text-xs text-muted-foreground">:</span>
      )}

      {/* Value */}
      {isContainer ? (
        <span className={cn('text-xs font-medium', typeColors[node.type])}>
          {String(node.value)}
        </span>
      ) : (
        <span
          className={cn(
            'text-xs font-mono px-1.5 py-0.5 rounded',
            typeColors[node.type],
            typeBgColors[node.type]
          )}
        >
          {node.type === 'string' ? `"${String(node.value)}"` : String(node.value)}
        </span>
      )}

      {/* Row count for container when collapsed */}
      {isContainer && !isCollapsible && (
        <span className="text-[10px] text-muted-foreground">{String(node.value)}</span>
      )}
    </div>
  )
}

export default function JsonEditor({ value }: JsonEditorProps) {
  const [showRaw, setShowRaw] = useState(false)
  const [collapsedPaths, setCollapsedPaths] = useState<Set<string>>(new Set())

  const parsedJson = useMemo(() => {
    try {
      const parsed = JSON.parse(value.value)
      return { success: true, data: parsed, error: null }
    } catch (e) {
      return { success: false, data: null, error: (e as Error).message }
    }
  }, [value.value])

  const allNodes = useMemo(() => {
    if (!parsedJson.success) return []
    return parseJsonNodes(parsedJson.data)
  }, [parsedJson])

  const isCollapsed = useCallback(
    (path: string) => collapsedPaths.has(path),
    [collapsedPaths]
  )

  const toggleCollapse = useCallback((path: string) => {
    setCollapsedPaths((prev) => {
      const next = new Set(prev)
      if (next.has(path)) {
        next.delete(path)
      } else {
        next.add(path)
      }
      return next
    })
  }, [])

  const collapseAll = useCallback(() => {
    const all = new Set<string>()
    for (const node of allNodes) {
      if (node.type === 'object' || node.type === 'array') {
        all.add(node.path)
      }
    }
    setCollapsedPaths(all)
  }, [allNodes])

  const expandAll = useCallback(() => {
    setCollapsedPaths(new Set())
  }, [])

  // Filter visible nodes based on collapse state
  const visibleNodes = useMemo(() => {
    const collapsedPathsSet = new Set<string>()
    // Walk nodes and determine which should be hidden
    const result: JsonNode[] = []
    const hiddenPaths = new Set<string>()

    for (const node of allNodes) {
      // Check if any ancestor is collapsed
      let shouldHide = false
      const parts = node.path.split(/[.[\]]/).filter(Boolean)
      let checkPath = ''
      for (let i = 0; i < parts.length; i++) {
        const part = parts[i]
        if (part === '$') {
          checkPath = '$'
          continue
        }
        // Build parent path
        if (node.type === 'array' && node.path.startsWith(checkPath)) continue
        // Reconstruct parent path properly
        const parentPath = node.path.substring(0, node.path.lastIndexOf(checkPath === '$' ? '.' : checkPath))
        // Simpler approach: check all ancestor container paths
      }

      // Simpler: check if any parent container at a shallower depth is collapsed
      for (const n of allNodes) {
        if (n.depth < node.depth && (n.type === 'object' || n.type === 'array')) {
          if (node.path.startsWith(n.path) && node.path !== n.path) {
            if (collapsedPaths.has(n.path)) {
              shouldHide = true
              break
            }
          }
        }
      }

      if (!shouldHide) {
        result.push(node)
      }
    }

    return result
  }, [allNodes, collapsedPaths])

  if (!parsedJson.success) {
    return (
      <div className="flex flex-col h-full">
        <div className="flex items-center gap-2 px-3 py-2 border-b bg-muted/30">
          <Braces className="h-4 w-4 text-muted-foreground" />
          <span className="text-xs font-medium">JSON View</span>
          <div className="flex-1" />
          <Button
            variant={showRaw ? 'ghost' : 'default'}
            size="sm"
            className="h-7 text-xs"
            onClick={() => setShowRaw(!showRaw)}
          >
            <FileText className="h-3.5 w-3.5 mr-1" />
            Raw
          </Button>
        </div>
        <ScrollArea className="flex-1">
          <pre className="p-4 text-xs font-mono text-red-500 whitespace-pre-wrap">
            Invalid JSON: {parsedJson.error}
          </pre>
        </ScrollArea>
      </div>
    )
  }

  return (
    <div className="flex flex-col h-full">
      {/* Toolbar */}
      <div className="flex items-center gap-2 px-3 py-2 border-b bg-muted/30">
        <Braces className="h-4 w-4 text-muted-foreground" />
        <span className="text-xs font-medium">JSON View</span>
        <div className="flex-1" />
        {!showRaw && (
          <>
            <Button variant="ghost" size="sm" className="h-7 text-xs" onClick={collapseAll}>
              <MinusSquare className="h-3.5 w-3.5 mr-1" />
              折叠
            </Button>
            <Button variant="ghost" size="sm" className="h-7 text-xs" onClick={expandAll}>
              <PlusSquare className="h-3.5 w-3.5 mr-1" />
              展开
            </Button>
          </>
        )}
        <Button
          variant={showRaw ? 'default' : 'ghost'}
          size="sm"
          className="h-7 text-xs"
          onClick={() => setShowRaw(!showRaw)}
        >
          <FileText className="h-3.5 w-3.5 mr-1" />
          Raw
        </Button>
      </div>

      {/* Content */}
      <ScrollArea className="flex-1">
        {showRaw ? (
          <pre className="p-3 text-xs font-mono whitespace-pre-wrap">{value.value}</pre>
        ) : (
          <div className="py-1">
            {visibleNodes.map((node) => (
              <JsonNodeRow
                key={node.path}
                node={node}
                isExpanded={!collapsedPaths.has(node.path)}
                onToggle={() => toggleCollapse(node.path)}
                isCollapsible={node.type === 'object' || node.type === 'array'}
              />
            ))}
          </div>
        )}
      </ScrollArea>
    </div>
  )
}
