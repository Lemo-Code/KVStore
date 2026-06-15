import { cn } from '@/lib/utils'
import { motion } from 'framer-motion'
import type { ReactNode } from 'react'

interface CardProps {
  children: ReactNode
  className?: string
  variant?: 'default' | 'gradient' | 'bordered' | 'elevated'
  hover?: boolean
  onClick?: () => void
}

const variantStyles = {
  default: 'bg-surface-1 border border-border-subtle',
  gradient: 'bg-gradient-to-b from-surface-1 to-surface-2 border border-border-subtle',
  bordered: 'bg-surface-1 border border-border',
  elevated: 'bg-surface-1 border border-border-subtle shadow-lg shadow-surface-0/50',
}

export function Card({
  children,
  className,
  variant = 'default',
  hover = false,
  onClick,
}: CardProps) {
  return (
    <div
      onClick={onClick}
      className={cn(
        'rounded-xl overflow-hidden',
        variantStyles[variant],
        hover && 'hover:border-border hover:bg-surface-2 transition-all cursor-pointer',
        onClick && 'cursor-pointer',
        className
      )}
    >
      {children}
    </div>
  )
}

interface CardHeaderProps {
  children: ReactNode
  className?: string
  icon?: React.ReactNode
  action?: React.ReactNode
}

export function CardHeader({ children, className, icon, action }: CardHeaderProps) {
  return (
    <div className={cn('flex items-center justify-between p-4 border-b border-border-subtle', className)}>
      <div className="flex items-center gap-2.5">
        {icon && <div className="text-accent-red">{icon}</div>}
        <h3 className="text-sm font-semibold">{children}</h3>
      </div>
      {action}
    </div>
  )
}

interface CardContentProps {
  children: ReactNode
  className?: string
}

export function CardContent({ children, className }: CardContentProps) {
  return <div className={cn('p-4', className)}>{children}</div>
}

interface CardFooterProps {
  children: ReactNode
  className?: string
  align?: 'left' | 'center' | 'right'
}

export function CardFooter({ children, className, align = 'left' }: CardFooterProps) {
  const alignClasses = {
    left: 'justify-start',
    center: 'justify-center',
    right: 'justify-end',
  }

  return (
    <div className={cn('flex items-center gap-2 p-4 border-t border-border-subtle', alignClasses[align], className)}>
      {children}
    </div>
  )
}

interface StatCardProps {
  value: string | number
  label: string
  icon: React.ElementType
  color?: 'red' | 'amber' | 'teal' | 'purple' | 'blue'
  trend?: { value: number; positive: boolean }
  className?: string
}

const colorMap = {
  red: { icon: 'text-accent-red', bg: 'bg-accent-red/10', border: 'border-accent-red/20' },
  amber: { icon: 'text-accent-amber', bg: 'bg-accent-amber/10', border: 'border-accent-amber/20' },
  teal: { icon: 'text-accent-teal', bg: 'bg-accent-teal/10', border: 'border-accent-teal/20' },
  purple: { icon: 'text-accent-purple', bg: 'bg-accent-purple/10', border: 'border-accent-amber/20' },
  blue: { icon: 'text-accent-blue', bg: 'bg-accent-blue/10', border: 'border-accent-blue/20' },
}

export function StatCard({
  value,
  label,
  icon: Icon,
  color = 'red',
  trend,
  className,
}: StatCardProps) {
  const colors = colorMap[color]

  return (
    <motion.div
      whileHover={{ y: -2 }}
      className={cn(
        'rounded-xl border bg-surface-1 p-4 transition-all hover:border-border',
        colors.border,
        className
      )}
    >
      <div className="flex items-center justify-between mb-3">
        <div className={cn('flex h-10 w-10 items-center justify-center rounded-lg', colors.bg)}>
          <Icon size={18} className={colors.icon} />
        </div>
        {trend && (
          <span
            className={cn(
              'text-[11px] font-medium',
              trend.positive ? 'text-success' : 'text-danger'
            )}
          >
            {trend.positive ? '+' : ''}{trend.value}%
          </span>
        )}
      </div>
      <div className="text-2xl font-bold">{value}</div>
      <div className="text-[11px] text-text-muted mt-0.5">{label}</div>
    </motion.div>
  )
}

interface InfoCardProps {
  title: string
  description?: string
  icon: React.ElementType
  action?: { label: string; onClick: () => void }
  color?: 'red' | 'amber' | 'teal' | 'purple' | 'blue'
  className?: string
}

export function InfoCard({
  title,
  description,
  icon: Icon,
  action,
  color = 'purple',
  className,
}: InfoCardProps) {
  const colors = colorMap[color]

  return (
    <div className={cn('rounded-xl border p-4', colors.border, colors.bg.replace('/10', '/5'), className)}>
      <div className="flex items-start gap-3">
        <div className={cn('flex h-10 w-10 shrink-0 items-center justify-center rounded-lg', colors.bg)}>
          <Icon size={18} className={colors.icon} />
        </div>
        <div className="flex-1 min-w-0">
          <h4 className={cn('text-sm font-semibold', colors.icon)}>{title}</h4>
          {description && (
            <p className="text-xs text-text-secondary mt-1 leading-relaxed">{description}</p>
          )}
          {action && (
            <button
              onClick={action.onClick}
              className={cn('text-xs mt-2 hover:underline', colors.icon)}
            >
              {action.label} →
            </button>
          )}
        </div>
      </div>
    </div>
  )
}
