import { useEffect, useState, useRef } from 'react';
import { Typography, Avatar, App } from 'antd';
import { UserOutlined, RobotOutlined, CopyOutlined, PlusOutlined } from '@ant-design/icons';
import ReactMarkdown from 'react-markdown';
import { Prism as SyntaxHighlighter } from 'react-syntax-highlighter';
import { oneDark } from 'react-syntax-highlighter/dist/esm/styles/prism';
import { useAI } from '../../stores/aiStore';
import { useRedis } from '../../stores/redisStore';

const { Text } = Typography;
const C = { border:'#232631', text:'#cdd0db', dim:'#878b9e', muted:'#5c6178', blue:'#3b82f6', green:'#22c55e', bg2:'#141720' };

export default function AiPanel() {
  const { convs, activeConvId, messages, streaming, streamContent, loadConvs, loadMessages, send, newConv } = useAI();
  const { activeConnId } = useRedis();
  const { message } = App.useApp();
  const [input, setInput] = useState('');
  const scrollRef = useRef<HTMLDivElement>(null);

  useEffect(() => { loadConvs(); }, []);
  useEffect(() => {
    if (scrollRef.current) { scrollRef.current.scrollTop = scrollRef.current.scrollHeight; }
  }, [messages, streamContent]);

  const handleSend = () => {
    if (!input.trim() || streaming) { return; }
    const m = input.trim(); setInput('');
    send(m, activeConnId || undefined);
  };

  return (
    <div style={{ height:'100%', display:'flex', flexDirection:'column' }}>
      {/* Header */}
      <div style={{ padding:'10px 14px', borderBottom:`1px solid ${C.border}`, flexShrink:0 }}>
        <div style={{ display:'flex', justifyContent:'space-between', alignItems:'center' }}>
          <div style={{ display:'flex', alignItems:'center', gap:6 }}>
            <RobotOutlined style={{ color:C.green }} />
            <Text strong style={{ color:C.text, fontSize:13 }}>AI Redis 助手</Text>
          </div>
          <button onClick={newConv} style={{ background:'none', border:'none', color:C.muted, cursor:'pointer', fontSize:14 }}>
            <PlusOutlined />
          </button>
        </div>
        {/* Session pills */}
        {convs.length > 0 && (
          <div style={{ display:'flex', gap:4, marginTop:6, overflow:'auto' }}>
            {convs.slice(0, 6).map((c) => (
              <button key={c.id} onClick={() => { loadMessages(c.id); }}
                style={{
                  padding:'3px 10px', borderRadius:10, border:`1px solid ${C.border}`,
                  background: c.id === activeConvId ? 'rgba(59,130,246,0.12)' : 'transparent',
                  color: c.id === activeConvId ? C.blue : C.muted, fontSize:10, whiteSpace:'nowrap',
                  cursor:'pointer', maxWidth:100, overflow:'hidden', textOverflow:'ellipsis',
                }}
              >{c.title}</button>
            ))}
          </div>
        )}
      </div>

      {/* Messages */}
      <div ref={scrollRef} style={{ flex:1, overflow:'auto', padding:10 }}>
        {messages.length === 0 && !streaming ? (
          <div style={{ textAlign:'center', padding:24 }}>
            <RobotOutlined style={{ fontSize:32, color:'#1c1f2d' }} />
            <br /><br />
            <Text style={{ color:C.muted, fontSize:11 }}>我是 Redis 学习助手，有什么可以帮你的？</Text>
          </div>
        ) : (
          <>
            {messages.map((m) => (
              <div key={m.id} style={{
                display:'flex', gap:8, marginBottom:12,
                flexDirection: m.role === 'user' ? 'row-reverse' : 'row',
              }}>
                <Avatar size={24}
                  icon={m.role === 'user' ? <UserOutlined /> : <RobotOutlined />}
                  style={{ background: m.role === 'user' ? C.blue : C.green, flexShrink:0 }} />
                <div style={{
                  maxWidth:'82%', padding:'8px 12px', borderRadius:8,
                  background: m.role === 'user' ? 'rgba(59,130,246,0.06)' : 'rgba(255,255,255,0.015)',
                  border: `1px solid ${m.role === 'user' ? 'rgba(59,130,246,0.12)' : C.border}`,
                  fontSize:12, color:C.text, lineHeight:1.65,
                }}>
                  {m.role === 'assistant' ? (
                    <ReactMarkdown
                      components={{
                        code({ className, children, ...props }: any) {
                          const match = /language-(\w+)/.exec(className || '');
                          const codeStr = String(children).replace(/\n$/, '');
                          if (match) {
                            return (
                              <div style={{ position:'relative', margin:'6px 0' }}>
                                <button onClick={() => { navigator.clipboard.writeText(codeStr); message.success('已复制'); }}
                                  style={{ position:'absolute', right:4, top:4, zIndex:1, background:'none', border:'none', color:C.muted, cursor:'pointer', fontSize:10 }}>
                                  <CopyOutlined />
                                </button>
                                <SyntaxHighlighter style={oneDark} language={match[1]} PreTag="div"
                                  customStyle={{ borderRadius:6, fontSize:11, padding:'10px 14px' }}>
                                  {codeStr}
                                </SyntaxHighlighter>
                              </div>
                            );
                          }
                          return <code style={{ background:'rgba(255,255,255,0.06)', padding:'1px 5px', borderRadius:3, fontSize:11 }}>{String(children)}</code>;
                        },
                      }}
                    >{m.content}</ReactMarkdown>
                  ) : (
                    <span>{m.content}</span>
                  )}
                </div>
              </div>
            ))}
            {streaming && streamContent && (
              <div style={{ display:'flex', gap:8, marginBottom:12 }}>
                <Avatar size={24} icon={<RobotOutlined />} style={{ background:C.green }} />
                <div style={{ maxWidth:'82%', padding:'8px 12px', borderRadius:8, background:'rgba(255,255,255,0.015)', border:`1px solid ${C.border}`, fontSize:12, color:C.text, lineHeight:1.65 }}>
                  <ReactMarkdown>{streamContent}</ReactMarkdown>
                </div>
              </div>
            )}
          </>
        )}
      </div>

      {/* Input */}
      <div style={{ padding:'8px 10px', borderTop:`1px solid ${C.border}`, flexShrink:0, display:'flex', gap:6 }}>
        <input
          value={input} onChange={(e) => { setInput(e.target.value); }}
          onKeyDown={(e) => { if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); handleSend(); } }}
          placeholder="向 AI 提问 Redis 相关问题..."
          disabled={streaming}
          style={{ flex:1, height:32, background:C.bg2, border:`1px solid ${C.border}`, borderRadius:4, color:C.text, fontSize:12, padding:'0 10px', outline:'none' }}
        />
        <button onClick={handleSend} disabled={!input.trim() || streaming}
          style={{ background:C.green, border:'none', borderRadius:4, color:'#fff', cursor:'pointer', fontSize:12, padding:'4px 12px', fontWeight:500, opacity: (!input.trim()||streaming) ? 0.4 : 1 }}>
          发送
        </button>
      </div>
    </div>
  );
}
