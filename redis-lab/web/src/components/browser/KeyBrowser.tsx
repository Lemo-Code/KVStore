import { useEffect, useState, useCallback } from 'react';
import { Tree, Input, Typography, Button, Space, Spin, Empty, Popconfirm, App } from 'antd';
import { SearchOutlined, ReloadOutlined, KeyOutlined, CopyOutlined, DeleteOutlined } from '@ant-design/icons';
import { useRedis } from '../../stores/redisStore';
import { brand } from '../../styles/theme';

const { Text } = Typography;

const typeIcons: Record<string, { color: string; label: string }> = {
  string: { color: brand.blue.primary, label: 'Str' },
  hash: { color: '#3b82f6', label: 'Hash' },
  list: { color: brand.green.primary, label: 'List' },
  set: { color: brand.red.primary, label: 'Set' },
  zset: { color: brand.purple.primary, label: 'ZSet' },
};

interface TreeNode {
  title: string; key: string; icon?: React.ReactNode; children?: TreeNode[]; isLeaf?: boolean;
}

export default function KeyBrowser() {
  const { activeConnId, keys, selectedKey, loadKeys, loadKeyDetail, deleteKey } = useRedis();
  const { message } = App.useApp();
  const [pattern, setPattern] = useState('*');
  const [loading, setLoading] = useState(false);
  const [expanded, setExpanded] = useState<string[]>([]);

  useEffect(() => { if (activeConnId) refresh(); }, [activeConnId]);

  const refresh = async () => { setLoading(true); await loadKeys(pattern); setLoading(false); };

  const buildTree = useCallback((keyList: string[]): TreeNode[] => {
    const map = new Map<string, TreeNode>();
    for (const k of [...keyList].sort()) {
      const parts = k.split(':'); let path = '';
      for (let i = 0; i < parts.length; i++) {
        const isLast = i === parts.length - 1;
        const parent = path; path = path ? `${path}:${parts[i]}` : parts[i];
        if (!map.has(path)) {
          map.set(path, {
            title: parts[i], key: path,
            icon: isLast ? <KeyOutlined style={{ color: '#f59e0b', fontSize: 12 }} /> : undefined,
            isLeaf: isLast, children: undefined,
          });
        }
        const node = map.get(path)!;
        if (!isLast) { node.isLeaf = false; if (!node.children) node.children = []; }
        if (parent && map.has(parent)) {
          const p = map.get(parent)!;
          if (!p.children?.find((c) => c.key === path)) p.children!.push(node);
        }
      }
    }
    // Collect top-level nodes
    const roots: TreeNode[] = [];
    for (const [k, v] of map) {
      if (!k.includes(':')) roots.push(v);
    }
    return roots;
  }, []);

  if (!activeConnId) {
    return <div style={{ padding: 24, textAlign: 'center' }}><Empty description="请选择连接" image={Empty.PRESENTED_IMAGE_SIMPLE} /></div>;
  }

  return (
    <div style={{ padding: '4px 0' }}>
      <div style={{ padding: '0 12px 6px' }}>
        <Text style={{ color: '#595e73', fontSize: 10, textTransform: 'uppercase', letterSpacing: 1, fontWeight: 600 }}>
          Key 浏览器
        </Text>
      </div>

      <div style={{ padding: '0 12px', marginBottom: 6 }}>
        <Space.Compact style={{ width: '100%' }}>
          <Input size="small" prefix={<SearchOutlined style={{ color: '#595e73' }} />}
            placeholder="pattern..."
            value={pattern} onChange={(e) => setPattern(e.target.value)}
            onPressEnter={refresh} />
          <Button size="small" icon={<ReloadOutlined />} onClick={refresh} loading={loading} />
        </Space.Compact>
      </div>

      <div style={{ overflow: 'auto', flex: 1 }}>
        {loading ? <div style={{ textAlign: 'center', padding: 24 }}><Spin size="small" /></div> :
          keys.length === 0 ? <Text style={{ color: '#595e73', fontSize: 11, padding: '0 12px', display: 'block' }}>暂无 Key</Text> :
            <Tree showIcon treeData={buildTree(keys)}
              selectedKeys={selectedKey ? [selectedKey] : []}
              expandedKeys={expanded} onExpand={(k) => setExpanded(k as string[])}
              onSelect={(k) => { if (k.length > 0) loadKeyDetail(k[0] as string); }}
              titleRender={(node) => (
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', width: '100%', paddingRight: 4 }}>
                  <Text className="truncate mono" style={{ fontSize: 11, flex: 1 }} title={node.key as string}>
                    {node.title as string}
                  </Text>
                  <Space size={0} className="key-actions" style={{ flexShrink: 0, opacity: 0 }}>
                    <Button type="text" size="small" icon={<CopyOutlined style={{ fontSize: 9 }} />}
                      onClick={(e) => { e.stopPropagation(); navigator.clipboard.writeText(node.key as string); message.success('已复制'); }} />
                    <Popconfirm title="删除？" onConfirm={(e) => { e?.stopPropagation(); deleteKey(node.key as string); }}>
                      <Button type="text" size="small" danger icon={<DeleteOutlined style={{ fontSize: 9 }} />}
                        onClick={(e) => e.stopPropagation()} />
                    </Popconfirm>
                  </Space>
                </div>
              )}
              style={{ background: 'transparent', fontSize: 11, padding: '0 8px' }} />
        }
      </div>
      <div style={{ padding: '3px 12px', borderTop: '1px solid #1c1f2d' }}>
        <Text style={{ color: '#595e73', fontSize: 10 }}>{keys.length} keys</Text>
      </div>
      <style>{`.key-actions:hover, .ant-tree-treenode:hover .key-actions { opacity: 1 !important; }`}</style>
    </div>
  );
}
