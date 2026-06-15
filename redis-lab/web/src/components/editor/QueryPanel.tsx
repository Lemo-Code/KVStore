import { useState, useRef, useEffect } from 'react';
import { Typography } from 'antd';
import { PlayCircleOutlined, ClearOutlined } from '@ant-design/icons';
import Editor, { OnMount } from '@monaco-editor/react';
import { useRedis } from '../../stores/redisStore';

const { Text } = Typography;
const C = { bg:'#0a0b10', bg2:'#11131a', border:'#232631', text:'#cdd0db', dim:'#878b9e', muted:'#5c6178', blue:'#3b82f6', green:'#22c55e', red:'#ef4444' };

const CMDS = [
  'GET','SET','DEL','EXISTS','TYPE','KEYS','EXPIRE','TTL','PEXPIRE','PTTL','PERSIST',
  'INCR','INCRBY','DECR','DECRBY','APPEND','STRLEN','MGET','MSET','PING','ECHO','INFO',
  'DBSIZE','FLUSHDB','RANDOMKEY','SCAN','HGET','HSET','HGETALL','HKEYS','HVALS','HDEL',
  'LPUSH','RPUSH','LPOP','RPOP','LRANGE','LLEN','SADD','SREM','SMEMBERS','SISMEMBER',
  'ZADD','ZREM','ZRANGE','ZRANK','ZCARD',
];

export default function QueryPanel() {
  const { activeConnId, getActiveConn, exec, results, history, executing } = useRedis();
  const [cmd, setCmd] = useState('');
  const editorRef = useRef<any>(null);
  const conn = getActiveConn();
  const result = results[0];

  useEffect(() => { setCmd(''); }, [activeConnId]);

  const handleExec = async () => {
    if (!activeConnId || !cmd.trim() || executing) { return; }
    try { await exec(cmd.trim()); } catch { /* */ }
  };

  const handleMount: OnMount = (editor, monaco) => {
    editorRef.current = editor;
    monaco.languages.registerCompletionItemProvider('redis', {
      provideCompletionItems: (model: any, pos: any) => {
        const w = model.getWordUntilPosition(pos);
        return {
          suggestions: CMDS.map((c) => ({
            label: c, kind: monaco.languages.CompletionItemKind.Keyword,
            insertText: c, detail: 'Redis',
            range: { startLineNumber: pos.lineNumber, endLineNumber: pos.lineNumber, startColumn: w.startColumn, endColumn: w.endColumn },
          })),
        };
      },
    });
    editor.addAction({
      id: 'exec', label: 'Execute',
      keybindings: [monaco.KeyMod.CtrlCmd | monaco.KeyCode.Enter],
      run: handleExec,
    });
  };

  return (
    <div style={{ height:'100%', display:'flex', flexDirection:'column', background:C.bg }}>
      {/* Toolbar */}
      <div style={{ display:'flex', alignItems:'center', gap:8, padding:'7px 14px', background:C.bg2, borderBottom:`1px solid ${C.border}` }}>
        {conn ? (
          <div style={{ display:'flex', alignItems:'center', gap:6 }}>
            <span style={{ width:7,height:7,borderRadius:'50%',background: executing ? '#f59e0b' : C.green, boxShadow: executing ? 'none' : `0 0 3px ${C.green}80` }} />
            <Text style={{ color:C.text, fontFamily:'monospace', fontSize:11, fontWeight:500 }}>{conn.name} {'>'}</Text>
          </div>
        ) : (
          <Text style={{ color:C.muted, fontSize:11 }}>未连接</Text>
        )}

        <div style={{ flex:1 }} />

        {/* History selector */}
        <select
          onChange={(e) => { if (e.target.value) { setCmd(e.target.value); editorRef.current?.setValue(e.target.value); } }}
          style={{ background:'none', border:'none', color:C.muted, fontSize:11, outline:'none', cursor:'pointer' }}
        >
          <option value="">历史...</option>
          {history.slice(0, 20).map((h, i) => (
            <option key={i} value={h}>{h.length > 60 ? h.slice(0, 60) + '...' : h}</option>
          ))}
        </select>

        <button
          onClick={() => { setCmd(''); if (editorRef.current) { editorRef.current.setValue(''); } }}
          style={{ background:'none', border:'none', color:C.muted, cursor:'pointer', fontSize:14, padding:'0 4px' }}
          title="清除"
        >
          <ClearOutlined />
        </button>

        <button
          onClick={handleExec}
          disabled={!activeConnId || !cmd.trim() || executing}
          style={{
            display:'flex', alignItems:'center', gap:4, background:C.green, border:'none', borderRadius:4,
            color:'#fff', cursor:'pointer', fontSize:12, fontWeight:600, padding:'5px 12px',
            opacity: (!activeConnId || !cmd.trim()) ? 0.4 : 1,
          }}
        >
          <PlayCircleOutlined />
          {executing ? '执行中...' : '执行'}
        </button>
      </div>

      {/* Monaco Editor */}
      <div style={{ borderBottom:`1px solid ${C.border}`, height:140, flexShrink:0 }}>
        <Editor
          height={138} language="redis" theme="vs-dark" value={cmd}
          onChange={(v) => { setCmd(v || ''); }} onMount={handleMount}
          options={{
            fontSize:13, fontFamily:"'JetBrains Mono','Fira Code',Consolas,monospace",
            minimap:{ enabled:false }, scrollBeyondLastLine:false, wordWrap:'on',
            padding:{ top:10 }, lineNumbersMinChars:3, overviewRulerLanes:0,
            renderLineHighlight:'line', glyphMargin:false, lineDecorationsWidth:2,
            automaticLayout:true,
          }}
        />
      </div>

      {/* Result area */}
      <div style={{ flex:1, overflow:'auto', padding:14 }}>
        {executing ? (
          <Text style={{ color:C.muted, fontSize:12 }}>执行中...</Text>
        ) : result ? (
          <ResultView value={result} />
        ) : (
          <Text style={{ color:C.muted, fontSize:12 }}>输入 Redis 命令，Ctrl+Enter 执行</Text>
        )}
      </div>
    </div>
  );
}

