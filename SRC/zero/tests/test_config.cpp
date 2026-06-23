// test_config.cpp — Comprehensive Config unit tests
// Tests Config singleton, set/get for int/string/bool/double,
// typed accessors (get_int/get_string/get_bool/get_double),
// int64 accessors, nested paths ("a.b.c"), default values,
// load_from_file() and load_from_env(), has(), remove(),
// clear(), watch() change listeners, to_string() dump,
// override/update, edge cases, thread safety, stress tests.

#include <gtest/gtest.h>
#include "zero/zero.h"
#include "zero/config/config.h"

#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <atomic>
#include <vector>

using namespace zero;

// ============================================================
// Config singleton
// ============================================================

TEST(Config, SingletonAccess) {
    Config& c1 = Config::instance();
    Config& c2 = Config::instance();
    EXPECT_EQ(&c1, &c2);
}

TEST(Config, GetConfigFreeFunction) {
    Config& c1 = GetConfig();
    Config& c2 = Config::instance();
    EXPECT_EQ(&c1, &c2);
}

// ============================================================
// Basic set/get for int
// ============================================================

TEST(Config, SetAndGetInt) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set<int>("port", 8080);
    EXPECT_EQ(cfg.get<int>("port", 0), 8080);
    EXPECT_EQ(cfg.get_int("port", 0), 8080);
}

TEST(Config, SetIntGetInt) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set_int("threads", 8);
    EXPECT_EQ(cfg.get_int("threads", 0), 8);
}

TEST(Config, GetIntDefaultWhenMissing) {
    auto& cfg = Config::instance();
    cfg.clear();
    EXPECT_EQ(cfg.get<int>("nonexistent", 42), 42);
    EXPECT_EQ(cfg.get_int("nonexistent", 99), 99);
}

// ============================================================
// Basic set/get for string
// ============================================================

TEST(Config, SetAndGetString) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set<std::string>("host", "127.0.0.1");
    EXPECT_EQ(cfg.get<std::string>("host", ""), "127.0.0.1");
    EXPECT_EQ(cfg.get_string("host", ""), "127.0.0.1");
}

TEST(Config, SetStringGetString) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set_string("name", "zero_server");
    EXPECT_EQ(cfg.get_string("name", ""), "zero_server");
}

TEST(Config, GetStringDefault) {
    auto& cfg = Config::instance();
    cfg.clear();
    EXPECT_EQ(cfg.get<std::string>("missing.key", "fallback"), "fallback");
    EXPECT_EQ(cfg.get_string("another.missing", "default_val"), "default_val");
}

// ============================================================
// Basic set/get for bool
// ============================================================

TEST(Config, SetAndGetBool) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set<bool>("debug", true);
    EXPECT_TRUE(cfg.get<bool>("debug", false));
    EXPECT_TRUE(cfg.get_bool("debug", false));

    cfg.set<bool>("debug", false);
    EXPECT_FALSE(cfg.get<bool>("debug", true));
    EXPECT_FALSE(cfg.get_bool("debug", true));
}

TEST(Config, SetBoolGetBool) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set_bool("enabled", true);
    EXPECT_TRUE(cfg.get_bool("enabled", false));
}

// ============================================================
// Basic set/get for double
// ============================================================

TEST(Config, SetAndGetDouble) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set<double>("pi", 3.14159);
    EXPECT_DOUBLE_EQ(cfg.get<double>("pi", 0.0), 3.14159);
    EXPECT_DOUBLE_EQ(cfg.get_double("pi", 0.0), 3.14159);
}

TEST(Config, SetDoubleGetDouble) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set_double("threshold", 0.001);
    EXPECT_DOUBLE_EQ(cfg.get_double("threshold", 0.0), 0.001);
}

// ============================================================
// int64 accessors
// ============================================================

TEST(Config, SetAndGetInt64) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set_int64("big", INT64_MAX);
    EXPECT_EQ(cfg.get_int64("big", 0), INT64_MAX);

    cfg.set_int64("small", INT64_MIN);
    EXPECT_EQ(cfg.get_int64("small", 0), INT64_MIN);
}

TEST(Config, GetInt64Default) {
    auto& cfg = Config::instance();
    cfg.clear();
    EXPECT_EQ(cfg.get_int64("nope", 12345678901234LL), 12345678901234LL);
}

// ============================================================
// Nested paths (dot-separated)
// ============================================================

