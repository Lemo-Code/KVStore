#ifndef __SYLAR_HTTP_HTTP_SERVER_H__
#define __SYLAR_HTTP_HTTP_SERVER_H__

#include "sylar/tcp_server.h"
#include "http_session.h"
#include "servlet.h"

namespace sylar {
namespace http {

//1.创建会话处理请求->创建响应体->交给分发器disptch找到合适的servlet处理
//  servlet组装好响应体->回发响应体。
//2.涉及到一个keepalive，若是客户端设置了keepalive,服务端不会退出循环，当且仅当
//  客户端主动调用close，或者长时间没有心跳包导致异常断开链接，才会退出while
class HttpServer : public TcpServer {
public:
    /// 智能指针类型
    typedef std::shared_ptr<HttpServer> ptr;

    HttpServer(bool keepalive = false
               ,sylar::IOManager* worker = sylar::IOManager::GetThis()
               ,sylar::IOManager* io_worker = sylar::IOManager::GetThis()
               ,sylar::IOManager* accept_worker = sylar::IOManager::GetThis());
    ServletDispatch::ptr getServletDispatch() const { return m_dispatch;}

    void setServletDispatch(ServletDispatch::ptr v) { m_dispatch = v;}

    virtual void setName(const std::string& v) override;
protected:
    virtual void handleClient(Socket::ptr client) override;
private:
    /// 是否支持长连接
    bool m_isKeepalive;
    /// Servlet分发器
    ServletDispatch::ptr m_dispatch;
};

}
}

#endif
