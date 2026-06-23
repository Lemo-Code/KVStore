#pragma once
#include "kvstore/common/kv_types.h"
#include "kvstore/common/kv_error.h"
#include <vector>
#include <atomic>
namespace zero { namespace kvstore {
class HealthChecker {
public:
    void Start();
    void Stop();
    bool IsAlive(const NodeId& id) const;
private:
    std::atomic<bool> running_{false};
};
}} // namespace
