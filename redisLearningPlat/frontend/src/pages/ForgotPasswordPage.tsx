import { useState } from 'react'
import { Link } from 'react-router-dom'
import { ArrowLeft, Loader2, CheckCircle2, Mail } from 'lucide-react'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'

export default function ForgotPasswordPage() {
  const [email, setEmail] = useState('')
  const [loading, setLoading] = useState(false)
  const [sent, setSent] = useState(false)

  const submit = async (e: React.FormEvent) => {
    e.preventDefault()
    if (!email.includes('@')) return
    setLoading(true)
    await new Promise((r) => setTimeout(r, 1000))
    setSent(true)
    setLoading(false)
  }

  return (
    <div className="flex min-h-screen items-center justify-center bg-stone-50 px-4">
      <div className="w-full max-w-sm">
        <div className="mb-10 text-center">
          <div className="mx-auto mb-5 flex h-11 w-11 items-center justify-center rounded-xl bg-stone-900">
            <svg className="h-5 w-5 text-white" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round"><rect x="3" y="3" width="18" height="18" rx="4"/><path d="M7 9h10M7 13h10M7 17h6"/></svg>
          </div>
          <h1 className="text-xl font-semibold tracking-tight text-stone-900">重置密码</h1>
          <p className="mt-1.5 text-sm text-stone-500">输入邮箱接收重置链接</p>
        </div>

        <div className="rounded-2xl border border-stone-200/60 bg-white px-6 py-8 shadow-sm">
          {sent ? (
            <div className="py-4 text-center">
              <div className="mx-auto flex h-14 w-14 items-center justify-center rounded-2xl bg-emerald-50">
                <CheckCircle2 className="h-7 w-7 text-emerald-500" />
              </div>
              <h2 className="mt-4 text-base font-semibold text-stone-900">邮件已发送</h2>
              <p className="mt-2 text-[13px] leading-relaxed text-stone-500">
                如果 <span className="font-medium text-stone-700">{email}</span> 已注册，你将收到一封包含密码重置链接的邮件。
              </p>
              <Link to="/login" className="mt-5 inline-flex items-center gap-1.5 text-[13px] font-medium text-stone-700 hover:text-stone-900">
                <ArrowLeft className="h-4 w-4" />返回登录
              </Link>
            </div>
          ) : (
            <form onSubmit={submit} className="space-y-4">
              <div>
                <label className="mb-1.5 block text-[13px] font-medium text-stone-700">邮箱地址</label>
                <div className="relative">
                  <Mail className="pointer-events-none absolute left-3 top-2.5 h-4 w-4 text-stone-400" />
                  <Input type="email" placeholder="you@example.com" value={email} onChange={(e) => setEmail(e.target.value)} className="border-stone-200 bg-stone-50 pl-10" />
                </div>
              </div>
              <Button type="submit" disabled={loading || !email.includes('@')} className="h-10 w-full rounded-lg bg-stone-900 text-sm font-medium text-white hover:bg-stone-800">
                {loading && <Loader2 className="mr-2 h-4 w-4 animate-spin" />}
                {loading ? '发送中...' : '发送重置链接'}
              </Button>
            </form>
          )}
        </div>

        {!sent && (
          <p className="mt-6 text-center text-[13px] text-stone-400">
            <Link to="/login" className="inline-flex items-center gap-1 font-medium text-stone-700 hover:text-stone-900"><ArrowLeft className="h-4 w-4" />返回登录</Link>
          </p>
        )}
      </div>
    </div>
  )
}
