#include"sylar/log.h"
#include"sylar/config.h"

#include<iostream>
#include<tuple>
#include<sys/stat.h>
#include<cstdio>
#include<iomanip>

namespace sylar {

const char* LogLevel::ToString(LogLevel::Level level)
{
    switch(level){
#define XX(str)         \
    case LogLevel::str: \
    return #str;        \
    break;
    XX(DEBUG);
    XX(INFO);
    XX(WARN);
    XX(ERROR);
    XX(FATAL);
#undef XX
    default:
        return "UNKNOWN";
    }
    return "UNKNOWN";
}

LogLevel::Level LogLevel::FromString(const std::string& str){
#define XX(level,stre)        \
    if(str == #stre){       \
        return LogLevel::level;  \
    }
    XX(DEBUG,DEBUG)
    XX(INFO,INFO)
    XX(WARN,WARN)
    XX(ERROR,ERROR)
    XX(FATAL,FATAL)
    XX(DEBUG,debug)
    XX(INFO, info)
    XX(WARN, warn)
    XX(ERROR,error)
    XX(FATAL,fatal)
    return LogLevel::UNKNOWN;
#undef XX
}


LogEvent::LogEvent(std::shared_ptr<Logger>logger, LogLevel::Level level ,
            const char* file, int32_t line, uint32_t elapse
            ,uint32_t threadId, uint32_t fiberId, uint64_t time
            ,const std::string& threadname)
    :file_(file)
    ,line_(line)
    ,elapse_(elapse)
    ,threadId_(threadId)
    ,fiberId_(fiberId)
    ,time_(time)
    ,threadName_(threadname)
    ,logger_(logger)
    ,level_(level){

            }
void LogEvent::format(const char* fmt,...)
{
    va_list al;
    va_start(al,fmt);
    format(fmt,al);
    va_end(al);
}

void LogEvent::format(const char* fmt,va_list al)
{
    char *buf = nullptr;
    int len = vasprintf(&buf,fmt,al);
    if( len != -1){
        ss_ << std::string(buf,len);
        free(buf);
    }
}

void LogEvent::resetSS() {
    ss_.clear();
    ss_.str("");
}

void LogEvent::reset(std::shared_ptr<Logger> logger,LogLevel::Level level 
        ,const char* file,int32_t line,uint32_t elapse
        ,uint32_t threadId,uint32_t fiberId
        ,uint64_t time,const std::string& threadname){
    logger_ = logger;
    level_ = level;
    file_ = file;
    line_ = line;
    elapse_ = elapse;
    threadId_ = threadId;
    fiberId_ = fiberId;
    time_ = time;
    threadName_ = threadname;
}

//massageFmt -1
class MessageFormatItem: public LogFormatter::FormatItem
{
public:
    MessageFormatItem(const std::string& str = ""){}
    void format(std::ostream& os,std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getSS().str();
    }
};

//LevelFmt -2
class LevelFormatItem: public LogFormatter::FormatItem
{
public:
    LevelFormatItem(const std::string& str = "") {}
    void format(std::ostream& os,std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<LogLevel::ToString(level);
    }
};

//ElapseFmt -3
class ElapseFormatItem: public LogFormatter::FormatItem
{
public:
    ElapseFormatItem(const std::string& str = "") {}
    void format(std::ostream& os,std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getElapse();
    }
};

//loggerNameFmt -4
class NameFormatItem: public LogFormatter::FormatItem
{
public:
    NameFormatItem(const std::string& str = "") {}
    void format(std::ostream& os,std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getLogger()->getName();
    }
};

//ThreadIdFmt -5
class ThreadIdFormatItem: public LogFormatter::FormatItem
{
public:
    ThreadIdFormatItem(const std::string& str = "") {}
    void format(std::ostream& os,std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getThreadId();
    }
};

//FiberIdFmt -6
class FiberIdFormatItem: public LogFormatter::FormatItem
{
public:
    FiberIdFormatItem(const std::string& str = "") {}
    void format(std::ostream& os,std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getFilberId();
    }
};

//DateTimeFmt -7
class DateTimeFormatItem: public LogFormatter::FormatItem
{
public:
    DateTimeFormatItem(const std::string& fmt = "%Y-%m-%d %H:%M:%S")
            :fmt_(fmt){

            }
    void format(std::ostream& os,std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event) override{
        struct tm t;
        time_t time = event->getTime();
        localtime_r(&time,&t);
        char buf[64];
        strftime(buf,sizeof(buf),fmt_.c_str(),&t);
        os<<buf;
    }
private:
    std::string fmt_;
};

//FileNameFmt -8
class FileNameFormatItem: public LogFormatter::FormatItem
{
public:
    FileNameFormatItem(const std::string& str = "") {}
    void format(std::ostream& os,std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getFile();
    }
};

//LineFmt -9
class LineFormatItem: public LogFormatter::FormatItem
{
public:
    LineFormatItem(const std::string& str = "") {}
    void format(std::ostream& os,std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getLine();
    }
};

//newLineFmt -10
class NewLineFormatItem: public LogFormatter::FormatItem
{
public:
    NewLineFormatItem(const std::string& str = "") {}
    void format(std::ostream& os,std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<"\n";
    }
};

//StringFmt -11
class StringFormatItem: public LogFormatter::FormatItem
{
public:
    StringFormatItem(const std::string& str = "")
            :FormatItem(str),string_(str){

    }
    void format(std::ostream& os,std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<string_;
    }
private:
    std::string string_;
};

//TabFmt -12
class TabFormatItem: public LogFormatter::FormatItem
{
public:
    TabFormatItem(const std::string& str = "") {}
    void format(std::ostream& os,std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<"\t";
    }
};

//ThreadNameFmt -13
class ThreadNameFormatItem: public LogFormatter::FormatItem
{
public:
    ThreadNameFormatItem(const std::string& str=""){}
    void format(std::ostream& os,std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event) override{
        os << event->getThreadName();
    }
};


//normal fmt  
Logger::Logger(const std::string& name,bool isAsyc)
    :isAsyc_(isAsyc)
    ,name_(name)
    ,level_(LogLevel::DEBUG) {
    formatter_.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"));
        // if(name == "root"){
        //     //提供默认的输出地（有没有可能出现重复的问题 question）
        //     //解决方案 每次设置appenders 先清空在设置
        //     appenders_.push_back(LogAppender::ptr(new StdoutLogAppender));   //这里push报错的原因：appender没有设置默认formatter的处理 //也就是formatter是nullptr 那么访问log肯定报段错误的
        //     for(auto& i:appenders_){                                        //也没有必要设置默认的StdLogAppender了，因为LoggerManager中提供了默认的root_并传入了Logger中存储在root_
        //         if(i->getFormatter() == nullptr){                           //Logger的log函数做了 如果appenders_是空就选择root_输出的选项  而且这里再addAppenders一次 反而会加多个相同的Appender
        //             std::cout<<"nullptr"<<std::endl;                        //这么处理反而不太好，不如直接使用默认的root_,如果非要在这添加 可以使用下面的addppender，但是还要创建Formatter增加了没用的开销
        //         }
        //     }
        // }
}   

void Logger::addAppender(LogAppender::ptr appender)
{
    MutexType::Lock lock(mutex_);    
    if(!appender->getFormatter()){
        //这么设置 不会改变appender的has_formatter 仍是false的，但是不会出现位置错误
        //增加程序健壮性
        MutexType::Lock ll(appender->mutex_);    
       appender->formatter_ = formatter_;
    }
     appenders_.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender)
{
    MutexType::Lock lock(mutex_);    
    for(auto it = appenders_.begin();
            it != appenders_.end();it++){
        if(*it == appender){
            appenders_.erase(it);
            break;
        }
    }
}

void Logger::clearAppenders(){
    MutexType::Lock lock(mutex_);    
    if(!appenders_.empty()){
        appenders_.clear();
    } 
}

void Logger::flush() {
    MutexType::Lock lock(mutex_);
    for (auto& appender : appenders_) {
        if (appender) appender->flush();
    }
}

//不带判断设置 为整个logger设置formatter -r递归的影响下游的所有formatter
void Logger::setFormatter(LogFormatter::ptr val){
    MutexType::Lock lock(mutex_);
    formatter_ = val;

    for(auto &i:appenders_){
        MutexType::Lock ll(i->mutex_);
        if(i->HasFormatter()){
            i->formatter_ = formatter_;
        }
    }
}

//带判断设置
void Logger::setFormatter(const std::string& val){
    LogFormatter::ptr n_val(new sylar::LogFormatter(val));
    if(n_val->isError()){
        //默认的是最标准的fmt
        std::cout << "Logger setFormatter name=" << name_ 
                << val << " invalid formatter"
                << std::endl;
        return;
    }
    setFormatter(n_val);
    // formatter_ = n_val;
}

std::string Logger::getFormatterStr()const{
    //虽然有formatter_的fmt有默认值 保险还是判断一下
    return formatter_->getPattern();
}

LogFormatter::ptr Logger::getFormatter()const{
    return formatter_;
}

bool Logger::isAppenderExists(LogAppender::ptr appender){
    for(auto &it:appenders_){
        if(it == appender){
            return true;
        }
    }
    return false;
}

std::string Logger::toYamlString(){
    MutexType::Lock lock(mutex_);
    YAML::Node node;
    node["name"] = name_;
    node["isAsyc"] = isAsyc_;
    if(level_ != LogLevel::UNKNOWN){
        node["level"] = LogLevel::ToString(level_);
    }
    
    if(formatter_ != nullptr){
        node["formatter"] = formatter_->getPattern();
    }
    for(auto &i : appenders_){
        node["appenders"].push_back(YAML::Load(i->toYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

//构造函数没有添加默认的LogAppender
//如果你在LoggerManager中加了一个logger 啥也没处理，那么Logger就会选用root_输出
//如果你加了 就选用你自己的logger
//但是你重复添加相同的appender会引发错误
void Logger::log(LogLevel::Level level,LogEvent::ptr event)
{
    if(level >= level_){
        auto self = shared_from_this();
        MutexType::Lock lock(mutex_);    
        if(!appenders_.empty()){
            for(auto &i:appenders_){
                //level?
                i->log(self,level,event,isAsyc_);
            }
        } else if(root_) {
            //通过LoggerManager获取一定有root_的
            root_->log(level,event);
        } //else {
            // std::cout << " invalid reach "<< std::endl;
        // }
    }
}

void Logger::debug(LogEvent::ptr event)
{
    log(LogLevel::DEBUG,event);
}

void Logger::info(LogEvent::ptr event)
{
    log(LogLevel::INFO,event);
}    

void Logger::warn(LogEvent::ptr event)
{
    log(LogLevel::WARN,event);
}

void Logger::error(LogEvent::ptr event)
{
    log(LogLevel::ERROR,event);
}

void Logger::fatal(LogEvent::ptr event)
{
    log(LogLevel::FATAL,event);
}



void LogAppender::setFormatter(LogFormatter::ptr formatter){
    MutexType::Lock lock(mutex_);
    formatter_ = formatter;
    if(formatter_){
        has_Formatter = true;
    } else {
        has_Formatter = false;
    }
}

LogFormatter::ptr LogAppender::getFormatter(){
    MutexType::Lock lock(mutex_);
    return formatter_;
}

StdoutLogAppender::StdoutLogAppender(const std::string& name){ 
    async_ = AsyncLogMgr::GetInstance()->emplaceChannel(AsyncSinkType::STDOUT, name);
}

void StdoutLogAppender::flush() {
    std::cout.flush();
}

FileLogAppender::FileLogAppender(const std::string& name)
        :name_(name){
    if(!filestream_.is_open()){
        filestream_.open(name_, std::ios::app);
    }
    async_ = AsyncLogMgr::GetInstance()->emplaceChannel(AsyncSinkType::FILE, name);
}

void StdoutLogAppender::log(std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event,bool isAsyc)
{
    if(level >= level_){
        if(isAsyc){
            if(!async_){
                async_ = AsyncLogMgr::GetInstance()->emplaceChannel(AsyncSinkType::STDOUT, logger->getName());
            }
            auto s = formatter_->format(logger,level,event);
            async_->enqueue(s);
            AsyncLogMgr::GetInstance()->notify();
        } else {
            //打印日志
            MutexType::Lock lock(mutex_);    
            std::cout<<formatter_->format(logger,level,event);
        }
    }
}

std::string StdoutLogAppender::toYamlString(){
    MutexType::Lock lock(mutex_);    
    YAML::Node node;
    node["type"] = "StdoutAppender";
    if(level_ != LogLevel::UNKNOWN){
        node["level"] = LogLevel::ToString(level_);
    }
    if(formatter_ != nullptr && has_Formatter){
        node["formatter"] = formatter_->getPattern();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

//设置lastTime主要是考虑，当在服务器运行的过程中 日志文件不小心被删除了，但是服务器不是能察觉的
//因而当两次日志的输出时间隔秒 就重新打卡一次文件
void FileLogAppender::log(std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event,bool isAsyc)
{
    if(level >= level_){
        if(isAsyc){
            if(!async_){
                async_ = AsyncLogMgr::GetInstance()->emplaceChannel(AsyncSinkType::FILE, name_);
            }
            auto s = formatter_->format(logger,level,event);
            async_->enqueue(s);
            AsyncLogMgr::GetInstance()->notify();
        } else {
            //防止日志文件删除不能感知  每秒重新打开一次
            uint64_t now = time(0);
            if(now != lastTime_){
                reopen();
                lastTime_ = now;
            }
            //输出日志到文件
            MutexType::Lock lock(mutex_);    
            filestream_<<formatter_->format(logger,level,event);
        } 
    }
}

void FileLogAppender::flush() {
    MutexType::Lock lock(mutex_);
    if (filestream_) filestream_.flush();
}

//重新打开文件
bool FileLogAppender::reopen()
{
    MutexType::Lock lock(mutex_);    
    if(filestream_){
        filestream_.close();
    }
    //不加尾部追加文件重启将删除
    filestream_.open(name_,std::ios::app);
    return !filestream_;
}

std::string FileLogAppender::toYamlString(){
    MutexType::Lock lock(mutex_);    
    YAML::Node node;
    node["type"] = "FileLogAppender";
    node["file"] = name_;
    if(level_ != LogLevel::UNKNOWN){
        node["level"] = LogLevel::ToString(level_);
    }
    if(formatter_ != nullptr && has_Formatter){
        node["formatter"] = formatter_->getPattern();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

// ---------- StderrLogAppender ----------
StderrLogAppender::StderrLogAppender(const std::string& name) {
    async_ = AsyncLogMgr::GetInstance()->emplaceChannel(AsyncSinkType::STDERR, name.empty() ? "stderr" : name);
}

void StderrLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event, bool isAsyc) {
    if (level < level_) return;
    if (isAsyc && async_) {
        auto s = formatter_->format(logger, level, event);
        async_->enqueue(s);
        AsyncLogMgr::GetInstance()->notify();
    } else {
        MutexType::Lock lock(mutex_);
        std::cerr << formatter_->format(logger, level, event);
        std::cerr.flush();
    }
}

void StderrLogAppender::flush() {
    std::cerr.flush();
}

std::string StderrLogAppender::toYamlString() {
    MutexType::Lock lock(mutex_);
    YAML::Node node;
    node["type"] = "StderrLogAppender";
    if (level_ != LogLevel::UNKNOWN) node["level"] = LogLevel::ToString(level_);
    if (formatter_ != nullptr && has_Formatter) node["formatter"] = formatter_->getPattern();
    std::stringstream ss;
    ss << node;
    return ss.str();
}

// ---------- RollingFileLogAppender ----------
RollingFileLogAppender::RollingFileLogAppender(const std::string& filepath, uint64_t max_file_size,
                                               uint32_t max_files, const std::string& roll_interval, bool compress)
    : filepath_(filepath)
    , max_file_size_(max_file_size > 0 ? max_file_size : 100 * 1024 * 1024)
    , max_files_(max_files > 0 ? max_files : 10)
    , roll_interval_(roll_interval)
    , compress_(compress)
    , current_size_(0)
    , last_roll_time_(0) {
    openCurrent();
}

void RollingFileLogAppender::openCurrent() {
    if (filestream_.is_open()) filestream_.close();
    filestream_.open(filepath_, std::ios::app | std::ios::out | std::ios::binary);
    if (filestream_) {
        filestream_.seekp(0, std::ios::end);
        current_size_ = static_cast<uint64_t>(filestream_.tellp());
    }
    last_roll_time_ = time(nullptr);
}

void RollingFileLogAppender::rollBySize() {
    filestream_.close();
    for (uint32_t i = max_files_; i >= 1; --i) {
        std::string from = (i == 1) ? filepath_ : (filepath_ + "." + std::to_string(i - 1));
        std::string to = filepath_ + "." + std::to_string(i);
        if (i == 1)
            rename(filepath_.c_str(), to.c_str());
        else
            rename(from.c_str(), to.c_str());
    }
    openCurrent();
}

void RollingFileLogAppender::rollByTime() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    std::stringstream ss;
    ss << std::put_time(&t, "%Y-%m-%d");
    if (roll_interval_ == "hour") ss << "-" << std::setfill('0') << std::setw(2) << t.tm_hour;
    std::string suffix = ss.str();
    std::string dest = filepath_ + "." + suffix;
    filestream_.close();
    rename(filepath_.c_str(), dest.c_str());
    if (compress_) compressFile(dest);
    openCurrent();
}

bool RollingFileLogAppender::compressFile(const std::string& path) const {
    if (path.empty()) return false;
    std::string cmd = "gzip -f \"" + path + "\" 2>/dev/null";
    return std::system(cmd.c_str()) == 0;
}

void RollingFileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event, bool isAsyc) {
    if (level < level_) return;
    if (isAsyc) {
        // 异步时退化为单文件追加（不轮巡），与 FileLogAppender 行为一致
        if (!async_) async_ = AsyncLogMgr::GetInstance()->emplaceChannel(AsyncSinkType::FILE, filepath_);
        auto s = formatter_->format(logger, level, event);
        async_->enqueue(s);
        AsyncLogMgr::GetInstance()->notify();
        return;
    }
    MutexType::Lock lock(mutex_);
    time_t now = time(nullptr);
    bool time_roll = false;
    if (roll_interval_ == "day" || roll_interval_ == "hour") {
        struct tm t_cur, t_last;
        time_t t_last_t = static_cast<time_t>(last_roll_time_);
        localtime_r(&now, &t_cur);
        localtime_r(&t_last_t, &t_last);
        if (roll_interval_ == "day") {
            time_roll = (t_cur.tm_year != t_last.tm_year || t_cur.tm_yday != t_last.tm_yday);
        } else {
            time_roll = (t_cur.tm_year != t_last.tm_year || t_cur.tm_yday != t_last.tm_yday || t_cur.tm_hour != t_last.tm_hour);
        }
    }
    std::string content = formatter_->format(logger, level, event);
    uint64_t add = content.size();
    if (time_roll || (max_file_size_ > 0 && current_size_ + add >= max_file_size_)) {
        if (time_roll) rollByTime();
        else rollBySize();
    }
    if (filestream_) {
        filestream_ << content;
        filestream_.flush();
        current_size_ += add;
    }
}

void RollingFileLogAppender::flush() {
    MutexType::Lock lock(mutex_);
    if (filestream_) filestream_.flush();
}

std::string RollingFileLogAppender::toYamlString() {
    MutexType::Lock lock(mutex_);
    YAML::Node node;
    node["type"] = "RollingFileLogAppender";
    node["file"] = filepath_;
    node["max_file_size"] = static_cast<int64_t>(max_file_size_);
    node["max_files"] = max_files_;
    node["roll_interval"] = roll_interval_;
    node["compress"] = compress_;
    if (level_ != LogLevel::UNKNOWN) node["level"] = LogLevel::ToString(level_);
    if (formatter_ != nullptr && has_Formatter) node["formatter"] = formatter_->getPattern();
    std::stringstream ss;
    ss << node;
    return ss.str();
}

// ---------- MemoryBufferLogAppender ----------
MemoryBufferLogAppender::MemoryBufferLogAppender(size_t max_lines)
    : max_lines_(max_lines > 0 ? max_lines : 1000) {}

void MemoryBufferLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event, bool isAsyc) {
    if (level < level_) return;
    std::string line = formatter_->format(logger, level, event);
    sylar::Mutex::Lock lock(mutex_);
    lines_.push_back(line);
    while (lines_.size() > max_lines_) lines_.pop_front();
}

std::vector<std::string> MemoryBufferLogAppender::getRecentLines(size_t max_lines) const {
    sylar::Mutex::Lock lock(mutex_);
    if (max_lines == 0 || max_lines >= lines_.size()) {
        return std::vector<std::string>(lines_.begin(), lines_.end());
    }
    auto it = lines_.end() - static_cast<std::ptrdiff_t>(max_lines);
    return std::vector<std::string>(it, lines_.end());
}

std::string MemoryBufferLogAppender::toYamlString() {
    sylar::Mutex::Lock lock(mutex_);
    YAML::Node node;
    node["type"] = "MemoryBufferLogAppender";
    node["max_lines"] = static_cast<int>(max_lines_);
    if (level_ != LogLevel::UNKNOWN) node["level"] = LogLevel::ToString(level_);
    if (formatter_ != nullptr && has_Formatter) node["formatter"] = formatter_->getPattern();
    std::stringstream ss;
    ss << node;
    return ss.str();
}

LogFormatter::LogFormatter(const std::string& pattern)
    :pattern_(pattern){
    init();
}

//遍历vec，按顺序把结果放入ss流
std::string LogFormatter::format(std::shared_ptr<Logger>logger,LogLevel::Level level,LogEvent::ptr event)
{
    std::stringstream ss;
    for(auto &i:items_){
        i->format(ss,logger,level,event);
    }
    return ss.str();
}

//%a%t%b%t%n  ->  把a t b t n解析成对应的FormatItem，按顺序存储到vec中
void LogFormatter::init()
{
    //第一个string存的是要解析的char(key) 第二个string存储的对应的内容(val),int是标记
    //                        key         val   IsSpecial
    std::vector<std::tuple<std::string, std::string, int>> vec;
        // 结果字符串
        std::string NoParse; // 不需要解析
        std::string IsParse; // 需要解析
        std::string fmt;     // 子串
        //%d{%Y-%m-%d %H:%M:%S}%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n
        for (size_t i = 0; i < pattern_.size(); i++)
        {
            // 不需要解析的部分
            // 如果不是 % 直接添加
            if (pattern_[i] != '%')
            {
                NoParse.append(1, pattern_[i]);
                continue;
            }
            // 如果是% 后面不是字母 直接添加
            if (!isalpha(pattern_[i + 1]))
            {
                NoParse.append(1, pattern_[i + 1]);
                i++;
                continue;
            }
            // 需要解析的部分   需要注意{ }的位置和状态， { }外状态为0 {} 内状态为1
            size_t n = i + 1;
            int fmt_begin = 0;  //{的位置
            int fmt_status = 0; // 状态
            while (n < pattern_.size())
            { //%d{xxxx}     %dc%c  %%%cc..[]%S{AA%
                // 普通字母
                // 不是{  不是}   fmt_status==0（括号外） d不是字母 或者 dc两个连续的不能同时为字母
                if ((pattern_[n] != '{') && (pattern_[n] != '}') && (fmt_status == 0) && (isalpha(pattern_[n - 1]))) // n可能后面>没有字符了  还需要自己加判断条件//||(isalpha(pattern_[n]&&isalpha(pattern_[n+1])))
                {
                    // 拼接字母
                    IsParse = pattern_.substr(i + 1, 1); //%后的一个字符 d
                    i = i + 1;
                    fmt = "";
                    break;
                }

                // 存在子串
                if (fmt_status == 0)
                {
                    if (pattern_[n] == '{')
                    {
                        // 拼接父串
                        IsParse = pattern_.substr(n - 1, 1);
                        fmt_begin = n + 1; //{ 开始的位置
                        fmt_status = 1;    // 更新状态
                        n++;
                        continue;
                    }
                }
                else if (fmt_status == 1)
                {
                    if (pattern_[n] == '}')
                    {
                        fmt = pattern_.substr(fmt_begin, n - fmt_begin);
                        fmt_status = 0; // 更新状态
                        i = n;          // 更新i的状态
                        n++;
                        break; // 退出大括号 {}解析完毕
                    }
                }
                n++;
                // 运行到这个位置表示运行到结束了没有遇到} 说明要解析字符串有问题  或者 是n到了最后一个字符
                if (n == pattern_.size())
                {
                    // 最后一个字符
                    if (fmt_status == 0)
                    {
                        IsParse.append(1, pattern_[n - 1]);
                        i = n - 1;
                        fmt = "";
                        break;
                    }
                    // 未找到}
                    else if (fmt_status == 1)
                    {
                        // 先处理不需要解析的
                        for (size_t index = 0; index < NoParse.size(); index++)
                        {
                            std::string ch(1, NoParse[index]);
                            vec.push_back(std::make_tuple(ch, "", 0));
                        }
                        // 再处理需要解析的(错误的)
                        fmt = pattern_.substr(fmt_begin + 1, n - fmt_begin - 1);
                        // std::cout << "<format error>" << IsParse << " : " << fmt << " : " << 1 << std::endl;
                        std::string ch(1, pattern_[n - 1]);
                        vec.push_back(std::make_tuple(ch, "<format error>", 1));
                        has_error = true;
                        return;
                    }
                }
            }
            // 如果不空
            if (!NoParse.empty())
            {
                for (size_t index = 0; index < NoParse.size(); index++)
                {
                    std::string ch(1, NoParse[index]);
                    vec.push_back(std::make_tuple(ch, "", 0));
                    NoParse.clear();
                }
            }
            vec.push_back(std::make_tuple(IsParse, fmt, 1));
            IsParse.clear();
        }
        // // 解析完成
        // for (auto it : vec)
        // {
        //     std::cout << std::get<0>(it)
        //               << "  :  " << std::get<1>(it)
        //               << "  :  " << std::get<2>(it) << std::endl;
        // 创建解析器map容器  % r -> Elapese
        /*
         *  %m 消息                 MessageFormatItem
         *  %p 日志级别             LevelFormatItem
         *  %r 累计毫秒数           ElapseFormatItem
         *  %c 日志名称             LogNameFormatItem
         *  %t 线程id               ThreadIdFormatItem
         *  %n 换行                 NewLineFormatItem
         *  %d 时间                 DateFormatItem
         *  %f 文件名               FileFormatItem
         *  %l 行号                 LineFormatItem
         *  %T 制表符               TabFormatItem
         *  %F 协程id               FiberIdFormatItem
         *  %R 协程总数             FiberTotalNumItem
         *  %N 线程名               ThreadNameItem
         *     普通字符             StringFormatItem
         */
        // 用static 和 包装器的原因
        static std::map<std::string, std::function<FormatItem::ptr(const std::string &str)>> fmt_items = {
#define XX(str, C) \
    {#str, [](const std::string &fmt) { return FormatItem::ptr(new C(fmt)); }}
            XX(m, MessageFormatItem),
            XX(p, LevelFormatItem),
            XX(r, ElapseFormatItem),
            XX(c, NameFormatItem),
            XX(t, ThreadIdFormatItem),
            XX(n, NewLineFormatItem),
            XX(d, DateTimeFormatItem),
            XX(f, FileNameFormatItem),
            XX(l, LineFormatItem),
            XX(T, TabFormatItem),
            XX(F, FiberIdFormatItem),
            XX(N, ThreadNameFormatItem),
            // XX(R, FiberTotalNumFormatItem),
#undef XX
        };

        for (auto it : vec)
        {
            // 普通字符
            if (std::get<2>(it) == 0)
            {
                items_.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(it))));
            }
            // 特殊字符
            else if (std::get<2>(it) == 1)
            {
                auto iter = fmt_items.find(std::get<0>(it));
                if (iter == fmt_items.end())
                {
                    items_.push_back(FormatItem::ptr(new StringFormatItem("<format error%" + std::get<0>(it) + ">")));
                    has_error = true;
                }
                else
                {
                    // std::get<1>(it)设计这个std::string 参数主要是因为日期需要传格式 参数需要保持一致
                    items_.push_back(iter->second(std::get<1>(it)));
                }
            }
        }
}


LogEventWrap::LogEventWrap(LogEvent::ptr event)
    :event_(event){

}
LogEventWrap::~LogEventWrap()
{
    event_->getLogger()->log(event_->getLevel(),event_);
}    
std::stringstream& LogEventWrap::getSS()
{
    return event_->getSS();
}

LoggerManager::LoggerManager()
{
    root_.reset(new Logger);
    root_->addAppender(LogAppender::ptr(new StdoutLogAppender));//这里添加了 如果Logger构造函数再添加就会引发段错误
    loggers_[root_->name_] = root_;
}

Logger::ptr LoggerManager::getLogger(const std::string& name)
{   
    //存在即返回
    MutexType::Lock lock(mutex_);
    auto it = loggers_.find(name);
    if(it != loggers_.end()){
        return it->second;
    }
    //新创建的logger是啥也没有的 除了一些默认的配置，但是这里给logger传了一个默认的成员变量root_,
        //使logger在输出的时候也能有一个默认的输出选项，而不是会报段错误;
    Logger::ptr logger(new Logger(name));
    logger->root_ = root_;
    loggers_[name] = logger;
    return logger;
}


std::string LoggerManager::toYamlString(){
    MutexType::Lock lock(mutex_);
    YAML::Node node;
    for(auto& i : loggers_){
        node.push_back(YAML::Load(i.second->toYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

Logger::ptr LoggerManager::getRoot()
{
    //root_一定存在
    return root_;
}


AsycLoggerManager::AsycLoggerManager() {
    root_.reset(new Logger("root",true));
    root_->addAppender(LogAppender::ptr(new StdoutLogAppender));//这里添加了 如果Logger构造函数再添加就会引发段错误
    loggers_[root_->name_] = root_;
}

Logger::ptr AsycLoggerManager::getLogger(const std::string& name) {

    MutexType::Lock lock(mutex_);
    auto it = loggers_.find(name);
    if(it != loggers_.end()){
        return it->second;
    }
    Logger::ptr logger(new Logger(name,true));
    logger->root_ = root_;
    loggers_[name] = logger;
    return logger;
}

Logger::ptr AsycLoggerManager::getRoot() {
    return root_;
}

std::string AsycLoggerManager::toYamlString() {
    MutexType::Lock lock(mutex_);
    YAML::Node node;
    for(auto& i : loggers_){
        node.push_back(YAML::Load(i.second->toYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}


/*
logs:
   - name: "smer"
     level: debug
     formatter: "%a%b%c"
     appender:
            - type: FileLogAppender
              path: xxx.log
              type: StdoutAppender
   - name: "root"
     level: info
     (其余字段可选)
*/

// 日志输出地配置：type 1=File 2=Stdout 3=Stderr 4=RollingFile 5=MemoryBuffer
struct LogAppenderDefine{
    int type = 0;
    LogLevel::Level level = LogLevel::UNKNOWN;
    std::string formatter;
    std::string file;
    uint64_t roll_max_size = 100 * 1024 * 1024;
    uint32_t roll_max_files = 10;
    std::string roll_interval = "day";
    bool roll_compress = false;
    size_t max_lines = 1000;
    bool operator==(const LogAppenderDefine& oth)const{
        return type == oth.type && level == oth.level && formatter == oth.formatter
            && file == oth.file && roll_max_size == oth.roll_max_size
            && roll_max_files == oth.roll_max_files && roll_interval == oth.roll_interval
            && roll_compress == oth.roll_compress && max_lines == oth.max_lines;
    }
};

//日志器配置
struct LogDefine{
    bool isAsyc;
    std::string name;
    LogLevel::Level level = LogLevel::UNKNOWN; //设置默认值 防止系统默认值不确定
    std::string formatter;
    std::vector<LogAppenderDefine> appenders;
    
    bool operator==(const LogDefine& oth)const{
        return name == oth.name
            && level == oth.level
            && formatter == oth.formatter
            && appenders == oth.appenders;
    }
    bool operator<(const LogDefine& oth)const{
        return name < oth.name;
    }
};


template<>
class LexicalCast<std::set<LogDefine>,std::string>{
public:
    std::string operator()(const std::set<LogDefine>& val){
        YAML::Node node;
        for(auto& i : val){
            YAML::Node n;
            n["name"] = i.name;
            if(i.level != LogLevel::UNKNOWN){
                n["level"] = LogLevel::ToString(i.level);
            }
            n["isAsyc"] = i.isAsyc;
            if(i.formatter.empty()){
                n["formatter"] = i.formatter;
            }

            for(auto& a : i.appenders){
                YAML::Node na;
                if(a.type == 1){
                    na["type"] = "FileLogAppender";
                    na["file"] = a.file;
                } else if(a.type == 2){
                    na["type"] = "StdoutLogAppender";
                } else if(a.type == 3){
                    na["type"] = "StderrLogAppender";
                } else if(a.type == 4){
                    na["type"] = "RollingFileLogAppender";
                    na["file"] = a.file;
                    na["max_file_size"] = static_cast<int64_t>(a.roll_max_size);
                    na["max_files"] = a.roll_max_files;
                    na["roll_interval"] = a.roll_interval;
                    na["compress"] = a.roll_compress;
                } else if(a.type == 5){
                    na["type"] = "MemoryBufferLogAppender";
                    na["max_lines"] = static_cast<int>(a.max_lines);
                }
                if(a.level != LogLevel::UNKNOWN) na["level"] = LogLevel::ToString(a.level);
                if(!a.formatter.empty()) na["formatter"] = a.formatter;
                n["appenders"].push_back(na);
            }
            node.push_back(n);
        }
        std::stringstream ss;
        ss << node;
        // std::cout<<ss.str()<<std::endl;
        return ss.str();
    }
};


//若有 FromString exception invalid node 报错考虑是否判定node是否定义
template<>
class LexicalCast<std::string,std::set<LogDefine> >{
public:
    std::set<LogDefine> operator()(const std::string& val){
        YAML::Node node = YAML::Load(val);
        std::set<LogDefine> vec;
        for(size_t i = 0;i < node.size();i++){
            auto n = node[i];
            if(!n["name"].IsDefined()){
                std::cout << "log config error: name is null, "
                    << n <<std::endl;
                continue;
            }

            LogDefine ld;
            ld.name = n["name"].as<std::string>();
            //这里也判定了
            ld.level = LogLevel::FromString(n["level"].IsDefined()
                        ? n["level"].as<std::string>() : "");
            if(n["isAsyc"].IsDefined()) {
                ld.isAsyc = n["isAsyc"].as<bool>();
            }
            
            if(n["formatter"].IsDefined()){
                ld.formatter = n["formatter"].as<std::string>();
            }

            if(n["appenders"].IsDefined()){
                for(size_t x = 0;x < n["appenders"].size() ; x++){
                    auto a = n["appenders"][x];
                    if(!a["type"].IsDefined()){
                        std::cout << "log config error: appender type is null, "
                            << a << std::endl;
                        continue;
                    }
                    std::string type = a["type"].as<std::string>();
                    LogAppenderDefine lad;
                    if(type == "FileLogAppender"){
                        lad.type = 1;
                        if(!a["file"].IsDefined()){
                           std::cout << "log config error: fileappender file is null, "
                                    << a << std::endl;
                            continue;
                        }
                        lad.file = a["file"].as<std::string>();
                        if(a["formatter"].IsDefined()) lad.formatter = a["formatter"].as<std::string>();
                    } else if(type == "StdoutAppender" || type == "StdoutLogAppender"){
                        lad.type = 2;
                        if(a["formatter"].IsDefined()) lad.formatter = a["formatter"].as<std::string>();
                    } else if(type == "StderrLogAppender" || type == "StderrAppender"){
                        lad.type = 3;
                        if(a["formatter"].IsDefined()) lad.formatter = a["formatter"].as<std::string>();
                    } else if(type == "RollingFileLogAppender" || type == "RollingFileAppender"){
                        lad.type = 4;
                        if(!a["file"].IsDefined()){
                            std::cout << "log config error: RollingFileAppender file is null, " << a << std::endl;
                            continue;
                        }
                        lad.file = a["file"].as<std::string>();
                        if(a["max_file_size"].IsDefined()) lad.roll_max_size = static_cast<uint64_t>(a["max_file_size"].as<int64_t>());
                        if(a["max_files"].IsDefined()) lad.roll_max_files = static_cast<uint32_t>(a["max_files"].as<int>());
                        if(a["roll_interval"].IsDefined()) lad.roll_interval = a["roll_interval"].as<std::string>();
                        if(a["compress"].IsDefined()) lad.roll_compress = a["compress"].as<bool>();
                        if(a["formatter"].IsDefined()) lad.formatter = a["formatter"].as<std::string>();
                    } else if(type == "MemoryBufferLogAppender" || type == "MemoryBufferAppender"){
                        lad.type = 5;
                        if(a["max_lines"].IsDefined()) lad.max_lines = static_cast<size_t>(a["max_lines"].as<int>());
                        if(a["formatter"].IsDefined()) lad.formatter = a["formatter"].as<std::string>();
                    } else {
                        std::cout<< "log config error: appender type is invalid, " << type << std::endl;
                        continue;
                    }
                    //设置子与父的日志输出关系(必须判定关系 否则a["level"]在配置文件不定义有未定义的行为)
                    if(a["level"].IsDefined()){
                        lad.level = LogLevel::FromString(a["level"].as<std::string>());
                    }
                    
                    ld.appenders.push_back(lad);
                }
            }
            vec.insert(ld);
        }
        // std::cout<<val<<std::endl;
        return vec;
    }
};



// //不额外提供vec 只有root_ （输出地是std）
sylar::ConfigVar<std::set<LogDefine> >::ptr g_log_defines = 
    sylar::Config::Lookup("logs", std::set<LogDefine>() /* vec */,"logs config");

// //监听器的一个功能
// //在监听器中会把set<LogDefine>中的值 设置到logger中 并xxx
struct LogIniter {
    LogIniter(){
        g_log_defines->addListener([](const std::set<LogDefine>& old_values
                , const std::set<LogDefine>& new_values){
            if (new_values.empty()) return;
            sylar::Logger::ptr logger;
            for(auto &i:new_values){
                auto it = old_values.find(i);
                if(it == old_values.end()){
                    //新增 old没有 new有
                    // logger.reset(new sylar::Logger(i.name)); //创建了 但是没有加到容器
                    logger = SYLAR_LOG_NAME(i.name);
                } else {
                    //修改 old有 new有 但是内容不同
                    if(!(i == *it)){
                        logger = SYLAR_LOG_NAME(i.name);
                    }
                }
                logger->setLevel(i.level);
                if(!i.formatter.empty()){
                    logger->setFormatter(i.formatter);
                }
                logger->setAsyc(i.isAsyc);
                logger->clearAppenders();
                for(auto &a : i.appenders){
                    sylar::LogAppender::ptr ap;
                    if(a.type == 1){
                        ap.reset(new FileLogAppender(a.file));
                    } else if(a.type == 2){
                        ap.reset(new StdoutLogAppender);
                    } else if(a.type == 3){
                        ap.reset(new StderrLogAppender);
                    } else if(a.type == 4){
                        ap.reset(new RollingFileLogAppender(a.file, a.roll_max_size, a.roll_max_files, a.roll_interval, a.roll_compress));
                    } else if(a.type == 5){
                        ap.reset(new MemoryBufferLogAppender(a.max_lines));
                    } else {
                        continue;
                    }
                    ap->setLevel(a.level);
                    if(!a.formatter.empty()){
                        LogFormatter::ptr fmt(new LogFormatter(a.formatter));
                        if(!fmt->isError()){
                            ap->setFormatter(fmt);
                        } else {
                            std::cout << "appender type=" << a.type << " formatter=" << a.formatter << " is invalid " << std::endl;
                        }
                    }
                    logger->addAppender(ap);
                }
                
            }
            //删除 old有 new没有
            for(auto &i : old_values){
                auto it = new_values.find(i);
                if(it == new_values.end()){
                    //并不是真的删除 而是给他设置一个很高的日志输出级别
                    auto logger = SYLAR_LOG_NAME(i.name);
                    logger->setLevel((LogLevel::Level)100);
                    logger->clearAppenders();
                    //处理完以后虽然设置了级别 但是log的时候设置了root_
                    //所以真正的输入还是root_ ，可能是考虑程序的健壮性吧
                }
            }
        });
    }
};

static LogIniter __log_init;

}