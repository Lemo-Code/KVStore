#pragma once
#include <cstdint>
#include <string>
#include <atomic>
namespace zero { namespace kvstore {
class MetricsCollector {
public:
    void RecordGet(int64_t latency_us);
    void RecordPut(int64_t latency_us);
    uint64_t TotalOps() const;
    std::string Report() const;
private:
    std::atomic<uint64_t> total_ops_{0};
    std::atomic<uint64_t> total_latency_us_{0};
};
}} // namespace
