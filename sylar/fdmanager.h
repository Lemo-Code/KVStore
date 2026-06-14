#ifndef __SYLAR_FD_MANAGER_HH
#define __SYLAR_FD_MANAGER_HH
#include <memory>
#include <vector>
#include "sylar/thread.h"
#include "sylar/hook.h"
#include "sylar/singleton.h"
#include "sylar/mutex.h"

/**
 * 识别文件句柄的状态
 * 是否认为设置nonblock
 * 超时时间
 * ...
 */
namespace sylar
{//文件句柄识别类
    class FdCtx : public std::enable_shared_from_this<FdCtx>
    {
    public:
        typedef std::shared_ptr<FdCtx>ptr;
        FdCtx(int fd);
        ~FdCtx();

        bool init();
        bool isInit() const {return isInit_;}
        bool isSocket() const{return isSocket_;}
        bool isClose() const {return isClosed_;}

        void setUserNonBlock(bool v) {userNonBlock_ = v;}
        bool getUserNonBlock() const{return userNonBlock_;}

        void setSysNonBlock(bool v){sysNonBlock_ = v;}
        bool getSysNonBlock()const {return sysNonBlock_;}

        void setTimeout(int type,uint64_t v);
        uint64_t getTimeout(int type);

    private:
        bool isInit_: 1;
        bool isSocket_: 1;
        bool sysNonBlock_: 1;
        bool userNonBlock_: 1;
        bool isClosed_: 1;
        int fd_;
        uint64_t recvTimeout_;
        uint64_t sendTimeout_;
    };
//文件句柄识别类
//区分文件句柄 是否是socket
    class FdManager
    {
    public:
        typedef RWMutex RWMutexType;
        FdManager();

        FdCtx::ptr get(int fd,bool auto_create = false);
        void del(int fd);

    private:
        RWMutexType mutex_;
        std::vector<FdCtx::ptr> datas_;
    };

    typedef Singleton<FdManager> FdMgr;
}

#endif