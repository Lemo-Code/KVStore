#include "sylar/env.h"
#include "sylar/log.h"
#include <string.h>
#include <iostream>
#include <iomanip>
#include <unistd.h>

//环境变量
//getenv 
//setenv

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

namespace sylar
{
//main
bool Env::init(int argc, char** argv){
    char link[1024] = {0};//链接路径
    char path[1024] = {0};//真实路径
    sprintf(link,"/proc/%d/exe",getpid());
    if(-1 == readlink(link,path,sizeof(path))){
        SYLAR_LOG_FATAL(g_logger) << "Env::init() readlink failed, error="
            << errno << " strerrno=" << strerror(errno);
    }
    //  /path/xxx/exe
    exe_ = path;

    auto pos = exe_.find_last_of("/");
    cwd_ = exe_.substr(0,pos) + "/";

    //至少有一个元素
    program_ = argv[0];
    const char* now_key = nullptr;
    for(int i = 1; i < argc; ++i){
        if(argv[i][0] == '-'){
            if(strlen(argv[i]) > 1){
                if(now_key){
                    add(now_key,"");   
                }
                now_key = argv[i] + 1;
            } else {
                SYLAR_LOG_ERROR(g_logger) << "invalid arg idx=" << i
                    << " val=" << argv[i];
                return false;
            }
        } else {
            if(now_key){
                add(now_key, argv[i]);
                now_key = nullptr;
            } else {
                SYLAR_LOG_ERROR(g_logger) << "invalid arg idx=" << i
                    << " val=" << argv[i];
                return false;
            }
        }
    }
    if(now_key){
        add(now_key,"");
    }
    return true;
}

void Env::add(const std::string& key, const std::string& val){
    RWMutexType::WriteLock lock(mutex_);
    args_[key] = val;
}
bool Env::has(const std::string& key){
    RWMutexType::ReadLock lock(mutex_);
    auto it = args_.find(key);
    return it != args_.end();
}
//(key not in map)  return default_val
std::string Env::get(const std::string& key, const std::string& val){
    RWMutexType::ReadLock lock(mutex_);
    auto it = args_.find(key);
    return it != args_.end() ? it->second : val;    
}

//it方法和erase【】区别
void Env::del(const std::string& key){
    RWMutexType::ReadLock lock(mutex_);
    auto it = args_.find(key);
    if(it != args_.end()) {
        args_.erase(it);
    }
}

void Env::addHelp(const std::string& key, const std::string& desc){
    RWMutexType::WriteLock lock(mutex_);
    for(auto it = helps_.begin();
            it != helps_.end();){
        if(it->first == key){
            it = helps_.erase(it);
        } else {
            ++it;
        }
    }
    helps_.push_back(std::make_pair(key,desc));
}

void Env::removeHelp(const std::string& key){
    RWMutexType::WriteLock lock(mutex_);
    for(auto it = helps_.begin();
            it != helps_.end();){
        if(it->first == key){
            it = helps_.erase(it);//自动++？
        } else {
            ++it;
        }
    }
}

void Env::printHelp(){
    RWMutexType::ReadLock lock(mutex_);
    std::cout << "Usage: " << program_ << " [options]" << std::endl;
    for(auto& i : helps_){
        std::cout << std::setw(5) << "-" << i.first << " : " << i.second << std::endl;
    }   
}

bool Env::setEnv(const std::string& key, const std::string& val){
    return !setenv(key.c_str(),val.c_str(),1);
}

std::string Env::getEnv(const std::string& key, const std::string& default_value){
    const char* v = getenv(key.c_str());
    if(v == nullptr){
        return default_value;   
    }
    return v;
}

std::string Env::getAbsolutePath(const std::string& path)const{
    if(path.empty()) {
        return "/";
    }
    if(path[0] == '/'){
        return path;
    }
    return cwd_ + path;
}

std::string Env::getConfigPath() {
    return getAbsolutePath(get("c", "conf"));
}

}