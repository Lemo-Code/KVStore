import { useState, useMemo, useCallback } from 'react'
import { cn } from '@/lib/utils'
import { useRedisStore } from '@/stores/redisStore'
import { redisService } from '@/services/redisService'
import { Input } from '@/components/ui/input'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Search } from 'lucide-react'
import KeyTreeItem from './KeyTreeItem'
import KeyContextMenu from './KeyContextMenu'
import type { RedisKey, RedisDataType } from '@/types/redis'

interface TreeNode {
  name: string
  fullPath: string
  isLeaf: boolean
  type?: RedisDataType
  ttl?: number
  children: TreeNode[]
  count: number
}

function buildTree(keys: RedisKey[], filter: string): TreeNode[] {
  const root: TreeNode[] = []
  const filterLower = filter.trim().toLowerCase()

  const filtered = filterLower
    ? keys.filter((k) => k.name.toLowerCase().includes(filterLower))
    : keys

  for (const key of filtered) {
    const parts = key.name.split(':')
    let current = root

    for (let i = 0; i < parts.length; i++) {
      const isLeaf = i === parts.length - 1
      const part = parts[i]
      const fullPath = parts.slice(0, i + 1).join(':')

      let node = current.find((n) => n.name === part)

      if (!node) {
        node = {
          name: part,
          fullPath,
          isLeaf,
          type: isLeaf ? key.type : undefined,
          ttl: isLeaf ? key.ttl : undefined,
          children: [],
          count: 0,
        }
        current.push(node)
      }

      if (isLeaf) {
        node.isLeaf = true
        node.type = key.type
        node.ttl = key.ttl
      }

      node.count++

      // Also update parent count if this was an existing branch that got a new leaf
      current = node.children
    }
  }

  // Sort: folders first, then alphabetically
  const sortNodes = (nodes: TreeNode[]) => {
    nodes.sort((a, b) => {
      if (a.isLeaf !== b.isLeaf) return a.isLeaf ? 1 : -1
      return a.name.localeCompare(b.name)
    })
    for (const n of nodes) {
      if (!n.isLeaf) sortNodes(n.children)
    }
  }
  sortNodes(root)

  return root
}

export default function KeyTree() {
  const keys = useRedisStore((s) => s.keys)
  const selectedKey = useRedisStore((s) => s.selectedKey)
  const keyPattern = useRedisStore((s) => s.keyPattern)
  const activeConnectionId = useRedisStore((s) => s.activeConnectionId)
  const activeDb = useRedisStore((s) => s.activeDb)
  const setKeyPattern = useRedisStore((s) => s.setKeyPattern)
  const selectKey = useRedisStore((s) => s.selectKey)
  const deleteKeys = useRedisStore((s) => s.deleteKeys)
  const renameKey = useRedisStore((s) => s.renameKey)

  const [expandedPaths, setExpandedPaths] = useState<Set<string>>(new Set())
  const [contextMenu, setContextMenu] = useState<{
    x: number
    y: number
    keyName: string
  } | null>(null)

  const tree = useMemo(() => buildTree(keys, keyPattern), [keys, keyPattern])

  const toggleExpand = useCallback((fullPath: string) => {
    setExpandedPaths((prev) => {
      const next = new Set(prev)
      if (next.has(fullPath)) {
        next.delete(fullPath)
      } else {
        next.add(fullPath)
      }
      return next
    })
  }, [])

  const handleSelect = useCallback(
    (fullPath: string) => {
      selectKey(fullPath)
    },
    [selectKey]
  )

  const handleContextMenu = useCallback(
    (e: React.MouseEvent, fullPath: string) => {
      setContextMenu({ x: e.clientX, y: e.clientY, keyName: fullPath })
    },
    []
  )

  const handleOpenKey = useCallback(
    (keyName: string) => {
      selectKey(keyName)
      setContextMenu(null)
    },
    [selectKey]
  )

  const handleRename = useCallback(
    (keyName: string, newName?: string) => {
      if (newName) {
        renameKey(keyName, newName)
      }
      setContextMenu(null)
    },
    [renameKey]
  )

  const handleDelete = useCallback(
    (keyName: string) => {
      deleteKeys([keyName])
      setContextMenu(null)
    },
    [deleteKeys]
  )

  const handleCopy = useCallback(
    async (keyName: string) => {
      try {
        await navigator.clipboard.writeText(keyName)
      } catch {
        // Fallback: no clipboard API
      }
      setContextMenu(null)
    },
    []
  )

  const handleSetTTL = useCallback(
    (keyName: string) => {
      // Placeholder for TTL dialog integration
      setContextMenu(null)
    },
    []
  )

  const renderNode = (node: TreeNode, depth: number) => {
    const items: React.ReactNode[] = []
    const fullPath = node.fullPath
    const isExpanded = expandedPaths.has(fullPath)
    const isSelected = selectedKey === fullPath

    items.push(
      <KeyTreeItem
        key={fullPath}
        name={node.name}
        isLeaf={node.isLeaf}
        type={node.type}
        ttl={node.ttl}
        count={node.count}
        depth={depth}
        isSelected={isSelected}
        isExpanded={isExpanded}
        onToggle={() => toggleExpand(fullPath)}
        onSelect={() => handleSelect(fullPath)}
        onContextMenu={(e) => handleContextMenu(e, fullPath)}
      />
    )

    if (!node.isLeaf && isExpanded) {
      for (const child of node.children) {
        items.push(...(renderNode(child, depth + 1) as React.ReactNode[]))
      }
    }

    return items
  }

  const allItems = useMemo(() => {
    return tree.flatMap((node) => renderNode(node, 0))
  }, [tree, expandedPaths, selectedKey])

  return (
    <div className="flex h-full flex-col">
      {/* Search */}
      <div className="px-3 py-2 border-b">
        <div className="relative">
          <Search className="absolute left-2.5 top-1/2 -translate-y-1/2 h-3.5 w-3.5 text-muted-foreground" />
          <Input
            value={keyPattern}
            onChange={(e) => setKeyPattern(e.target.value)}
            placeholder="搜索 Key..."
            className="h-7 pl-8 text-xs"
          />
        </div>
      </div>

      {/* Tree */}
      <ScrollArea className="flex-1">
        <div className="py-1">
          {allItems.length === 0 ? (
            <div className="flex flex-col items-center justify-center py-8 text-muted-foreground">
              <Search className="h-6 w-6 mb-2 opacity-40" />
              <p className="text-xs">
                {keyPattern ? '无匹配的 Key' : activeConnectionId ? '暂无 Key 数据' : '请先选择连接'}
              </p>
            </div>
          ) : (
            allItems
          )}
        </div>
      </ScrollArea>

      {/* Context Menu */}
      {contextMenu && (
        <KeyContextMenu
          x={contextMenu.x}
          y={contextMenu.y}
          keyName={contextMenu.keyName}
          onClose={() => setContextMenu(null)}
          onOpen={handleOpenKey}
          onRename={handleRename}
          onDelete={handleDelete}
          onCopy={handleCopy}
          onSetTTL={handleSetTTL}
        />
      )}
    </div>
  )
}
