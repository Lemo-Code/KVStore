#ifndef __SYLAR_LEXICAL_CAST_H__
#define __SYLAR_LEXICAL_CAST_H__
#include<boost/lexical_cast.hpp>
#include<yaml-cpp/yaml.h>
#include<vector>
#include<list>
#include<map>
#include<unordered_map>
#include<set>
#include<unordered_set>

namespace sylar
{

template<class F,class T>
class LexicalCast
{
public:
    T operator()(const F& v){
        return boost::lexical_cast<T>(v);
    }
};

//string -> vector
template<class T>
class LexicalCast<std::string,std::vector<T> >{
public:
    std::vector<T> operator()(const std::string& val){
        YAML::Node node = YAML::Load(val);
        typename std::vector<T> vec;
        std::stringstream ss;
        for(size_t i = 0;i<node.size();i++){
            ss.str("");
            ss<<node[i];
            vec.push_back(LexicalCast<std::string,T>()(ss.str()));
        }
        return vec;
    }
};

//vector -> string
template<class T>
class LexicalCast<std::vector<T>, std::string>{
public:
    std::string operator()(const std::vector<T>& val){
        YAML::Node node;
        for(auto& i : val){
            node.push_back(YAML::Load(LexicalCast<T,std::string>()(i)));
        }
        std::stringstream ss;
        ss<<node;
        return ss.str();
    }
};

//string -> list
template<class T>
class LexicalCast<std::string,std::list<T> >{
public:
    std::list<T> operator()(const std::string& val){
        YAML::Node node = YAML::Load(val);
        std::list<T> vec;
        std::stringstream ss;
        for(size_t i = 0;i < node.size();i++){
            ss.str("");
            ss << node[i];
            vec.push_back(LexicalCast<std::string,T>()(ss.str()));
        }
        return vec;
    }
};

//list -> string
template<class T>
class LexicalCast<std::list<T>, std::string>{
public:
    std::string operator()(const std::list<T>& val){
        YAML::Node node;
        for(auto & i : val){
            node.push_back(YAML::Load(LexicalCast<T,std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};


//string -> set
template<class T>
class LexicalCast<std::string,std::set<T> >{
public:
    std::set<T> operator()(const std::string& val){
        YAML::Node node = YAML::Load(val);
        std::stringstream ss;
        std::set<T> vec;
        for(size_t i = 0;i < node.size(); i++){
            ss.str("");
            ss << node[i];
            vec.insert(LexicalCast<std::string,T>()(ss.str()));
        }
        return vec;
    }
};


//set -> string
template<class T>
class LexicalCast<std::set<T>, std::string>{
public:
    std::string operator()(const std::set<T>& val){
        YAML::Node node;
        for(auto &i : val){
            node.push_back(YAML::Load(LexicalCast<T,std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

//string -> unordered_set
template<class T>
class LexicalCast<std::string,std::unordered_set<T> >{
public:
    std::unordered_set<T> operator()(const std::string& val){
        YAML::Node node = YAML::Load(val);
        std::stringstream ss;
        std::unordered_set<T> vec;
        for(size_t i = 0;i < node.size(); i++){
            ss.str("");
            ss << node[i];
            vec.insert(LexicalCast<std::string,T>()(ss.str()));
        }
        return vec;
    }
};


//unordered_set -> string
template<class T>
class LexicalCast<std::unordered_set<T>, std::string>{
public:
    std::string operator()(const std::unordered_set<T>& val){
        YAML::Node node;
        for(auto &i : val){
            node.push_back(YAML::Load(LexicalCast<T,std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

//string -> map
template<class T>
class LexicalCast<std::string,std::map<std::string,T> >{
public:
    std::map<std::string,T> operator()(const std::string& val){
        YAML::Node node = YAML::Load(val);
        typename std::map<std::string,T>vec;
        std::stringstream ss;
        for(auto it = node.begin();
                it != node.end(); it++){
            ss.str("");
            ss << it->second;
            vec.insert(std::make_pair(it->first.Scalar()
                ,LexicalCast<std::string,T>()(ss.str())));
        }
        return vec;
    }
};

//map -> string
template<class T>
class LexicalCast<std::map<std::string,T>,std::string>{
public:
    std::string operator()(const std::map<std::string,T>& val){
        YAML::Node node;
        for(auto &i : val){
            node[i.first] = YAML::Load(LexicalCast<T,std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
    
};

//string -> unordered_map
template<class T>
class LexicalCast<std::string,std::unordered_map<std::string,T> >{
public:
    std::unordered_map<std::string,T> operator()(const std::string& val){
        YAML::Node node = YAML::Load(val);
        typename std::unordered_map<std::string,T>vec;
        std::stringstream ss;
        for(auto it = node.begin();
                it != node.end(); it++){
            ss.str("");
            ss << it->second;
            vec.insert(std::make_pair(it->first.Scalar()
                ,LexicalCast<std::string,T>()(ss.str())));
        }
        return vec;
    }
};


//unordered_map -> string
template<class T>
class LexicalCast<std::unordered_map<std::string,T>,std::string>{
public:
    std::string operator()(const std::unordered_map<std::string,T>& val){
        YAML::Node node;
        for(auto &i : val){
            node[i.first] = YAML::Load(LexicalCast<T,std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
    
};


}

#endif