#ifndef __SYLAR_SOCK_STREAM_H__
#define __SYLAR_SOCK_STREAM_H__

#include "sylar/stream.h"
#include "sylar/socket.h"

#include <memory>
namespace sylar
{

class SocketStream:public Stream
{
public:
    typedef std::shared_ptr<SocketStream> ptr;
    SocketStream(Socket::ptr sock, bool owner = true);
    ~SocketStream();

    virtual int read(void* buffer,size_t length) override;
    virtual int read(ByteArray::ptr ba, size_t length) override;
    virtual int read(Ringbuffer::ptr rb, size_t length) override;
        
    virtual int write(const void* buffer, size_t length) override;
    virtual int write(ByteArray::ptr ba, size_t length) override;
    virtual int write(Ringbuffer::ptr rb, size_t length) override;
    virtual void close() override;

    bool isConnected()const ;
    Socket::ptr getSocket() const{return socket_;}
protected:
    Socket::ptr socket_;
    bool owner_;
};

}


#endif