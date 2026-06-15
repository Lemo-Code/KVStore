import { cn } from '@/lib/utils'
import { AnimatePresence, motion } from 'framer-motion'
import { AlertTriangle, CheckCircle2, Info, X, XCircle } from 'lucide-react'
import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useRef,
  useState,
  type ReactNode,
} from 'react'

type ToastVariant = 'success' | 'error' | 'warning' | 'info'

interface ToastItem {
  id: string
  title: string
  description?: string
  variant: ToastVariant
  duration: number
}

interface ToastOptions {
  title: string
  description?: string
  variant?: ToastVariant
  duration?: number
}

interface ToastContextValue {
  toast: (options: ToastOptions) => string
  success: (title: string, description?: string) => string
  error: (title: string, description?: string) => string
  warning: (title: string, description?: string) => string
  info: (title: string, description?: string) => string
  dismiss: (id: string) => void
}

const ToastContext = createContext<ToastContextValue | null>(null)

const variantConfig: Record<
  ToastVariant,
  { icon: React.ElementType; accent: string; bar: string }
> = {
  success: { icon: CheckCircle2, accent: 'text-success', bar: 'bg-success' },
  error: { icon: XCircle, accent: 'text-danger', bar: 'bg-danger' },
  warning: { icon: AlertTriangle, accent: 'text-accent-amber', bar: 'bg-accent-amber' },
  info: { icon: Info, accent: 'text-accent-blue', bar: 'bg-accent-blue' },
}

export function ToastProvider({ children }: { children: ReactNode }) {
  const [toasts, setToasts] = useState<ToastItem[]>([])
  const timers = useRef<Map<string, ReturnType<typeof setTimeout>>>(new Map())

  const dismiss = useCallback((id: string) => {
    setToasts((prev) => prev.filter((t) => t.id !== id))
    const timer = timers.current.get(id)
    if (timer) {
      clearTimeout(timer)
      timers.current.delete(id)
    }
  }, [])

  const toast = useCallback(
    ({ title, description, variant = 'info', duration = 4000 }: ToastOptions) => {
      const id = `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`
      setToasts((prev) => [...prev, { id, title, description, variant, duration }])
      if (duration > 0) {
        const timer = setTimeout(() => dismiss(id), duration)
        timers.current.set(id, timer)
      }
      return id
    },
    [dismiss],
  )

  const helpers = useCallback(
    (variant: ToastVariant) => (title: string, description?: string) =>
      toast({ title, description, variant }),
    [toast],
  )

  useEffect(() => {
    const map = timers.current
    return () => {
      map.forEach((t) => clearTimeout(t))
      map.clear()
    }
  }, [])

  const value: ToastContextValue = {
    toast,
    success: helpers('success'),
    error: helpers('error'),
    warning: helpers('warning'),
    info: helpers('info'),
    dismiss,
  }

  return (
    <ToastContext.Provider value={value}>
      {children}
      <div className="pointer-events-none fixed bottom-4 right-4 z-[100] flex w-full max-w-sm flex-col gap-2">
        <AnimatePresence initial={false}>
          {toasts.map((t) => (
            <Toast key={t.id} item={t} onDismiss={() => dismiss(t.id)} />
          ))}
        </AnimatePresence>
      </div>
    </ToastContext.Provider>
  )
}

export function Toast({ item, onDismiss }: { item: ToastItem; onDismiss: () => void }) {
  const config = variantConfig[item.variant]
  const Icon = config.icon

  return (
    <motion.div
      layout
      initial={{ opacity: 0, x: 40, scale: 0.95 }}
      animate={{ opacity: 1, x: 0, scale: 1 }}
      exit={{ opacity: 0, x: 40, scale: 0.95 }}
      transition={{ type: 'spring', stiffness: 380, damping: 30 }}
      className="pointer-events-auto relative overflow-hidden rounded-xl border border-border-subtle bg-surface-2 shadow-lg shadow-black/30"
    >
      <div className="flex items-start gap-3 p-3.5">
        <Icon size={18} className={cn('mt-0.5 shrink-0', config.accent)} />
        <div className="min-w-0 flex-1">
          <div className="text-sm font-semibold text-text-primary">{item.title}</div>
          {item.description && (
            <div className="mt-0.5 text-xs leading-relaxed text-text-muted">{item.description}</div>
          )}
        </div>
        <button
          onClick={onDismiss}
          className="shrink-0 rounded-md p-1 text-text-muted hover:bg-surface-3 hover:text-text-secondary transition-colors"
        >
          <X size={14} />
        </button>
      </div>
      {item.duration > 0 && (
        <motion.div
          initial={{ scaleX: 1 }}
          animate={{ scaleX: 0 }}
          transition={{ duration: item.duration / 1000, ease: 'linear' }}
          className={cn('h-0.5 origin-left', config.bar)}
        />
      )}
    </motion.div>
  )
}

export function useToast() {
  const ctx = useContext(ToastContext)
  if (!ctx) {
    throw new Error('useToast must be used within a ToastProvider')
  }
  return ctx
}
