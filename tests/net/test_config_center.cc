#include "test_common.h"

#include "config/config_center.h"
#include "config/config_mgr.h"

#include <fstream>
#include <unistd.h>

namespace {

std::string TmpFile(const char* suffix) {
  return std::string("/tmp/net_config_test_") + std::to_string(getpid()) + suffix;
}

void WriteFile(const std::string& path, const std::string& content) {
  std::ofstream ofs(path);
  ofs << content;
}

}  // namespace

int main() {
  // --- 测试 ConfigVar 基本功能 ---
  {
    net::ConfigCenter::clear();
    
    auto var = net::ConfigCenter::lookup<int>("system.port", 8080, "服务端口");
    NET_CHECK(var != nullptr);
    NET_CHECK(var->getValue() == 8080);
    NET_CHECK(var->getName() == "system.port");
    
    var->setValue(9090);
    NET_CHECK(var->getValue() == 9090);
    
    // 测试字符串转换
    std::string str_val = var->toString();
    NET_CHECK(str_val == "9090");
    
    var->fromString("7070");
    NET_CHECK(var->getValue() == 7070);
    
    // 测试重复查找返回同一对象
    auto var2 = net::ConfigCenter::lookup<int>("system.port", 0);
    NET_CHECK(var2 == var);
    
    // 测试不同类型返回 nullptr
    auto var_str = net::ConfigCenter::lookup<std::string>("system.port", "");
    NET_CHECK(var_str == nullptr);
  }

  // --- 测试回调监听 ---
  {
    net::ConfigCenter::clear();
    
    auto var = net::ConfigCenter::lookup<int>("app.workers", 4);
    
    int old_val = 0, new_val = 0;
    uint64_t cb_id = var->addListener([&old_val, &new_val](const int& o, const int& n) {
      old_val = o;
      new_val = n;
    });
    
    var->setValue(8);
    NET_CHECK(old_val == 4);
    NET_CHECK(new_val == 8);
    
    // 相同值不触发回调
    old_val = new_val = 0;
    var->setValue(8);
    NET_CHECK(old_val == 0);
    NET_CHECK(new_val == 0);
    
    var->delListener(cb_id);
    var->setValue(16);
    NET_CHECK(old_val == 0);  // 回调已删除，不会被调用
  }

  // --- 测试 YAML 加载 ---
  {
    net::ConfigCenter::clear();
    
    // 声明配置项
    auto port = net::ConfigCenter::lookup<int>("server.port", 8080);
    auto workers = net::ConfigCenter::lookup<int>("server.workers", 4);
    auto timeout = net::ConfigCenter::lookup<double>("server.timeout", 30.0);
    auto features = net::ConfigCenter::lookup<std::vector<std::string>>("server.features", {});
    
    std::string yaml = R"(
server:
  port: 9090
  workers: 8
  timeout: 60.5
  features:
    - http2
    - ssl
    - compression
)";
    
    bool ok = net::ConfigCenter::loadFromYamlString(yaml);
    NET_CHECK(ok);
    NET_CHECK(port->getValue() == 9090);
    NET_CHECK(workers->getValue() == 8);
    NET_CHECK(timeout->getValue() == 60.5);
    
    auto vec = features->getValue();
    NET_CHECK(vec.size() == 3);
    NET_CHECK(vec[0] == "http2");
  }

  // --- 测试 LexicalCast 类型转换 ---
  {
    net::ConfigCenter::clear();
    
    // int
    auto i = net::ConfigCenter::lookup<int>("test.int", 0);
    i->fromString("42");
    NET_CHECK(i->getValue() == 42);
    
    // bool
    auto b = net::ConfigCenter::lookup<bool>("test.bool", false);
    b->fromString("true");
    NET_CHECK(b->getValue() == true);
    
    // double
    auto d = net::ConfigCenter::lookup<double>("test.double", 0.0);
    d->fromString("3.14");
    NET_CHECK(d->getValue() == 3.14);
    
    // vector<int>
    auto vec = net::ConfigCenter::lookup<std::vector<int>>("test.vec", {});
    vec->fromString("1, 2, 3, 4, 5");
    auto v = vec->getValue();
    NET_CHECK(v.size() == 5);
    NET_CHECK(v[0] == 1);
    NET_CHECK(v[4] == 5);
    
    // map<string, int>
    auto m = net::ConfigCenter::lookup<std::map<std::string, int>>("test.map", {});
    m->fromString("a:1, b:2, c:3");
    auto mp = m->getValue();
    NET_CHECK(mp.size() == 3);
    NET_CHECK(mp["a"] == 1);
    NET_CHECK(mp["c"] == 3);
  }

  // --- 测试 ConfigMgr 文件加载 ---
  {
    net::ConfigCenter::clear();
    
    auto var = net::ConfigCenter::lookup<std::string>("app.name", "default");
    
    std::string path = TmpFile(".yml");
    WriteFile(path, "app:\n  name: MyApplication\n");
    
    bool ok = net::ConfigMgrInstance::GetInstance()->loadFromFile(path);
    NET_CHECK(ok);
    NET_CHECK(var->getValue() == "MyApplication");
    
    // 测试目录加载
    net::ConfigCenter::clear();
    auto var2 = net::ConfigCenter::lookup<int>("config.value", 0);
    
    WriteFile(path, "config:\n  value: 12345\n");
    
    size_t count = net::ConfigMgrInstance::GetInstance()->loadFromConfDir("/tmp");
    NET_CHECK(count >= 1);  // 至少加载了我们创建的文件
    NET_CHECK(var2->getValue() == 12345);
    
    // 清理
    std::remove(path.c_str());
  }

  // --- 测试遍历 ---
  {
    net::ConfigCenter::clear();
    
    net::ConfigCenter::lookup<int>("a.b.c", 1);
    net::ConfigCenter::lookup<std::string>("x.y.z", "test");
    net::ConfigCenter::lookup<double>("m.n", 3.14);
    
    int count = 0;
    net::ConfigCenter::visit([&count](net::ConfigVarBase::ptr var) {
      ++count;
      NET_CHECK(!var->getName().empty());
    });
    NET_CHECK(count == 3);
  }

  std::printf("PASS test_config_center\n");
  return 0;
}
