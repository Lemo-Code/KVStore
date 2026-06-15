// Config 完整测试 — 标量 + 容器 (vector/list/set/map)
#include "zero/config/config.h"
#include <cstdio>
#include <cassert>
#include <iostream>

// 使用与 config.cc 一致的 typedef 避免链接错误
using VecStr = std::vector<std::string>;
using SetStr = std::set<std::string>;
using MapSI  = std::map<std::string, int>;
using VecInt = std::vector<int>;
using ListStr = std::list<std::string>;

// 注册测试用的配置项
void registerTestConfigs() {
    // 容器类型 (使用 typedef 匹配显式实例化)
    zero::Config::Lookup<VecStr>(
        "http.listen_addresses", VecStr{"0.0.0.0:8080"}, "Listen addresses");

    zero::Config::Lookup<SetStr>(
        "http.allowed_methods", SetStr{}, "Allowed HTTP methods");

    zero::Config::Lookup<MapSI>(
        "http.rate_limits", MapSI{}, "Rate limits");

    zero::Config::Lookup<VecInt>(
        "http.timeout_tiers", VecInt{}, "Timeout tiers");

    zero::Config::Lookup<ListStr>(
        "log.modules", ListStr{}, "Log modules");
}

int main() {
    printf("=== Config Container Type Test ===\n");

    // 1. 注册
    registerTestConfigs();
    printf("1. Registered container configs\n");

    // 2. 加载 YAML
    zero::Config::LoadFromYaml("config.yml");
    printf("2. Loaded config.yml\n");

    // 3. vector<string> (用 3-param 版本，已显式实例化)
    auto listen = zero::Config::Lookup<VecStr>("http.listen_addresses", VecStr{}, "");
    assert(listen);
    auto& addrs = listen->getValue();
    printf("3. listen_addresses (vector<string>): %zu entries\n", addrs.size());
    for (auto& a : addrs) printf("   - %s\n", a.c_str());
    assert(addrs.size() == 2);
    assert(addrs[0] == "0.0.0.0:8080");
    assert(addrs[1] == "0.0.0.0:8443");

    // 4. set<string>
    auto methods = zero::Config::Lookup<SetStr>("http.allowed_methods", SetStr{}, "");
    auto& meth = methods->getValue();
    printf("4. allowed_methods (set<string>): %zu entries\n", meth.size());
    for (auto& m : meth) printf("   - %s\n", m.c_str());
    assert(meth.count("GET"));
    assert(meth.count("POST"));
    assert(meth.size() == 3);

    // 5. map<string,int>
    auto limits = zero::Config::Lookup<MapSI>("http.rate_limits", MapSI{}, "");
    auto& rl = limits->getValue();
    printf("5. rate_limits (map<string,int>): %zu entries\n", rl.size());
    for (auto& [k, v] : rl) printf("   - %s: %d\n", k.c_str(), v);
    assert(rl["api"] == 100);
    assert(rl["static"] == 500);
    assert(rl["admin"] == 50);

    // 6. vector<int>
    auto tiers = zero::Config::Lookup<VecInt>("http.timeout_tiers", VecInt{}, "");
    auto& t = tiers->getValue();
    printf("6. timeout_tiers (vector<int>): %zu entries\n", t.size());
    for (auto v : t) printf("   - %d\n", v);
    assert(t.size() == 3);
    assert(t[0] == 1000);

    // 7. list<string>
    auto mods = zero::Config::Lookup<ListStr>("log.modules", ListStr{}, "");
    auto& lm = mods->getValue();
    printf("7. modules (list<string>): %zu entries\n", lm.size());
    for (auto& m : lm) printf("   - %s\n", m.c_str());
    assert(lm.size() == 3);

    // 8. 变更回调 (vector)
    int cb_fired = 0;
    listen->addListener([&cb_fired](const auto&, const auto&) { cb_fired++; });
    VecStr new_addrs = {"0.0.0.0:9090"};
    listen->setValue(new_addrs);
    assert(cb_fired == 1);
    printf("8. Callback fired on vector change\n");

    printf("\n=== Config Container Test: ALL PASSED ===\n");
    return 0;
}
