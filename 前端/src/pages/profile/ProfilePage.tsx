import { PageContent, PageHeader } from '@/components/layout/MainLayout'
import { Badge } from '@/components/ui/Badge'
import { Button } from '@/components/ui/Button'
import { Input } from '@/components/ui/Input'
import { useToast } from '@/components/ui/Toast'
import { useAuthStore } from '@/stores/authStore'
import { cn } from '@/lib/utils'
import { motion } from 'framer-motion'
import {
  Award,
  Calendar,
  Camera,
  Edit3,
  Flame,
  Mail,
  Save,
  Target,
  Trophy,
} from 'lucide-react'
import { useState } from 'react'

const achievements = [
  { icon: Flame, label: '连续学习 7 天', color: 'text-accent-red' },
  { icon: Trophy, label: '完成基础课程', color: 'text-accent-amber' },
  { icon: Target, label: '执行 100 条命令', color: 'text-accent-teal' },
  { icon: Award, label: 'AI 对话达人', color: 'text-accent-purple' },
]

export default function ProfilePage() {
  const user = useAuthStore((s) => s.user)
  const updateUser = useAuthStore((s) => s.updateUser)
  const [editing, setEditing] = useState(false)
  const [bio, setBio] = useState(user?.bio ?? '')
  const toast = useToast()

  const handleSave = () => {
    updateUser({ bio })
    setEditing(false)
    toast.success('资料已保存', '你的个人档案已更新')
  }

  return (
    <>
      <PageHeader
        title="个人资料"
        subtitle="管理你的账户信息和学习档案"
        actions={
          editing ? (
            <Button variant="accent" size="sm" onClick={handleSave}>
              <Save size={14} />
              保存
            </Button>
          ) : (
            <Button variant="outline" size="sm" onClick={() => setEditing(true)}>
              <Edit3 size={14} />
              编辑资料
            </Button>
          )
        }
      />
      <PageContent>
        <div className="max-w-3xl mx-auto space-y-6">
          {/* Profile card */}
          <motion.div
            initial={{ opacity: 0, y: 12 }}
            animate={{ opacity: 1, y: 0 }}
            className="rounded-2xl border border-border-subtle bg-surface-1 p-6"
          >
            <div className="flex items-start gap-6">
              <div className="relative">
                <div className="flex h-20 w-20 items-center justify-center rounded-2xl bg-gradient-to-br from-accent-red/20 to-accent-purple/20 text-2xl font-bold text-accent-red">
                  {user?.avatar ?? 'U'}
                </div>
                {editing && (
                  <button className="absolute -bottom-1 -right-1 flex h-7 w-7 items-center justify-center rounded-full bg-surface-3 border border-border hover:bg-surface-4 transition-colors">
                    <Camera size={12} />
                  </button>
                )}
              </div>
              <div className="flex-1">
                <h2 className="text-xl font-bold">{user?.username}</h2>
                <div className="flex items-center gap-2 mt-1">
                  <Badge variant="purple">{user?.level}</Badge>
                  <Badge variant="success">{user?.role === 'student' ? '学员' : user?.role}</Badge>
                </div>
                {editing ? (
                  <textarea
                    value={bio}
                    onChange={(e) => setBio(e.target.value)}
                    className="mt-3 w-full rounded-lg border border-border-subtle bg-surface-0 p-3 text-sm focus:outline-none focus:border-accent-red/40 resize-none"
                    rows={2}
                  />
                ) : (
                  <p className="mt-3 text-sm text-text-muted">{user?.bio}</p>
                )}
                <div className="flex items-center gap-4 mt-3 text-xs text-text-muted">
                  <span className="flex items-center gap-1"><Mail size={12} />{user?.email}</span>
                  <span className="flex items-center gap-1"><Calendar size={12} />加入于 {user?.joinDate}</span>
                </div>
              </div>
            </div>
          </motion.div>

          {/* Stats */}
          <div className="grid grid-cols-4 gap-3">
            {[
              { label: '学习天数', value: '7' },
              { label: '完成课时', value: '18' },
              { label: '执行命令', value: '342' },
              { label: '获得徽章', value: '4' },
            ].map((s) => (
              <div key={s.label} className="rounded-xl border border-border-subtle bg-surface-1 p-4 text-center">
                <div className="text-xl font-bold">{s.value}</div>
                <div className="text-[11px] text-text-muted mt-0.5">{s.label}</div>
              </div>
            ))}
          </div>

          {/* Achievements */}
          <div>
            <h3 className="text-sm font-semibold mb-3">成就徽章</h3>
            <div className="grid grid-cols-2 sm:grid-cols-4 gap-3">
              {achievements.map((a) => (
                <div
                  key={a.label}
                  className="rounded-xl border border-border-subtle bg-surface-1 p-4 text-center hover:border-border transition-colors"
                >
                  <a.icon size={24} className={cn('mx-auto mb-2', a.color)} />
                  <div className="text-[11px] font-medium">{a.label}</div>
                </div>
              ))}
            </div>
          </div>

          {/* Account info */}
          {editing && (
            <motion.div
              initial={{ opacity: 0, height: 0 }}
              animate={{ opacity: 1, height: 'auto' }}
              className="rounded-xl border border-border-subtle bg-surface-1 p-5 space-y-3"
            >
              <h3 className="text-sm font-semibold mb-2">编辑信息</h3>
              <Input label="用户名" defaultValue={user?.username} />
              <Input label="邮箱" type="email" defaultValue={user?.email} />
              <Input label="个人简介" defaultValue={bio} onChange={(e) => setBio(e.target.value)} />
            </motion.div>
          )}
        </div>
      </PageContent>
    </>
  )
}
