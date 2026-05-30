#include "test_common.h"

#include "config/lexical_cast.h"

#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>

int main() {
  // --- 基本类型转换 ---
  {
    // string -> int
    net::LexicalCast<std::string, int> s2i;
    NET_CHECK(s2i("42") == 42);
    NET_CHECK(s2i("-10") == -10);
    
    // int -> string
    net::LexicalCast<int, std::string> i2s;
    NET_CHECK(i2s(42) == "42");
    NET_CHECK(i2s(-10) == "-10");
    
    // string -> uint32_t
    net::LexicalCast<std::string, uint32_t> s2u32;
    NET_CHECK(s2u32("123456") == 123456u);
    
    // string -> bool
    net::LexicalCast<std::string, bool> s2b;
    NET_CHECK(s2b("true") == true);
    NET_CHECK(s2b("1") == true);
    NET_CHECK(s2b("false") == false);
    NET_CHECK(s2b("0") == false);
    
    // string -> double
    net::LexicalCast<std::string, double> s2d;
    NET_CHECK(s2d("3.14159") == 3.14159);
    
    // double -> string
    net::LexicalCast<double, std::string> d2s;
    std::string ds = d2s(3.14);
    NET_CHECK(ds.find("3.14") != std::string::npos);
    
    // string -> string
    net::LexicalCast<std::string, std::string> s2s;
    NET_CHECK(s2s("hello") == "hello");
  }

  // --- 容器类型转换 ---
  {
    // string -> vector<int>
    net::LexicalCast<std::string, std::vector<int>> s2v;
    auto vec = s2v("1, 2, 3, 4, 5");
    NET_CHECK(vec.size() == 5);
    NET_CHECK(vec[0] == 1);
    NET_CHECK(vec[4] == 5);
    
    // vector<int> -> string (YAML 格式)
    net::LexicalCast<std::vector<int>, std::string> v2s;
    std::string vs = v2s(vec);
    NET_CHECK(vs.find("1") != std::string::npos);
    NET_CHECK(vs.find("2") != std::string::npos);
    NET_CHECK(vs.find("5") != std::string::npos);
    
    // string -> list<std::string>
    net::LexicalCast<std::string, std::list<std::string>> s2l;
    auto lst = s2l("apple, banana, cherry");
    NET_CHECK(lst.size() == 3);
    NET_CHECK(lst.front() == "apple");
    
    // list<std::string> -> string (YAML 格式)
    net::LexicalCast<std::list<std::string>, std::string> l2s;
    std::string ls = l2s(lst);
    NET_CHECK(ls.find("apple") != std::string::npos);
    NET_CHECK(ls.find("banana") != std::string::npos);
    NET_CHECK(ls.find("cherry") != std::string::npos);
    
    // string -> set<int>
    net::LexicalCast<std::string, std::set<int>> s2set;
    auto st = s2set("3, 1, 2, 1, 3");  // 重复元素会被去重
    NET_CHECK(st.size() == 3);
    NET_CHECK(*st.begin() == 1);
    
    // set<int> -> string
    net::LexicalCast<std::set<int>, std::string> set2s;
    std::string set_s = set2s(st);
    // 集合是无序的，但我们的实现会按插入顺序输出
    NET_CHECK(!set_s.empty());
  }

  // --- 映射类型转换 ---
  {
    // string -> map<string, int>
    net::LexicalCast<std::string, std::map<std::string, int>> s2m;
    auto mp = s2m("key1:100, key2:200, key3:300");
    NET_CHECK(mp.size() == 3);
    NET_CHECK(mp["key1"] == 100);
    NET_CHECK(mp["key2"] == 200);
    NET_CHECK(mp["key3"] == 300);
    
    // map<string, int> -> string (YAML 格式)
    net::LexicalCast<std::map<std::string, int>, std::string> m2s;
    std::string ms = m2s(mp);
    NET_CHECK(!ms.empty());
    NET_CHECK(ms.find("key1") != std::string::npos);
    NET_CHECK(ms.find("100") != std::string::npos);
    
    // 嵌套映射
    std::map<std::string, std::string> nested;
    nested["host"] = "localhost";
    nested["port"] = "3306";
    
    net::LexicalCast<std::map<std::string, std::string>, std::string> mss2s;
    std::string nss = mss2s(nested);
    NET_CHECK(nss.find("host") != std::string::npos);
    NET_CHECK(nss.find("localhost") != std::string::npos);
  }

  // --- 边界情况 ---
  {
    // 空字符串
    net::LexicalCast<std::string, std::vector<int>> s2v;
    auto empty_vec = s2v("");
    NET_CHECK(empty_vec.empty());
    
    // 单个元素
    auto single = s2v("42");
    NET_CHECK(single.size() == 1);
    NET_CHECK(single[0] == 42);
    
    // 带多余空格
    auto spaced = s2v("  1  ,  2  ,  3  ");
    NET_CHECK(spaced.size() == 3);
    NET_CHECK(spaced[0] == 1);
  }

  std::printf("PASS test_lexical_cast\n");
  return 0;
}
