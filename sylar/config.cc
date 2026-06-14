#include"sylar/config.h"
#include<list>
#include <sys/stat.h>

namespace sylar {

//a:
//  b: 10  -> a.b   10
//将node的树形结构打平
static void ListAllMember(const std::string& prefix,
                        const YAML::Node& node,
                        std::list<std::pair<std::string,const YAML::Node> >& output){
        if(prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_.123456789")
            != std::string::npos){
            SYLAR_LOG_ERROR(g_loggers) << "Config invalid name: " << prefix << " : "
                << node;
                return;
        }
        output.push_back(std::make_pair(prefix,node));
        if(node.IsMap()){
            for(auto it = node.begin();
                    it != node.end() ; it++){
                // std::string res = prefix.empty() ? it->first.Scalar() : (prefix + "." +it->first.Scalar());
                // SYLAR_LOG_DEBUG(g_loggers) << res;
                ListAllMember(prefix.empty() ? it->first.Scalar() : (prefix + "." +it->first.Scalar()),
                   it->second,output);
            }
        } 
    }

ConfigVarBase::ptr Config::LookupBase(const std::string& name){
    auto it = getDatas().find(name);
    return it == getDatas().end() ? nullptr : it->second;
}

// 把打平以后的配置项加载到map容器中
//  加载规则：
//  约定优于配置 如果name已经存在于map中 才会修改数据 并且数据类型和硬编码的类型是相同的
void Config::LoadFromYaml(const YAML::Node& root){
    //allnode存放的是打平以后的结果key - value
    std::list<std::pair<std::string,const YAML::Node> >all_nodes;
    ListAllMember("",root,all_nodes);
    //配置文件中的是有大小写的 
    for(auto& i : all_nodes){
        std::string key = i.first;
        // SYLAR_LOG_DEBUG(g_loggers) <<key;
        if(key.empty()){
            continue;
        }

        std::transform(key.begin(),key.end(),key.begin(),::tolower);
        ConfigVarBase::ptr var = LookupBase(key);
        
        //约定优于配置  只有原配置中有了 才会通过配置文件进行配置
        if(var){
            if(i.second.IsScalar()){
                // SYLAR_LOG_DEBUG(g_loggers) << key << " - "<< i.second.Scalar();
                var->FromString(i.second.Scalar());
            } else {
                std::stringstream ss;
                // SYLAR_LOG_DEBUG(g_loggers) << key << " - " << ss.str();
                ss << i.second;
                var->FromString(ss.str());
            }
        }
    }
}

//可以通过cb对map中的所有配置（configVarBase）项进行处理
void Config::Visit(std::function<void(ConfigVarBase::ptr)> cb){
    RWMutexType::ReadLock lock(GetMutex());
    ConfigVarMap& m = getDatas();
    for(auto it = m.begin();
            it != m.end();it++){
        cb(it->second);
    }
}


//读取整个文件夹的锁和整个文件夹下配置文件的时间属性
static std::map<std::string, uint64_t> s_file2modifytime;
static sylar::Mutex s_mutex;

void Config::LoadFromConfDir(const std::string& path){
    std::string absoultePath = sylar::EnvMgr::GetInstance()->getAbsolutePath(path);
    std::vector<std::string> files;
    FSUtil::ListAllFile(files,absoultePath,".yml");

    for(auto &i : files){
        {
            //配置文件若不变 加载多次 也只会生效一次
            struct stat st;
            lstat(i.c_str(),&st);
            sylar::Mutex::Lock locks(s_mutex);
            if(s_file2modifytime[i] == (uint64_t)st.st_mtime){
                continue;
            }
            s_file2modifytime[i] = (uint64_t)st.st_mtime;
        }

        try{
            YAML::Node root = YAML::LoadFile(i);
            LoadFromYaml(root);
            SYLAR_LOG_INFO(g_loggers) << "LoadConfigFile file="
                << i << " ok";
        } catch(...){
            SYLAR_LOG_ERROR(g_loggers) << "LoadConfigFile file="
                << i <<" failed";
        }
    }
}


}
