#include "fiber/timer.h"

#include "common/util.h"

#include <climits>
#include <utility>

namespace net {

namespace {

constexpr uint64_t kClockRolloverThresholdMs = 3600000ull;

}  // namespace

Timer::Timer(uint64_t id, uint64_t interval_ms, std::function<void()> cb,
             bool recurring, TimerManager* manager)
    : id_(id),
      interval_ms_(interval_ms),
      next_deadline_ms_(GetCurrentMS() + interval_ms),
      recurring_(recurring),
      cb_(std::move(cb)),
      manager_(manager) {}

Timer::Timer(uint64_t id, uint64_t next_deadline_ms)
    : id_(id), next_deadline_ms_(next_deadline_ms) {}

bool Timer::cancel() {
  if (!manager_ || !cb_) {
    return false;
  }
  manager_->cancelTimer(id_);
  cb_ = nullptr;
  return true;
}

bool Timer::refresh() {
  if (!manager_ || !cb_) {
    return false;
  }
  return manager_->refreshTimer(this);
}

bool Timer::reset(uint64_t ms, bool from_now) {
  if (!manager_ || !cb_) {
    return false;
  }
  return manager_->resetTimer(this, ms, from_now);
}

TimerManager::TimerManager() {
  previous_time_ms_ = GetCurrentMS();
}

TimerManager::~TimerManager() {}

uint64_t TimerManager::allocateId() {
  return next_id_.fetch_add(1, std::memory_order_relaxed);
}

void TimerManager::removeFromWheel(TimerRecord& record) {
  wheel_.remove(record.level, record.slot, record.id);
}

void TimerManager::insertRecord(TimerRecord& record, uint64_t now_ms) {
  uint64_t delay = 0;
  if (record.deadline_ms > now_ms) {
    delay = record.deadline_ms - now_ms;
  }
  const WheelPlacement place =
      HierarchicalTimingWheel::computePlacement(wheel_.globalTick(), delay);
  record.level = place.level;
  record.rounds = place.rounds;
  record.slot = place.slot;
  wheel_.add(record.level, record.slot, record.id);
}

void TimerManager::refreshNextExpireLocked() {
  next_expire_ms_ = UINT64_MAX;
  for (const auto& kv : timers_) {
    const TimerRecord& record = kv.second;
    if (record.canceled) {
      continue;
    }
    if (record.deadline_ms < next_expire_ms_) {
      next_expire_ms_ = record.deadline_ms;
    }
  }
}

bool TimerManager::detectClockRollover(uint64_t now_ms) {
  if (now_ms < previous_time_ms_ &&
      previous_time_ms_ - now_ms > kClockRolloverThresholdMs) {
    return true;
  }
  previous_time_ms_ = now_ms;
  return false;
}

void TimerManager::rebuildAll(uint64_t now_ms) {
  for (auto& kv : timers_) {
    TimerRecord& record = kv.second;
    if (record.canceled) {
      continue;
    }
    removeFromWheel(record);
    insertRecord(record, now_ms);
  }
  refreshNextExpireLocked();
}

void TimerManager::fireRecord(TimerRecord& record, uint64_t now_ms,
                              std::vector<std::function<void()>>& cbs,
                              std::vector<TimerRecord*>& reinsert) {
  if (record.canceled) {
    return;
  }
  if (record.cond_guard && record.weak_cond.expired()) {
    record.canceled = true;
    if (active_count_ > 0) {
      --active_count_;
    }
    return;
  }
  if (record.cb) {
    cbs.push_back(record.cb);
  }
  if (record.recurring && record.cb) {
    record.deadline_ms = now_ms + record.interval_ms;
    if (record.handle) {
      record.handle->next_deadline_ms_ = record.deadline_ms;
    }
    reinsert.push_back(&record);
  } else {
    record.canceled = true;
    record.cb = nullptr;
    if (record.handle) {
      record.handle->cb_ = nullptr;
    }
    if (active_count_ > 0) {
      --active_count_;
    }
  }
}

void TimerManager::processL0Slot(size_t slot, uint64_t now_ms,
                                 std::vector<std::function<void()>>& cbs,
                                 std::vector<TimerRecord*>& reinsert) {
  std::vector<uint64_t>& bucket = wheel_.l0Bucket(slot);
  std::vector<uint64_t> keep;
  keep.reserve(bucket.size());

  for (uint64_t id : bucket) {
    auto it = timers_.find(id);
    if (it == timers_.end()) {
      continue;
    }
    TimerRecord& record = it->second;
    if (record.canceled) {
      continue;
    }
    if (record.rounds > 0) {
      --record.rounds;
      keep.push_back(id);
      continue;
    }
    if (record.deadline_ms <= now_ms) {
      fireRecord(record, now_ms, cbs, reinsert);
      if (record.recurring && !record.canceled) {
        continue;
      }
      continue;
    }
    keep.push_back(id);
  }
  bucket.swap(keep);
}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb,
                                  bool recurring) {
  const uint64_t id = allocateId();
  Timer::ptr timer(new Timer(id, ms, cb, recurring, this));

  TimerRecord record;
  record.id = id;
  record.interval_ms = ms;
  record.deadline_ms = GetCurrentMS() + ms;
  record.recurring = recurring;
  record.cb = timer->cb_;
  record.cond_guard = false;
  record.handle = timer;
  record.canceled = false;

  bool at_front = false;
  {
    MutexType::Lock lock(mutex_);
    const uint64_t now_ms = GetCurrentMS();
    if (wheel_.lastAdvanceMs() == 0) {
      wheel_.setLastAdvanceMs(now_ms);
    }
    insertRecord(record, now_ms);
    timers_[id] = record;
    ++active_count_;
    at_front = (record.deadline_ms < next_expire_ms_);
    if (at_front) {
      next_expire_ms_ = record.deadline_ms;
      ticked_ = true;
    }
  }

  if (at_front) {
    onTimerInsertedAtFront();
  }
  return timer;
}

Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb,
                                           std::weak_ptr<void> weak_cond,
                                           bool recurring) {
  const uint64_t id = allocateId();
  Timer::ptr timer(new Timer(id, ms, cb, recurring, this));

  TimerRecord record;
  record.id = id;
  record.interval_ms = ms;
  record.deadline_ms = GetCurrentMS() + ms;
  record.recurring = recurring;
  record.cb = timer->cb_;
  record.weak_cond = weak_cond;
  // 空 weak_ptr 表示无条件定时器；仅当创建时绑定了对象才在失效后取消
  record.cond_guard = static_cast<bool>(weak_cond.lock());
  record.handle = timer;
  record.canceled = false;

  bool at_front = false;
  {
    MutexType::Lock lock(mutex_);
    const uint64_t now_ms = GetCurrentMS();
    if (wheel_.lastAdvanceMs() == 0) {
      wheel_.setLastAdvanceMs(now_ms);
    }
    insertRecord(record, now_ms);
    timers_[id] = record;
    ++active_count_;
    at_front = (record.deadline_ms < next_expire_ms_);
    if (at_front) {
      next_expire_ms_ = record.deadline_ms;
      ticked_ = true;
    }
  }

  if (at_front) {
    onTimerInsertedAtFront();
  }
  return timer;
}

