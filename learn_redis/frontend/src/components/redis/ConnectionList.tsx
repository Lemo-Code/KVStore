import { useState } from 'react'
import { cn } from '@/lib/utils'
import { useRedisStore } from '@/stores/redisStore'
import { redisService } from '@/services/redisService'
import { Button } from '@/components/ui/button'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Tooltip, TooltipContent, TooltipTrigger } from '@/components/ui/tooltip'
import { Plus, Database, CheckCircle2, XCircle, Loader2, TestTube } from 'lucide-react'
import ConnectionForm from './ConnectionForm'
import type { RedisConnection } from '@/types/redis'

interface ConnectionItemProps {
  connection: RedisConnection
  isActive: boolean
  onSelect: () => void
}

function ConnectionItem({ connection, isActive, onSelect }: ConnectionItemProps) {
  const [isHovered, setIsHovered] = useState(false)
  const [isTesting, setIsTesting] = useState(false)

  const handleTestConnection = async (e: React.MouseEvent) => {
    e.stopPropagation()
    setIsTesting(true)
    try {
      await redisService.testConnection(connection)
    } finally {
      setIsTesting(false)
    }
  }

  const statusIcon = () => {
    switch (connection.status) {
      case 'connected':
        return <CheckCircle2 className="h-3.5 w-3.5 text-emerald-500 flex-shrink-0" />
      case 'connecting':
        return <Loader2 className="h-3.5 w-3.5 text-yellow-500 animate-spin flex-shrink-0" />
      case 'disconnected':
        return <XCircle className="h-3.5 w-3.5 text-red-500 flex-shrink-0" />
    }
  }

  const statusDot = () => {
    switch (connection.status) {
      case 'connected':
        return <span className="h-2 w-2 rounded-full bg-emerald-500 flex-shrink-0" />
      case 'connecting':
        return <span className="h-2 w-2 rounded-full bg-yellow-500 animate-pulse flex-shrink-0" />
      case 'disconnected':
        return <span className="h-2 w-2 rounded-full bg-red-500 flex-shrink-0" />
    }
  }

  return (
    <div
      className={cn(
        'group relative flex cursor-pointer items-center gap-3 rounded-lg px-3 py-2.5 transition-all duration-150',
        isActive
          ? 'bg-primary/10 text-primary border border-primary/20'
          : 'hover:bg-accent/50 text-foreground border border-transparent'
      )}
      onClick={onSelect}
      onMouseEnter={() => setIsHovered(true)}
      onMouseLeave={() => setIsHovered(false)}
    >
      <div className="flex-shrink-0">
        <Database className={cn('h-4 w-4', isActive ? 'text-primary' : 'text-muted-foreground')} />
      </div>

      <div className="flex-1 min-w-0">
        <div className="flex items-center gap-1.5">
          <span className="text-sm font-medium truncate">{connection.name}</span>
          {statusDot()}
        </div>
        <div className="flex items-center gap-2 text-xs text-muted-foreground">
          <span className="truncate">{connection.host}:{connection.port}</span>
          {connection.group && (
            <span className="px-1.5 py-0.5 rounded-sm bg-muted text-[10px] font-medium uppercase truncate">
              {connection.group}
            </span>
          )}
        </div>
      </div>

      {isHovered && (
        <Tooltip>
          <TooltipTrigger asChild>
            <Button
              variant="ghost"
              size="icon"
              className="h-7 w-7 opacity-0 group-hover:opacity-100 transition-opacity flex-shrink-0"
              onClick={handleTestConnection}
              disabled={isTesting}
            >
              {isTesting ? (
                <Loader2 className="h-3.5 w-3.5 animate-spin" />
              ) : (
                <TestTube className="h-3.5 w-3.5" />
              )}
            </Button>
          </TooltipTrigger>
          <TooltipContent side="right">
            <p>测试连接</p>
          </TooltipContent>
        </Tooltip>
      )}
    </div>
  )
}

export default function ConnectionList() {
  const [showForm, setShowForm] = useState(false)
  const [editingConnection, setEditingConnection] = useState<RedisConnection | undefined>(undefined)

  const connections = useRedisStore((s) => s.connections)
  const activeConnectionId = useRedisStore((s) => s.activeConnectionId)
  const setActiveConnection = useRedisStore((s) => s.setActiveConnection)
  const addConnection = useRedisStore((s) => s.addConnection)
  const updateConnection = useRedisStore((s) => s.updateConnection)

  const handleAddClick = () => {
    setEditingConnection(undefined)
    setShowForm(true)
  }

  const handleSave = (connection: RedisConnection) => {
    if (editingConnection) {
      updateConnection(connection.id, connection)
    } else {
      addConnection(connection)
    }
    setShowForm(false)
    setEditingConnection(undefined)
  }

  const handleCloseForm = () => {
    setShowForm(false)
    setEditingConnection(undefined)
  }

  return (
    <div className="flex h-full flex-col">
      <div className="flex items-center justify-between px-3 py-2 border-b">
        <h3 className="text-sm font-semibold text-foreground">连接列表</h3>
        <Button
          variant="ghost"
          size="icon"
          className="h-7 w-7"
          onClick={handleAddClick}
        >
          <Plus className="h-4 w-4" />
        </Button>
      </div>

      <ScrollArea className="flex-1">
        <div className="flex flex-col gap-0.5 p-2">
          {connections.length === 0 && (
            <div className="flex flex-col items-center justify-center py-8 text-muted-foreground">
              <Database className="h-8 w-8 mb-2 opacity-40" />
              <p className="text-xs">暂无连接</p>
              <Button
                variant="link"
                size="sm"
                className="text-xs mt-1 h-auto p-0"
                onClick={handleAddClick}
              >
                添加连接
              </Button>
            </div>
          )}
          {connections.map((conn) => (
            <ConnectionItem
              key={conn.id}
              connection={conn}
              isActive={conn.id === activeConnectionId}
              onSelect={() => setActiveConnection(conn.id)}
            />
          ))}
        </div>
      </ScrollArea>

      <ConnectionForm
        open={showForm}
        onClose={handleCloseForm}
        connection={editingConnection}
        onSave={handleSave}
      />
    </div>
  )
}
