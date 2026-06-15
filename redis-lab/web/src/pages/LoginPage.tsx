import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { Card, Form, Input, Button, Tabs, Typography, Space, App } from 'antd';
import { UserOutlined, LockOutlined, MailOutlined, CodeOutlined } from '@ant-design/icons';
import { useAuth } from '../stores/authStore';

const { Title, Text } = Typography;

export default function LoginPage() {
  const nav = useNavigate();
  const { login, register, loading } = useAuth();
  const { message } = App.useApp();
  const [tab, setTab] = useState('login');

  const handleLogin = async (v: { username: string; password: string }) => {
    try { await login(v.username, v.password); message.success('欢迎回来'); nav('/workspace'); }
    catch (e: any) { message.error(e.response?.data?.error || '登录失败'); }
  };

  const handleRegister = async (v: { username: string; email: string; password: string }) => {
    try { await register(v.username, v.email, v.password); message.success('注册成功，请登录'); setTab('login'); }
    catch (e: any) { message.error(e.response?.data?.error || '注册失败'); }
  };

  return (
    <div style={{
      minHeight: '100vh', display: 'flex', background: '#0a0b10', position: 'relative', overflow: 'hidden',
    }}>
      {/* Ambient glow */}
      <div style={{ position: 'absolute', top: '-15%', left: '-5%', width: 500, height: 500, background: 'radial-gradient(circle, rgba(59,130,246,0.06) 0%, transparent 70%)', borderRadius: '50%' }} />
      <div style={{ position: 'absolute', bottom: '-15%', right: '-5%', width: 400, height: 400, background: 'radial-gradient(circle, rgba(34,197,94,0.04) 0%, transparent 70%)', borderRadius: '50%' }} />

      {/* Left: Brand */}
      <div style={{
        flex: 1, display: 'flex', justifyContent: 'center', alignItems: 'center', padding: 40, zIndex: 1,
      }}>
        <div style={{ maxWidth: 400 }}>
          <CodeOutlined style={{ fontSize: 38, color: '#3b82f6', marginBottom: 20, display: 'block' }} />
          <Title level={1} style={{ color: '#e1e4ed', marginBottom: 10, fontSize: 32, fontWeight: 700, letterSpacing: -0.5 }}>
            RedisLab
          </Title>
          <Text style={{ color: '#8b8fa8', fontSize: 15, lineHeight: 1.7, display: 'block', marginBottom: 32 }}>
            企业级 Redis 数据管理与学习协作平台
          </Text>

          <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
            {[
              { t: '可视化数据管理', d: 'Navicat 级 Redis 客户端，Key 浏览、命令执行、多类型数据查看' },
              { t: '实时协作交流', d: '多房间即时通讯，学习团队实时讨论，代码高亮分享' },
              { t: 'AI 智能辅助', d: '上下文感知的 Redis 专家，命令优化建议与概念深度解答' },
            ].map((item) => (
              <div key={item.t} style={{
                padding: '14px 16px', background: 'rgba(255,255,255,0.02)', borderRadius: 8,
                border: '1px solid #1c1f2d',
              }}>
                <Text strong style={{ color: '#e1e4ed', fontSize: 13 }}>{item.t}</Text>
                <br />
                <Text style={{ color: '#595e73', fontSize: 12 }}>{item.d}</Text>
              </div>
            ))}
          </div>
        </div>
      </div>

      {/* Right: Form Card */}
      <div style={{ width: 460, display: 'flex', justifyContent: 'center', alignItems: 'center', padding: 40, zIndex: 1 }}>
        <Card style={{
          width: '100%', background: '#11131a', border: '1px solid #252836', borderRadius: 12,
        }} styles={{ body: { padding: 28 } }}>
          <Tabs activeKey={tab} onChange={setTab} centered size="large"
            items={[
              {
                key: 'login', label: '登录',
                children: (
                  <Form layout="vertical" onFinish={handleLogin} size="large" style={{ marginTop: 12 }}>
                    <Form.Item name="username" rules={[{ required: true, message: '请输入用户名' }]}>
                      <Input prefix={<UserOutlined style={{ color: '#595e73' }} />} placeholder="用户名" />
                    </Form.Item>
                    <Form.Item name="password" rules={[{ required: true, message: '请输入密码' }]}>
                      <Input.Password prefix={<LockOutlined style={{ color: '#595e73' }} />} placeholder="密码" />
                    </Form.Item>
                    <Form.Item style={{ marginTop: 20 }}>
                      <Button type="primary" htmlType="submit" block size="large" loading={loading} style={{ height: 42, fontWeight: 600 }}>
                        登录
                      </Button>
                    </Form.Item>
                  </Form>
                ),
              },
              {
                key: 'register', label: '注册',
                children: (
                  <Form layout="vertical" onFinish={handleRegister} size="large" style={{ marginTop: 12 }}>
                    <Form.Item name="username" rules={[{ required: true, message: '请输入用户名' }]}>
                      <Input prefix={<UserOutlined style={{ color: '#595e73' }} />} placeholder="用户名" />
                    </Form.Item>
                    <Form.Item name="email" rules={[{ required: true, type: 'email', message: '请输入有效邮箱' }]}>
                      <Input prefix={<MailOutlined style={{ color: '#595e73' }} />} placeholder="邮箱" />
                    </Form.Item>
                    <Form.Item name="password" rules={[{ required: true, min: 6, message: '密码至少6位' }]}>
                      <Input.Password prefix={<LockOutlined style={{ color: '#595e73' }} />} placeholder="密码" />
                    </Form.Item>
                    <Form.Item style={{ marginTop: 20 }}>
                      <Button type="primary" htmlType="submit" block size="large" loading={loading} style={{ height: 42, fontWeight: 600 }}>
                        注册
                      </Button>
                    </Form.Item>
                  </Form>
                ),
              },
            ]}
          />
        </Card>
      </div>
    </div>
  );
}
