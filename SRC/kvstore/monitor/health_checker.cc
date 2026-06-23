#include "kvstore/monitor/health_checker.h"
namespace zero { namespace kvstore {
void HealthChecker::Start() { running_ = true; }
void HealthChecker::Stop() { running_ = false; }
bool HealthChecker::IsAlive(const NodeId&) const { return running_; }
}} // namespace
