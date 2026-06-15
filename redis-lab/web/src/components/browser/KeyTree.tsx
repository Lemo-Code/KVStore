import { useEffect, useState } from 'react';
import { Typography, App } from 'antd';
import { KeyOutlined } from '@ant-design/icons';
import { useRedis } from '../../stores/redisStore';

const { Text } = Typography;
const C = { border:'#232631', text:'#cdd0db', dim:'#878b9e', muted:'#5c6178', blue:'#3b82f6', yellow:'#f59e0b' };

export default function KeyTree() {
  const { activeConnId, keys, selectedKey, loadKeys, loadKeyDetail, deleteKey } = useRedis();
  const { message } = App.useApp();
  const [pattern, setPattern] = useState('*');

  useEffect(() => {
    if (activeConnId) { loadKeys(pattern); }
  }, [activeConnId]);

  const handleSearch = () => { loadKeys(pattern); };

  // Build hierarchical tree from flat keys
  const buildTree = () => {
    const map = new Map<string, any>();
    for (const k of [...keys].sort()) {
      const parts = k.split(':');
      let path = '';
      for (let i = 0; i < parts.length; i++) {
        const last = i === parts.length - 1;
        const parent = path;
        path = path ? `${path}:${parts[i]}` : parts[i];
        if (!map.has(path)) {
          map.set(path, { title: parts[i], key: path, isLeaf: last, children: undefined });
        }
        const node = map.get(path);
        if (!last) { node.isLeaf = false; if (!node.children) node.children = []; }
        if (parent && map.has(parent)) {
          const p = map.get(parent);
          if (p.children && !p.children.find((c: any) => c.key === path)) {
            p.children.push(node);
          }
        }
      }
    }
    return Array.from(map.values()).filter((n: any) => !n.key.includes(':'));
  };

  if (!activeConnId) {
    return (
      <div style={{ padding: 24, textAlign: 'center' }}>
        <Text style={{ color: C.muted, fontSize: 11 }}>请先选择连接</Text>
      </div>
    );
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', flex: 1 }}>
      <div style={{ padding:'4px 14px 0' }}>
        <Text style={{ color:C.muted, fontSize:10, textTransform:'uppercase', letterSpacing:1, fontWeight:600 }}>Keys</Text>
      </div>

      <div style={{ padding:'4px 10px', display:'flex', gap:4 }}>
        <input
          value={pattern} onChange={(e) => { setPattern(e.target.value); }}
          onKeyDown={(e) => { if (e.key === 'Enter') { handleSearch(); } }}
          placeholder="pattern..."
          style={{ flex:1, height:26, background:'#141720', border:'1px solid #232631', borderRadius:4, color:C.text, fontSize:11, padding:'0 8px', outline:'none' }}
        />
        <button onClick={handleSearch}
          style={{ background:'none', border:'1px solid #232631', borderRadius:4, color:C.dim, cursor:'pointer', padding:'0 8px', fontSize:13 }}>
          ⟳
        </button>
      </div>

      <div style={{ flex:1, overflow:'auto', padding:'2px 4px', fontSize:11 }}>
        {keys.length === 0 ? (
          <div style={{ padding:16, textAlign:'center', color:C.muted, fontSize:11 }}>暂无 Key</div>
        ) : (
          buildTree().map((n) => (
            <TreeNodeView key={n.key} node={n} selectedKey={selectedKey}
              onSelect={(k) => { loadKeyDetail(k); }}
              onDelete={(k) => { deleteKey(k); message.success(`已删除 ${k}`); }}
              depth={0} />
          ))
        )}
      </div>

      <div style={{ padding:'4px 14px', borderTop:`1px solid ${C.border}`, color:C.muted, fontSize:9 }}>
        {keys.length} keys
      </div>
    </div>
  );
}

function TreeNodeView({ node, selectedKey, onSelect, onDelete, depth }: {
  node: any; selectedKey: string | null;
  onSelect: (k: string) => void; onDelete: (k: string) => void; depth: number;
}) {
  const [open, setOpen] = useState(depth < 2);
  const isDir = !node.isLeaf && node.children && node.children.length > 0;
  const isSel = selectedKey === node.key;

  return (
    <div>
      <div
        onClick={() => { if (isDir) { setOpen(!open); } else { onSelect(node.key); } }}
        style={{
          display:'flex', alignItems:'center', gap:4, padding:'3px 6px',
          paddingLeft: 8 + depth * 14, cursor:'pointer', borderRadius:3,
          background: isSel ? 'rgba(59,130,246,0.10)' : 'transparent',
          color: isSel ? C.blue : C.dim, fontFamily:'monospace',
        }}
      >
        {isDir ? (
          <span style={{ fontSize:10, width:12, color:C.muted }}>{open ? '▾' : '▸'}</span>
        ) : (
          <KeyOutlined style={{ fontSize:10, color:C.yellow }} />
        )}
        <span style={{
          flex:1, overflow:'hidden', textOverflow:'ellipsis', whiteSpace:'nowrap',
        }}>{node.title}</span>
        {node.isLeaf && (
          <button
            onClick={(e) => { e.stopPropagation(); onDelete(node.key); }}
            style={{ background:'none', border:'none', color:C.muted, cursor:'pointer', fontSize:10, padding:'0 2px', opacity:0 }}
            className="kt-del"
          >✕</button>
        )}
      </div>
      {isDir && open && node.children.map((c: any) => (
        <TreeNodeView key={c.key} node={c} selectedKey={selectedKey} onSelect={onSelect} onDelete={onDelete} depth={depth + 1} />
      ))}
    </div>
  );
}
