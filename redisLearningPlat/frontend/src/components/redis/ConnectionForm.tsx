import { useState, useCallback } from 'react'
import { cn } from '@/lib/utils'
import { redisService } from '@/services/redisService'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
  DialogDescription,
  DialogFooter,
} from '@/components/ui/dialog'
import { Eye, EyeOff, Loader2, TestTube, ShieldCheck } from 'lucide-react'
import type { RedisConnection } from '@/types/redis'

interface ConnectionFormProps {
  open: boolean
  onClose: () => void
  connection?: RedisConnection
  onSave: (connection: RedisConnection) => void
}

interface FormErrors {
  name?: string
  host?: string
  port?: string
}

export default function ConnectionForm({ open, onClose, connection, onSave }: ConnectionFormProps) {
  const isEditing = !!connection

  const [name, setName] = useState(connection?.name ?? '')
  const [host, setHost] = useState(connection?.host ?? '127.0.0.1')
  const [port, setPort] = useState(String(connection?.port ?? 6379))
  const [password, setPassword] = useState(connection?.password ?? '')
  const [showPassword, setShowPassword] = useState(false)
  const [ssl, setSsl] = useState(connection?.ssl ?? false)
  const [group, setGroup] = useState(connection?.group ?? '')
  const [errors, setErrors] = useState<FormErrors>({})
  const [isTesting, setIsTesting] = useState(false)
  const [testResult, setTestResult] = useState<boolean | null>(null)

  const validate = useCallback((): boolean => {
    const newErrors: FormErrors = {}

    if (!name.trim()) {
      newErrors.name = '连接名称不能为空'
    }
    if (!host.trim()) {
      newErrors.host = '主机地址不能为空'
    }

    const portNum = parseInt(port, 10)
    if (!port.trim() || isNaN(portNum)) {
      newErrors.port = '端口必须为数字'
    } else if (portNum < 1 || portNum > 65535) {
      newErrors.port = '端口范围: 1-65535'
    }

    setErrors(newErrors)
    return Object.keys(newErrors).length === 0
  }, [name, host, port])

  const handleTestConnection = async () => {
    if (!validate()) return

    setIsTesting(true)
    setTestResult(null)
    try {
      const result = await redisService.testConnection({
        name: name.trim(),
        host: host.trim(),
        port: parseInt(port, 10),
        password: password || undefined,
        db: 0,
        ssl,
        group: group.trim() || undefined,
      })
      setTestResult(result)
    } catch {
      setTestResult(false)
    } finally {
      setIsTesting(false)
    }
  }

  const handleSave = () => {
    if (!validate()) return

    const conn: RedisConnection = {
      id: connection?.id ?? `conn-${Date.now()}`,
      name: name.trim(),
      host: host.trim(),
      port: parseInt(port, 10),
      password: password || undefined,
      db: connection?.db ?? 0,
      status: connection?.status ?? 'disconnected',
      ssl,
      group: group.trim() || undefined,
      createdAt: connection?.createdAt ?? new Date().toISOString(),
      lastConnectedAt: connection?.lastConnectedAt,
    }

    onSave(conn)
  }

  const handleClose = () => {
    setErrors({})
    setTestResult(null)
    onClose()
  }

  return (
    <Dialog open={open} onOpenChange={(open) => !open && handleClose()}>
      <DialogContent className="sm:max-w-[480px]">
        <DialogHeader>
          <DialogTitle>{isEditing ? '编辑连接' : '添加连接'}</DialogTitle>
          <DialogDescription>
            {isEditing ? '修改 Redis 连接配置' : '配置新的 Redis 服务器连接'}
          </DialogDescription>
        </DialogHeader>

        <div className="grid gap-4 py-2">
          {/* Name */}
          <div className="grid gap-1.5">
            <label className="text-sm font-medium">连接名称 *</label>
            <Input
              value={name}
              onChange={(e) => { setName(e.target.value); setErrors((p) => ({ ...p, name: undefined })) }}
              placeholder="例如: 本地开发 Redis"
              className={cn(errors.name && 'border-red-500')}
            />
            {errors.name && <span className="text-xs text-red-500">{errors.name}</span>}
          </div>

          {/* Host & Port */}
          <div className="grid grid-cols-3 gap-3">
            <div className="col-span-2 grid gap-1.5">
              <label className="text-sm font-medium">主机地址 *</label>
              <Input
                value={host}
                onChange={(e) => { setHost(e.target.value); setErrors((p) => ({ ...p, host: undefined })) }}
                placeholder="127.0.0.1"
                className={cn(errors.host && 'border-red-500')}
              />
              {errors.host && <span className="text-xs text-red-500">{errors.host}</span>}
            </div>
            <div className="grid gap-1.5">
              <label className="text-sm font-medium">端口 *</label>
              <Input
                value={port}
                onChange={(e) => { setPort(e.target.value); setErrors((p) => ({ ...p, port: undefined })) }}
                placeholder="6379"
                className={cn(errors.port && 'border-red-500')}
              />
              {errors.port && <span className="text-xs text-red-500">{errors.port}</span>}
            </div>
          </div>

          {/* Password */}
          <div className="grid gap-1.5">
            <label className="text-sm font-medium">密码</label>
            <div className="relative">
              <Input
                type={showPassword ? 'text' : 'password'}
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                placeholder="留空表示无密码"
                className="pr-10"
              />
              <Button
                type="button"
                variant="ghost"
                size="icon"
                className="absolute right-0 top-0 h-full px-3 hover:bg-transparent"
                onClick={() => setShowPassword(!showPassword)}
              >
                {showPassword ? <EyeOff className="h-4 w-4" /> : <Eye className="h-4 w-4" />}
              </Button>
            </div>
          </div>

          {/* SSL Toggle */}
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-2">
              <ShieldCheck className="h-4 w-4 text-muted-foreground" />
              <span className="text-sm font-medium">SSL/TLS 加密</span>
            </div>
            <Button
              type="button"
              variant={ssl ? 'default' : 'outline'}
              size="sm"
              onClick={() => setSsl(!ssl)}
              className="h-7 text-xs"
            >
              {ssl ? '已启用' : '未启用'}
            </Button>
          </div>

          {/* Group */}
          <div className="grid gap-1.5">
            <label className="text-sm font-medium">分组</label>
            <Input
              value={group}
              onChange={(e) => setGroup(e.target.value)}
              placeholder="例如: 本地 / 测试 / 生产"
            />
          </div>

          {/* Test Result */}
          {testResult !== null && (
            <div
              className={cn(
                'rounded-md px-3 py-2 text-sm',
                testResult
                  ? 'bg-emerald-50 text-emerald-700 border border-emerald-200'
                  : 'bg-red-50 text-red-700 border border-red-200'
              )}
            >
              {testResult ? '连接测试成功' : '连接测试失败，请检查配置'}
            </div>
          )}
        </div>

        <DialogFooter className="gap-2">
          <Button variant="outline" onClick={handleTestConnection} disabled={isTesting}>
            {isTesting ? <Loader2 className="h-4 w-4 mr-1.5 animate-spin" /> : <TestTube className="h-4 w-4 mr-1.5" />}
            测试连接
          </Button>
          <div className="flex-1" />
          <Button variant="ghost" onClick={handleClose}>
            取消
          </Button>
          <Button onClick={handleSave}>
            {isEditing ? '保存修改' : '保存连接'}
          </Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  )
}
