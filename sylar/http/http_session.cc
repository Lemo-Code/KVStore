#include "http_session.h"
#include "http_parser.h"

namespace sylar {
namespace http {

HttpSession::HttpSession(Socket::ptr sock, bool owner)
    :SocketStream(sock, owner) {
}

//这里GetHttpRequestBufferSize起到一个防止攻击机恶意发送大请求体的作用
//我解析头部完成以后，发现剩余数据（请求体）依旧很大（超出我设定的最大限度了），那么我认定你是恶意攻击
HttpRequest::ptr HttpSession::recvRequest() {
    HttpRequestParser::ptr parser(new HttpRequestParser);
    uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
    //uint64_t buff_size = 100;
    //只向HttpRequest里面存储请求头和请求行的内容
    std::shared_ptr<char> buffer(
            new char[buff_size], [](char* ptr){
                delete[] ptr;
            });
    char* data = buffer.get();
    int offset = 0;
    do {
        int len = read(data + offset, buff_size - offset);
        if(len <= 0) {
            close();
            return nullptr;
        }
        len += offset;
        size_t nparse = parser->execute(data, len);
        if(parser->hasError()) {
            close();
            return nullptr;
        }
        offset = len - nparse;
        if(offset == (int)buff_size) {
            close();
            return nullptr;
        }
        if(parser->isFinished()) {
            break;
        }
    } while(true);
    //框架可能设置成非阻塞（大部分情况都是），那么可能数据还没发送完，我就read返回了，那么还有一部分数据没有解读
    //这种情况并不是error，所以需要循环读取，直到读取完所有数据
    
    //向HttpRequest里面存储请求体的内容（data剩余的数据就是请求体）
    int64_t length = parser->getContentLength();//获取的content-length
    if(length > 0) {
        std::string body;
        body.resize(length);

        int len = 0;
        if(length >= offset) {
            //客户端未按 Content-Length 承诺发送完整请求体，违反 HTTP 协议规范，属于无效请求，需要close
            memcpy(&body[0], data, offset);
            len = offset;
        } else {
            //你指定的数据少，只能听从前端的安排，剩余数据我就不读取了
            memcpy(&body[0], data, length);
            len = length;
        }
        length -= offset;
        if(length > 0) {
            if(readFixSize(&body[len], length) <= 0) {
                close();
                return nullptr;
            }
        }
        parser->getData()->setBody(body);
    }
    //处理connection: keep-alive 或 close
    parser->getData()->init();
    return parser->getData();
}

//粘包：固定字长
int HttpSession::sendResponse(HttpResponse::ptr rsp) {
    std::stringstream ss;
    ss << *rsp;
    std::string data = ss.str();
    return writeFixSize(data.c_str(), data.size());
}

}
}
