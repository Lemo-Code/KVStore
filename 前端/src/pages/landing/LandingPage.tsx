import { Link } from 'react-router-dom'
import { Button } from '@/components/ui/Button'
import { motion } from 'framer-motion'
import {
  ArrowRight,
  Bot,
  Database,
  MessageCircle,
  Play,
  Sparkles,
  Star,
  Users,
  Zap,
} from 'lucide-react'

const features = [
  {
    icon: Database,
    title: '专业 Redis 管理',
    desc: '连接树浏览、多类型数据查看、Monaco 命令控制台，媲美 Navicat 体验',
    gradient: 'from-accent-red/20 to-accent-red/5',
    iconColor: 'text-accent-red',
  },
  {
    icon: Bot,
    title: 'AI 智能导师',
    desc: '基于当前键和命令历史的上下文感知分析，个性化学习路径与实时答疑',
    gradient: 'from-accent-purple/20 to-accent-purple/5',
    iconColor: 'text-accent-purple',
  },
  {
    icon: MessageCircle,
    title: '实时协作学习',
    desc: '学习室群聊、代码共享、导师在线答疑，与同学一起攻克 Redis 难题',
    gradient: 'from-accent-teal/20 to-accent-teal/5',
    iconColor: 'text-accent-teal',
  },
]

const testimonials = [
  { name: '张明', role: '后端工程师', text: 'AI 导师帮我快速理解了 ZSet 排行榜的设计思路' },
  { name: '李雪', role: '计算机专业学生', text: '协作室讨论氛围很好，比独自看书效率高很多' },
  { name: '王浩', role: '全栈开发者', text: '可视化界面让 Redis 数据结构一目了然' },
]

export default function LandingPage() {
  return (
    <div className="h-full overflow-y-auto bg-surface-0">
      {/* Nav */}
      <nav className="sticky top-0 z-50 border-b border-border-subtle bg-surface-0/80 backdrop-blur-xl">
        <div className="max-w-6xl mx-auto flex items-center justify-between px-6 h-14">
          <div className="flex items-center gap-2.5">
            <div className="flex h-8 w-8 items-center justify-center rounded-lg bg-gradient-to-br from-accent-red to-accent-red-dim shadow-md shadow-accent-red/25">
              <Sparkles size={16} className="text-white" />
            </div>
            <span className="font-bold">
              Redis Lab <span className="text-accent-red">Studio</span>
            </span>
          </div>
          <div className="flex items-center gap-3">
            <Link to="/login" className="text-sm text-text-secondary hover:text-text-primary transition-colors">
              登录
            </Link>
            <Link to="/register">
              <Button variant="accent" size="sm">
                免费注册
                <ArrowRight size={14} />
              </Button>
            </Link>
          </div>
        </div>
      </nav>

      {/* Hero */}
      <section className="relative overflow-hidden">
        <div className="absolute inset-0 bg-gradient-to-b from-accent-red/5 via-transparent to-transparent" />
        <div className="absolute top-20 left-1/2 -translate-x-1/2 w-[600px] h-[600px] bg-accent-purple/5 rounded-full blur-3xl" />

        <div className="relative max-w-6xl mx-auto px-6 pt-20 pb-24 text-center">
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }}>
            <div className="inline-flex items-center gap-2 rounded-full border border-accent-red/20 bg-accent-red/5 px-3 py-1 text-xs text-accent-red mb-6">
              <Zap size={12} />
              AI + Redis + 即时通讯 一体化学习平台
            </div>
            <h1 className="text-4xl sm:text-5xl lg:text-6xl font-bold tracking-tight leading-tight">
              学 Redis，
              <br />
              <span className="bg-gradient-to-r from-accent-red via-accent-amber to-accent-purple bg-clip-text text-transparent">
                从未如此智能
              </span>
            </h1>
            <p className="mt-6 text-lg text-text-muted max-w-2xl mx-auto leading-relaxed">
              可视化数据库管理、AI 导师实时答疑、协作室一起学习 ——
              三位一体的 Redis 学习体验，从零基础到生产实践。
            </p>
            <div className="mt-8 flex items-center justify-center gap-3">
              <Link to="/register">
                <Button variant="accent" className="h-12 px-8 text-base">
                  <Play size={18} />
                  开始学习
                </Button>
              </Link>
              <Link to="/login">
                <Button variant="outline" className="h-12 px-8 text-base">
                  登录账号
                </Button>
              </Link>
            </div>
            <div className="mt-6 flex items-center justify-center gap-6 text-xs text-text-muted">
              <span className="flex items-center gap-1"><Users size={12} /> 12,480+ 学员</span>
              <span className="flex items-center gap-1"><Star size={12} className="text-accent-amber" /> 4.9 评分</span>
              <span className="flex items-center gap-1"><Bot size={12} /> AI 24/7 在线</span>
            </div>
          </motion.div>
        </div>
      </section>

      {/* Features */}
      <section className="max-w-6xl mx-auto px-6 py-16">
        <div className="text-center mb-12">
          <h2 className="text-2xl font-bold">三大核心能力</h2>
          <p className="text-sm text-text-muted mt-2">数据库管理 · AI 辅导 · 实时协作，一个平台全部搞定</p>
        </div>
        <div className="grid md:grid-cols-3 gap-6">
          {features.map((f, i) => (
            <motion.div
              key={f.title}
              initial={{ opacity: 0, y: 20 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true }}
              transition={{ delay: i * 0.1 }}
              className={`rounded-2xl border border-border-subtle bg-gradient-to-b ${f.gradient} p-6`}
            >
              <div className="flex h-12 w-12 items-center justify-center rounded-xl bg-surface-2 border border-border-subtle mb-4">
                <f.icon size={22} className={f.iconColor} />
              </div>
              <h3 className="text-lg font-semibold mb-2">{f.title}</h3>
              <p className="text-sm text-text-muted leading-relaxed">{f.desc}</p>
            </motion.div>
          ))}
        </div>
      </section>

      {/* Testimonials */}
      <section className="border-t border-border-subtle bg-surface-1 py-16">
        <div className="max-w-6xl mx-auto px-6">
          <h2 className="text-2xl font-bold text-center mb-10">学员评价</h2>
          <div className="grid md:grid-cols-3 gap-4">
            {testimonials.map((t) => (
              <div key={t.name} className="rounded-xl border border-border-subtle bg-surface-2 p-5">
                <p className="text-sm text-text-secondary leading-relaxed mb-4">"{t.text}"</p>
                <div className="flex items-center gap-2">
                  <div className="h-8 w-8 rounded-full bg-accent-red/15 flex items-center justify-center text-xs font-bold text-accent-red">
                    {t.name[0]}
                  </div>
                  <div>
                    <div className="text-xs font-semibold">{t.name}</div>
                    <div className="text-[10px] text-text-muted">{t.role}</div>
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      </section>

      {/* CTA */}
      <section className="max-w-6xl mx-auto px-6 py-20 text-center">
        <h2 className="text-3xl font-bold mb-4">准备好开始了吗？</h2>
        <p className="text-text-muted mb-8">免费注册，立即获得 Redis 沙箱环境与 AI 导师</p>
        <Link to="/register">
          <Button variant="accent" className="h-12 px-10 text-base">
            免费注册
            <ArrowRight size={18} />
          </Button>
        </Link>
      </section>

      {/* Footer */}
      <footer className="border-t border-border-subtle py-8 text-center text-xs text-text-muted">
        © 2025 Redis Lab Studio. AI + Redis + 即时通讯学习平台.
      </footer>
    </div>
  )
}
