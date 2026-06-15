import { useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { AuthLayout } from '@/components/auth/AuthLayout'
import { Button } from '@/components/ui/Button'
import { useAuthStore } from '@/stores/authStore'
import { motion } from 'framer-motion'
import { Loader2, Mail, RefreshCw } from 'lucide-react'

export default function VerifyEmailPage() {
  const navigate = useNavigate()
  const user = useAuthStore((s) => s.user)
  const [code, setCode] = useState(['', '', '', '', '', ''])
  const [loading, setLoading] = useState(false)
  const [countdown, setCountdown] = useState(60)

  const handleCodeChange = (index: number, value: string) => {
    if (!/^\d*$/.test(value)) return
    const next = [...code]
    next[index] = value.slice(-1)
    setCode(next)
    if (value && index < 5) {
      document.getElementById(`code-${index + 1}`)?.focus()
    }
  }

  const handleVerify = async () => {
    setLoading(true)
    await new Promise((r) => setTimeout(r, 1000))
    setLoading(false)
    navigate('/onboarding')
  }

  const handleResend = () => {
    setCountdown(60)
    const timer = setInterval(() => {
      setCountdown((c) => {
        if (c <= 1) { clearInterval(timer); return 0 }
        return c - 1
      })
    }, 1000)
  }

  return (
    <AuthLayout title="验证邮箱" subtitle="我们已向你的邮箱发送了 6 位验证码">
      <div className="flex justify-center mb-6">
        <div className="flex h-14 w-14 items-center justify-center rounded-2xl bg-accent-purple/15">
          <Mail size={28} className="text-accent-purple" />
        </div>
      </div>

      <p className="text-center text-sm text-text-muted mb-6">
        验证码已发送至{' '}
        <span className="text-text-primary font-medium">{user?.email ?? 'your@email.com'}</span>
      </p>

      <div className="flex justify-center gap-2 mb-6">
        {code.map((digit, i) => (
          <motion.input
            key={i}
            id={`code-${i}`}
            initial={{ opacity: 0, y: 10 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ delay: i * 0.05 }}
            type="text"
            inputMode="numeric"
            maxLength={1}
            value={digit}
            onChange={(e) => handleCodeChange(i, e.target.value)}
            className="w-11 h-13 text-center text-lg font-mono font-bold rounded-lg border border-border-subtle bg-surface-0 focus:border-accent-purple/50 focus:ring-2 focus:ring-accent-purple/20 focus:outline-none transition-all"
          />
        ))}
      </div>

      <Button
        variant="accent"
        className="w-full h-11 !bg-accent-purple hover:!bg-accent-purple/80 shadow-accent-purple/20"
        disabled={loading || code.some((d) => !d)}
        onClick={handleVerify}
      >
        {loading ? <Loader2 size={16} className="animate-spin" /> : null}
        验证并继续
      </Button>

      <div className="mt-4 text-center">
        <button
          onClick={handleResend}
          disabled={countdown > 0}
          className="inline-flex items-center gap-1.5 text-xs text-text-muted hover:text-text-secondary disabled:opacity-50 transition-colors"
        >
          <RefreshCw size={12} />
          {countdown > 0 ? `${countdown}s 后可重新发送` : '重新发送验证码'}
        </button>
      </div>
    </AuthLayout>
  )
}
