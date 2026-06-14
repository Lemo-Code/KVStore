#ifndef __SYLAR_CONFIG_H__
#define __SYLAR_CONFIG_H__
#include "sylar/lexicalcast.h"
#include "sylar/mutex.h"
#include "sylar/env.h"
#include "sylar/log.h"

#include<memory>
#include<string>
#include<map>
#include<stdexcept>
#include<algorithm>
#include<functional>

namespace sylar {
//日志的类型
static sylar::Logger::ptr g_loggers = SYLAR_LOG_NAME("system");

//配置的基类[key description]
class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;
    //基类的构造 在传递name的时候会进行一个自动的大小写转化[在sylar看来所有配置应当是小写的，如果客户没有区分需要我们区分]
    ConfigVarBase(const std::string& name,const std::string& description = "")
            :name_(name)
            ,description_(description){
                std::transform(name.begin(),name.end(),name_.begin(),::tolower);
            }
    virtual ~ConfigVarBase(){}
    //获取配置信息的key
    std::string getName()const {return name_;}
    //获取配置信息的描述
    std::string getDescription()const {return description_;}
    
    //纯虚方法
    virtual std::string ToString() = 0;
    virtual bool FromString(const std::string& val) = 0;
    virtual std::string getTypeName() const = 0;
protected:
    std::string name_;          //配置的key
    std::string description_;   //配置的描述（不太重要）
};



//存贮配置信息[key value description 配置变更事件]
template<class T,class FromStr = LexicalCast<std::string,T>
                    ,class ToStr = LexicalCast<T,std::string>>
class ConfigVar:public ConfigVarBase
{
public:
    typedef RWMutex RWMutexType;
    typedef std::function<void(const T& old_value,const T& new_value)> on_change_cb;
    typedef std::shared_ptr<ConfigVar> ptr;
    //子类的构造: 在设置val的时候会自动遍历监听器，若值不相同，会执行监听函数，值相同直接返回
    ConfigVar(const std::string& name
            ,const T& default_val
            ,const std::string& description )
                : ConfigVarBase(name,description){
                setValue(default_val);
            }

    //存在配置变更事件相关的内容
    void setValue(const T& val){
        {
            RWMutexType::ReadLock lock(mutex_);
            if (val == value_){
                return;
            }
            for (auto &i : cbs_){
                i.second(value_, val);
            }
        }
        RWMutexType::WriteLock lock(mutex_);
        value_ = val;
    }

    //获取返回值
    const T getValue() {
        RWMutexType::ReadLock lock(mutex_);
        return value_;
    }

    //返回string类型的val
    std::string ToString()override{
        try{
            RWMutexType::ReadLock lock(mutex_);
            return ToStr()(value_);
        } catch(std::exception& e){
            SYLAR_LOG_ERROR(g_loggers) << "ConfigVar::ToString exception "
                << e.what() << " convert: " <<typeid(value_).name() 
                << " to string";
        }
        return "";
    }
    
    //将string转成正确的T类型 并存储在val_
    bool FromString(const std::string& val)override{
        try{
            setValue(FromStr()(val));
            // value_ = boost::lexical_cast<T>(val);
        } catch(std::exception& e) {
            SYLAR_LOG_ERROR(g_loggers) << "ConfigVar::FromString exception "
                << e.what() <<" convert: string to " << typeid(T).name();
            return false;
        }
        return true;
    }

    //返回T的数据类型
    std::string getTypeName()const override{return typeid(T).name();}
    
    //设置监听器(变更函数)
    uint64_t addListener(on_change_cb cb){
        //原方法是通过传参的方法设置id，但是可能导致id重复，会存在回调覆盖的情况，所以改用静态局部变量
        static uint64_t s_fun_id = 0;
        RWMutexType::WriteLock lock(mutex_);
        ++s_fun_id;
        cbs_[s_fun_id] = cb;
        return s_fun_id;
    } 

    //删除变更函数
    void delListener(uint64_t key){
        RWMutexType::WriteLock lock(mutex_);
        cbs_.erase(key);
    }

    //获取变更函数
    on_change_cb getListener(uint64_t key){
        RWMutexType::ReadLock lock(mutex_);
        auto it = cbs_.find(key);
        return it == cbs_.end() ? nullptr : it->second;
    }

    //清空变更函数
    void clearListeners(){
        RWMutexType::WriteLock lock(mutex_);
        cbs_.clear();
    }

private:
    T value_;                               //配置的value
    std::map<uint64_t,on_change_cb> cbs_;   //事件变更函数
    RWMutexType mutex_;                     //读写锁（配置一般读多写少）
};


//存储所有的配置项
class Config {
public:
    typedef std::map<std::string,ConfigVarBase::ptr>ConfigVarMap;
    typedef RWMutex RWMutexType;
    //创建配置项 
    template<class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& names,
                const T& value,const std::string& description = ""){
        RWMutexType::WriteLock lock(GetMutex());
        std::string name = names;
        std::transform(name.begin(),name.end(),name.begin(),::tolower);
        auto it = getDatas().find(name);
        if(it != getDatas().end()){
            auto tmp = std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
            if(tmp){
                SYLAR_LOG_INFO(g_loggers) << "Lookup name=" << name << " exists";
                return tmp;
            } else {
                SYLAR_LOG_ERROR(g_loggers) << "Lookup name="<< name << " exists buf type not "
                    << typeid(T).name() << "  and read_type=" << it->second->getTypeName()
                    << " " <<it->second->ToString();
                return nullptr;
            }
        }
        
        if(name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.")
            != std::string::npos){
                SYLAR_LOG_ERROR(g_loggers) << "Lookup invalid name:" << name;
                throw std::invalid_argument("name");
            }
        typename ConfigVar<T>::ptr var(new ConfigVar<T>(name,value,description));
        getDatas()[name] = var;
        return std::dynamic_pointer_cast<ConfigVar<T> >(getDatas()[name]);
    }

    //查找配置项
    template<class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name){
        RWMutexType::ReadLock lock(GetMutex());
        auto it = getDatas().find(name);
        if(it == getDatas().end()){
            return nullptr;
        }
        return std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
    }

    //获取datas [方便调试]
    static const ConfigVarMap getData() {return getDatas();}
    //将配置从文件加载到datas_
    static void LoadFromYaml(const YAML::Node& root);
    //是否存在name的基类
    static ConfigVarBase::ptr LookupBase(const std::string& name);

    //查看定义了多少变量 （调试接口） toString是通过YAML格式转的，可以在转成YAML格式string
    //可以通过cb对map中的所有配置（configVarBase）项进行处理
    static void Visit(std::function<void(ConfigVarBase::ptr)> cb);
    //完成整个文件下的路径加载
    static void LoadFromConfDir(const std::string& path);
private:
    //静态成员初始化问题 保证datas_在进入其他静态方法时完全初始化
    static ConfigVarMap& getDatas(){
        static ConfigVarMap datas_;
        return datas_;
    }
    //互斥锁
    static RWMutexType& GetMutex(){
        static RWMutexType s_mutex;
        return s_mutex;
    }
};


} 

#endif