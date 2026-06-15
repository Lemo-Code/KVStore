import { useEffect, useState, useRef } from 'react';
import { Typography, Avatar, Empty } from 'antd';
import { UserOutlined, MessageOutlined } from '@ant-design/icons';
import { useChat } from '../../stores/chatStore';

const { Text } = Typography;
const C = { border:'#232631', text:'#cdd0db', dim:'#878b9e', muted:'#5c6178', blue:'#3b82f6', green:'#22c55e', red:'#ef4444', bg2:'#141720' };

export default function ChatPanel() {
  const { rooms, activeRoomId, messages, unread, joinRoom, send, connected, loadRooms } = useChat();
  const [input, setInput] = useState('');
  const scrollRef = useRef<HTMLDivElement>(null);
  const activeMsgs = activeRoomId ? (messages[activeRoomId] || []) : [];
  const activeRoom = rooms.find((r) => r.id === activeRoomId);

  useEffect(() => { loadRooms(); }, []);
  useEffect(() => {
    if (scrollRef.current) { scrollRef.current.scrollTop = scrollRef.current.scrollHeight; }
  }, [activeMsgs]);

  const handleSend = () => {
    if (!input.trim()) { return; }
    send(input.trim());
    setInput('');
  };

  return (
    <div style={{ height:'100%', display:'flex', flexDirection:'column' }}>
      {/* Header */}
      <div style={{ padding:'10px 14px', borderBottom:`1px solid ${C.border}`, flexShrink:0 }}>
        <div style={{ display:'flex', alignItems:'center', gap:6 }}>
          <MessageOutlined style={{ color:C.blue }} />
          <Text strong style={{ color:C.text, fontSize:13 }}>{activeRoom ? activeRoom.title : '聊天室'}</Text>
          <span style={{ width:6,height:6,borderRadius:'50%',background:connected?C.green:C.muted }} />
        </div>
      </div>

      {/* Room list */}
      <div style={{ maxHeight:130, overflow:'auto', borderBottom:`1px solid ${C.border}`, flexShrink:0 }}>
        {rooms.map((r) => {
          const active = r.id === activeRoomId;
          const count = unread[r.id] || 0;
          return (
            <div key={r.id} onClick={() => { joinRoom(r.id); }}
              style={{
                cursor:'pointer', padding:'6px 14px', fontSize:12,
                background: active ? 'rgba(59,130,246,0.08)' : 'transparent',
                color: active ? C.blue : C.dim, display:'flex', justifyContent:'space-between',
                borderLeft: active ? `2px solid ${C.blue}` : '2px solid transparent',
              }}
            >
              <span># {r.title}</span>
              {!active && count > 0 && (
                <span style={{ background:C.red, color:'#fff', borderRadius:8, padding:'0 6px', fontSize:10, minWidth:18, textAlign:'center', lineHeight:'16px' }}>
                  {count}
                </span>
              )}
            </div>
          );
        })}
      </div>

      {/* Messages */}
      <div ref={scrollRef} style={{ flex:1, overflow:'auto', padding:10 }}>
        {activeMsgs.length === 0 ? (
          <div style={{ textAlign:'center', padding:24 }}>
            <Text style={{ color:C.muted, fontSize:11 }}>暂无消息</Text>
          </div>
        ) : (
          activeMsgs.map((m) => (
            <div key={m.id} style={{ display:'flex', gap:8, marginBottom:12 }}>
              <Avatar size={24} icon={<UserOutlined />} style={{ background:C.blue, flexShrink:0 }} />
              <div style={{ flex:1, minWidth:0 }}>
                <div style={{ display:'flex', alignItems:'baseline', gap:6, marginBottom:2 }}>
                  <Text strong style={{ color:C.text, fontSize:11 }}>{m.user?.username}</Text>
                  <Text style={{ color:C.muted, fontSize:9 }}>
                    {new Date(m.created_at).toLocaleTimeString('zh-CN', { hour:'2-digit', minute:'2-digit' })}
                  </Text>
                </div>
                <Text style={{ color:C.dim, fontSize:12, lineHeight:1.5, wordBreak:'break-word' }}>{m.content}</Text>
              </div>
            </div>
          ))
        )}
      </div>

      {/* Input */}
      {activeRoomId && (
        <div style={{ padding:'8px 10px', borderTop:`1px solid ${C.border}`, flexShrink:0, display:'flex', gap:6 }}>
          <input
            value={input} onChange={(e) => { setInput(e.target.value); }}
            onKeyDown={(e) => { if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); handleSend(); } }}
            placeholder="输入消息... Enter 发送"
            disabled={!connected}
            style={{ flex:1, height:32, background:C.bg2, border:`1px solid ${C.border}`, borderRadius:4, color:C.text, fontSize:12, padding:'0 10px', outline:'none' }}
          />
          <button
            onClick={handleSend} disabled={!input.trim() || !connected}
            style={{ background:C.blue, border:'none', borderRadius:4, color:'#fff', cursor:'pointer', fontSize:12, padding:'4px 12px', fontWeight:500, opacity: (!input.trim()||!connected) ? 0.4 : 1 }}
          >发送</button>
        </div>
      )}
    </div>
  );
}
