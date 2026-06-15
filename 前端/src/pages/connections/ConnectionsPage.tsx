import { PageContent, PageHeader } from '@/components/layout/MainLayout'
import { Badge } from '@/components/ui/Badge'
import { Button } from '@/components/ui/Button'
import { EmptyState } from '@/components/ui/EmptyState'
import { Input } from '@/components/ui/Input'
import { useToast } from '@/components/ui/Toast'
import { cn } from '@/lib/utils'
import { motion } from 'framer-motion'
import {
  CheckCircle2,
  Loader2,
  MoreVertical,
  Plus,
  RefreshCw,
  Search,
  Server,
  TestTube,
  Wifi,
  WifiOff,
  XCircle,
} from 'lucide-react'
import { useState } from 'react'
import { useNavigate } from 'react-router-dom'

interface Connection {
  id: string
  name: string
  host: string
  port: number
  status: 'connected' | 'disconnected' | 'error'
  version?: string
  keys?: number
  memory?: string
  type: 'local' | 'sandbox' | 'cloud'
  lastUsed: string
}

const mockConnections: Connection[] = [
  { id: '1', name: '本地 Redis 学习环境', host: '127.0.0.1', port: 6379, status: 'connected', version: '7.2.4', keys: 847, memory: '12.4 MB', type: 'local', lastUsed: '刚刚' },
  { id: '2', name: '平台沙箱环境', host: 'sandbox.redis.lab', port: 6379, status: 'connected', version: '7.2.4', keys: 156, memory: '3.2 MB', type: 'sandbox', lastUsed: '2 小时前' },
  { id: '3', name: '阿里云 Redis', host: 'r-xxx.redis.aliyuncs.com', port: 6379, status: 'disconnected', type: 'cloud', lastUsed: '3 天前' },
  { id: '4', name: '测试环境', host: '192.168.1.100', port: 6380, status: 'error', type: 'local', lastUsed: '1 周前' },
]

const typeLabels = { local: '本地', sandbox: '沙箱', cloud: '云端' }

