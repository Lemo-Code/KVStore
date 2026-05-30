#ifndef NET_FIBER_TIMER_H
#define NET_FIBER_TIMER_H

#include "fiber/timing_wheel.h"
#include "thread/mutex.h"
#include "thread/noncopyable.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace net {

class TimerManager;

/**
 * @brief 定时器句柄：cancel / refresh / reset。
 */
class Timer : public std::enable_shared_from_this<Timer> {
 public:
  friend class TimerManager;

  typedef std::shared_ptr<Timer> ptr;

  bool cancel();
  bool refresh();
  bool reset(uint64_t ms, bool from_now);

  uint64_t getId() const { return id_; }
  uint64_t getIntervalMs() const { return interval_ms_; }
  bool isRecurring() const { return recurring_; }

 private:
  Timer(uint64_t id, uint64_t interval_ms, std::function<void()> cb,
        bool recurring, TimerManager* manager);
  Timer(uint64_t id, uint64_t next_deadline_ms);

  uint64_t id_ = 0;
  uint64_t interval_ms_ = 0;
  uint64_t next_deadline_ms_ = 0;
  bool recurring_ = false;
  std::function<void()> cb_;
  std::weak_ptr<void> weak_cond_;
  TimerManager* manager_ = nullptr;
};

/**
 * @brief 定时器管理器：四层时间轮 + Timer 对象生命周期。
 */
class TimerManager : Noncopyable {
 public:
  friend class Timer;

  typedef Mutex MutexType;

  TimerManager();
  virtual ~TimerManager();

  Timer::ptr addTimer(uint64_t ms, std::function<void()> cb,
                      bool recurring = false);
  Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb,
                               std::weak_ptr<void> weak_cond,
                               bool recurring = false);

  void cancelTimer(uint64_t timer_id);
  void cancelAll();
  bool refreshTimer(Timer* timer);
  bool resetTimer(Timer* timer, uint64_t ms, bool from_now);

  uint64_t getNextTimer() const;
  bool hasTimer() const;
  size_t timerCount() const;

  void listExpiredCb(std::vector<std::function<void()>>& cbs);

 protected:
  virtual void onTimerInsertedAtFront() = 0;

  void addTimer(Timer::ptr timer, MutexType::Lock& lock);
  bool detectClockRollover(uint64_t now_ms);

 private:
  struct TimerRecord {
    uint64_t id = 0;
    uint64_t deadline_ms = 0;
    uint64_t interval_ms = 0;
    uint32_t rounds = 0;
    uint8_t level = 0;
    size_t slot = 0;
    bool recurring = false;
    bool canceled = false;
    bool cond_guard = false;
    std::function<void()> cb;
    std::weak_ptr<void> weak_cond;
    Timer::ptr handle;
  };

  uint64_t allocateId();
  void insertRecord(TimerRecord& record, uint64_t now_ms);
  void removeFromWheel(TimerRecord& record);
  void rebuildAll(uint64_t now_ms);
  void refreshNextExpireLocked();
  void fireRecord(TimerRecord& record, uint64_t now_ms,
                  std::vector<std::function<void()>>& cbs,
                  std::vector<TimerRecord*>& reinsert);
  void processL0Slot(size_t slot, uint64_t now_ms,
                       std::vector<std::function<void()>>& cbs,
                       std::vector<TimerRecord*>& reinsert);

  mutable MutexType mutex_;
  HierarchicalTimingWheel wheel_;
  std::unordered_map<uint64_t, TimerRecord> timers_;
  std::atomic<uint64_t> next_id_{1};
  uint64_t next_expire_ms_ = UINT64_MAX;
  uint64_t previous_time_ms_ = 0;
  size_t active_count_ = 0;
  bool ticked_ = false;
};

}  // namespace net

#endif  // NET_FIBER_TIMER_H
