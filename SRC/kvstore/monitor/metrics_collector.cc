#include "kvstore/monitor/metrics_collector.h"
#include <sstream>
namespace zero { namespace kvstore {
void MetricsCollector::RecordGet(int64_t us) { total_ops_++; total_latency_us_ += us; }
void MetricsCollector::RecordPut(int64_t us) { total_ops_++; total_latency_us_ += us; }
uint64_t MetricsCollector::TotalOps() const { return total_ops_.load(); }
std::string MetricsCollector::Report() const {
    uint64_t ops = total_ops_;
    uint64_t lat = total_latency_us_;
    std::ostringstream oss;
    oss << "ops=" << ops << " avg_latency_us=" << (ops > 0 ? lat / ops : 0);
    return oss.str();
}
}} // namespace
