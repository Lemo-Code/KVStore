#ifndef __SYLAR_HTTP_SESSION_H__
#define __SYLAR_HTTP_SESSION_H__

#include "sylar/stream/socket_stream.h"
#include "http.h"
#include "sylar/utils/json_util.h"

namespace sylar {
namespace http {
    
//接受http请求->经自动化解析到parser里面->手动设置请求体
//接受处理好的响应，回发响应
class HttpSession : public SocketStream {
public:
    /// 智能指针类型定义
    typedef std::shared_ptr<HttpSession> ptr;

    HttpSession(Socket::ptr sock, bool owner = true);

    HttpRequest::ptr recvRequest();

    int sendResponse(HttpResponse::ptr rsp);
};

}
}

#endif
