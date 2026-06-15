import { useState } from 'react'
import { Link, useNavigate } from 'react-router-dom'
import { AuthLayout } from '@/components/auth/AuthLayout'
import { Button } from '@/components/ui/Button'
import { Input } from '@/components/ui/Input'
import { motion } from 'framer-motion'
import { CheckCircle2, Eye, EyeOff, Loader2, Lock } from 'lucide-react'

export default function ResetPasswordPage() {
  const navigate = useNavigate()
  const [password, setPassword] = useState('')
  const [confirm, setConfirm] = useState('')
  const [showPwd, setShowPwd] = useState(false)
  const [loading, setLoading] = useState(false)
  const [done, setDone] = useState(false)

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault()
    if (password.length < 8 || password !== confirm) return
    setLoading(true)
    await new Promise((r) => setTimeout(r, 1000))
    setDone(true)
    setLoading(false)
  }

  return (
    <AuthLayout title="重置密码" subtitle="请设置你的新密码">
      {!done ? (
        <form onSubmit={handleSubmit} className="space-y-4">
          <Input
            label="新密码"
            type={showPwd ? 'text' : 'password'}
            placeholder="至少 8 位字符"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            leftIcon={<Lock size={16} />}
            rightIcon={
              <button type="button" onClick={() => setShowPwd(!showPwd)}>
                {showPwd ? <EyeOff size={16} /> : <Eye size={16} />}
              </button>
            }
          />
          <Input
            label="确认新密码"
            type="password"
            placeholder="再次输入新密码"
            value={confirm}
            onChange={(e) => setConfirm(e.target.value)}
            leftIcon={<Lock size={16} />}
          />
          <Button
            type="submit"
            variant="accent"
            className="w-full h-11"
            disabled={loading || password.length < 8 || password !== confirm}
          >
            {loading ? <Loader2 size={16} className="animate-spin" /> : null}
            确认重置
          </Button>
        </form>
      ) : (
        <motion.div initial={{ opacity: 0 }} animate={{ opacity: 1 }} className="text-center py-6">
          <div className="flex justify-center mb-4">
            <div className="flex h-16 w-16 items-center justify-center rounded-full bg-success/15">
              <CheckCircle2 size={32} className="text-success" />
            </div>
          </div>
          <h3 className="text-lg font-semibold mb-2">密码已重置</h3>
          <p className="text-sm text-text-muted mb-6">你的密码已成功更新，请使用新密码登录。</p>
          <Button variant="accent" className="w-full h-11" onClick={() => navigate('/login')}>
            前往登录
          </Button>
        </motion.div>
      )}

      {!done && (
        <p className="mt-8 text-center text-xs text-text-muted">
          <Link to="/login" className="text-accent-red hover:underline">返回登录</Link>
        </p>
      )}
    </AuthLayout>
  )
}
