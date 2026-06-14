#ifndef __SYLAR_LOG_H__
#define __SYLAR_LOG_H__
#include"sylar/util.h"
#include"sylar/singleton.h"
#include"sylar/mutex.h"
#include "sylar/asyc_log.h"

#include<string>
#include<stdint.h>
#include<memory>
#include<vector>
#include<list>
#include<deque>
#include<fstream>
#include<sstream>
#include<map>
#include<functional>
#include<time.h>
#include<stdarg.h>
#include<map>

#define SYLAR_LOG_LEVEL(logger,level)       \
    if(logger->getLevel() <= level)         \
        sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger,level,__FILE__,__LINE__,0,sylar::GetThreadId()   \
            ,sylar::GetFiberId(),time(0),sylar::GetThreadName()))).getSS()  

#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger,sylar::LogLevel::DEBUG)
#define SYLAR_LOG_INFO(logger)  SYLAR_LOG_LEVEL(logger,sylar::LogLevel::INFO)
#define SYLAR_LOG_WARN(logger)  SYLAR_LOG_LEVEL(logger,sylar::LogLevel::WARN)
#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger,sylar::LogLevel::ERROR)
#define SYLAR_LOG_FATAL(logger) SYLAR_LOG_LEVEL(logger,sylar::LogLevel::FATAL)

#define SYLAR_LOG_FMT_LEVEL(logger,level,fmt,...)   \
    if((logger)->getLevel() <= (level))   \
        sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger,level,  \
            __FILE__,__LINE__,0,sylar::GetThreadId()        \
            ,sylar::GetFiberId(),time(0),sylar::GetThreadName()))).getEvent()->format(fmt,__VA_ARGS__)

#define SYLAR_LOG_FMT_DEBUG(logger,fmt,...) SYLAR_LOG_FMT_LEVEL(logger,sylar::LogLevel::DEBUG,fmt,__VA_ARGS__)
#define SYLAR_LOG_FMT_INFO(logger,fmt,...)  SYLAR_LOG_FMT_LEVEL(logger,sylar::LogLevel::INFO,fmt,__VA_ARGS__)
#define SYLAR_LOG_FMT_WARN(logger,fmt,...)  SYLAR_LOG_FMT_LEVEL(logger,sylar::LogLevel::WARN,fmt,__VA_ARGS__)
#define SYLAR_LOG_FMT_ERROR(logger,fmt,...) SYLAR_LOG_FMT_LEVEL(logger,sylar::LogLevel::ERROR,fmt,__VA_ARGS__)
#define SYLAR_LOG_FMT_FATAL(logger,fmt,...) SYLAR_LOG_FMT_LEVEL(logger,sylar::LogLevel::FATAL,fmt,__VA_ARGS__)

//自动asyc
#define SYLAR_LOG_ROOT() sylar::LoggerMgr::GetInstance()->getRoot()
#define SYLAR_LOG_NAME(name) sylar::LoggerMgr::GetInstance()->getLogger(name)

//手动asyc
#define SYLAR_ASYC_LOG_ROOT() sylar::AsycLoggerMgr::GetInstance()->getRoot()
#define SYLAR_ASYC_LOG_NAME(name) sylar::AsycLoggerMgr::GetInstance()->getLogger(name)


namespace sylar {
class Logger;

//日志级别
class LogLevel
{
public:
    enum Level
    {
        UNKNOWN = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5,
    };
    //将Level类型的日志级别转成string类型
    static const char* ToString(LogLevel::Level level);
    //将string转成Level
    static LogLevel::Level FromString(const std::string& str);
};

//日志事件
class LogEvent 
{
public:
    typedef std::shared_ptr<LogEvent> ptr;
    //参数的初始化
    LogEvent(std::shared_ptr<Logger>logger,LogLevel::Level level 
        ,const char* file,int32_t line,uint32_t elapse
        ,uint32_t threadId,uint32_t fiberId
        ,uint64_t time,const std::string& threadname);