bool TimerManager::refreshTimer(Timer* timer) {
  MutexType::Lock lock(mutex_);
  auto it = timers_.find(timer->id_);
  if (it == timers_.end() || it->second.canceled) {
    return false;
  }
  TimerRecord& record = it->second;
  removeFromWheel(record);
  timer->next_deadline_ms_ = GetCurrentMS() + timer->interval_ms_;
  record.deadline_ms = timer->next_deadline_ms_;
  insertRecord(record, GetCurrentMS());
  refreshNextExpireLocked();
  return true;
}

bool TimerManager::resetTimer(Timer* timer, uint64_t ms, bool from_now) {
  if (ms == timer->interval_ms_ && !from_now) {
    return true;
  }
  MutexType::Lock lock(mutex_);
  auto it = timers_.find(timer->id_);
  if (it == timers_.end() || it->second.canceled) {
    return false;
  }
  TimerRecord& record = it->second;
  removeFromWheel(record);

  uint64_t start = 0;
  if (from_now) {
    start = GetCurrentMS();
  } else {
    start = timer->next_deadline_ms_ - timer->interval_ms_;
  }
  timer->interval_ms_ = ms;
  record.interval_ms = ms;
  timer->next_deadline_ms_ = start + ms;
  record.deadline_ms = timer->next_deadline_ms_;
  insertRecord(record, GetCurrentMS());
  refreshNextExpireLocked();
  return true;
}

