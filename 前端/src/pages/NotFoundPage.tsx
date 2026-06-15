import { Button } from '@/components/ui/Button'
import { Link } from 'react-router-dom'
import { Home, Search, Sparkles } from 'lucide-react'

export default function NotFoundPage() {
  return (
    <div className="flex h-full items-center justify-center bg-surface-0 p-6">
      <div className="text-center max-w-md">
        <div className="flex justify-center mb-6">
          <div className="relative">
            <div className="text-8xl font-black text-surface-4 select-none">404</div>
            <Sparkles size={24} className="absolute -top-2 -right-4 text-accent-red" />
          </div>
        </div>
        <h1 className="text-2xl font-bold mb-2">页面不存在</h1>
        <p className="text-sm text-text-muted mb-8 leading-relaxed">
          你访问的页面可能已被移除，或地址输入有误。试试全局搜索找到你想要的内容。
        </p>
        <div className="flex items-center justify-center gap-3">
          <Link to="/dashboard">
            <Button variant="accent">
              <Home size={16} />
              返回首页
            </Button>
          </Link>
          <Link to="/help">
            <Button variant="outline">
              <Search size={16} />
              帮助中心
            </Button>
          </Link>
        </div>
      </div>
    </div>
  )
}
