import { useState, useRef, useEffect } from 'react';
import { Button, Space, Typography, App, Spin, Dropdown, Tooltip } from 'antd';
import { PlayCircleOutlined, ClearOutlined, HistoryOutlined } from '@ant-design/icons';
import Editor, { OnMount } from '@monaco-editor/react';
import { useRedis } from '../../stores/redisStore';
import ResultViewer from '../result/ResultViewer';

const { Text } = Typography;

const COMMANDS = ['GET','SET','DEL','EXISTS','TYPE','KEYS','EXPIRE','TTL','PEXPIRE','PTTL','PERSIST',
  'INCR','INCRBY','DECR','DECRBY','APPEND','STRLEN','MGET','MSET','PING','ECHO','INFO','DBSIZE',
  'FLUSHDB','RANDOMKEY','SELECT','SCAN','HGET','HSET','HGETALL','HKEYS','HVALS','HDEL','HLEN',
  'LPUSH','RPUSH','LPOP','RPOP','LRANGE','LLEN','LINDEX','SADD','SREM','SMEMBERS','SISMEMBER','SCARD',
  'ZADD','ZREM','ZRANGE','ZRANK','ZCARD','ZSCORE'];

export default function QueryEditor() {
  const { activeConnId, getActiveConn, exec, results, history, executing } = useRedis();
  const { message } = App.useApp();
  const [cmd, setCmd] = useState('');
  const [tabIdx] = useState(0);
  const editorRef = useRef<any>(null);

  useEffect(() => {
    // Reset when connection changes
    setCmd('');
  }, [activeConnId]);

  const handleMount: OnMount = (editor, monaco) => {
    editorRef.current = editor;
    monaco.languages.registerCompletionItemProvider('redis', {
      provideCompletionItems: (model: any, pos: any) => {
        const w = model.getWordUntilPosition(pos);
        return {
          suggestions: COMMANDS.map((c) => ({
            label: c, kind: monaco.languages.CompletionItemKind.Keyword,
            insertText: c, detail: 'Redis command',
            range: { startLineNumber: pos.lineNumber, endLineNumber: pos.lineNumber, startColumn: w.startColumn, endColumn: w.endColumn },
          })),
        };
      },
    });
    editor.addAction({
      id: 'exec', label: 'Execute',
      keybindings: [monaco.KeyMod.CtrlCmd | monaco.KeyCode.Enter],
      run: () => handleExec(),
    });
  };

  const handleExec = async () => {
    if (!activeConnId || !cmd.trim() || executing) return;
    try { await exec(cmd.trim(), tabIdx); } catch (e: any) { message.error(e.message); }
  };

  const handleClear = () => { setCmd(''); editorRef.current?.setValue(''); };

  const conn = getActiveConn();
  const result = results[tabIdx];

  const statusColor = !activeConnId ? '#595e73' : executing ? '#f59e0b' : '#22c55e';
  const statusText = !activeConnId ? '未连接' : executing ? '执行中...' : conn ? `${conn.name} >` : '就绪';

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/* Toolbar */}
      <div style={{
        display: 'flex', alignItems: 'center', gap: 6,
        padding: '6px 12px', background: '#11131a', borderBottom: '1px solid #252836',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
          <span className="status-dot" style={{
            background: statusColor, width: 6, height: 6,
            boxShadow: statusColor === '#22c55e' ? '0 0 4px rgba(34,197,94,0.4)' : 'none',
          }} />
          <Text style={{ color: '#8b8fa8', fontFamily: 'monospace', fontSize: 11, fontWeight: 500 }}>
            {statusText}
          </Text>
        </div>

        <div style={{ flex: 1 }} />

        <Space size={2}>
          <Dropdown menu={{
            items: history.slice(0, 20).map((h, i) => ({
              key: i, label: <Text style={{ fontFamily: 'monospace', fontSize: 11 }} className="truncate" title={h}>{h.length > 60 ? h.slice(0, 60) + '...' : h}</Text>,
              onClick: () => { setCmd(h); editorRef.current?.setValue(h); },
            })),
          }} trigger={['click']}>
            <Tooltip title="命令历史">
              <Button type="text" size="small" icon={<HistoryOutlined />} style={{ color: '#8b8fa8' }} />
            </Tooltip>
          </Dropdown>
          <Tooltip title="清除 (Ctrl+L)">
            <Button type="text" size="small" icon={<ClearOutlined />} onClick={handleClear} style={{ color: '#8b8fa8' }} />
          </Tooltip>
          <Tooltip title="执行 (Ctrl+Enter)">
            <Button type="primary" size="small" icon={<PlayCircleOutlined />}
              onClick={handleExec} loading={executing} disabled={!activeConnId || !cmd.trim()}
              style={{ background: '#22c55e', borderColor: '#22c55e', fontWeight: 600, marginLeft: 4 }}>
              执行
            </Button>
          </Tooltip>
        </Space>
      </div>

      {/* Editor */}
      <div style={{ borderBottom: '1px solid #252836' }}>
        <Editor height={130} language="redis" theme="vs-dark" value={cmd}
          onChange={(v) => setCmd(v || '')} onMount={handleMount}
          options={{
            fontSize: 13, fontFamily: "'JetBrains Mono','Fira Code',Consolas,monospace",
            minimap: { enabled: false }, scrollBeyondLastLine: false, wordWrap: 'on',
            padding: { top: 10 }, lineNumbersMinChars: 3, overviewRulerLanes: 0,
            renderLineHighlight: 'line', glyphMargin: false, folding: true,
            lineDecorationsWidth: 2, automaticLayout: true,
          }} />
      </div>

      {/* Result */}
      <div style={{ flex: 1, overflow: 'auto', padding: 12 }}>
        {executing ? (
          <div style={{ textAlign: 'center', padding: 24 }}>
            <Spin size="small" />
            <br /><Text style={{ color: '#595e73', fontSize: 11, marginTop: 6, display: 'block' }}>执行中...</Text>
          </div>
        ) : result ? (
          <ResultViewer result={result} />
        ) : (
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%', opacity: 0.4 }}>
            <Text style={{ color: '#595e73', fontSize: 12 }}>输入 Redis 命令，Ctrl+Enter 执行</Text>
          </div>
        )}
      </div>
    </div>
  );
}
