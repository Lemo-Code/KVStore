#ifndef LEMO_FIBER_SCHEDULER_H
#define LEMO_FIBER_SCHEDULER_H

#include "lemo/fiber/fiber.h"
#include "lemo/fiber/run_queue.h"
#include "lemo/fiber/timer.h"
#include "lemo/thread/mutex.h"
#include "lemo/thread/semaphore.h"
#include "lemo/thread/thread.h"
#include "lemo/utils/noncopyable.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace lemo {
namespace fiber {

/**
 * @brief Go 风格协程调度器（GMP）+ TimerManager 定时器。
 */
class Scheduler : public TimerManager {
 public:
  typedef std::shared_ptr<Scheduler> ptr;
  using MutexType = thread::ColdMutex;

  Scheduler(size_t threads = 1, bool use_caller = true,
            const std::string& name = "");
  virtual ~Scheduler();

  const std::string& getName() const { return name_; }
  bool hasThreadIdle() const { return idle_thread_count_ > 0; }

  static Fiber* GetMainFiber();
  static Scheduler* GetThis();
  static size_t GetProcessorId();

  int threadIdForProcessor(size_t proc_id) const;

  virtual void start();
  virtual void stop();
  void switchTo(int thread = -1);
  std::ostream& dump(std::ostream& os);

  uint64_t addTimer(uint64_t delay_ms, std::function<void()> cb);
  Timer::ptr addTimerPtr(uint64_t delay_ms, std::function<void()> cb,
                         bool recurring = false);

  template <class FiberOrCb>
  void schedule(FiberOrCb fc, int thread = -1) {
    const bool need_tickle = enqueue(FiberOrCbTag{}, fc, thread);
    if (need_tickle) {
      tickle();
    }
  }

  template <class InputIterator>
  void schedule(InputIterator begin, InputIterator end) {
    bool need_tickle = false;
    while (begin != end) {
      need_tickle = enqueue(FiberOrCbTag{}, *begin, -1) || need_tickle;
      ++begin;
    }
    if (need_tickle) {
      tickle();
    }
  }

  /** I/O 等同线程唤醒：写入 runnext 槽，不入队、不 tickle。 */
  void scheduleNext(Fiber::ptr fiber, int thread = -1) {
    setRunnext(ScheduleTask(std::move(fiber), thread));
  }

  void scheduleNext(Fiber::ptr* fiber, int thread = -1) {
    setRunnext(ScheduleTask(fiber, thread));
  }

  void scheduleNext(std::function<void()> cb, int thread = -1) {
    setRunnext(ScheduleTask(std::move(cb), thread));
  }

  void scheduleNext(std::function<void()>* cb, int thread = -1) {
    setRunnext(ScheduleTask(cb, thread));
  }

 protected:
  void onTimerInsertedAtFront() override;

  virtual Fiber::ptr newIdleFiber() {
    return Fiber::ptr(new Fiber(std::bind(&Scheduler::idle, this)));
  }

  virtual void run();
  virtual void idle();
  virtual bool stopping();
  virtual void tickle();
  void setThis();

  struct FiberOrCbTag {};

  template <class FiberOrCb>
  bool enqueue(FiberOrCbTag, FiberOrCb fc, int thread) {
    ScheduleTask task(fc, thread);
    if (!task.valid()) {
      return false;
    }
    if (task.fiber && task.fiber->getState() == Fiber::EXEC) {
      task.fiber->setState(Fiber::READY);
    }
    return enqueueTask(std::move(task), thread);
  }

  bool enqueueTask(ScheduleTask task, int pin_thread);
  void requeueTask(ScheduleTask task);
  void setRunnext(ScheduleTask task);
  bool takeRunnext(ScheduleTask& task);
  void processTimers();

 private:
  enum class TaskSource { kRunnext, kLocal, kGlobal, kSteal };

  size_t pickProcessorForEnqueue(int pin_thread);
  size_t resolveProcessorId() const;

  bool fetchTask(ScheduleTask& task, size_t proc_id);
  bool dispatchCandidate(ScheduleTask& out, ScheduleTask candidate,
                         int self_thread, TaskSource source);
  bool fetchFromGlobal(ScheduleTask& task, int self_thread);
  bool popGlobalCandidate(ScheduleTask& candidate);
  bool stealTask(ScheduleTask& task, size_t proc_id, int self_thread);

  bool taskRunnable(const ScheduleTask& task, int self_thread) const;
  bool allQueuesEmpty() const;

  void runFiberTask(ScheduleTask& task);
  void runCbTask(ScheduleTask& task, Fiber::ptr& cb_fiber);
  bool parkIdle(Fiber::ptr& idle_fiber);
  void ensureIdleFiber(Fiber::ptr& idle_fiber);

  std::vector<std::unique_ptr<LocalRunQueue>> runqs_;
  std::vector<int> proc_thread_ids_;
  GlobalRunQueue global_runq_;
  std::atomic<size_t> enqueue_round_robin_{0};
  std::atomic<size_t> steal_seq_{0};
  std::atomic<uint64_t> local_pop_count_{0};
  std::atomic<uint64_t> global_pop_count_{0};
  std::atomic<uint64_t> steal_count_{0};
  std::atomic<uint64_t> overflow_count_{0};
  thread::Semaphore idle_sem_{0};

  static constexpr int kIdleSpinRounds = 64;
  static constexpr size_t kGlobalPrefetchBatch = GlobalRunQueue::kMaxBatchPop;

  std::vector<thread::Thread::ptr> threads_;
  Fiber::ptr root_fiber_;
  std::string name_;

  std::vector<int> thread_ids_;
  size_t thread_count_ = 0;
  std::atomic<size_t> active_thread_count_{0};
  std::atomic<size_t> idle_thread_count_{0};
  bool stopping_ = true;
  bool auto_stop_ = false;
  int root_thread_ = 0;
};

class SchedulerSwitcher : public utils::NonCopyable {
 public:
  explicit SchedulerSwitcher(Scheduler* target = nullptr);
  ~SchedulerSwitcher();

 private:
  Scheduler* caller_ = nullptr;
};

}  // namespace fiber
}  // namespace lemo

#endif  // LEMO_FIBER_SCHEDULER_H
