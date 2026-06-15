import { PageContent, PageHeader } from '@/components/layout/MainLayout'
import { Badge } from '@/components/ui/Badge'
import { Button } from '@/components/ui/Button'
import { useToast } from '@/components/ui/Toast'
import { cn } from '@/lib/utils'
import { useAuthStore } from '@/stores/authStore'
import { motion } from 'framer-motion'
import { Navigate } from 'react-router-dom'
import {
  Activity,
  BookOpen,
  Database,
  Shield,
  TrendingUp,
  Users,
} from 'lucide-react'

const stats = [
  { label: '注册用户', value: '12,480', change: '+128 本周', icon: Users, color: 'text-accent-blue' },
  { label: '活跃连接', value: '3,241', change: '+5.2%', icon: Database, color: 'text-accent-red' },
  { label: 'AI 对话', value: '48.2k', change: '+12%', icon: Activity, color: 'text-accent-purple' },
  { label: '课程完成', value: '8,920', change: '+3.8%', icon: BookOpen, color: 'text-accent-amber' },
]

const recentUsers = [
  { name: '新用户_2847', email: 'user2847@test.com', role: 'student', join: '2 分钟前' },
  { name: 'redis_master', email: 'master@corp.com', role: 'mentor', join: '15 分钟前' },
  { name: 'learner_99', email: 'l99@school.edu', role: 'student', join: '1 小时前' },
  { name: 'dev_ops', email: 'ops@startup.io', role: 'student', join: '3 小时前' },
]

const systemLogs = [
  { level: 'info', msg: '沙箱环境 auto-scale 完成', time: '11:32:01' },
  { level: 'warn', msg: '连接 conn-8821 响应超时', time: '11:28:44' },
  { level: 'info', msg: 'AI 模型 v2.1 部署成功', time: '11:15:00' },
  { level: 'error', msg: '用户 u-992 登录失败 (3次)', time: '11:02:33' },
]

export default function AdminPage() {
  const user = useAuthStore((s) => s.user)
  const toast = useToast()

  if (user?.role !== 'admin') {
    return <Navigate to="/dashboard" replace />
  }

  return (
    <>
      <PageHeader
        title="管理后台"
        subtitle="平台运营数据与用户管理"
        actions={
          <Badge variant="purple">
            <Shield size={10} className="inline mr-1" />
            管理员
          </Badge>
        }
      />
      <PageContent>
        <div className="grid grid-cols-2 lg:grid-cols-4 gap-3 mb-8">
          {stats.map((s, i) => (
            <motion.div
              key={s.label}
              initial={{ opacity: 0, y: 8 }}
              animate={{ opacity: 1, y: 0 }}
              transition={{ delay: i * 0.05 }}
              className="rounded-xl border border-border-subtle bg-surface-1 p-4"
            >
              <div className="flex items-center justify-between mb-2">
                <s.icon size={16} className={s.color} />
                <TrendingUp size={12} className="text-success" />
              </div>
              <div className="text-2xl font-bold">{s.value}</div>
              <div className="flex items-center justify-between mt-1">
                <span className="text-[11px] text-text-muted">{s.label}</span>
                <span className="text-[10px] text-success">{s.change}</span>
              </div>
            </motion.div>
          ))}
        </div>

        <div className="grid lg:grid-cols-2 gap-6">
          <div className="rounded-xl border border-border-subtle bg-surface-1 overflow-hidden">
            <div className="flex items-center justify-between px-4 py-3 border-b border-border-subtle">
              <h3 className="text-sm font-semibold">最近注册用户</h3>
              <Button
                variant="ghost"
                size="sm"
                className="!text-[10px]"
                onClick={() => toast.info('用户管理', '完整用户列表功能即将上线')}
              >
                查看全部
              </Button>
            </div>
            <table className="w-full text-xs">
              <thead>
                <tr className="text-left text-text-muted border-b border-border-subtle">
                  <th className="px-4 py-2">用户</th>
                  <th className="px-4 py-2">角色</th>
                  <th className="px-4 py-2">时间</th>
                </tr>
              </thead>
              <tbody>
                {recentUsers.map((u) => (
                  <tr key={u.email} className="border-b border-border-subtle/50 hover:bg-surface-2">
                    <td className="px-4 py-2.5">
                      <div className="font-medium">{u.name}</div>
                      <div className="text-[10px] text-text-muted">{u.email}</div>
                    </td>
                    <td className="px-4 py-2.5">
                      <Badge variant={u.role === 'mentor' ? 'warning' : 'default'}>{u.role}</Badge>
                    </td>
                    <td className="px-4 py-2.5 text-text-muted">{u.join}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>

          <div className="rounded-xl border border-border-subtle bg-surface-1 overflow-hidden">
            <div className="px-4 py-3 border-b border-border-subtle">
              <h3 className="text-sm font-semibold">系统日志</h3>
            </div>
            <div className="divide-y divide-border-subtle/50">
              {systemLogs.map((log) => (
                <div key={log.time + log.msg} className="flex items-start gap-3 px-4 py-2.5 text-xs">
                  <span className={cn(
                    'shrink-0 rounded px-1.5 py-0.5 text-[9px] font-bold uppercase',
                    log.level === 'error' ? 'bg-danger/15 text-danger' :
                    log.level === 'warn' ? 'bg-warning/15 text-warning' :
                    'bg-success/15 text-success',
                  )}>
                    {log.level}
                  </span>
                  <span className="flex-1 text-text-secondary">{log.msg}</span>
                  <span className="font-mono text-[10px] text-text-muted shrink-0">{log.time}</span>
                </div>
              ))}
            </div>
          </div>
        </div>
      </PageContent>
    </>
  )
}