    //获得日志输出的文件名（代码）
    const char* getFile()const {return file_;}
    //获得日志输出的行号（代码）
    int32_t getLine()const {return line_;}
    //获得服务器启动的时间
    uint32_t getElapse()const {return elapse_;}
    //获得日志输出的线程号
    uint32_t getThreadId()const {return threadId_;}
    //获得日志输出的协程号
    uint32_t getFilberId()const {return fiberId_;}
    //获得日志产生的时间
    uint64_t getTime()const {return time_;}
    //获得event属于的logger
    std::shared_ptr<Logger> getLogger()const {return logger_;}
    //获得日志的级别阈值
    LogLevel::Level getLevel()const {return level_;}
    //自己想输出的内容 log << "hello"  ss_->hello
    std::stringstream& getSS() {return ss_;}
    //获得日志输出的线程名
    std::string getThreadName()const{return threadName_;}
    //可变参的形式给event传值
    void format(const char* fmt,...);
    //format... 调用的底层方法
    void format(const char* fmt,va_list al);
    //清空ss_
    void resetSS();
    //重置event
    void reset(std::shared_ptr<Logger> logger,LogLevel::Level level 
        ,const char* file,int32_t line,uint32_t elapse
        ,uint32_t threadId,uint32_t fiberId
        ,uint64_t time,const std::string& threadname);
private:
    const char* file_ = nullptr;        //文件名（代码文件）
    int32_t line_ = 0;                  //行号（代码行号）
    uint32_t elapse_ = 0;               //服务器启动的时间
    uint32_t threadId_ = 0;             //线程号
    uint32_t fiberId_ = 0;              //协程号
    uint64_t time_ = 0;                 //当前时间
    std::string threadName_ = "unknow"; //线程名
    std::stringstream ss_;              //日志输出的内容（message）
    std::shared_ptr<Logger> logger_;    //logger（主要是方便获得logger的成员）
    LogLevel::Level level_;             //日志的级别阈值
};

/**
 * 单次创建一个event的方案输出日志，输出完即进行析构
 * 解决了有时候程序结束了 缓存中的数据都不能刷新出来的问题
 */
//日志事件包装器
class LogEventWrap
{
public:
    //初始化
    LogEventWrap(LogEvent::ptr event);
    //析构的时候日志自动输出
    ~LogEventWrap();
    //把message输出到event的getSS() -> 把容器存入event
    std::stringstream& getSS();
    //获得event
    LogEvent::ptr getEvent()const {return event_;}
private:
    LogEvent::ptr event_;   //日志事件
};

//日志格式器
class LogFormatter
{
public:
    typedef std::shared_ptr<LogFormatter> ptr;
    //init()
    LogFormatter(const std::string& pattern);
    //%a%t%b%t%n  ->  把a t b t n解析成对应的FormatItem，按顺序存储到vec中
    void init();
    //遍历vec，按顺序把结果放入ss流
    std::string format(std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event);
    
    //获得日志的格式
    std::string getPattern()const {return pattern_;}
    //设置格式器
    void setForm(const std::string& fmt = ""){pattern_ = fmt;}
    //格式是否是空的
    bool isPatternEmpty()const {return pattern_.empty();}
    //格式解析是否有错
    bool isError()const {return has_error;}
    //格式解析是否正确
    bool isTrue() const {return !has_error;}
public:
    class FormatItem
    {
    public:
        typedef std::shared_ptr<FormatItem> ptr;
        //提供一个适配的构造
        FormatItem(const std::string& str = ""){}
        virtual ~FormatItem() {}
        //把指定的参数传入os
        virtual void format(std::ostream& os,std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event) = 0;
    };
private:
    std::string pattern_;               //日志的格式
    std::vector<FormatItem::ptr> items_;//Item格式器
    bool has_error = false;             //格式是否有错误
};

//日志输出地
//level_最好设一个初始化参数 不然的话有时候程序完成编译一首 level_的默认参数不一定是0，所以导致日志不能输出到终端
//而不是没刷新缓冲区的问题
class LogAppender
{
friend class Logger;
public:
    typedef sylar::Spinlock MutexType;
    typedef std::shared_ptr<LogAppender> ptr;
    LogAppender(const std::string& name = "") {}
    virtual ~LogAppender() {}

