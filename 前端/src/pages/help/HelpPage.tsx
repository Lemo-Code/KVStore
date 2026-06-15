import { PageContent, PageHeader } from '@/components/layout/MainLayout'
import { Badge } from '@/components/ui/Badge'
import { cn } from '@/lib/utils'
import { motion } from 'framer-motion'
import { useState } from 'react'
import {
  ChevronRight,
  Command,
  HelpCircle,
  Keyboard,
  Search,
  Zap,
} from 'lucide-react'

const sections = [
  { id: 'quickstart', label: '快速入门', icon: Zap },
  { id: 'commands', label: '命令速查', icon: Command },
  { id: 'shortcuts', label: '快捷键', icon: Keyboard },
  { id: 'faq', label: '常见问题', icon: HelpCircle },
]

const commands = [
  { cmd: 'SET key value', desc: '设置字符串值', type: 'String' },
  { cmd: 'GET key', desc: '获取字符串值', type: 'String' },
  { cmd: 'HSET key field value', desc: '设置 Hash 字段', type: 'Hash' },
  { cmd: 'HGETALL key', desc: '获取 Hash 所有字段', type: 'Hash' },
  { cmd: 'LPUSH key value', desc: '左侧插入列表元素', type: 'List' },
  { cmd: 'LRANGE key 0 -1', desc: '获取列表范围', type: 'List' },
  { cmd: 'SADD key member', desc: '添加 Set 成员', type: 'Set' },
  { cmd: 'SMEMBERS key', desc: '获取 Set 所有成员', type: 'Set' },
  { cmd: 'ZADD key score member', desc: '添加有序集合成员', type: 'ZSet' },
  { cmd: 'ZREVRANGE key 0 9 WITHSCORES', desc: '获取 Top N 排名', type: 'ZSet' },
  { cmd: 'SCAN cursor MATCH pattern', desc: '安全遍历键（推荐）', type: 'Key' },
  { cmd: 'TTL key', desc: '查看键剩余过期时间', type: 'Key' },
  { cmd: 'DEL key', desc: '删除键', type: 'Key' },
  { cmd: 'EXPIRE key seconds', desc: '设置过期时间', type: 'Key' },
]

const shortcuts = [
  { keys: ['⌘', 'K'], action: '打开全局搜索' },
  { keys: ['⌘', 'Enter'], action: '运行 Redis 命令（工作台）' },
  { keys: ['⌘', 'B'], action: '切换左侧连接树' },
  { keys: ['⌘', 'J'], action: '聚焦 AI 导师输入框' },
  { keys: ['Esc'], action: '关闭弹窗 / 命令面板' },
  { keys: ['⌘', '/'], action: '打开帮助中心' },
]

const faqs = [
  { q: '如何连接本地 Redis？', a: '进入「连接管理」，点击「新建连接」，填写 127.0.0.1:6379，测试连接后保存。' },
  { q: 'AI 导师能访问我的 Redis 数据吗？', a: 'AI 导师基于你当前选中的键和命令历史提供上下文分析，不会上传完整数据库。' },
  { q: '协作室和 AI 导师有什么区别？', a: '协作室是与同学/导师的实时群聊；AI 导师是 24/7 智能答疑，适合自主学习和调试。' },
  { q: '练习场的命令如何验证？', a: '命令类练习会检查命令语法和关键字；设计/调试类练习支持自我评估或咨询 AI。' },
  { q: '平台沙箱和本地 Redis 有何不同？', a: '沙箱免配置、隔离环境，适合学习；本地 Redis 可连接真实数据，适合生产实践。' },
  { q: '如何导出命令历史？', a: '在工作台命令控制台输出区，点击下载按钮即可导出为文本文件。' },
]

