#ifndef __SYLAR_UTIL_H__
#define __SYLAR_UTIL_H__
#include <sys/syscall.h> /* Definition of SYS_* constants */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <execinfo.h>
#include <vector>
#include <string>
#include <dirent.h>
#include <fstream>
#include <ifaddrs.h>
#include <arpa/inet.h> // 包含 in_addr_t 类型定义
#include <netinet/in.h>
#include <cxxabi.h>
#include <jsoncpp/json/json.h>
#include <yaml-cpp/yaml.h>
#include <boost/lexical_cast.hpp>

namespace sylar
{
    //获取线程id 这个是系统型的线程号 util.h
    pid_t GetThreadId();
    //获取协程id util.h
    uint32_t GetFiberId();

    //获取堆栈信息
    void Backtrace(std::vector<std::string>& bt,int size = 64, int skip = 2);
    //堆栈信息->string
    std::string BacktraceToString(int size = 64,int skip = 2,const std::string& prefix = "");
    
    std::string GetThreadName();

    //获取事件 getTimeOfDay
    uint64_t GetCurrentMS();
    uint64_t GetCurrentUS();

    std::string Time2Str(time_t ts, const std::string& format = "%Y-%m-%d %H:%M%S");
    //获取分隔符信息
    std::vector<std::string> split(const std::string& str, const std::string& delimiter);
    //操作文件
    class FSUtil {
    public:
        static void ListAllFile(std::vector<std::string>& files 
                                    , const std::string& path
                                    , const std::string& subfix);
        static bool Mkdir(const std::string& dirname);
        static bool IsRunningPidfile(const std::string& pidfile);
        static bool Rm(const std::string& path);
        static bool Mv(const std::string& from, const std::string& to);
        static bool Realpath(const std::string& path, std::string& rpath);
        static bool Symlink(const std::string& frm, const std::string& to);
        static bool Unlink(const std::string& filename, bool exist = false);
        static std::string Dirname(const std::string& filename);
        static std::string Basename(const std::string& filename);
        static bool OpenForRead(std::ifstream& ifs, const std::string& filename
                        ,std::ios_base::openmode mode);
        static bool OpenForWrite(std::ofstream& ofs, const std::string& filename
                        ,std::ios_base::openmode mode);
       
    };

    template<class T>
    const char* TypeToName() {
        static const char* s_name = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);
        return s_name;
    }

    
    class TypeUtil {
    public:
        static int8_t ToChar(const std::string& str);
        static int64_t Atoi(const std::string& str);
        static double Atof(const std::string& str);
        static int8_t ToChar(const char* str);
        static int64_t Atoi(const char* str);
        static double Atof(const char* str);
    };
    bool YamlToJson(const YAML::Node& ynode, Json::Value& jnode);
    bool JsonToYaml(const Json::Value& jnode, YAML::Node& ynode);
    in_addr_t GetIPv4Inet() ;

    template<class V, class Map, class K>
    V GetParamValue(const Map& m, const K& k, const V& def = V()) {
        auto it = m.find(k);
        if(it == m.end()) {
            return def;
        }
        try {
            return boost::lexical_cast<V>(it->second);
        } catch (...) {
        }
    return def;
}
    class StringUtil {
    public:
        static std::string Format(const char* fmt, ...);
        static std::string Formatv(const char* fmt, va_list ap);

        static std::string UrlEncode(const std::string& str, bool space_as_plus = true);
        static std::string UrlDecode(const std::string& str, bool space_as_plus = true);

        static std::string Trim(const std::string& str, const std::string& delimit = " \t\r\n");
        static std::string TrimLeft(const std::string& str, const std::string& delimit = " \t\r\n");
        static std::string TrimRight(const std::string& str, const std::string& delimit = " \t\r\n");


        static std::string WStringToString(const std::wstring& ws);
        static std::wstring StringToWString(const std::string& s);

    };

    
}

#endif