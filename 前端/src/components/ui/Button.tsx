import { cn } from '@/lib/utils'
import type { ButtonHTMLAttributes } from 'react'

interface ButtonProps extends ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: 'default' | 'ghost' | 'outline' | 'danger' | 'accent'
  size?: 'sm' | 'md' | 'icon'
}

const variants = {
  default: 'bg-surface-3 hover:bg-surface-hover text-text-primary border border-border',
  ghost: 'hover:bg-surface-3 text-text-secondary hover:text-text-primary',
  outline: 'border border-border hover:border-accent-red/50 text-text-secondary hover:text-text-primary',
  danger: 'bg-danger/10 hover:bg-danger/20 text-danger border border-danger/20',
  accent: 'bg-accent-red hover:bg-accent-red-dim text-white shadow-lg shadow-accent-red/20',
}

const sizes = {
  sm: 'h-7 px-2.5 text-xs gap-1.5',
  md: 'h-8 px-3 text-sm gap-2',
  icon: 'h-7 w-7',
}

export function Button({
  className,
  variant = 'default',
  size = 'md',
  children,
  ...props
}: ButtonProps) {
  return (
    <button
      className={cn(
        'inline-flex items-center justify-center rounded-md font-medium transition-all duration-150',
        'disabled:opacity-40 disabled:pointer-events-none',
        'focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent-red/40',
        variants[variant],
        sizes[size],
        className,
      )}
      {...props}
    >
      {children}
    </button>
  )
}
