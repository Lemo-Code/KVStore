import { cn } from '@/lib/utils'
import type { InputHTMLAttributes } from 'react'
import { forwardRef } from 'react'

interface InputProps extends InputHTMLAttributes<HTMLInputElement> {
  label?: string
  error?: string
  hint?: string
  leftIcon?: React.ReactNode
  rightIcon?: React.ReactNode
}

export const Input = forwardRef<HTMLInputElement, InputProps>(
  ({ className, label, error, hint, leftIcon, rightIcon, id, ...props }, ref) => {
    const inputId = id ?? label?.toLowerCase().replace(/\s/g, '-')

    return (
      <div className="space-y-1.5">
        {label && (
          <label htmlFor={inputId} className="block text-xs font-medium text-text-secondary">
            {label}
          </label>
        )}
        <div className="relative">
          {leftIcon && (
            <div className="absolute left-3 top-1/2 -translate-y-1/2 text-text-muted">
              {leftIcon}
            </div>
          )}
          <input
            ref={ref}
            id={inputId}
            className={cn(
              'w-full rounded-lg border bg-surface-0 px-3 py-2.5 text-sm text-text-primary',
              'placeholder:text-text-muted transition-all duration-150',
              'focus:outline-none focus:ring-2 focus:ring-accent-red/25 focus:border-accent-red/50',
              error ? 'border-danger/50' : 'border-border-subtle hover:border-border',
              leftIcon && 'pl-10',
              rightIcon && 'pr-10',
              className,
            )}
            {...props}
          />
          {rightIcon && (
            <div className="absolute right-3 top-1/2 -translate-y-1/2 text-text-muted">
              {rightIcon}
            </div>
          )}
        </div>
        {error && <p className="text-[11px] text-danger">{error}</p>}
        {hint && !error && <p className="text-[11px] text-text-muted">{hint}</p>}
      </div>
    )
  },
)
Input.displayName = 'Input'

interface CheckboxProps {
  label: React.ReactNode
  checked?: boolean
  onChange?: (checked: boolean) => void
  className?: string
}

export function Checkbox({ label, checked, onChange, className }: CheckboxProps) {
  return (
    <label className={cn('flex items-start gap-2.5 cursor-pointer group', className)}>
      <input
        type="checkbox"
        checked={checked}
        onChange={(e) => onChange?.(e.target.checked)}
        className="mt-0.5 h-4 w-4 rounded border-border bg-surface-0 text-accent-red focus:ring-accent-red/30 focus:ring-offset-0"
      />
      <span className="text-xs text-text-secondary group-hover:text-text-primary transition-colors leading-relaxed">
        {label}
      </span>
    </label>
  )
}
