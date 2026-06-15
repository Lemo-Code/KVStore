import { useRedis } from '../../stores/redisStore';
import { useChat } from '../../stores/chatStore';

const C = { bg:'#11131a', border:'#232631', muted:'#5c6178', green:'#22c55e' };

export default function StatusBar() {
  const { activeConnId, getActiveConn, quota } = useRedis();
  const { connected: chatOn } = useChat();
  const conn = getActiveConn();

  return (
    <div style={{
      display:'flex', alignItems:'center', justifyContent:'space-between',
      padding:'0 14px', height:24, background:C.bg, borderTop:`1px solid ${C.border}`,
      flexShrink:0, fontSize:10, color:C.muted,
    }}>
      <div style={{ display:'flex', alignItems:'center', gap:14 }}>
        {conn ? (
          <span style={{ display:'flex', alignItems:'center', gap:5 }}>
            <span style={{ width:6,height:6,borderRadius:'50%',background:C.green,boxShadow:`0 0 3px ${C.green}80` }} />
            {conn.host}:{conn.port}
          </span>
        ) : <span>未连接</span>}
        {quota && <span>Keys {quota.used_keys}/{quota.max_keys}</span>}
      </div>
      <div style={{ display:'flex', alignItems:'center', gap:14 }}>
        <span style={{ display:'flex', alignItems:'center', gap:4 }}>
          <span style={{ width:5,height:5,borderRadius:'50%',background:chatOn?C.green:C.muted }} />
          Chat {chatOn ? '在线' : '离线'}
        </span>
        {quota && <span>{quota.total_cmds} cmds</span>}
      </div>
    </div>
  );
}
