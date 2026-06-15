import { Typography } from 'antd';
import { useRedis } from '../../stores/redisStore';

const { Text } = Typography;
const C = { border:'#232631', text:'#cdd0db', dim:'#878b9e', muted:'#5c6178', blue:'#3b82f6', green:'#22c55e', red:'#ef4444' };

interface Props { onAdd: () => void; }

export default function ConnectionList({ onAdd }: Props) {
  const { conns, activeConnId, setActiveConn, removeConn, quota, loadKeys } = useRedis();

  return (
    <div>
      <div style={{ display:'flex', justifyContent:'space-between', alignItems:'center', padding:'8px 14px 4px' }}>
        <Text style={{ color:C.muted, fontSize:10, textTransform:'uppercase', letterSpacing:1, fontWeight:600 }}>连接</Text>
        <button onClick={onAdd} style={{ background:'none', border:'none', color:C.dim, cursor:'pointer', fontSize:14, padding:'0 2px' }}>+</button>
      </div>

      {conns.length === 0 ? (
        <div style={{ padding:'16px 14px', textAlign:'center' }}>
          <Text style={{ color:C.muted, fontSize:11 }}>暂无连接，点击 + 创建</Text>
        </div>
      ) : (
        conns.map((c) => {
          const active = c.id === activeConnId;
          return (
            <div key={c.id}
              onClick={() => { setActiveConn(c.id); }}
              style={{
                cursor:'pointer', padding:'7px 14px', margin:'0 6px 1px', borderRadius:4,
                background: active ? 'rgba(59,130,246,0.08)' : 'transparent',
                borderLeft: active ? `2px solid ${C.blue}` : '2px solid transparent',
                display:'flex', alignItems:'center', justifyContent:'space-between',
                transition:'background 0.12s',
              }}
            >
              <div style={{ display:'flex', alignItems:'center', gap:7 }}>
                <span style={{ width:7,height:7,borderRadius:'50%',background:C.green,boxShadow:`0 0 3px ${C.green}80`,flexShrink:0 }} />
                <div>
                  <div style={{ color: active ? C.text : C.dim, fontSize:12, fontWeight: active ? 600 : 400 }}>
                    {c.name}
                  </div>
                  <div style={{ color:C.muted, fontSize:10 }}>{c.host}:{c.port}</div>
                </div>
              </div>
              <button
                onClick={(e) => { e.stopPropagation(); removeConn(c.id); }}
                style={{
                  background:'none', border:'none', color:C.muted, cursor:'pointer',
                  fontSize:12, padding:'0 2px', opacity:0, transition:'opacity 0.12s',
                }}
                title="删除"
                onMouseEnter={(e) => { e.currentTarget.style.opacity = '1'; }}
                onMouseLeave={(e) => { e.currentTarget.style.opacity = '0'; }}
              >✕</button>
            </div>
          );
        })
      )}

      {/* Quota mini bar */}
      {quota && (
        <div style={{ padding:'6px 14px', borderTop:`1px solid ${C.border}`, marginTop:4 }}>
          <div style={{ display:'flex', justifyContent:'space-between', marginBottom:3 }}>
            <span style={{ color:C.muted, fontSize:9 }}>配额</span>
            <span style={{ color:C.dim, fontSize:9 }}>{quota.used_keys}/{quota.max_keys}</span>
          </div>
          <div style={{ height:3, background:'#1c1f2d', borderRadius:2, overflow:'hidden' }}>
            <div style={{
              height:'100%', width:`${Math.min(quota.keys_percent,100)}%`,
              background: quota.keys_percent > 80 ? C.red : C.blue,
              transition:'width 0.4s', borderRadius:2,
            }} />
          </div>
        </div>
      )}
    </div>
  );
}