export default function ConnectionsPage() {
  const [search, setSearch] = useState('')
  const [showNew, setShowNew] = useState(false)
  const [testing, setTesting] = useState(false)
  const [connectingId, setConnectingId] = useState<string | null>(null)
  const toast = useToast()
  const navigate = useNavigate()

  const filtered = mockConnections.filter(
    (c) => c.name.includes(search) || c.host.includes(search),
  )

  const handleConnect = (conn: Connection) => {
    setConnectingId(conn.id)
    setTimeout(() => {
      setConnectingId(null)
      toast.success('连接成功', `已连接到 ${conn.name} (${conn.host}:${conn.port})`)
      navigate('/workspace')
    }, 900)
  }

  const handleTest = () => {
    setTesting(true)
    setTimeout(() => {
      setTesting(false)
      toast.success('测试通过', 'Redis 实例可达，延迟 2ms')
    }, 1000)
  }

  const handleSave = () => {
    setShowNew(false)
    toast.success('已保存连接', '新的 Redis 连接已添加到列表')
  }

  return (
    <>
      <PageHeader
        title="连接管理"
        subtitle="管理你的 Redis 连接，支持本地、沙箱和云端环境"
        actions={
          <Button variant="accent" size="sm" onClick={() => setShowNew(true)}>
            <Plus size={14} />
            新建连接
          </Button>
        }
      />
      <PageContent>
        <div className="flex items-center gap-3 mb-6">
          <div className="flex-1 max-w-sm">
            <Input
              placeholder="搜索连接..."
              value={search}
              onChange={(e) => setSearch(e.target.value)}
              leftIcon={<Search size={14} />}
            />
          </div>
          <Button variant="ghost" size="sm">
            <RefreshCw size={14} />
            刷新全部
          </Button>
        </div>

        {filtered.length === 0 ? (
          <EmptyState
            icon={search ? Search : Server}
            title={search ? '未找到匹配的连接' : '还没有任何连接'}
            description={
              search
                ? `没有与「${search}」匹配的 Redis 连接，试试其它关键词。`
                : '添加你的第一个 Redis 连接，开始浏览键和执行命令。'
            }
            action={search ? { label: '清除搜索', onClick: () => setSearch('') } : { label: '新建连接', onClick: () => setShowNew(true) }}
          />
        ) : (
        <div className="grid gap-3">
          {filtered.map((conn, i) => (
            <motion.div
              key={conn.id}
              initial={{ opacity: 0, y: 8 }}
              animate={{ opacity: 1, y: 0 }}
              transition={{ delay: i * 0.05 }}
              className="flex items-center gap-4 rounded-xl border border-border-subtle bg-surface-1 p-4 hover:border-border transition-colors group"
            >
              <div
                className={cn(
                  'flex h-11 w-11 shrink-0 items-center justify-center rounded-xl',
                  conn.status === 'connected' ? 'bg-success/10' : conn.status === 'error' ? 'bg-danger/10' : 'bg-surface-3',
                )}
              >
                <Server
                  size={20}
                  className={cn(
                    conn.status === 'connected' ? 'text-success' : conn.status === 'error' ? 'text-danger' : 'text-text-muted',
                  )}
                />
              </div>

              <div className="flex-1 min-w-0">
                <div className="flex items-center gap-2">
                  <span className="text-sm font-semibold truncate">{conn.name}</span>
                  <Badge>{typeLabels[conn.type]}</Badge>
                  {conn.status === 'connected' && (
                    <span className="flex items-center gap-1 text-[10px] text-success">
                      <Wifi size={10} /> 已连接
                    </span>
                  )}
                  {conn.status === 'disconnected' && (
                    <span className="flex items-center gap-1 text-[10px] text-text-muted">
                      <WifiOff size={10} /> 未连接
                    </span>
                  )}
                  {conn.status === 'error' && (
                    <span className="flex items-center gap-1 text-[10px] text-danger">
                      <XCircle size={10} /> 连接失败
                    </span>
                  )}
                </div>
                <div className="flex items-center gap-3 mt-1 text-[11px] text-text-muted">
                  <span className="font-mono">{conn.host}:{conn.port}</span>
                  {conn.version && <span>v{conn.version}</span>}
                  {conn.keys !== undefined && <span>{conn.keys} 键</span>}
                  {conn.memory && <span>{conn.memory}</span>}
                  <span>上次使用 {conn.lastUsed}</span>
                </div>
              </div>

              <div className="flex items-center gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
                {conn.status !== 'connected' && (
                  <Button
                    variant="outline"
                    size="sm"
                    onClick={() => handleConnect(conn)}
                    disabled={connectingId === conn.id}
                  >
                    {connectingId === conn.id ? (
                      <Loader2 size={13} className="animate-spin" />
                    ) : (
                      <Wifi size={13} />
                    )}
                    {connectingId === conn.id ? '连接中' : '连接'}
                  </Button>
                )}
                {conn.status === 'connected' && (
                  <Button variant="accent" size="sm" onClick={() => navigate('/workspace')}>
                    <TestTube size={13} />
                    打开
                  </Button>
                )}
                <Button variant="ghost" size="icon" className="h-7 w-7">
                  <MoreVertical size={14} />
                </Button>
              </div>
            </motion.div>
          ))}
        </div>
        )}

        {/* New connection dialog (inline) */}
        {showNew && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm"
            onClick={() => setShowNew(false)}
          >
            <motion.div
              initial={{ scale: 0.95, opacity: 0 }}
              animate={{ scale: 1, opacity: 1 }}
              className="w-full max-w-lg rounded-2xl border border-border-subtle bg-surface-1 p-6 shadow-2xl"
              onClick={(e) => e.stopPropagation()}
            >
              <h2 className="text-lg font-bold mb-1">新建 Redis 连接</h2>
              <p className="text-xs text-text-muted mb-5">填写连接信息以添加新的 Redis 实例</p>
              <div className="space-y-3">
                <Input label="连接名称" placeholder="我的 Redis" />
                <div className="grid grid-cols-3 gap-3">
                  <div className="col-span-2">
                    <Input label="主机地址" placeholder="127.0.0.1" />
                  </div>
                  <Input label="端口" placeholder="6379" />
                </div>
                <Input label="密码（可选）" type="password" placeholder="留空表示无密码" />
                <Input label="数据库编号" placeholder="0" hint="默认连接 db0" />
              </div>
              <div className="flex justify-end gap-2 mt-6">
                <Button variant="ghost" onClick={() => setShowNew(false)}>取消</Button>
                <Button variant="outline" onClick={handleTest} disabled={testing}>
                  {testing ? <Loader2 size={14} className="animate-spin" /> : <CheckCircle2 size={14} />}
                  {testing ? '测试中' : '测试连接'}
                </Button>
                <Button variant="accent" onClick={handleSave}>保存并连接</Button>
              </div>
            </motion.div>
          </motion.div>
        )}
      </PageContent>
    </>
  )
}
