import { PageContent, PageHeader } from '@/components/layout/MainLayout'
import { Badge } from '@/components/ui/Badge'
import { Button } from '@/components/ui/Button'
import { usePlatformStore } from '@/stores/platformStore'
import { cn } from '@/lib/utils'
import { motion } from 'framer-motion'
import { Link, useParams } from 'react-router-dom'
import {
  ArrowLeft,
  CheckCircle2,
  Lightbulb,
  Play,
  RotateCcw,
  Send,
  XCircle,
} from 'lucide-react'
import { useState } from 'react'

export default function ExerciseDetailPage() {
  const { exerciseId } = useParams()
  const exercises = usePlatformStore((s) => s.exercises)
  const completeExercise = usePlatformStore((s) => s.completeExercise)
  const exercise = exercises.find((e) => e.id === exerciseId) ?? exercises[2]

  const [command, setCommand] = useState('')
  const [showHint, setShowHint] = useState(false)
  const [result, setResult] = useState<'idle' | 'success' | 'fail'>('idle')
  const [output, setOutput] = useState('')

  const handleSubmit = () => {
    if (!command.trim()) return
    const normalized = command.trim().toUpperCase()
    const expected = exercise.expectedCommand?.toUpperCase() ?? ''
    const isCorrect = expected && normalized.includes(expected.split(' ')[0]) && normalized.length > 5

    if (isCorrect || normalized === expected) {
      setResult('success')
      setOutput('OK — 命令执行成功！\n(integer) 1')
      completeExercise(exercise.id)
    } else {
      setResult('fail')
      setOutput('(error) ERR 命令不正确，请检查语法或参考提示')
    }
  }

  const handleReset = () => {
    setCommand('')
    setResult('idle')
    setOutput('')
    setShowHint(false)
  }

  return (
    <>
      <PageHeader
        title={exercise.title}
        subtitle={exercise.description}
        actions={
          <Link to="/exercises">
            <Button variant="ghost" size="sm">
              <ArrowLeft size={14} />
              返回列表
            </Button>
          </Link>
        }
      />
      <PageContent>
        <div className="max-w-3xl mx-auto space-y-6">
          <div className="flex items-center gap-2">
            <Badge variant={exercise.difficulty === 'easy' ? 'success' : exercise.difficulty === 'medium' ? 'warning' : 'type'}>
              {exercise.difficulty === 'easy' ? '简单' : exercise.difficulty === 'medium' ? '中等' : '困难'}
            </Badge>
            {exercise.completed && (
              <span className="flex items-center gap-1 text-xs text-success">
                <CheckCircle2 size={14} /> 已完成
              </span>
            )}
            {exercise.tags.map((t) => (
              <span key={t} className="text-[10px] font-mono text-text-muted bg-surface-3 px-2 py-0.5 rounded">
                {t}
              </span>
            ))}
          </div>

          <div className="rounded-xl border border-border-subtle bg-surface-1 p-5">
            <h3 className="text-sm font-semibold mb-2">题目要求</h3>
            <p className="text-sm text-text-secondary leading-relaxed">{exercise.description}</p>
            {exercise.type === 'command' && (
              <p className="text-xs text-text-muted mt-3">
                在下方输入 Redis 命令，点击提交验证。也可
                <Link to="/workspace" className="text-accent-red hover:underline mx-1">打开工作台</Link>
                实际操作。
              </p>
            )}
          </div>

          {exercise.type === 'command' ? (
            <div className="rounded-xl border border-border-subtle bg-surface-0 overflow-hidden">
              <div className="flex items-center justify-between px-4 py-2 bg-surface-2 border-b border-border-subtle">
                <span className="text-[10px] font-mono text-text-muted">redis&gt;</span>
                <div className="flex gap-1">
                  <Button variant="ghost" size="sm" className="!h-6 !text-[10px]" onClick={() => setShowHint(!showHint)}>
                    <Lightbulb size={11} />
                    提示
                  </Button>
                  <Button variant="ghost" size="sm" className="!h-6 !text-[10px]" onClick={handleReset}>
                    <RotateCcw size={11} />
                    重置
                  </Button>
                </div>
              </div>
              {showHint && exercise.hint && (
                <motion.div
                  initial={{ height: 0, opacity: 0 }}
                  animate={{ height: 'auto', opacity: 1 }}
                  className="px-4 py-2 bg-accent-amber/5 border-b border-accent-amber/20 text-xs text-accent-amber"
                >
                  💡 {exercise.hint}
                </motion.div>
              )}
              <textarea
                value={command}
                onChange={(e) => setCommand(e.target.value)}
                placeholder="输入 Redis 命令..."
                rows={3}
                className="w-full p-4 font-mono text-sm bg-transparent focus:outline-none resize-none text-text-primary placeholder:text-text-muted"
              />
              <div className="flex items-center justify-between px-4 py-2 border-t border-border-subtle">
                <div className="flex items-center gap-2">
                  {result === 'success' && <CheckCircle2 size={14} className="text-success" />}
                  {result === 'fail' && <XCircle size={14} className="text-danger" />}
                  {result !== 'idle' && (
                    <span className={cn('text-xs', result === 'success' ? 'text-success' : 'text-danger')}>
                      {result === 'success' ? '正确！' : '再试一次'}
                    </span>
                  )}
                </div>
                <Button variant="accent" size="sm" onClick={handleSubmit}>
                  <Send size={13} />
                  提交验证
                </Button>
              </div>
              {output && (
                <pre className="px-4 py-3 border-t border-border-subtle font-mono text-xs text-accent-teal bg-surface-1">
                  {output}
                </pre>
              )}
            </div>
          ) : (
            <div className="rounded-xl border border-border-subtle bg-surface-1 p-5 space-y-4">
              <textarea
                rows={6}
                placeholder="写下你的分析和方案..."
                className="w-full rounded-lg border border-border-subtle bg-surface-0 p-4 text-sm focus:outline-none focus:border-accent-purple/40 resize-none"
              />
              {showHint && exercise.hint && (
                <p className="text-xs text-accent-amber">💡 {exercise.hint}</p>
              )}
              <div className="flex gap-2">
                <Button variant="ghost" size="sm" onClick={() => setShowHint(true)}>
                  <Lightbulb size={14} /> 查看提示
                </Button>
                <Link to="/ai">
                  <Button variant="outline" size="sm">
                    咨询 AI 导师
                  </Button>
                </Link>
                <Button variant="accent" size="sm" onClick={() => completeExercise(exercise.id)}>
                  标记完成
                </Button>
              </div>
            </div>
          )}

          {result === 'success' && (
            <motion.div
              initial={{ opacity: 0, y: 10 }}
              animate={{ opacity: 1, y: 0 }}
              className="rounded-xl border border-success/20 bg-success/5 p-5 text-center"
            >
              <CheckCircle2 size={32} className="text-success mx-auto mb-2" />
              <h3 className="font-semibold text-success mb-1">练习完成！</h3>
              <p className="text-xs text-text-muted mb-4">继续保持，挑战下一道题</p>
              <div className="flex justify-center gap-2">
                <Link to="/exercises">
                  <Button variant="outline" size="sm">更多练习</Button>
                </Link>
                <Link to="/workspace">
                  <Button variant="accent" size="sm">
                    <Play size={14} />
                    在工作台实践
                  </Button>
                </Link>
              </div>
            </motion.div>
          )}
        </div>
      </PageContent>
    </>
  )
}
