/**
 * @file test_hot_reload.cc
 * @brief 配置热更新测试程序
 *
 * 用法:
 *   1. 创建 a.yaml 文件:
 *      server:
 *        port: 8080
 *        workers: 4
 *
 *   2. 终端1：先启动程序（仅注册监听，不读文件，等待你操作）
 *      ./bin/net/test_hot_reload -h a.yaml
 *
 *   3. 终端2 或编辑器：修改 a.yaml 并保存，例如:
 *      server:
 *        port: 9090
 *        workers: 8
 *
 *   4. 回到终端1 按 Enter 触发热更新，应看到 [回调] 与 [✓] 通知
 */
#include "test_common.h"

#include "config/config_center.h"
#include "config/config_mgr.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {

// 配置项
net::ConfigVar<int>::ptr g_port;
net::ConfigVar<int>::ptr g_workers;
net::ConfigVar<std::string>::ptr g_name;

// 回调触发标记
bool g_port_changed = false;
bool g_workers_changed = false;
int g_old_port = 0, g_new_port = 0;
int g_old_workers = 0, g_new_workers = 0;

// 显示帮助信息
void showUsage(const char* prog) {
  std::printf("Usage: %s -h <yaml_file>\n", prog);
  std::printf("  -h <file>  指定 YAML 配置文件路径\n");
  std::printf("  -?         显示帮助\n\n");
  std::printf("示例:\n");
  std::printf("  1. 创建 test_config.yaml:\n");
  std::printf("     server:\n");
  std::printf("       port: 8080\n");
  std::printf("       workers: 4\n");
  std::printf("       name: \"MyServer\"\n\n");
  std::printf("  2. 运行程序:\n");
  std::printf("     %s -h test_config.yaml\n\n", prog);
  std::printf("  3. 修改 test_config.yaml 中的值\n");
  std::printf("  4. 按回车键触发重载，观察通知\n");
}

// 创建示例配置文件
void createExampleConfig(const std::string& path) {
  std::ofstream ofs(path);
  ofs << "# 热更新测试配置文件\n";
  ofs << "server:\n";
  ofs << "  port: 8080\n";
  ofs << "  workers: 4\n";
  ofs << "  name: \"MyServer\"\n";
  ofs.close();
  std::printf("已创建示例配置文件: %s\n", path.c_str());
}

// 显示当前配置值
void showCurrentConfig() {
  std::printf("\n========== 当前配置 ==========\n");
  std::printf("server.port    = %d\n", g_port ? g_port->getValue() : 0);
  std::printf("server.workers = %d\n", g_workers ? g_workers->getValue() : 0);
  std::printf("server.name    = %s\n", g_name ? g_name->getValue().c_str() : "null");
  std::printf("==============================\n\n");
}

// 重置回调标记
void resetCallbacks() {
  g_port_changed = false;
  g_workers_changed = false;
  g_old_port = g_new_port = 0;
  g_old_workers = g_new_workers = 0;
}

// 显示回调通知结果
void showCallbacks() {
  std::printf("\n========== 回调通知 ==========\n");
  if (g_port_changed) {
    std::printf("[✓] port 变更通知: %d -> %d\n", g_old_port, g_new_port);
  } else {
    std::printf("[ ] port 未变更\n");
  }
  
  if (g_workers_changed) {
    std::printf("[✓] workers 变更通知: %d -> %d\n", g_old_workers, g_new_workers);
  } else {
    std::printf("[ ] workers 未变更\n");
  }
  std::printf("==============================\n\n");
}

