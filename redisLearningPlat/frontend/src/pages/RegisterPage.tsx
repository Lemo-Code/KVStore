import { useState } from 'react'
import { useNavigate, Link } from 'react-router-dom'
import { ArrowRight, Loader2, Check, X } from 'lucide-react'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'

const checks = [
  { test: (p: string) => p.length >= 8, label: '至少 8 个字符' },
  { test: (p: string) => /[A-Z]/.test(p), label: '包含大写字母' },
  { test: (p: string) => /[0-9]/.test(p), label: '包含数字' },
  { test: (p: string) => /[^A-Za-z0-9]/.test(p), label: '包含特殊字符' },
]

const inp = 'h-11 rounded-xl border-[#c0c0c0] bg-white text-[15px] text-[#111] placeholder:text-[#aaa] focus:border-[#DC2626] focus:ring-2 focus:ring-red-100 shadow-sm'
const lab = 'mb-1.5 block text-[14px] font-semibold text-[#222]'

function barColor(n: number) { return n <= 1 ? 'bg-red-500' : n === 2 ? 'bg-orange-500' : n === 3 ? 'bg-amber-500' : 'bg-emerald-500' }
function label(n: number) { return n <= 1 ? '弱' : n === 2 ? '一般' : n === 3 ? '强' : '很强' }

export default function RegisterPage() {
  const nav = useNavigate()
  const [form, setForm] = useState({ username: '', email: '', password: '', confirm: '' })
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState('')

  const strength = checks.filter((c) => c.test(form.password)).length
  const pwOk = strength >= 3
  const mismatch = form.confirm && form.password !== form.confirm
  const valid = form.username.length >= 3 && form.email.includes('@') && pwOk && form.password === form.confirm

  const submit = async (e: React.FormEvent) => {
    e.preventDefault(); setError('')
    if (!valid) { setError('请正确填写所有字段'); return }
    setLoading(true)
    await new Promise((r) => setTimeout(r, 800))
    nav('/login')
  }

  return (
    <div className="flex min-h-screen items-center justify-center bg-[#f3f3f3] px-4 py-12">
      <div className="w-full max-w-[400px]">
        <div className="mb-10 text-center">
          <div className="mx-auto mb-5 flex h-12 w-12 items-center justify-center rounded-2xl bg-[#DC2626] shadow-lg shadow-red-200">
            <svg className="h-6 w-6 text-white" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
              <rect x="2" y="2" width="20" height="20" rx="5"/><path d="M7 8h10M7 12h10M7 16h6"/>
            </svg>
          </div>
          <h1 className="text-[22px] font-bold tracking-tight text-[#111]">创建账户</h1>
          <p className="mt-1.5 text-[15px] text-[#666]">加入 Redis 智能学习平台</p>
        </div>

        <div className="rounded-2xl border border-[#ddd] bg-white p-7 shadow-sm">
          <form onSubmit={submit} className="space-y-4">
            <div>
              <label className={lab}>用户名</label>
              <Input placeholder="至少 3 个字符" value={form.username} onChange={(e) => setForm({ ...form, username: e.target.value })} className={inp} />
            </div>
            <div>
              <label className={lab}>邮箱</label>
              <Input type="email" placeholder="you@example.com" value={form.email} onChange={(e) => setForm({ ...form, email: e.target.value })} className={inp} />
            </div>
            <div>
              <label className={lab}>密码</label>
              <Input type="password" placeholder="设置强密码" value={form.password} onChange={(e) => setForm({ ...form, password: e.target.value })} className={inp} />
              {form.password && (
                <div className="mt-2 space-y-2">
                  <div className="flex items-center gap-2">
                    <div className="flex flex-1 gap-1">{[1,2,3,4].map((i) => <div key={i} className={`h-1 flex-1 rounded-full ${i <= strength ? barColor(strength) : 'bg-[#ddd]'}`} />)}</div>
                    <span className="text-xs text-[#888]">{label(strength)}</span>
                  </div>
                  <div className="flex flex-wrap gap-x-4 gap-y-0.5">
                    {checks.map((c) => {
                      const ok = c.test(form.password)
                      return <span key={c.label} className={`flex items-center gap-1 text-xs ${ok ? 'text-emerald-600' : 'text-[#aaa]'}`}>{ok ? <Check className="h-3 w-3" /> : <X className="h-3 w-3" />}{c.label}</span>
                    })}
                  </div>
                </div>
              )}
            </div>
            <div>
              <label className={lab}>确认密码</label>
              <Input type="password" placeholder="再次输入密码" value={form.confirm} onChange={(e) => setForm({ ...form, confirm: e.target.value })} className={inp + (mismatch ? ' !border-red-400' : '')} />
              {mismatch && <p className="mt-1 text-[13px] text-red-500">两次密码不一致</p>}
              {form.confirm && !mismatch && <p className="mt-1 flex items-center gap-1 text-[13px] text-emerald-600"><Check className="h-3 w-3" />密码匹配</p>}
            </div>

            {error && <div className="rounded-xl border border-red-100 bg-red-50 px-4 py-3 text-[14px] font-medium text-red-600">{error}</div>}

            <Button type="submit" disabled={loading || !valid}
              className="h-11 w-full gap-2 rounded-xl bg-[#DC2626] text-[15px] font-semibold text-white shadow-sm shadow-red-200 transition-all hover:bg-[#b91c1c] active:scale-[0.98] disabled:opacity-60">
              {loading && <Loader2 className="h-4 w-4 animate-spin" />}
              {loading ? '创建中...' : '创建账户'}
              {!loading && <ArrowRight className="h-4 w-4" />}
            </Button>
          </form>
        </div>

        <p className="mt-6 text-center text-[14px] text-[#888]">
          已有账户？ <Link to="/login" className="font-semibold text-[#DC2626] hover:underline">登录</Link>
        </p>
      </div>
    </div>
  )
}
