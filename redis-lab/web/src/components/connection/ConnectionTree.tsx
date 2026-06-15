import { List, Typography, Button, Popconfirm, Space, App } from 'antd';
import { PlusOutlined, DeleteOutlined, EditOutlined, LinkOutlined } from '@ant-design/icons';
import { useRedis } from '../../stores/redisStore';

const { Text } = Typography;

interface Props { onAdd: () => void; }

export default function ConnectionTree({ onAdd }: Props) {
  const { conns, activeConnId, setActiveConn, removeConn, testConn } = useRedis();
  const { message } = App.useApp();

  const handleTest = async (id: number, e: React.MouseEvent) => {
    e.stopPropagation();
    try {
      const r = await testConn(id);
      message[r.success ? 'success' : 'error'](r.message);
    } catch { message.error('测试失败'); }
  };

  return (
    <div style={{ padding: '4px 0' }}>
      <div style={{
        display: 'flex', justifyContent: 'space-between', alignItems: 'center',
        padding: '6px 12px',
      }}>
        <Text style={{ color: '#595e73', fontSize: 10, textTransform: 'uppercase', letterSpacing: 1, fontWeight: 600 }}>
          连接列表
        </Text>
        <Button type="text" size="small" icon={<PlusOutlined />} onClick={onAdd}
          style={{ color: '#8b8fa8', fontSize: 12 }} />
      </div>

      <List
        dataSource={conns}
        size="small"
        split={false}
        locale={{ emptyText: <Text style={{ color: '#595e73', fontSize: 11, padding: 16, display: 'block', textAlign: 'center' }}>暂无连接</Text> }}
        renderItem={(conn) => {
          const isActive = conn.id === activeConnId;
          return (
            <div
              onClick={() => setActiveConn(conn.id)}
              style={{
                cursor: 'pointer', padding: '6px 12px', margin: '1px 6px', borderRadius: 4,
                background: isActive ? 'rgba(59,130,246,0.08)' : 'transparent',
                borderLeft: isActive ? '2px solid #3b82f6' : '2px solid transparent',
                transition: 'all 0.12s',
              }}
              onMouseEnter={(e) => { if (!isActive) e.currentTarget.style.background = 'rgba(255,255,255,0.02)'; }}
              onMouseLeave={(e) => { if (!isActive) e.currentTarget.style.background = 'transparent'; }}
            >
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <Space size={6}>
                  <span className="status-dot online" />
                  <div>
                    <Text style={{
                      color: isActive ? '#e1e4ed' : '#8b8fa8',
                      fontSize: 12, fontWeight: isActive ? 600 : 400,
                    }}>
                      {conn.name}
                    </Text>
                    <br />
                    <Text style={{ color: '#595e73', fontSize: 10 }}>
                      {conn.host}:{conn.port}
                    </Text>
                  </div>
                </Space>

                <Space size={0} className="conn-actions" style={{ opacity: 0, transition: 'opacity 0.12s' }}>
                  <Button type="text" size="small" icon={<LinkOutlined style={{ fontSize: 10 }} />}
                    onClick={(e) => handleTest(conn.id, e)} style={{ color: '#595e73' }} />
                  <Popconfirm title="删除此连接？" onConfirm={() => removeConn(conn.id)}
                    placement="right">
                    <Button type="text" size="small" icon={<DeleteOutlined style={{ fontSize: 10 }} />}
                      onClick={(e) => e.stopPropagation()} style={{ color: '#595e73' }} />
                  </Popconfirm>
                </Space>
              </div>
            </div>
          );
        }}
      />
      <style>{`.conn-actions:hover, div:hover > .conn-actions, [style*="rgba(255,255,255,0.02)"] .conn-actions { opacity: 1 !important; }`}</style>
    </div>
  );
}
