import { Button } from '@/components/ui/Button'
import { cn } from '@/lib/utils'

interface EmptyStateProps {
  icon: React.ElementType
  title: string
  description?: string
  action?: { label: string; onClick: () => void }
  className?: string
}

export function EmptyState({ icon: Icon, title, description, action, className }: EmptyStateProps) {
  return (
    <div
      className={cn(
        'flex flex-col items-center justify-center rounded-xl border border-dashed border-border-subtle bg-surface-1/50 px-6 py-14 text-center',
        className,
      )}
    >
      <div className="flex h-14 w-14 items-center justify-center rounded-2xl bg-surface-3">
        <Icon size={26} className="text-text-muted" />
      </div>
      <h3 className="mt-4 text-sm font-semibold text-text-primary">{title}</h3>
      {description && (
        <p className="mt-1.5 max-w-xs text-xs leading-relaxed text-text-muted">{description}</p>
      )}
      {action && (
        <Button variant="outline" size="sm" className="mt-4" onClick={action.onClick}>
          {action.label}
        </Button>
      )}
    </div>
  )
}
