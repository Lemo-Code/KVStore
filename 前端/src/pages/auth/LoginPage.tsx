import { useState } from 'react'
import { Link, useNavigate } from 'react-router-dom'
import { AuthLayout, SocialLoginButtons } from '@/components/auth/AuthLayout'
import { Button } from '@/components/ui/Button'
import { Checkbox, Input } from '@/components/ui/Input'
import { useAuthStore } from '@/stores/authStore'
import { Eye, EyeOff, Loader2, Lock, Mail } from 'lucide-react'

export default function LoginPage() {
  const navigate = useNavigate()
  const login = useAuthStore((s) => s.login)

  const [email, setEmail] = useState('')
  const [password, setPassword] = useState('')
  const [showPwd, setShowPwd] = useState(false)
  const [remember, setRemember] = useState(true)
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState('')

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault()
    if (!email || !password) {
      setError('请填写邮箱和密码')
      return
    }
    setLoading(true)
    setError('')
    try {
      await login(email, password)
      const { hasOnboarded } = useAuthStore.getState()
      navigate(hasOnboarded ? '/dashboard' : '/onboarding')
    } catch {
      setError('登录失败，请检查账号密码')
    } finally {
      setLoading(false)
    }
  }

  return (
    <AuthLayout title="欢迎回来" subtitle="登录以继续你的 Redis 学习之旅">
      <form onSubmit={handleSubmit} className="space-y-4">
        <Input
          label="邮箱地址"
          type="email"
          placeholder="you@example.com"
          value={email}
          onChange={(e) => setEmail(e.target.value)}
          leftIcon={<Mail size={16} />}
          error={error && !email ? '请输入邮箱' : undefined}
        />
        <Input
          label="密码"
          type={showPwd ? 'text' : 'password'}
          placeholder="输入密码"
          value={password}
          onChange={(e) => setPassword(e.target.value)}
          leftIcon={<Lock size={16} />}
          rightIcon={
            <button type="button" onClick={() => setShowPwd(!showPwd)} className="hover:text-text-secondary">
              {showPwd ? <EyeOff size={16} /> : <Eye size={16} />}
            </button>
          }
        />

        <div className="flex items-center justify-between">
          <Checkbox label="记住我" checked={remember} onChange={setRemember} />
          <Link to="/forgot-password" className="text-xs text-accent-red hover:underline">
            忘记密码？
          </Link>
        </div>

        {error && <p className="text-xs text-danger text-center">{error}</p>}

        <Button type="submit" variant="accent" className="w-full h-11" disabled={loading}>
          {loading ? <Loader2 size={16} className="animate-spin" /> : null}
          登录
        </Button>
      </form>

      <div className="mt-6">
        <SocialLoginButtons />
      </div>

      <p className="mt-8 text-center text-xs text-text-muted">
        还没有账号？{' '}
        <Link to="/register" className="text-accent-red font-medium hover:underline">
          立即注册
        </Link>
      </p>
    </AuthLayout>
  )
}