// Simple result renderer
function ResultView({ value }: { value: { type:string; value:any } }) {
  if (value.type === 'error') {
    return (
      <div style={{ padding:'8px 14px', background:'rgba(239,68,68,0.06)', border:'1px solid rgba(239,68,68,0.15)', borderRadius:6 }}>
        <Text style={{ color:'#fca5a5', fontSize:12, fontFamily:'monospace', whiteSpace:'pre-wrap' }}>{String(value.value)}</Text>
      </div>
    );
  }
  if (value.type === 'null') {
    return <Text style={{ color:C.muted, fontFamily:'monospace', fontSize:12 }}>(nil)</Text>;
  }
  if (value.type === 'integer') {
    return <Text style={{ color:C.green, fontSize:15, fontFamily:'monospace', fontWeight:600 }}>(integer) {String(value.value)}</Text>;
  }
  if (value.type === 'string') {
    const s = String(value.value);
    if (s === 'OK' || s === 'PONG') {
      return <Text style={{ color:C.green, fontFamily:'monospace', fontSize:13, fontWeight:600 }}>{s}</Text>;
    }
    return (
      <pre style={{ padding:'10px 14px', background:'rgba(255,255,255,0.015)', border:`1px solid ${C.border}`, borderRadius:6, fontFamily:"'JetBrains Mono',monospace", fontSize:12, whiteSpace:'pre-wrap', wordBreak:'break-all', color:C.text, margin:0 }}>
        {s}
      </pre>
    );
  }
  if (value.type === 'array' && Array.isArray(value.value)) {
    const arr = value.value as any[];
    return (
      <div>
        <Text style={{ color:C.muted, fontSize:10, marginBottom:4, display:'block' }}>Array ({arr.length} items)</Text>
        <div style={{ border:`1px solid ${C.border}`, borderRadius:6, overflow:'hidden' }}>
          {arr.map((v, i) => (
            <div key={i} style={{
              padding:'5px 10px', display:'flex', gap:8, fontFamily:"'JetBrains Mono',monospace", fontSize:11,
              background: i%2===0 ? '#0a0b10' : '#0f1117', borderBottom: i<arr.length-1?`1px solid ${C.border}`:'none',
            }}>
              <span style={{ color:C.muted, minWidth:24 }}>{i+1})</span>
              <span style={{ color:C.text, wordBreak:'break-all' }}>{String(v)}</span>
            </div>
          ))}
        </div>
      </div>
    );
  }
  return <pre style={{ color:C.dim, fontSize:11 }}>{JSON.stringify(value.value)}</pre>;
}
