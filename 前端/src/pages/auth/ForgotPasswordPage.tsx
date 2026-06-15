import { useState } from 'react'
import { Link } from 'react-router-dom'
import { AuthLayout } from '@/components/auth/AuthLayout'
import { Button } from '@/components/ui/Button'
import { Input } from '@/components/ui/Input'
import { motion } from 'framer-motion'
import { ArrowLeft, CheckCircle2, Loader2, Mail } from 'lucide-react'

export default function ForgotPasswordPage() {
  const [email, setEmail] = useState('')
  const [sent, setSent] = useState(false)
  const [loading, setLoading] = useState(false)

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault()
    if (!email) return
    setLoading(true)
    await new Promise((r) => setTimeout(r, 1000))
    setSent(true)
    setLoading(false)
  }

  return (
    <AuthLayout title="找回密码" subtitle="我们将向你的邮箱发送重置链接">
      {!sent ? (
        <form onSubmit={handleSubmit} className="space-y-4">
          <Input
            label="注册邮箱"
            type="email"
            placeholder="you@example.com"
            value={email}
            onChange={(e) => setEmail(e.target.value)}
            leftIcon={<Mail size={16} />}
            hint="输入你注册时使用的邮箱地址"
          />
          <Button type="submit" variant="accent" className="w-full h-11" disabled={loading || !email}>
            {loading ? <Loader2 size={16} className="animate-spin" /> : null}
            发送重置链接
          </Button>
        </form>
      ) : (
        <motion.div
          initial={{ opacity: 0, scale: 0.95 }}
          animate={{ opacity: 1, scale: 1 }}
          className="text-center py-6"
        >
          <div className="flex justify-center mb-4">
            <div className="flex h-16 w-16 items-center justify-center rounded-full bg-success/15">
              <CheckCircle2 size={32} className="text-success" />
            </div>
          </div>
          <h3 className="text-lg font-semibold mb-2">邮件已发送</h3>
          <p className="text-sm text-text-muted leading-relaxed">
            重置链接已发送至 <span className="text-text-primary font-medium">{email}</span>
            <br />
            请查收邮件并点击链接重置密码，链接 30 分钟内有效。
          </p>
          <Button variant="outline" className="mt-6" onClick={() => setSent(false)}>
            重新发送
          </Button>
        </motion.div>
      )}

      <Link
        to="/login"
        className="mt-8 flex items-center justify-center gap-1.5 text-xs text-text-muted hover:text-text-secondary transition-colors"
      >
        <ArrowLeft size={14} />
        返回登录
      </Link>
    </AuthLayout>
  )
}