void TimerManager::cancelTimer(uint64_t timer_id) {
  MutexType::Lock lock(mutex_);
  auto it = timers_.find(timer_id);
  if (it == timers_.end() || it->second.canceled) {
    return;
  }
  TimerRecord& record = it->second;
  removeFromWheel(record);
  record.canceled = true;
  record.cb = nullptr;
  if (record.handle) {
    record.handle->cb_ = nullptr;
  }
  if (active_count_ > 0) {
    --active_count_;
  }
  refreshNextExpireLocked();
}

void TimerManager::cancelAll() {
  MutexType::Lock lock(mutex_);
  for (auto& kv : timers_) {
    TimerRecord& record = kv.second;
    if (record.canceled) {
      continue;
    }
    removeFromWheel(record);
    record.canceled = true;
    record.cb = nullptr;
    if (record.handle) {
      record.handle->cb_ = nullptr;
    }
  }
  active_count_ = 0;
  next_expire_ms_ = UINT64_MAX;
}

uint64_t TimerManager::getNextTimer() const {
  MutexType::Lock lock(mutex_);
  const uint64_t now_ms = GetCurrentMS();
  if (next_expire_ms_ <= now_ms) {
    return 0;
  }
  if (next_expire_ms_ == UINT64_MAX) {
    return UINT64_MAX;
  }
  return next_expire_ms_ - now_ms;
}

bool TimerManager::hasTimer() const {
  MutexType::Lock lock(mutex_);
  return active_count_ > 0;
}

size_t TimerManager::timerCount() const {
  MutexType::Lock lock(mutex_);
  return active_count_;
}

void TimerManager::listExpiredCb(std::vector<std::function<void()>>& cbs) {
  const uint64_t now_ms = GetCurrentMS();
  std::vector<TimerRecord*> reinsert;

  MutexType::Lock lock(mutex_);
  if (detectClockRollover(now_ms)) {
    rebuildAll(now_ms);
  }

  std::vector<TickEvent> events;
  wheel_.advanceTo(now_ms, events);

  for (const TickEvent& ev : events) {
    processL0Slot(ev.l0_slot, now_ms, cbs, reinsert);
    for (uint64_t cid : ev.cascade_ids) {
      auto it = timers_.find(cid);
      if (it == timers_.end() || it->second.canceled) {
        continue;
      }
      removeFromWheel(it->second);
      insertRecord(it->second, now_ms);
    }
  }

  for (auto& kv : timers_) {
    TimerRecord& record = kv.second;
    if (record.canceled || record.deadline_ms > now_ms) {
      continue;
    }
    if (record.level != 0) {
      removeFromWheel(record);
      fireRecord(record, now_ms, cbs, reinsert);
    }
  }

  for (TimerRecord* record : reinsert) {
    removeFromWheel(*record);
    insertRecord(*record, now_ms);
  }

  refreshNextExpireLocked();
  ticked_ = false;
}

}  // namespace net
