#include "sylar/stream/socket_stream.h"


namespace sylar {

SocketStream::SocketStream(Socket::ptr sock, bool owner) 
    :socket_(sock)
    ,owner_(owner){
}

SocketStream::~SocketStream() {
    if(owner_ && socket_){
        socket_->close();
    }
} 

bool SocketStream::isConnected()const{
    return socket_ && socket_->isConnected();
}

//从套接字读 写入buffer
int SocketStream::read(void* buffer,size_t length) {
    if(!isConnected()){
        return -1;
    }
    return socket_->recv(buffer,length);
}

//从套接字读 写入buffer(bytearray的作用)(两个方法的区别)
int SocketStream::read(ByteArray::ptr ba, size_t length) {
    if(!isConnected()){
        return -1;
    }
    std::vector<iovec> iovs;
    ba->getWriteBuffers(iovs,length);
    int rt = socket_->recv(&iovs[0],iovs.size());
    if(rt > 0){
        ba->setPosition(ba->getPosition() + rt);
    }
    return rt;
}

//从buffer取数据 写入套接字
int SocketStream::write(const void* buffer, size_t length) {
    if(!isConnected()){
        return -1;
    }
    return socket_->send(buffer,length);
}

//从buffer取数据 写入套接字
int SocketStream::write(ByteArray::ptr ba, size_t length) {
    if (!isConnected()){
        return -1;
    }
    std::vector<iovec> iovs;
    ba->getReadBuffers(iovs, length);
    int rt = socket_->send(&iovs[0],iovs.size());
    if(rt > 0){
        ba->setPosition(ba->getPosition() + rt);
    }
    return rt;
}
void SocketStream::close() {
    if(socket_){
        socket_->close();
    }
}

//从buffer取数据 写入套接字
// 修复 write 方法：使用传入的 length 参数限制发送长度
int SocketStream::write(Ringbuffer::ptr rb, size_t length) {
    if (!isConnected()){
        return -1;
    }
    // 取实际可发送长度（取缓冲区可读字节数与传入 length 的最小值）
    size_t send_len = std::min(rb->readableBytes(), length);
    int rt = socket_->send(rb->peek(), send_len);
    if (rt > 0) {
        rb->retrieve(rt); // 仅在发送成功时移动缓冲区指针
    }
    return rt;
}

// 修复 read 方法：正确初始化接收缓冲区
int SocketStream::read(Ringbuffer::ptr rb, size_t length) {
    if (!isConnected()){
        return -1;
    }
    std::string buff(length, '\0'); // 直接分配并初始化指定长度的内存
    int rt = socket_->recv(&buff[0], length);
    if (rt > 0) {
        rb->append(buff.data(), rt); // 仅追加实际接收的字节数
    }
    return rt;
}



}