// 注册配置监听
void setupListeners() {
  // 声明配置项
  g_port = net::ConfigCenter::lookup<int>("server.port", 8080);
  g_workers = net::ConfigCenter::lookup<int>("server.workers", 4);
  g_name = net::ConfigCenter::lookup<std::string>("server.name", "Default");
  
  // 注册 port 变更监听
  g_port->addListener([](const int& old_val, const int& new_val) {
    g_port_changed = true;
    g_old_port = old_val;
    g_new_port = new_val;
    std::printf("[回调] port 变更: %d -> %d\n", old_val, new_val);
  });
  
  // 注册 workers 变更监听
  g_workers->addListener([](const int& old_val, const int& new_val) {
    g_workers_changed = true;
    g_old_workers = old_val;
    g_new_workers = new_val;
    std::printf("[回调] workers 变更: %d -> %d\n", old_val, new_val);
  });
  
  // 注册 name 变更监听
  g_name->addListener([](const std::string& old_val, const std::string& new_val) {
    std::printf("[回调] name 变更: \"%s\" -> \"%s\"\n", 
                old_val.c_str(), new_val.c_str());
  });
  
  std::printf("[INFO] 已注册 3 个配置监听\n\n");
}

// 执行热重载
void doReload(const std::string& yaml_file) {
  std::printf("\n>>> 【热更新】重新读取磁盘: %s\n", yaml_file.c_str());
  
  resetCallbacks();
  
  bool ok = net::ConfigCenter::loadFromYamlFile(yaml_file);
  if (ok) {
    std::printf("[OK] 配置文件加载成功\n");
    showCallbacks();
    showCurrentConfig();
  } else {
    std::printf("[ERROR] 配置文件加载失败: %s\n", yaml_file.c_str());
  }
}

// 交互式测试循环
void interactiveTest(const std::string& yaml_file) {
  std::printf("\n");
  std::printf("========================================\n");
  std::printf("      配置热更新测试程序\n");
  std::printf("========================================\n");
  std::printf("\n");
  std::printf("正确测试顺序（请严格按此操作）:\n");
  std::printf("  [1] 本程序已启动（尚未读 YAML，下面为内存默认值）\n");
  std::printf("  [2] 去修改并保存: %s\n", yaml_file.c_str());
  std::printf("  [3] 回到本窗口按 [Enter] 触发热更新\n");
  std::printf("  [4] 应看到 [回调] 行 + [✓] 变更通知\n");
  std::printf("  [5] 输入 q 退出\n");
  std::printf("\n");

  std::printf(">>> 进程已就绪，当前为 lookup 默认值（未加载文件）:\n");
  showCurrentConfig();
  std::printf(">>> 请先去改配置文件，保存后再回来按 Enter\n\n");

  // 只有按 Enter 才会 loadFromYamlFile，才算热更新
  std::string input;
  while (true) {
    std::printf("修改文件后按 Enter 热更新 / 输入 q 退出 > ");
    std::getline(std::cin, input);
    
    if (input == "q" || input == "Q" || input == "quit") {
      std::printf("退出程序.\n");
      break;
    }
    
    if (input.empty() || input == "r" || input == "reload") {
      doReload(yaml_file);
    }
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  std::string yaml_file;
  
  // 解析命令行参数
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    
    if (arg == "-h" || arg == "--help") {
      if (i + 1 < argc) {
        yaml_file = argv[++i];
      } else {
        showUsage(argv[0]);
        return 1;
      }
    } else if (arg == "-?" || arg == "--help-only") {
      showUsage(argv[0]);
      return 0;
    } else if (arg[0] != '-') {
      // 非选项参数视为文件路径
      yaml_file = arg;
    }
  }
  
  // 检查文件路径
  if (yaml_file.empty()) {
    std::printf("[ERROR] 未指定 YAML 配置文件\n\n");
    showUsage(argv[0]);
    
    // 创建示例配置
    std::string example = "/tmp/test_hot_reload.yaml";
    createExampleConfig(example);
    std::printf("\n已自动创建示例配置，是否使用? (y/n): ");
    char c;
    std::cin >> c;
    if (c == 'y' || c == 'Y') {
      yaml_file = example;
      std::cin.ignore();  // 消耗换行
    } else {
      return 1;
    }
  }
  
  // 检查文件是否存在
  if (access(yaml_file.c_str(), F_OK) != 0) {
    std::printf("[ERROR] 文件不存在: %s\n", yaml_file.c_str());
    return 1;
  }
  
  // 设置配置监听
  setupListeners();
  
  // 进入交互式测试
  interactiveTest(yaml_file);
  
  std::printf("\nPASS test_hot_reload\n");
  return 0;
}
