import { cn } from '@/lib/utils'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Separator } from '@/components/ui/separator'
import { Database, Users, Clock } from 'lucide-react'
import type { StreamValue } from '@/types/redis'

interface StreamEditorProps {
  value: StreamValue
}

export default function StreamEditor({ value }: StreamEditorProps) {
  return (
    <div className="flex flex-col h-full">
      {/* Toolbar */}
      <div className="flex items-center gap-2 px-3 py-2 border-b bg-muted/30">
        <Database className="h-4 w-4 text-muted-foreground" />
        <span className="text-xs text-muted-foreground">
          消息: <span className="font-mono font-medium text-foreground">{value.length}</span>
        </span>
        <span className="text-xs text-muted-foreground">
          消费者组: <span className="font-mono font-medium text-foreground">{value.consumerGroups.length}</span>
        </span>
      </div>

      <ScrollArea className="flex-1">
        {/* Consumer Groups Section */}
        {value.consumerGroups.length > 0 && (
          <div className="border-b">
            <div className="px-3 py-2 bg-muted/20 flex items-center gap-2">
              <Users className="h-3.5 w-3.5 text-muted-foreground" />
              <span className="text-xs font-semibold">Consumer Groups</span>
            </div>
            <table className="w-full text-sm">
              <thead className="bg-muted/30">
                <tr className="border-b">
                  <th className="px-3 py-1.5 text-left text-xs font-medium text-muted-foreground">Group Name</th>
                  <th className="px-3 py-1.5 text-right text-xs font-medium text-muted-foreground">Consumers</th>
                  <th className="px-3 py-1.5 text-right text-xs font-medium text-muted-foreground">Pending</th>
                </tr>
              </thead>
              <tbody>
                {value.consumerGroups.map((cg, idx) => (
                  <tr
                    key={cg.name}
                    className={cn('border-b', idx % 2 === 0 && 'bg-muted/5')}
                  >
                    <td className="px-3 py-1.5">
                      <span className="text-xs font-mono font-medium">{cg.name}</span>
                    </td>
                    <td className="px-3 py-1.5 text-right">
                      <span className="text-xs font-mono tabular-nums">{cg.consumers}</span>
                    </td>
                    <td className="px-3 py-1.5 text-right">
                      <span
                        className={cn(
                          'text-xs font-mono tabular-nums',
                          cg.pending > 0 ? 'text-orange-500 font-semibold' : 'text-muted-foreground'
                        )}
                      >
                        {cg.pending}
                      </span>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}

        {/* Messages Section */}
        <div>
          <div className="px-3 py-2 bg-muted/20 flex items-center gap-2">
            <Clock className="h-3.5 w-3.5 text-muted-foreground" />
            <span className="text-xs font-semibold">Messages</span>
            <span className="text-[10px] text-muted-foreground">({value.messages.length})</span>
          </div>

          <div className="divide-y">
            {value.messages.map((msg) => (
              <div key={msg.id} className="px-3 py-2 hover:bg-muted/20 transition-colors">
                <div className="flex items-baseline gap-2 mb-1.5">
                  <span className="text-[10px] font-mono font-semibold text-blue-600 bg-blue-50 px-1.5 py-0.5 rounded">
                    {msg.id}
                  </span>
                </div>
                <div className="flex flex-wrap gap-2">
                  {Object.entries(msg.fields).map(([field, value]) => (
                    <div
                      key={field}
                      className="inline-flex items-center gap-1 rounded border bg-background px-2 py-0.5"
                    >
                      <span className="text-[10px] font-semibold text-muted-foreground">{field}:</span>
                      <span className="text-[10px] font-mono">{value}</span>
                    </div>
                  ))}
                </div>
              </div>
            ))}
            {value.messages.length === 0 && (
              <div className="flex items-center justify-center py-8 text-xs text-muted-foreground">
                暂无消息
              </div>
            )}
          </div>
        </div>
      </ScrollArea>
    </div>
  )
}
