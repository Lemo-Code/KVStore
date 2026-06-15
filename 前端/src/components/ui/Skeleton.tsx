import { cn } from '@/lib/utils'

interface SkeletonProps {
  className?: string
  variant?: 'text' | 'circular' | 'rectangular' | 'rounded'
  width?: string | number
  height?: string | number
  animate?: boolean
}

export function Skeleton({
  className,
  variant = 'text',
  width,
  height,
  animate = true,
}: SkeletonProps) {
  const baseStyles = 'bg-surface-3'
  const animationStyles = animate ? 'animate-pulse' : ''

  const variantStyles = {
    text: 'rounded',
    circular: 'rounded-full',
    rectangular: '',
    rounded: 'rounded-xl',
  }

  const style: React.CSSProperties = {}
  if (width) style.width = typeof width === 'number' ? `${width}px` : width
  if (height) style.height = typeof height === 'number' ? `${height}px` : height

  return (
    <div
      className={cn(baseStyles, animationStyles, variantStyles[variant], className)}
      style={style}
    />
  )
}

interface SkeletonTextProps {
  lines?: number
  className?: string
  lastLineWidth?: string | number
}

export function SkeletonText({ lines = 3, className, lastLineWidth = '60%' }: SkeletonTextProps) {
  return (
    <div className={cn('space-y-2', className)}>
      {Array.from({ length: lines }).map((_, i) => (
        <Skeleton
          key={i}
          height={12}
          width={i === lines - 1 ? lastLineWidth : '100%'}
          className="rounded"
        />
      ))}
    </div>
  )
}

interface SkeletonCardProps {
  hasImage?: boolean
  lines?: number
  className?: string
}

export function SkeletonCard({ hasImage = true, lines = 3, className }: SkeletonCardProps) {
  return (
    <div className={cn('rounded-xl border border-border-subtle bg-surface-1 p-4 space-y-4', className)}>
      {hasImage && <Skeleton height={160} variant="rounded" className="w-full" />}
      <SkeletonText lines={lines} />
      <div className="flex items-center gap-2">
        <Skeleton variant="circular" width={32} height={32} />
        <Skeleton height={12} width={100} />
      </div>
    </div>
  )
}

interface SkeletonListProps {
  items?: number
  className?: string
}

export function SkeletonList({ items = 5, className }: SkeletonListProps) {
  return (
    <div className={cn('space-y-3', className)}>
      {Array.from({ length: items }).map((_, i) => (
        <div key={i} className="flex items-center gap-3 p-3 rounded-lg border border-border-subtle">
          <Skeleton variant="circular" width={40} height={40} />
          <div className="flex-1 space-y-2">
            <Skeleton height={14} width="40%" />
            <Skeleton height={10} width="70%" />
          </div>
        </div>
      ))}
    </div>
  )
}

export function SkeletonDashboard() {
  return (
    <div className="space-y-6">
      <div className="grid grid-cols-4 gap-3">
        {Array.from({ length: 4 }).map((_, i) => (
          <div key={i} className="rounded-xl border border-border-subtle bg-surface-1 p-4">
            <Skeleton variant="circular" width={24} height={24} className="mb-3" />
            <Skeleton height={28} width={60} className="mb-1" />
            <Skeleton height={12} width={80} />
          </div>
        ))}
      </div>
      <div className="grid grid-cols-3 gap-6">
        <div className="col-span-2 space-y-4">
          <Skeleton height={200} variant="rounded" />
          <Skeleton height={300} variant="rounded" />
        </div>
        <div className="space-y-4">
          <Skeleton height={180} variant="rounded" />
          <Skeleton height={220} variant="rounded" />
        </div>
      </div>
    </div>
  )
}