    //输出到指定目的地
    virtual void log(std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event,bool isAysc) = 0;
    //设置日志的格式
    void setFormatter(LogFormatter::ptr formatter);
    //获得日志的格式器
    LogFormatter::ptr getFormatter();
    //获得日志输出地的级别阈值
    LogLevel::Level getLevel()const {return level_;}
    //设置日志输出地的级别阈值
    void setLevel(LogLevel::Level level) {level_ = level;}
    //配置相关的参数
    virtual std::string getTypeName() = 0;
    virtual std::string toYamlString() = 0;
    // 刷缓冲到设备（文件/终端），同步 Logger 可用
    virtual void flush() {}
    //是否有日志格式器
    bool HasFormatter()const {return has_Formatter;}
protected:
    LogLevel::Level level_ = LogLevel::DEBUG;   //日志输出地的级别阈值
    LogFormatter::ptr formatter_ = nullptr;     //日志的格式器
    bool has_Formatter = false;                 //是否有日志格式器
    MutexType mutex_;                           //互斥锁
    AsyncLogChannel::ptr async_;
};

//输出到终端的Appender
class StdoutLogAppender: public LogAppender
{
friend class Logger;
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;
    StdoutLogAppender(const std::string& name = "");
    void flush() override;
    //把appender的变量转成yaml格式（日志配置整合）
    std::string toYamlString()override;
    //日志配置整合
    std::string getTypeName()override {return "stdOutLogAppender";}
    //输出到终端
    void log(std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event,bool isAsyc)override;
};

//输出到文件的Appender（单文件，无轮巡）
class FileLogAppender: public LogAppender
{
friend class Logger;
public:
    typedef std::shared_ptr<FileLogAppender> ptr;
    FileLogAppender(const std::string& filename);
    std::string getTypeName()override {return "FileLogAppender";}
    std::string toYamlString()override;
    void flush() override;
    void log(std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event,bool isAsyc) override;
    bool reopen();
private:
    std::string name_;
    std::ofstream filestream_;
    uint64_t lastTime_;
};

// 输出到 stderr（错误流，独立于 stdout）
class StderrLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<StderrLogAppender> ptr;
    StderrLogAppender(const std::string& name = "");
    void flush() override;
    std::string getTypeName() override { return "StderrLogAppender"; }
    std::string toYamlString() override;
    void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event, bool isAsyc) override;
};

// 轮巡文件：按大小和/或按时间切割，可选保留数量与压缩
// roll_interval: none | hour | day
// 仅同步输出（isAsyc=false）时轮巡生效；异步时退化为单文件追加
class RollingFileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<RollingFileLogAppender> ptr;
    RollingFileLogAppender(const std::string& filepath, uint64_t max_file_size = 100 * 1024 * 1024,
                           uint32_t max_files = 10, const std::string& roll_interval = "day", bool compress = false);
    std::string getTypeName() override { return "RollingFileLogAppender"; }
    std::string toYamlString() override;
    void flush() override;
    void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event, bool isAsyc) override;
private:
    void rollBySize();
    void rollByTime();
    void openCurrent();
    bool compressFile(const std::string& path) const;
    std::string filepath_;
    uint64_t max_file_size_;
    uint32_t max_files_;
    std::string roll_interval_;  // none | hour | day
    bool compress_;
    std::ofstream filestream_;
    uint64_t current_size_;
    uint64_t last_roll_time_;    // 上次按时间切割的 time_t 粒度
};

// 内存循环缓冲：仅保留最新 N 条，不落盘，供 getRecentLines() 读取
class MemoryBufferLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<MemoryBufferLogAppender> ptr;
    explicit MemoryBufferLogAppender(size_t max_lines = 1000);
    std::string getTypeName() override { return "MemoryBufferLogAppender"; }
    std::string toYamlString() override;
    void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event, bool isAsyc) override;
    // 获取最近若干条（从旧到新）
    std::vector<std::string> getRecentLines(size_t max_lines = 0) const;
    size_t getMaxLines() const { return max_lines_; }
private:
    size_t max_lines_;
    mutable sylar::Mutex mutex_;
    std::deque<std::string> lines_;
};

