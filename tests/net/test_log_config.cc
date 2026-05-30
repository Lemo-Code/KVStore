#include "test_common.h"

#include "log/log.h"

#include <string>

int main() {
  net::LogConfig& cfg = net::LogConfig::instance();
  cfg.resetToDefaults();

  NET_CHECK(cfg.flushIntervalMs() == static_cast<uint32_t>(NET_LOG_ASYNC_FLUSH_MS));
  NET_CHECK(cfg.bufBytes() == static_cast<size_t>(NET_LOG_ASYNC_BUF_BYTES));
  NET_CHECK(cfg.softCap() == static_cast<size_t>(NET_LOG_ASYNC_SOFT_CAP));
  NET_CHECK(cfg.degradeMode() == NET_LOG_DEGRADE_MODE);
  NET_CHECK(cfg.sampleRate() == static_cast<uint32_t>(NET_LOG_DEGRADE_SAMPLE_RATE));
  NET_CHECK(cfg.defaultLevel() == NET_LOG_DEFAULT_LEVEL);
  NET_CHECK(cfg.defaultPattern() == NET_LOG_DEFAULT_PATTERN);

  net::AsyncLogSettings async = cfg.settings().async;
  async.degrade_mode = 1;
  async.soft_cap = 4;
  cfg.applyAsync(async);
  cfg.resetStats();

  NET_CHECK(cfg.allowAsyncEnqueue(3));
  NET_CHECK(!cfg.allowAsyncEnqueue(4));
  NET_CHECK(cfg.droppedCount() == 1);

  async.degrade_mode = 2;
  async.soft_cap = 2;
  async.sample_rate = 4;
  cfg.applyAsync(async);
  cfg.resetStats();

  size_t accepted = 0;
  for (int i = 0; i < 8; ++i) {
    if (cfg.allowAsyncEnqueue(10)) {
      ++accepted;
    }
  }
  NET_CHECK(accepted >= 1);
  NET_CHECK(cfg.droppedCount() + cfg.sampledCount() == 8);

  async.degrade_mode = 0;
  cfg.applyAsync(async);
  cfg.resetStats();
  NET_CHECK(cfg.allowAsyncEnqueue(100000));

  net::LogModuleSettings custom;
  custom.default_level = 3;
  custom.default_pattern = "%m%n";
  custom.async = cfg.settings().async;
  cfg.apply(custom);
  NET_CHECK(cfg.defaultLevel() == 3);
  NET_CHECK(cfg.defaultPattern() == "%m%n");

  cfg.resetToDefaults();
  NET_CHECK(cfg.defaultLevel() == NET_LOG_DEFAULT_LEVEL);

  std::printf("PASS test_log_config\n");
  return 0;
}