export default function HelpPage() {
  const [active, setActive] = useState('quickstart')
  const [cmdSearch, setCmdSearch] = useState('')

  const filteredCommands = commands.filter(
    (c) => !cmdSearch || c.cmd.toLowerCase().includes(cmdSearch.toLowerCase()) || c.desc.includes(cmdSearch),
  )

  return (
    <>
      <PageHeader title="帮助中心" subtitle="快速入门、命令速查、快捷键与常见问题" />
      <PageContent className="!p-0">
        <div className="flex h-full min-h-[600px]">
          <aside className="w-52 shrink-0 border-r border-border-subtle bg-surface-1 p-3">
            {sections.map((s) => (
              <button
                key={s.id}
                onClick={() => setActive(s.id)}
                className={cn(
                  'w-full flex items-center gap-2.5 rounded-lg px-3 py-2 text-xs font-medium transition-all mb-0.5',
                  active === s.id ? 'bg-accent-red/10 text-accent-red' : 'text-text-secondary hover:bg-surface-2',
                )}
              >
                <s.icon size={14} />
                {s.label}
              </button>
            ))}
          </aside>

          <div className="flex-1 overflow-y-auto p-8">
            {active === 'quickstart' && (
              <motion.div initial={{ opacity: 0 }} animate={{ opacity: 1 }}>
                <h2 className="text-xl font-bold mb-4">快速入门</h2>
                <div className="space-y-4">
                  {[
                    { step: 1, title: '注册并完成引导', desc: '选择学习目标、兴趣方向和 Redis 环境' },
                    { step: 2, title: '连接 Redis', desc: '使用平台沙箱或添加本地连接' },
                    { step: 3, title: '浏览键与执行命令', desc: '在工作台左侧浏览键，中间执行 Redis 命令' },
                    { step: 4, title: '学习 + 练习', desc: '跟随课程学习，在练习场动手实践' },
                    { step: 5, title: '协作与 AI', desc: '加入学习室讨论，随时向 AI 导师提问' },
                  ].map((item) => (
                    <div key={item.step} className="flex gap-4 rounded-xl border border-border-subtle bg-surface-1 p-4">
                      <div className="flex h-8 w-8 shrink-0 items-center justify-center rounded-full bg-accent-red/15 text-sm font-bold text-accent-red">
                        {item.step}
                      </div>
                      <div>
                        <h3 className="text-sm font-semibold">{item.title}</h3>
                        <p className="text-xs text-text-muted mt-0.5">{item.desc}</p>
                      </div>
                    </div>
                  ))}
                </div>
              </motion.div>
            )}

            {active === 'commands' && (
              <motion.div initial={{ opacity: 0 }} animate={{ opacity: 1 }}>
                <h2 className="text-xl font-bold mb-4">命令速查</h2>
                <div className="relative mb-4 max-w-sm">
                  <Search size={14} className="absolute left-3 top-1/2 -translate-y-1/2 text-text-muted" />
                  <input
                    value={cmdSearch}
                    onChange={(e) => setCmdSearch(e.target.value)}
                    placeholder="搜索命令..."
                    className="w-full rounded-lg border border-border-subtle bg-surface-0 py-2 pl-9 pr-3 text-sm focus:outline-none focus:border-accent-red/40"
                  />
                </div>
                <div className="rounded-xl border border-border-subtle overflow-hidden">
                  <table className="w-full text-xs">
                    <thead>
                      <tr className="bg-surface-2 border-b border-border-subtle text-left">
                        <th className="px-4 py-2.5 font-semibold text-text-muted">命令</th>
                        <th className="px-4 py-2.5 font-semibold text-text-muted">说明</th>
                        <th className="px-4 py-2.5 font-semibold text-text-muted w-20">类型</th>
                      </tr>
                    </thead>
                    <tbody>
                      {filteredCommands.map((c, i) => (
                        <tr key={c.cmd} className={cn('border-b border-border-subtle/50', i % 2 === 0 ? 'bg-surface-1' : 'bg-surface-2/30')}>
                          <td className="px-4 py-2 font-mono text-accent-teal">{c.cmd}</td>
                          <td className="px-4 py-2 text-text-secondary">{c.desc}</td>
                          <td className="px-4 py-2"><Badge>{c.type}</Badge></td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              </motion.div>
            )}

            {active === 'shortcuts' && (
              <motion.div initial={{ opacity: 0 }} animate={{ opacity: 1 }}>
                <h2 className="text-xl font-bold mb-4">键盘快捷键</h2>
                <div className="space-y-2">
                  {shortcuts.map((s) => (
                    <div key={s.action} className="flex items-center justify-between rounded-lg border border-border-subtle bg-surface-1 px-4 py-3">
                      <span className="text-sm">{s.action}</span>
                      <div className="flex gap-1">
                        {s.keys.map((k) => (
                          <kbd key={k} className="rounded bg-surface-3 border border-border-subtle px-2 py-0.5 text-xs font-mono">
                            {k}
                          </kbd>
                        ))}
                      </div>
                    </div>
                  ))}
                </div>
              </motion.div>
            )}

            {active === 'faq' && (
              <motion.div initial={{ opacity: 0 }} animate={{ opacity: 1 }}>
                <h2 className="text-xl font-bold mb-4">常见问题</h2>
                <div className="space-y-3">
                  {faqs.map((faq) => (
                    <details key={faq.q} className="group rounded-xl border border-border-subtle bg-surface-1 overflow-hidden">
                      <summary className="flex items-center justify-between px-4 py-3 cursor-pointer text-sm font-medium hover:bg-surface-2 transition-colors list-none">
                        {faq.q}
                        <ChevronRight size={14} className="text-text-muted group-open:rotate-90 transition-transform" />
                      </summary>
                      <div className="px-4 pb-4 text-xs text-text-muted leading-relaxed border-t border-border-subtle pt-3">
                        {faq.a}
                      </div>
                    </details>
                  ))}
                </div>
              </motion.div>
            )}
          </div>
        </div>
      </PageContent>
    </>
  )
}
