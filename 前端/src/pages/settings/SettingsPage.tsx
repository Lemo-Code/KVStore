import { PageContent, PageHeader } from '@/components/layout/MainLayout'
import { Button } from '@/components/ui/Button'
import { Checkbox, Input } from '@/components/ui/Input'
import { Modal } from '@/components/ui/Modal'
import { useToast } from '@/components/ui/Toast'
import { useSettingsStore } from '@/stores/settingsStore'
import { cn } from '@/lib/utils'
import { useState } from 'react'
import {
  AlertTriangle,
  Bell,
  Globe,
  Key,
  Monitor,
  Moon,
  Palette,
  Shield,
  Sun,
  Trash2,
} from 'lucide-react'

const tabs = [
  { id: 'general', label: '通用', icon: Globe },
  { id: 'appearance', label: '外观', icon: Palette },
  { id: 'notifications', label: '通知', icon: Bell },
  { id: 'security', label: '安全', icon: Shield },
]

export default function SettingsPage() {
  const [activeTab, setActiveTab] = useState('general')
  const [confirmDelete, setConfirmDelete] = useState(false)
  const toast = useToast()

  const theme = useSettingsStore((s) => s.theme)
  const setTheme = useSettingsStore((s) => s.setTheme)
  const editorFontSize = useSettingsStore((s) => s.editorFontSize)
  const setEditorFontSize = useSettingsStore((s) => s.setEditorFontSize)
  const reduceMotion = useSettingsStore((s) => s.reduceMotion)
  const setReduceMotion = useSettingsStore((s) => s.setReduceMotion)
  const autoConnect = useSettingsStore((s) => s.autoConnect)
  const setAutoConnect = useSettingsStore((s) => s.setAutoConnect)
  const autoSaveHistory = useSettingsStore((s) => s.autoSaveHistory)
  const setAutoSaveHistory = useSettingsStore((s) => s.setAutoSaveHistory)

  return (
    <>
      <PageHeader title="设置" subtitle="个性化你的 Redis Lab Studio 体验" />
      <PageContent>
        <div className="max-w-3xl mx-auto flex gap-6">
          {/* Tab sidebar */}
          <div className="w-48 shrink-0 space-y-0.5">
            {tabs.map((tab) => (
              <button
                key={tab.id}
                onClick={() => setActiveTab(tab.id)}
                className={cn(
                  'w-full flex items-center gap-2.5 rounded-lg px-3 py-2 text-xs font-medium transition-all',
                  activeTab === tab.id
                    ? 'bg-surface-3 text-text-primary'
                    : 'text-text-secondary hover:bg-surface-2',
                )}
              >
                <tab.icon size={14} />
                {tab.label}
              </button>
            ))}
          </div>

          {/* Content */}
          <div className="flex-1 space-y-6">
            {activeTab === 'general' && (
              <div className="rounded-xl border border-border-subtle bg-surface-1 p-5 space-y-4">
                <h3 className="text-sm font-semibold">通用设置</h3>
                <Input label="显示语言" defaultValue="简体中文" />
                <Input label="时区" defaultValue="Asia/Shanghai (UTC+8)" />
                <Checkbox
                  label="启动时自动连接上次的 Redis 实例"
                  checked={autoConnect}
                  onChange={setAutoConnect}
                />
                <Checkbox
                  label="命令执行后自动保存到历史记录"
                  checked={autoSaveHistory}
                  onChange={setAutoSaveHistory}
                />
              </div>
            )}

            {activeTab === 'appearance' && (
              <div className="rounded-xl border border-border-subtle bg-surface-1 p-5 space-y-4">
                <h3 className="text-sm font-semibold">外观主题</h3>
                <div className="grid grid-cols-3 gap-3">
                  {([
                    { id: 'dark' as const, icon: Moon, label: '深色' },
                    { id: 'light' as const, icon: Sun, label: '浅色' },
                    { id: 'system' as const, icon: Monitor, label: '跟随系统' },
                  ]).map((t) => (
                    <button
                      key={t.id}
                      onClick={() => setTheme(t.id)}
                      className={cn(
                        'flex flex-col items-center gap-2 rounded-xl border p-4 transition-all',
                        theme === t.id
                          ? 'border-accent-red/40 bg-accent-red/5'
                          : 'border-border-subtle hover:border-border',
                      )}
                    >
                      <t.icon size={20} className={theme === t.id ? 'text-accent-red' : 'text-text-muted'} />
                      <span className="text-xs font-medium">{t.label}</span>
                    </button>
                  ))}
                </div>
                <Input
                  label="编辑器字体大小"
                  type="number"
                  min={10}
                  max={24}
                  value={editorFontSize}
                  onChange={(e) => setEditorFontSize(Number(e.target.value) || 13)}
                  hint="Monaco 编辑器字号 (px)"
                />
                <Checkbox
                  label="启用界面动画效果"
                  checked={!reduceMotion}
                  onChange={(v) => setReduceMotion(!v)}
                />
              </div>
            )}

            {activeTab === 'notifications' && (
              <div className="rounded-xl border border-border-subtle bg-surface-1 p-5 space-y-3">
                <h3 className="text-sm font-semibold">通知偏好</h3>
                {[
                  '协作室新消息通知',
                  'AI 导师回复通知',
                  '课程更新提醒',
                  '学习打卡提醒',
                  '系统公告',
                ].map((label) => (
                  <Checkbox key={label} label={label} checked={label !== '系统公告'} />
                ))}
              </div>
            )}

            {activeTab === 'security' && (
              <div className="space-y-4">
                <div className="rounded-xl border border-border-subtle bg-surface-1 p-5 space-y-4">
                  <h3 className="text-sm font-semibold">修改密码</h3>
                  <Input label="当前密码" type="password" />
                  <Input label="新密码" type="password" />
                  <Input label="确认新密码" type="password" />
                  <Button
                    variant="accent"
                    size="sm"
                    onClick={() => toast.success('密码已更新', '下次登录请使用新密码')}
                  >
                    <Key size={14} />
                    更新密码
                  </Button>
                </div>
                <div className="rounded-xl border border-danger/20 bg-danger/5 p-5">
                  <h3 className="text-sm font-semibold text-danger mb-2">危险操作</h3>
                  <p className="text-xs text-text-muted mb-3">删除账户将清除所有学习记录和数据，此操作不可撤销。</p>
                  <Button variant="danger" size="sm" onClick={() => setConfirmDelete(true)}>
                    <Trash2 size={14} />
                    删除账户
                  </Button>
                </div>
              </div>
            )}

            <div className="flex justify-end">
              <Button
                variant="accent"
                size="sm"
                onClick={() => toast.success('设置已保存', '你的偏好已生效')}
              >
                保存设置
              </Button>
            </div>
          </div>
        </div>
      </PageContent>

      <Modal
        open={confirmDelete}
        onClose={() => setConfirmDelete(false)}
        title="确认删除账户？"
        size="sm"
      >
        <div className="flex items-start gap-3 mb-5">
          <div className="flex h-10 w-10 shrink-0 items-center justify-center rounded-lg bg-danger/10">
            <AlertTriangle size={20} className="text-danger" />
          </div>
          <p className="text-sm text-text-secondary leading-relaxed">
            此操作将永久清除你的所有学习记录、连接配置和数据，且<span className="text-danger font-semibold">无法恢复</span>。请确认是否继续。
          </p>
        </div>
        <div className="flex justify-end gap-2">
          <Button variant="ghost" size="sm" onClick={() => setConfirmDelete(false)}>
            取消
          </Button>
          <Button
            variant="danger"
            size="sm"
            onClick={() => {
              setConfirmDelete(false)
              toast.error('账户删除请求已提交', '我们将在 24 小时内处理')
            }}
          >
            <Trash2 size={14} />
            确认删除
          </Button>
        </div>
      </Modal>
    </>
  )
}