TEST(Config, NestedPath) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set<int>("server.http.port", 6379);
    cfg.set<std::string>("server.http.host", "0.0.0.0");
    cfg.set<bool>("server.http.enabled", true);

    EXPECT_EQ(cfg.get_int("server.http.port", 0), 6379);
    EXPECT_EQ(cfg.get_string("server.http.host", ""), "0.0.0.0");
    EXPECT_TRUE(cfg.get_bool("server.http.enabled", false));
}

TEST(Config, DeepNestedPath) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set<int>("a.b.c.d.e.f.g", 100);
    EXPECT_EQ(cfg.get_int("a.b.c.d.e.f.g", 0), 100);
    EXPECT_EQ(cfg.get_int("a.b.c.d.e.f.x", 200), 200); // Returns default
}

TEST(Config, SiblingSections) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set<int>("db.mysql.port", 3306);
    cfg.set<int>("db.redis.port", 6379);
    cfg.set<std::string>("db.mysql.host", "mysql_host");
    cfg.set<std::string>("db.redis.host", "redis_host");

    EXPECT_EQ(cfg.get_int("db.mysql.port", 0), 3306);
    EXPECT_EQ(cfg.get_int("db.redis.port", 0), 6379);
    EXPECT_EQ(cfg.get_string("db.mysql.host", ""), "mysql_host");
    EXPECT_EQ(cfg.get_string("db.redis.host", ""), "redis_host");
}

TEST(Config, MultipleValuesSameSection) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set_int("app.workers", 8);
    cfg.set_string("app.name", "zero");
    cfg.set_bool("app.debug", false);
    cfg.set_double("app.timeout", 30.0);

    EXPECT_EQ(cfg.get_int("app.workers", 0), 8);
    EXPECT_EQ(cfg.get_string("app.name", ""), "zero");
    EXPECT_FALSE(cfg.get_bool("app.debug", true));
    EXPECT_DOUBLE_EQ(cfg.get_double("app.timeout", 0.0), 30.0);
}

// ============================================================
// Default values when key doesn't exist
// ============================================================

TEST(Config, DefaultValuesAllTypes) {
    auto& cfg = Config::instance();
    cfg.clear();
    EXPECT_EQ(cfg.get<int>("nope", -1), -1);
    EXPECT_EQ(cfg.get<std::string>("nope", "def"), "def");
    EXPECT_TRUE(cfg.get<bool>("nope", true));
    EXPECT_DOUBLE_EQ(cfg.get<double>("nope", 3.14), 3.14);
}

// ============================================================
// Override and update existing values
// ============================================================

TEST(Config, OverrideValue) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set<int>("key", 1);
    EXPECT_EQ(cfg.get_int("key", 0), 1);
    cfg.set<int>("key", 2);
    EXPECT_EQ(cfg.get_int("key", 0), 2);
    cfg.set<int>("key", 100);
    EXPECT_EQ(cfg.get_int("key", 0), 100);
}

TEST(Config, OverrideNested) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set<double>("calc.scale", 1.5);
    EXPECT_DOUBLE_EQ(cfg.get_double("calc.scale", 0.0), 1.5);
    cfg.set<double>("calc.scale", 2.0);
    EXPECT_DOUBLE_EQ(cfg.get_double("calc.scale", 0.0), 2.0);
}

// ============================================================
// has() — check if key exists
// ============================================================

TEST(Config, Has) {
    auto& cfg = Config::instance();
    cfg.clear();
    EXPECT_FALSE(cfg.has("some.key"));
    cfg.set<int>("some.key", 42);
    EXPECT_TRUE(cfg.has("some.key"));
}

TEST(Config, HasNested) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set<std::string>("section.sub.key", "val");
    EXPECT_TRUE(cfg.has("section.sub.key"));
    EXPECT_FALSE(cfg.has("section.other.key"));
}

// ============================================================
// remove() — remove a key
// ============================================================

TEST(Config, Remove) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set<int>("removable", 100);
    EXPECT_TRUE(cfg.has("removable"));
    EXPECT_TRUE(cfg.remove("removable"));
    EXPECT_FALSE(cfg.has("removable"));
}

TEST(Config, RemoveNested) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set<std::string>("a.b.c", "xyz");
    EXPECT_TRUE(cfg.has("a.b.c"));
    cfg.remove("a.b.c");
    EXPECT_FALSE(cfg.has("a.b.c"));
}

TEST(Config, RemoveNonexistent) {
    auto& cfg = Config::instance();
    cfg.clear();
    EXPECT_FALSE(cfg.remove("does.not.exist"));
}

// ============================================================
// clear() — clear all configuration
// ============================================================

