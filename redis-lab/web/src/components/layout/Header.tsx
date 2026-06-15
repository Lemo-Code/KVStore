import { Typography, Avatar, Dropdown, Tooltip } from 'antd';
import { CodeOutlined, UserOutlined, LogoutOutlined, PlusOutlined, MessageOutlined, RobotOutlined } from '@ant-design/icons';
import { useAuth } from '../../stores/authStore';
import { useRedis } from '../../stores/redisStore';
import type { RightPanelType } from '../../types';

const { Text } = Typography;

const S = {
  hdr: { display:'flex' as const, alignItems:'center', gap:10, padding:'0 14px', height:44, background:'#11131a', borderBottom:'1px solid #232631', flexShrink:0 },
  dot: { width:7, height:7, borderRadius:'50%', background:'#22c55e', boxShadow:'0 0 4px rgba(34,197,94,0.5)', flexShrink:0 },
  dotOff: { width:7, height:7, borderRadius:'50%', background:'#5c6178', flexShrink:0 },
  divider: { width:1, height:18, background:'#232631' },
  btn: (active:boolean, color:string) => ({
    background: active ? `${color}18` : 'none', border:'none', borderRadius:4,
    color: active ? color : '#878b9e', cursor:'pointer', fontSize:12, padding:'4px 10px',
    display:'flex', alignItems:'center', gap:4,
  }),
  sel: { width:210, height:28, background:'#141720', border:'1px solid #232631', borderRadius:4, color:'#cdd0db', fontSize:12, padding:'0 8px', outline:'none' as const },
};

interface Props {
  rightPanel: RightPanelType;
  onToggleRight: (p: RightPanelType) => void;
  onAddConnection: () => void;
}

export default function Header({ rightPanel, onToggleRight, onAddConnection }: Props) {
  const { user, logout } = useAuth();
  const { conns, activeConnId, setActiveConn } = useRedis();
  const conn = conns.find((c) => c.id === activeConnId);

  return (
    <div style={S.hdr}>
      <CodeOutlined style={{ fontSize:18, color:'#3b82f6' }} />
      <Text strong style={{ color:'#cdd0db', fontSize:14, letterSpacing:-0.3 }}>RedisLab</Text>
      <div style={S.divider} />

      <select
        value={activeConnId || ''}
        onChange={(e) => { setActiveConn(e.target.value ? Number(e.target.value) : null); }}
        style={S.sel}
      >
        <option value="">选择连接...</option>
        {conns.map((c) => (
          <option key={c.id} value={c.id}>{c.name} ({c.host}:{c.port})</option>
        ))}
      </select>

      <Tooltip title="新建连接">
        <button onClick={onAddConnection} style={{ background:'none', border:'none', color:'#878b9e', cursor:'pointer', fontSize:16, padding:'0 4px' }}>
          <PlusOutlined />
        </button>
      </Tooltip>

      <div style={{ flex:1 }} />

      <button onClick={() => { onToggleRight(rightPanel === 'chat' ? null : 'chat'); }}
        style={S.btn(rightPanel === 'chat', '#3b82f6')}>
        <MessageOutlined /> 聊天
      </button>
      <button onClick={() => { onToggleRight(rightPanel === 'ai' ? null : 'ai'); }}
        style={S.btn(rightPanel === 'ai', '#22c55e')}>
        <RobotOutlined /> AI
      </button>

      <div style={S.divider} />

      <Dropdown
        menu={{ items: [
          { key:'u', label: <Text strong style={{ fontSize:12 }}>{user?.username}</Text>, disabled: true },
          { type:'divider' },
          { key:'out', icon: <LogoutOutlined />, label: '退出登录', onClick: () => { logout(); window.location.href = '/login'; } },
        ]}}
        trigger={['click']}
        placement="bottomRight"
      >
        <Avatar size={28} icon={<UserOutlined />} style={{ cursor:'pointer', background:'#3b82f6' }} />
      </Dropdown>
    </div>
  );
}
