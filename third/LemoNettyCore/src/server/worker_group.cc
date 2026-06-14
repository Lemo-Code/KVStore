#include "lemo/server/worker_group.h"

namespace lemo {
namespace server {

WorkerGroup::WorkerGroup(size_t workers, bool use_caller,
                         const std::string& name) {
  if (workers == 0) {
    workers = 1;
  }
  workers_.reserve(workers);
  for (size_t i = 0; i < workers; ++i) {
    workers_.push_back(io::Runtime::ptr(
        new io::Runtime(1, use_caller && i == 0, name + "_" + std::to_string(i))));
  }
}

WorkerGroup::~WorkerGroup() { stop(); }

void WorkerGroup::stop() {
  for (size_t i = 0; i < workers_.size(); ++i) {
    if (workers_[i]) {
      workers_[i]->stop();
    }
  }
}

io::Runtime& WorkerGroup::pick() {
  const size_t n = workers_.size();
  const size_t idx = round_robin_.fetch_add(1, std::memory_order_relaxed) % n;
  return *workers_[idx];
}

io::Runtime& WorkerGroup::worker(size_t index) {
  return *workers_.at(index);
}

const io::Runtime& WorkerGroup::worker(size_t index) const {
  return *workers_.at(index);
}

}  // namespace server
}  // namespace lemo