//日志器
class Logger:public std::enable_shared_from_this<Logger>
{
friend class AsycLoggerManager;
friend class LoggerManager;
public:
    typedef Spinlock MutexType;
    typedef std::shared_ptr<Logger> ptr;

    //参数的初始化
    Logger(const std::string& name = "root",bool isAsyc = false);
    //输出到不同的目的地
    void log(LogLevel::Level level,LogEvent::ptr event);
    //输出debug级别的日志
    void debug(LogEvent::ptr event);
    //输出info级别的日志
    void info(LogEvent::ptr event);
    //输出warn级别的日志
    void warn(LogEvent::ptr event);
    //输出error级别的日志
    void error(LogEvent::ptr event);
    //输出fatal级别的日志
    void fatal(LogEvent::ptr event);

    //添加日志输出地
    void addAppender(LogAppender::ptr appender);
    //删除日志输出地
    void delAppender(LogAppender::ptr appender);
    //清空日志输出地
    void clearAppenders();
    // 刷缓冲到设备（对同步 Logger 保证落盘/终端）
    void flush();

    //获得vec容器的大小
    size_t getAppendersSize()const{return appenders_.size();}
    //指定的appender是否存在
    bool isAppenderExists(LogAppender::ptr appender);

    //设置日志的格式器
    void setFormatter(LogFormatter::ptr val);
    //设置日志的格式
    void setFormatter(const std::string& val);
    //获得日志的格式
    std::string getFormatterStr()const;
    //获得日志的格式器
    LogFormatter::ptr getFormatter()const;
    //获得日志器级别阈值
    LogLevel::Level getLevel()const {return level_;}
    //设置日志器级别阈值
    void setLevel(LogLevel::Level level) {level_ = level;}
    //设置日志名称
    void setName(const std::string& name) {name_ = name;}
    //获取日志的名称
    std::string getName()const {return name_;}
    //logger的成员转成yaml（配置整合）
    std::string toYamlString();
    //设置异步
    void setAsyc(bool isAsyc) {isAsyc_ = isAsyc;}
    //获得异步方式
    bool getAsyc() const {return isAsyc_;}
private:
    bool isAsyc_ = false;
    std::string name_ = "root";                //日志名称
    LogLevel::Level level_ = LogLevel::DEBUG; //日志级别阈值(最好给初始值)
    LogFormatter::ptr formatter_ = nullptr;   //日志格式器
    std::list<LogAppender::ptr>appenders_;    //日志输出地
    Logger::ptr root_;          //默认的root日志器（配置整合）
    MutexType mutex_;           //互斥锁
};


class LoggerManager
{
public:
    typedef Spinlock MutexType;
    typedef std::map<std::string,Logger::ptr> LoggerMAP;
    //设置root_ ，并在LoggerMAP中存入 root - logger
    LoggerManager();
    //获得指定name 的logger //如果没有则创建，并在logger中设置root_
    Logger::ptr getLogger(const std::string& name);
    //获得root_
    Logger::ptr getRoot();
    //把全部的logger转成yaml格式（配置整合）
    std::string toYamlString();

private:
    LoggerMAP loggers_; //存储logger
    //修改配置文件中的root 那么没有给出定义的logger也会受影响
    Logger::ptr root_;  //默认logger
    MutexType mutex_;   //互斥锁
};

class AsycLoggerManager
{
public:
    typedef Spinlock MutexType;
    typedef std::map<std::string,Logger::ptr> LoggerMAP;
    //设置root_ ，并在LoggerMAP中存入 root - logger
    AsycLoggerManager();
    //获得指定name 的logger //如果没有则创建，并在logger中设置root_
    Logger::ptr getLogger(const std::string& name);
    //获得root_
    Logger::ptr getRoot();
    //把全部的logger转成yaml格式（配置整合）
    std::string toYamlString();
private:
    LoggerMAP loggers_; //存储logger
    //修改配置文件中的root 那么没有给出定义的logger也会受影响
    Logger::ptr root_;  //默认logger
    MutexType mutex_;   //互斥锁
};

//单例模式
typedef sylar::Singleton<LoggerManager> LoggerMgr;
typedef sylar::Singleton<AsycLoggerManager> AsycLoggerMgr;


}

#endif