TEST(Config, ClearAll) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set<int>("a.b.c", 1);
    cfg.set<std::string>("x.y.z", "hello");
    cfg.clear();
    EXPECT_FALSE(cfg.has("a.b.c"));
    EXPECT_FALSE(cfg.has("x.y.z"));
}

// ============================================================
// to_string() — dump config
// ============================================================

TEST(Config, ToString) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set<int>("test.val", 42);
    cfg.set<std::string>("test.name", "foo");
    std::string output = cfg.to_string();
    EXPECT_FALSE(output.empty());
}

TEST(Config, ToStringEmpty) {
    auto& cfg = Config::instance();
    cfg.clear();
    std::string output = cfg.to_string();
    // May be empty or contain minimal structure
    SUCCEED();
}

// ============================================================
// load_from_file() — YAML config file
// ============================================================

TEST(Config, LoadFromFile) {
    auto& cfg = Config::instance();
    cfg.clear();

    const std::string filepath = "/tmp/zero_test_config_" +
        std::to_string(getpid()) + ".yaml";
    {
        std::ofstream f(filepath);
        f << "server:\n";
        f << "  port: 9090\n";
        f << "  host: 127.0.0.1\n";
        f << "debug: true\n";
        f << "threshold: 0.5\n";
    }

    bool loaded = cfg.load_from_file(filepath);
    EXPECT_TRUE(loaded);

    EXPECT_EQ(cfg.get_int("server.port", 0), 9090);
    EXPECT_EQ(cfg.get_string("server.host", ""), "127.0.0.1");
    EXPECT_TRUE(cfg.get_bool("debug", false));
    EXPECT_DOUBLE_EQ(cfg.get_double("threshold", 0.0), 0.5);

    std::remove(filepath.c_str());
}

TEST(Config, LoadFromFileNonexistent) {
    auto& cfg = Config::instance();
    cfg.clear();
    bool loaded = cfg.load_from_file("/tmp/zero_nonexistent_cfg_99999.yaml");
    EXPECT_FALSE(loaded);
}

// ============================================================
// load_from_env() — environment variable substitution
// ============================================================

TEST(Config, LoadFromEnv) {
    auto& cfg = Config::instance();
    cfg.clear();

    setenv("ZERO_SERVER_PORT", "8080", 1);
    setenv("ZERO_SERVER_HOST", "env_host", 1);
    setenv("ZERO_DEBUG", "true", 1);

    cfg.load_from_env("ZERO_");

    EXPECT_EQ(cfg.get_int("SERVER.PORT", 0), 8080);
    EXPECT_EQ(cfg.get_string("SERVER.HOST", ""), "env_host");
    EXPECT_TRUE(cfg.get_bool("DEBUG", false));

    unsetenv("ZERO_SERVER_PORT");
    unsetenv("ZERO_SERVER_HOST");
    unsetenv("ZERO_DEBUG");
}

TEST(Config, LoadFromEnvDefaultPrefix) {
    auto& cfg = Config::instance();
    cfg.clear();
    setenv("ZERO_TEST_VAL", "42", 1);
    cfg.load_from_env();  // Default prefix "ZERO_"
    EXPECT_EQ(cfg.get_int("TEST.VAL", 0), 42);
    unsetenv("ZERO_TEST_VAL");
}

// ============================================================
// load_from_args — command line
// ============================================================

TEST(Config, LoadFromArgs) {
    auto& cfg = Config::instance();
    cfg.clear();

    const char* argv[] = {
        "program",
        "--server.port=7070",
        "--debug=true",
        "--name=test_app",
        nullptr
    };
    int argc = 4;

    cfg.load_from_args(argc, const_cast<char**>(argv));

    EXPECT_EQ(cfg.get_int("server.port", 0), 7070);
    EXPECT_TRUE(cfg.get_bool("debug", false));
    EXPECT_EQ(cfg.get_string("name", ""), "test_app");
}

// ============================================================
// watch() — change listeners
// ============================================================

TEST(Config, WatchIntListener) {
    auto& cfg = Config::instance();
    cfg.clear();

    int old_val = 0;
    int new_val = 0;
    bool called = false;

    cfg.set<int>("watched.value", 10);
    cfg.watch<int>("watched.value",
        [&](const int& old_v, const int& new_v) {
            old_val = old_v;
            new_val = new_v;
            called = true;
        });

    cfg.set<int>("watched.value", 20);
    EXPECT_TRUE(called);
    EXPECT_EQ(old_val, 10);
    EXPECT_EQ(new_val, 20);
}

