import { useState } from 'react'
import { useNavigate, Link } from 'react-router-dom'
import { Eye, EyeOff, Loader2 } from 'lucide-react'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { useAuthStore } from '@/stores/authStore'

const inp = 'h-11 rounded-xl border-[#c0c0c0] bg-white text-[15px] text-[#111] placeholder:text-[#aaa] focus:border-[#DC2626] focus:ring-2 focus:ring-red-100 shadow-sm'

export default function LoginPage() {
  const nav = useNavigate()
  const login = useAuthStore((s) => s.login)
  const [form, setForm] = useState({ username: '', password: '' })
  const [showPw, setShowPw] = useState(false)
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState('')
  const [fieldErrors, setFieldErrors] = useState<Record<string, string>>({})

  const validate = () => {
    const e: Record<string, string> = {}
    if (!form.username.trim()) e.username = '请输入用户名'
    if (!form.password) e.password = '请输入密码'
    setFieldErrors(e)
    return Object.keys(e).length === 0
  }

  const submit = async (e: React.FormEvent) => {
    e.preventDefault(); setError('')
    if (!validate()) return
    setLoading(true)
    await new Promise((r) => setTimeout(r, 600))
    if (form.username === 'admin' && form.password === 'admin123') {
      login({ id: '1', username: form.username, email: 'admin@redis.local', avatarUrl: undefined }, 'demo-token')
      nav('/')
    } else { setError('用户名或密码错误') }
    setLoading(false)
  }

  return (
    <div className="flex min-h-screen items-center justify-center bg-[#f3f3f3] px-4">
      <div className="w-full max-w-[400px]">
        <div className="mb-10 text-center">
          <div className="mx-auto mb-5 flex h-12 w-12 items-center justify-center rounded-2xl bg-[#DC2626] shadow-lg shadow-red-200">
            <svg className="h-6 w-6 text-white" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
              <rect x="2" y="2" width="20" height="20" rx="5"/><path d="M7 8h10M7 12h10M7 16h6"/>
            </svg>
          </div>
          <h1 className="text-[22px] font-bold tracking-tight text-[#111]">登录</h1>
          <p className="mt-1.5 text-[15px] text-[#666]">欢迎使用 Redis 智能学习平台</p>
        </div>

        <div className="rounded-2xl border border-[#ddd] bg-white p-7 shadow-sm">
          <form onSubmit={submit} className="space-y-5">
            <div>
              <label className="mb-1.5 block text-[14px] font-semibold text-[#222]">用户名</label>
              <Input value={form.username}
                onChange={(e) => { setForm({ ...form, username: e.target.value }); if (fieldErrors.username) setFieldErrors({ ...fieldErrors, username: '' }) }}
                placeholder="输入用户名" className={inp + (fieldErrors.username ? ' !border-red-400' : '')} />
              {fieldErrors.username && <p className="mt-1 text-[13px] text-red-500">{fieldErrors.username}</p>}
            </div>
            <div>
              <div className="mb-1.5 flex items-center justify-between">
                <label className="text-[14px] font-semibold text-[#222]">密码</label>
                <Link to="/forgot-password" className="text-[13px] text-[#888] hover:text-[#DC2626]">忘记密码？</Link>
              </div>
              <div className="relative">
                <Input type={showPw ? 'text' : 'password'} value={form.password}
                  onChange={(e) => { setForm({ ...form, password: e.target.value }); if (fieldErrors.password) setFieldErrors({ ...fieldErrors, password: '' }) }}
                  placeholder="输入密码" className={inp + ' pr-10' + (fieldErrors.password ? ' !border-red-400' : '')} />
                <button type="button" onClick={() => setShowPw(!showPw)} className="absolute right-3.5 top-3 text-[#999] hover:text-[#333]">
                  {showPw ? <EyeOff className="h-4 w-4" /> : <Eye className="h-4 w-4" />}
                </button>
              </div>
              {fieldErrors.password && <p className="mt-1 text-[13px] text-red-500">{fieldErrors.password}</p>}
            </div>

            {error && <div className="rounded-xl border border-red-100 bg-red-50 px-4 py-3 text-[14px] font-medium text-red-600">{error}</div>}

            <Button type="submit" disabled={loading}
              className="h-11 w-full rounded-xl bg-[#DC2626] text-[15px] font-semibold text-white shadow-sm shadow-red-200 transition-all hover:bg-[#b91c1c] active:scale-[0.98] disabled:opacity-60">
              {loading && <Loader2 className="mr-2 h-4 w-4 animate-spin" />}
              {loading ? '登录中...' : '登录'}
            </Button>
          </form>
        </div>

        <p className="mt-6 text-center text-[14px] text-[#888]">
          还没有账户？ <Link to="/register" className="font-semibold text-[#DC2626] hover:underline">创建账户</Link>
        </p>
        <p className="mt-2 text-center text-[12px] text-[#bbb]">测试账号: admin / admin123</p>
      </div>
    </div>
  )
}
