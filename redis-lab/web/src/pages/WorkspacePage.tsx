import { useEffect, useState } from 'react';
import { Allotment } from 'allotment';
import 'allotment/dist/style.css';
import { Modal, Form, Input, InputNumber, Space, App } from 'antd';
import Header from '../components/layout/Header';
import StatusBar from '../components/layout/StatusBar';
import ConnectionList from '../components/connection/ConnectionList';
import KeyTree from '../components/browser/KeyTree';
import QueryPanel from '../components/editor/QueryPanel';
import ChatPanel from '../components/chat/ChatPanel';
import AiPanel from '../components/ai/AiPanel';
import { useRedis } from '../stores/redisStore';
import { useChat } from '../stores/chatStore';
import { useAI } from '../stores/aiStore';
import type { RightPanelType } from '../types';

export default function WorkspacePage() {
  const { message } = App.useApp();
  const { loadConns, createConn, loadQuota } = useRedis();
  const { loadRooms, connect: wsConnect } = useChat();
  const { loadConvs } = useAI();
  const [rightPanel, setRightPanel] = useState<RightPanelType>(null);
  const [connModal, setConnModal] = useState(false);
  const [form] = Form.useForm();

  useEffect(() => {
    loadConns();
    loadQuota();
    loadRooms();
    loadConvs();
    wsConnect();
  }, []);

  const handleCreateConn = async (v: { name: string; host: string; port: number; password?: string }) => {
    try {
      await createConn(v);
      message.success('连接已创建');
      setConnModal(false);
      form.resetFields();
    } catch (e: any) {
      message.error(e.response?.data?.error || '创建失败');
    }
  };

  return (
    <div style={{ height: '100vh', display: 'flex', flexDirection: 'column', background: '#0a0b10' }}>
      <Header
        rightPanel={rightPanel}
        onToggleRight={setRightPanel}
        onAddConnection={() => { setConnModal(true); }}
      />

      <div style={{ flex: 1, overflow: 'hidden' }}>
        <Allotment>
          <Allotment.Pane preferredSize={260} minSize={200} maxSize={420}>
            <div style={{ height: '100%', display: 'flex', flexDirection: 'column', background: '#0f1117', borderRight: '1px solid #232631' }}>
              <div style={{ flex: 1, overflow: 'auto' }}>
                <ConnectionList onAdd={() => { setConnModal(true); }} />
                <KeyTree />
              </div>
            </div>
          </Allotment.Pane>

          <Allotment.Pane minSize={300}>
            <QueryPanel />
          </Allotment.Pane>

          {rightPanel && (
            <Allotment.Pane preferredSize={340} minSize={280} maxSize={500}>
              <div style={{ height: '100%', background: '#0f1117', borderLeft: '1px solid #232631' }}>
                {rightPanel === 'chat' ? <ChatPanel /> : <AiPanel />}
              </div>
            </Allotment.Pane>
          )}
        </Allotment>
      </div>

      <StatusBar />

      <Modal
        title="新建 Redis 连接"
        open={connModal}
        onCancel={() => { setConnModal(false); }}
        onOk={() => { form.submit(); }}
        okText="创建"
        width={440}
      >
        <Form
          form={form} layout="vertical" onFinish={handleCreateConn}
          initialValues={{ host: '127.0.0.1', port: 6380 }}
        >
          <Form.Item name="name" label="名称" rules={[{ required: true }]}>
            <Input placeholder="本地学习 Redis" />
          </Form.Item>
          <Space>
            <Form.Item name="host" label="主机" rules={[{ required: true }]}>
              <Input />
            </Form.Item>
            <Form.Item name="port" label="端口" rules={[{ required: true }]}>
              <InputNumber min={1} max={65535} />
            </Form.Item>
          </Space>
          <Form.Item name="password" label="密码">
            <Input.Password placeholder="留空" />
          </Form.Item>
        </Form>
      </Modal>
    </div>
  );
}
