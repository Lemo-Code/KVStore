import { useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { Button } from '@/components/ui/Button'
import { useAuthStore } from '@/stores/authStore'
import { motion, AnimatePresence } from 'framer-motion'
import {
  ArrowRight,
  BookOpen,
  Bot,
  Check,
  Database,
  MessageCircle,
  Sparkles,
  Target,
} from 'lucide-react'
import { cn } from '@/lib/utils'

const steps = [
  {
    id: 'goal',
    title: '你的学习目标',
    subtitle: '帮助我们为你推荐合适的课程',
    options: [
      { id: 'beginner', label: '零基础入门', desc: '从未接触过 Redis', icon: BookOpen },
      { id: 'interview', label: '面试备战', desc: '准备技术面试', icon: Target },
      { id: 'production', label: '生产实践', desc: '在工作中使用 Redis', icon: Database },
      { id: 'advanced', label: '进阶深造', desc: '集群、性能优化', icon: Sparkles },
    ],
  },
  {
    id: 'interest',
    title: '感兴趣的方向',
    subtitle: '可多选，随时可在设置中修改',
    options: [
      { id: 'data-structure', label: '数据结构', desc: 'String/Hash/List/Set/ZSet', icon: Database },
      { id: 'cache', label: '缓存设计', desc: '缓存策略与最佳实践', icon: Target },
      { id: 'ai', label: 'AI 辅助学习', desc: '智能答疑与代码分析', icon: Bot },
      { id: 'collab', label: '协作学习', desc: '与同学一起讨论', icon: MessageCircle },
    ],
  },
  {
    id: 'env',
    title: 'Redis 环境',
    subtitle: '选择你的学习环境',
    options: [
      { id: 'sandbox', label: '平台沙箱', desc: '免配置，开箱即用', icon: Sparkles },
      { id: 'local', label: '本地 Redis', desc: '连接本机 6379 端口', icon: Database },
      { id: 'cloud', label: '云 Redis', desc: '连接阿里云/腾讯云等', icon: Target },
      { id: 'later', label: '稍后再说', desc: '先浏览课程内容', icon: BookOpen },
    ],
  },
]

export default function OnboardingPage() {
  const navigate = useNavigate()
  const completeOnboarding = useAuthStore((s) => s.completeOnboarding)
  const [step, setStep] = useState(0)
  const [selections, setSelections] = useState<Record<string, string[]>>({
    goal: [],
    interest: [],
    env: [],
  })

  const current = steps[step]
  const isMulti = current.id !== 'goal'

  const toggle = (optionId: string) => {
    setSelections((prev) => {
      const stepKey = current.id
      const selected = prev[stepKey] ?? []
      if (isMulti) {
        return {
          ...prev,
          [stepKey]: selected.includes(optionId)
            ? selected.filter((id: string) => id !== optionId)
            : [...selected, optionId],
        }
      }
      return { ...prev, [stepKey]: [optionId] }
    })
  }

  const canNext = (selections[current.id]?.length ?? 0) > 0

  const handleNext = () => {
    if (step < steps.length - 1) {
      setStep(step + 1)
    } else {
      completeOnboarding()
      navigate('/dashboard')
    }
  }

  return (
    <div className="flex h-full flex-col bg-surface-0">
      {/* Progress */}
      <div className="flex items-center justify-between px-8 py-5 border-b border-border-subtle">
        <div className="flex items-center gap-2">
          <div className="flex h-8 w-8 items-center justify-center rounded-lg bg-gradient-to-br from-accent-red to-accent-red-dim">
            <Sparkles size={14} className="text-white" />
          </div>
          <span className="font-bold text-sm">Redis Lab Studio</span>
        </div>
        <div className="flex items-center gap-2">
          {steps.map((_, i) => (
            <div
              key={i}
              className={cn(
                'h-1.5 rounded-full transition-all duration-300',
                i === step ? 'w-8 bg-accent-red' : i < step ? 'w-4 bg-accent-red/50' : 'w-4 bg-surface-4',
              )}
            />
          ))}
          <span className="text-xs text-text-muted ml-2">
            {step + 1} / {steps.length}
          </span>
        </div>
      </div>

      {/* Content */}
      <div className="flex-1 flex items-center justify-center p-8 overflow-y-auto">
        <div className="w-full max-w-2xl">
          <AnimatePresence mode="wait">
            <motion.div
              key={step}
              initial={{ opacity: 0, x: 30 }}
              animate={{ opacity: 1, x: 0 }}
              exit={{ opacity: 0, x: -30 }}
              transition={{ duration: 0.25 }}
            >
              <h1 className="text-2xl font-bold mb-1">{current.title}</h1>
              <p className="text-sm text-text-muted mb-8">{current.subtitle}</p>

              <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
                {current.options.map((opt) => {
                  const selected = selections[current.id]?.includes(opt.id)
                  const Icon = opt.icon
                  return (
                    <button
                      key={opt.id}
                      onClick={() => toggle(opt.id)}
                      className={cn(
                        'relative flex items-start gap-3 rounded-xl border p-4 text-left transition-all',
                        selected
                          ? 'border-accent-red/40 bg-accent-red/8 shadow-lg shadow-accent-red/5'
                          : 'border-border-subtle bg-surface-1 hover:border-border hover:bg-surface-2',
                      )}
                    >
                      <div
                        className={cn(
                          'flex h-10 w-10 shrink-0 items-center justify-center rounded-lg',
                          selected ? 'bg-accent-red/15' : 'bg-surface-3',
                        )}
                      >
                        <Icon size={18} className={selected ? 'text-accent-red' : 'text-text-muted'} />
                      </div>
                      <div>
                        <div className="text-sm font-semibold">{opt.label}</div>
                        <div className="text-xs text-text-muted mt-0.5">{opt.desc}</div>
                      </div>
                      {selected && (
                        <div className="absolute top-3 right-3 flex h-5 w-5 items-center justify-center rounded-full bg-accent-red">
                          <Check size={12} className="text-white" />
                        </div>
                      )}
                    </button>
                  )
                })}
              </div>
            </motion.div>
          </AnimatePresence>
        </div>
      </div>

      {/* Footer */}
      <div className="flex items-center justify-between px-8 py-5 border-t border-border-subtle">
        <button
          onClick={() => (step > 0 ? setStep(step - 1) : navigate('/login'))}
          className="text-sm text-text-muted hover:text-text-secondary transition-colors"
        >
          {step > 0 ? '上一步' : '返回登录'}
        </button>
        <Button variant="accent" onClick={handleNext} disabled={!canNext} className="h-10 px-6">
          {step < steps.length - 1 ? (
            <>
              下一步
              <ArrowRight size={16} />
            </>
          ) : (
            '进入平台'
          )}
        </Button>
      </div>
    </div>
  )
}
