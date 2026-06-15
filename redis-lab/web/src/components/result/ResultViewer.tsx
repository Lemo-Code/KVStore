import { Typography, Tag } from 'antd';
import { CheckCircleFilled, CloseCircleFilled } from '@ant-design/icons';
import type { CommandResult } from '../../types';

const { Text } = Typography;

interface Props { result: CommandResult; }

export default function ResultViewer({ result }: Props) {
  if (result.type === 'error') {
    return (
      <div style={{ padding: '10px 14px', background: 'rgba(239,68,68,0.06)', border: '1px solid rgba(239,68,68,0.15)', borderRadius: 8, display: 'flex', gap: 10 }}>
        <CloseCircleFilled style={{ color: '#ef4444', marginTop: 2 }} />
        <div>
          <Text strong style={{ color: '#ef4444', fontSize: 12 }}>Error</Text>
          <pre style={{ color: '#fca5a5', margin: '4px 0 0', fontSize: 12, whiteSpace: 'pre-wrap', wordBreak: 'break-all', fontFamily: 'JetBrains Mono, monospace' }}>
            {String(result.value)}
          </pre>
        </div>
      </div>
    );
  }

  if (result.type === 'null') {
    return <Text type="secondary" style={{ fontFamily: 'monospace', fontSize: 12 }}>(nil)</Text>;
  }

  if (result.type === 'integer') {
    return (
      <div style={{ padding: '8px 14px', background: 'rgba(34,197,94,0.05)', border: '1px solid rgba(34,197,94,0.12)', borderRadius: 6, display: 'flex', gap: 8, alignItems: 'center' }}>
        <CheckCircleFilled style={{ color: '#22c55e', fontSize: 14 }} />
        <Text type="secondary" style={{ fontSize: 10 }}>Integer</Text>
        <Text strong style={{ fontFamily: 'JetBrains Mono, monospace', fontSize: 15, color: '#22c55e' }}>{String(result.value)}</Text>
      </div>
    );
  }

  if (result.type === 'string') {
    const s = String(result.value);
    if (s === 'OK' || s === 'PONG') {
      return (
        <div style={{ padding: '6px 14px', background: 'rgba(34,197,94,0.05)', border: '1px solid rgba(34,197,94,0.12)', borderRadius: 6, display: 'flex', gap: 8, alignItems: 'center' }}>
          <CheckCircleFilled style={{ color: '#22c55e' }} />
          <Text strong style={{ color: '#22c55e', fontFamily: 'monospace' }}>{s}</Text>
        </div>
      );
    }
    return (
      <div>
        <Tag color="blue" style={{ marginBottom: 6, fontSize: 10 }}>String</Tag>
        <pre style={{ padding: '10px 14px', background: 'rgba(255,255,255,0.015)', border: '1px solid #252836', borderRadius: 6, fontFamily: 'JetBrains Mono, monospace', fontSize: 12, whiteSpace: 'pre-wrap', wordBreak: 'break-all', color: '#e1e4ed', margin: 0 }}>{s}</pre>
      </div>
    );
  }

  if (result.type === 'array' && Array.isArray(result.value)) {
    const arr = result.value as unknown[];
    return (
      <div>
        <Tag color="green" style={{ marginBottom: 6, fontSize: 10 }}>Array ({arr.length})</Tag>
        <div style={{ border: '1px solid #252836', borderRadius: 6, overflow: 'hidden' }}>
          {arr.map((item, i) => (
            <div key={i} style={{
              padding: '5px 12px', display: 'flex', gap: 8, fontFamily: 'JetBrains Mono, monospace', fontSize: 11,
              background: i % 2 === 0 ? '#0c0d14' : '#11131a',
              borderBottom: i < arr.length - 1 ? '1px solid #1c1f2d' : 'none',
            }}>
              <Text type="secondary" style={{ minWidth: 24, fontSize: 10 }}>{i + 1})</Text>
              <Text style={{ color: '#e1e4ed', wordBreak: 'break-all' }}>{String(item)}</Text>
            </div>
          ))}
        </div>
      </div>
    );
  }

  return <pre style={{ color: '#8b8fa8', fontSize: 11 }}>{JSON.stringify(result.value)}</pre>;
}