TEST(Config, WatchStringListener) {
    auto& cfg = Config::instance();
    cfg.clear();

    std::string old_val;
    std::string new_val;
    bool called = false;

    cfg.set<std::string>("watched.str", "initial");
    cfg.watch<std::string>("watched.str",
        [&](const std::string& old_v, const std::string& new_v) {
            old_val = old_v;
            new_val = new_v;
            called = true;
        });

    cfg.set<std::string>("watched.str", "updated");
    EXPECT_TRUE(called);
    EXPECT_EQ(old_val, "initial");
    EXPECT_EQ(new_val, "updated");
}

// ============================================================
// File load then programmatic override
// ============================================================

TEST(Config, FileLoadThenOverride) {
    auto& cfg = Config::instance();
    cfg.clear();

    const std::string filepath = "/tmp/zero_test_override_" +
        std::to_string(getpid()) + ".yaml";
    {
        std::ofstream f(filepath);
        f << "value: 100\n";
    }

    cfg.load_from_file(filepath);
    EXPECT_EQ(cfg.get_int("value", 0), 100);

    cfg.set_int("value", 200);
    EXPECT_EQ(cfg.get_int("value", 0), 200);

    std::remove(filepath.c_str());
}

// ============================================================
// Edge cases
// ============================================================

TEST(Config, ZeroAndNegativeValues) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set_int("zero_int", 0);
    cfg.set_double("zero_double", 0.0);
    cfg.set_int("neg_int", -1000);

    EXPECT_EQ(cfg.get_int("zero_int", 999), 0);
    EXPECT_DOUBLE_EQ(cfg.get_double("zero_double", 999.0), 0.0);
    EXPECT_EQ(cfg.get_int("neg_int", 0), -1000);
}

TEST(Config, EmptyString) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set<std::string>("empty", "");
    EXPECT_EQ(cfg.get_string("empty", "not_empty"), "");
}

TEST(Config, VeryLongKey) {
    auto& cfg = Config::instance();
    cfg.clear();
    std::string long_key(1000, 'k');
    cfg.set<int>(long_key, 42);
    EXPECT_EQ(cfg.get<int>(long_key, 0), 42);
}

TEST(Config, VeryLongValue) {
    auto& cfg = Config::instance();
    cfg.clear();
    std::string long_value(10000, 'x');
    cfg.set<std::string>("long", long_value);
    EXPECT_EQ(cfg.get_string("long", ""), long_value);
}

// ============================================================
// Stress: many keys
// ============================================================

TEST(Config, ManyKeys) {
    auto& cfg = Config::instance();
    cfg.clear();

    const int kNumKeys = 500;
    for (int i = 0; i < kNumKeys; ++i) {
        std::string path = "stress.key_" + std::to_string(i);
        cfg.set<int>(path, i * 10);
    }
    for (int i = 0; i < kNumKeys; ++i) {
        std::string path = "stress.key_" + std::to_string(i);
        EXPECT_EQ(cfg.get<int>(path, -1), i * 10);
    }
}

TEST(Config, ManyNestedKeys) {
    auto& cfg = Config::instance();
    cfg.clear();

    for (int i = 0; i < 100; ++i) {
        std::string path = "level1.level2.level3.key" + std::to_string(i);
        cfg.set<std::string>(path, "value_" + std::to_string(i));
    }
    for (int i = 0; i < 100; ++i) {
        std::string path = "level1.level2.level3.key" + std::to_string(i);
        EXPECT_EQ(cfg.get<std::string>(path, ""), "value_" + std::to_string(i));
    }
}

// ============================================================
// Thread safety
// ============================================================

TEST(Config, ThreadSafetyReadWrite) {
    auto& cfg = Config::instance();
    cfg.clear();
    std::atomic<bool> start{false};
    const int kThreads = 8;
    const int kPerThread = 500;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t]() {
            while (!start.load()) {}
            for (int i = 0; i < kPerThread; ++i) {
                std::string key = "thread" + std::to_string(t) +
                                  "_key" + std::to_string(i);
                cfg.set<int>(key, i);
                EXPECT_EQ(cfg.get<int>(key, -1), i);
            }
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();
}

TEST(Config, ThreadSafetyConcurrentRead) {
    auto& cfg = Config::instance();
    cfg.clear();
    cfg.set<int>("shared.key", 999);

    std::atomic<int> successes{0};
    const int kThreads = 16;
    const int kIterations = 500;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < kIterations; ++i) {
                if (cfg.get<int>("shared.key", -1) == 999) {
                    successes.fetch_add(1);
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(successes.load(), kThreads * kIterations);
}
