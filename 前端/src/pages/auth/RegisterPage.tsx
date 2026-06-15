import { useState } from 'react'
import { Link, useNavigate } from 'react-router-dom'
import { AuthLayout, SocialLoginButtons } from '@/components/auth/AuthLayout'
import { Button } from '@/components/ui/Button'
import { Checkbox, Input } from '@/components/ui/Input'
import { useAuthStore } from '@/stores/authStore'
import { Eye, EyeOff, Loader2, Lock, Mail, User } from 'lucide-react'

export default function RegisterPage() {
  const navigate = useNavigate()
  const register = useAuthStore((s) => s.register)

  const [form, setForm] = useState({ username: '', email: '', password: '', confirm: '' })
  const [showPwd, setShowPwd] = useState(false)
  const [agreed, setAgreed] = useState(false)
  const [loading, setLoading] = useState(false)
  const [errors, setErrors] = useState<Record<string, string>>({})

  const validate = () => {
    const e: Record<string, string> = {}
    if (!form.username.trim()) e.username = '请输入用户名'
    if (!form.email.trim()) e.email = '请输入邮箱'
    if (form.password.length < 8) e.password = '密码至少 8 位'
    if (form.password !== form.confirm) e.confirm = '两次密码不一致'
    if (!agreed) e.agreed = '请同意服务条款'
    setErrors(e)
    return Object.keys(e).length === 0
  }

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault()
    if (!validate()) return
    setLoading(true)
    try {
      await register({ email: form.email, username: form.username, password: form.password })
      navigate('/verify-email')
    } finally {
      setLoading(false)
    }
  }

  return (
    <AuthLayout title="创建账号" subtitle="加入 Redis Lab Studio，开启智能学习之旅">
      <form onSubmit={handleSubmit} className="space-y-4">
        <Input
          label="用户名"
          placeholder="你的昵称"
          value={form.username}
          onChange={(e) => setForm({ ...form, username: e.target.value })}
          leftIcon={<User size={16} />}
          error={errors.username}
        />
        <Input
          label="邮箱地址"
          type="email"
          placeholder="you@example.com"
          value={form.email}
          onChange={(e) => setForm({ ...form, email: e.target.value })}
          leftIcon={<Mail size={16} />}
          error={errors.email}
        />
        <Input
          label="密码"
          type={showPwd ? 'text' : 'password'}
          placeholder="至少 8 位字符"
          value={form.password}
          onChange={(e) => setForm({ ...form, password: e.target.value })}
          leftIcon={<Lock size={16} />}
          rightIcon={
            <button type="button" onClick={() => setShowPwd(!showPwd)}>
              {showPwd ? <EyeOff size={16} /> : <Eye size={16} />}
            </button>
          }
          error={errors.password}
          hint="建议包含大小写字母、数字和特殊字符"
        />
        <Input
          label="确认密码"
          type="password"
          placeholder="再次输入密码"
          value={form.confirm}
          onChange={(e) => setForm({ ...form, confirm: e.target.value })}
          leftIcon={<Lock size={16} />}
          error={errors.confirm}
        />

        <Checkbox
          label={
            <>
              我已阅读并同意{' '}
              <span className="text-accent-red">服务条款</span> 和{' '}
              <span className="text-accent-red">隐私政策</span>
            </>
          }
          checked={agreed}
          onChange={setAgreed}
        />
        {errors.agreed && <p className="text-[11px] text-danger">{errors.agreed}</p>}

        <Button type="submit" variant="accent" className="w-full h-11" disabled={loading}>
          {loading ? <Loader2 size={16} className="animate-spin" /> : null}
          创建账号
        </Button>
      </form>

      <div className="mt-6">
        <SocialLoginButtons />
      </div>

      <p className="mt-8 text-center text-xs text-text-muted">
        已有账号？{' '}
        <Link to="/login" className="text-accent-red font-medium hover:underline">
          直接登录
        </Link>
      </p>
    </AuthLayout>
  )
}
