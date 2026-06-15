import { cn } from '@/lib/utils'

interface BadgeProps {
  children: React.ReactNode
  variant?: 'default' | 'type' | 'success' | 'warning' | 'purple'
  className?: string
}

const variants = {
  default: 'bg-surface-4 text-text-secondary',
  type: 'bg-accent-red/15 text-accent-red border border-accent-red/20',
  success: 'bg-success/15 text-success border border-success/20',
  warning: 'bg-warning/15 text-warning border border-warning/20',
  purple: 'bg-accent-purple/15 text-accent-purple border border-accent-purple/20',
}

export function Badge({ children, variant = 'default', className }: BadgeProps) {
  return (
    <span
      className={cn(
        'inline-flex items-center rounded px-1.5 py-0.5 text-[10px] font-semibold uppercase tracking-wider',
        variants[variant],
        className,
      )}
    >
      {children}
    </span>
  )
}
