#ifndef __SYLAR_ENV_H__
#define __SYLAR_ENV_H__
/**
 * 解析参数 / 操作环境变量
 */
#include "sylar/singleton.h"
#include "sylar/thread.h"
#include "sylar/mutex.h"
#include <map>
#include <string>
#include <vector>

namespace sylar
{

class Env
{
public:
    typedef RWMutex RWMutexType;
    bool init(int argc, char** argv);

    void add(const std::string& key, const std::string& val);
    bool has(const std::string& key);
    //(key not in map)  return default_val
    std::string get(const std::string& key, const std::string& val);
    void del(const std::string& key);

    void addHelp(const std::string& key, const std::string& desc);
    void removeHelp(const std::string& key);
    void printHelp();

    std::string getExe() const {return exe_;}
    std::string getCwd() const {return cwd_;}

    bool setEnv(const std::string& key, const std::string& val);
    std::string getEnv(const std::string& key, const std::string& default_value = "");

    //获得绝对路径
    std::string getAbsolutePath(const std::string& path)const;
    std::string getConfigPath();
private:
    RWMutexType mutex_;
    std::map<std::string, std::string> args_;
    std::vector<std::pair<std::string,std::string>> helps_;
    std::string program_;
    std::string exe_;
    std::string cwd_;
};

typedef sylar::Singleton<Env> EnvMgr;

}

#endif