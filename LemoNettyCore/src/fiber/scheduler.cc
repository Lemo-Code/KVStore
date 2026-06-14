#include "lemo/fiber/scheduler.h"
#include "lemo/fiber/fiber_pool.h"
#include "lemo/memory/stack_pool.h"
#include "lemo/utils/thread_util.h"

#include <cassert>
#include <climits>

namespace lemo {
namespace fiber {

namespace {

thread_local Scheduler* t_scheduler = nullptr;
thread_local Fiber* t_scheduler_fiber = nullptr;
thread_local size_t t_processor_id = static_cast<size_t>(-1);
thread_local ScheduleTask t_run_next;
thread_local std::deque<ScheduleTask> t_global_prefetch;

}  // namespace

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name)
    : name_(name) {
  assert(threads > 0);

  runqs_.clear();
  runqs_.reserve(threads);
  for (size_t i = 0; i < threads; ++i) {
    runqs_.emplace_back(new LocalRunQueue());
  }
  proc_thread_ids_.resize(threads, -1);

  if (use_caller) {
    Fiber::GetThis();
    --threads;

    assert(GetThis() == nullptr);
    t_scheduler = this;

    root_fiber_.reset(new Fiber([this] { this->run(); }, 0, true));
    thread::Thread::SetName(name_);
    t_scheduler_fiber = root_fiber_.get();
    root_thread_ = static_cast<int>(utils::GetThreadId());
    thread_ids_.push_back(root_thread_);

    proc_thread_ids_[0] = root_thread_;
  } else {
    root_thread_ = -1;
  }
  thread_count_ = threads;
}

Scheduler::~Scheduler() {
  assert(stopping_);
  if (GetThis() == this) {
    t_scheduler = nullptr;
  }
}

void Scheduler::start() {
  if (!stopping_) {
    return;
  }
  stopping_ = false;
  assert(threads_.empty());

  threads_.resize(thread_count_);
  const size_t worker_proc_base = (root_fiber_ != nullptr) ? 1 : 0;
  for (size_t i = 0; i < thread_count_; ++i) {
    threads_[i].reset(new thread::Thread([this] { this->run(); },
                                name_ + "_" + std::to_string(i)));
    const int tid = static_cast<int>(threads_[i]->getId());
    thread_ids_.push_back(tid);
    const size_t proc_id = worker_proc_base + i;
    assert(proc_id < runqs_.size());
    proc_thread_ids_[proc_id] = tid;
  }
}

void Scheduler::stop() {
  auto_stop_ = true;
  if (root_fiber_ && thread_count_ == 0 &&
      (root_fiber_->getState() == Fiber::INIT ||
       root_fiber_->getState() == Fiber::TERM)) {
    stopping_ = true;
    if (stopping()) {
      return;
    }
  }

  if (root_thread_ != -1) {
    assert(GetThis() == this);
  } else {
    assert(GetThis() != this);
  }

  stopping_ = true;
  cancelAll();
  for (size_t i = 0; i < thread_count_; ++i) {
    tickle();
  }
  if (root_fiber_) {
    tickle();
  }

  if (root_fiber_) {
    if (!stopping()) {
      root_fiber_->call();
    }
  }

  std::vector<thread::Thread::ptr> workers;
  workers.swap(threads_);
  for (auto& worker : workers) {
    worker->join();
  }
}

size_t Scheduler::pickProcessorForEnqueue(int pin_thread) {
  if (pin_thread != -1) {
    for (size_t i = 0; i < proc_thread_ids_.size(); ++i) {
      if (proc_thread_ids_[i] == pin_thread) {
        return i;
      }
    }
  }

  if (t_processor_id < runqs_.size()) {
    return t_processor_id;
  }

  const size_t n = runqs_.size();
  if (n == 0) {
    return 0;
  }
  return enqueue_round_robin_.fetch_add(1, std::memory_order_relaxed) % n;
}

size_t Scheduler::resolveProcessorId() const {
  const int self_tid = static_cast<int>(utils::GetThreadId());
  if (root_fiber_ && self_tid == root_thread_) {
    return 0;
  }
  for (size_t i = 0; i < proc_thread_ids_.size(); ++i) {
    if (proc_thread_ids_[i] == self_tid) {
      return i;
    }
  }
  return 0;
}

void Scheduler::setRunnext(ScheduleTask task) {
  assert(task.valid());
  if (t_run_next.valid()) {
    const int thr = t_run_next.thread;
    enqueueTask(std::move(t_run_next), thr);
  }
  t_run_next = std::move(task);
}

bool Scheduler::takeRunnext(ScheduleTask& task) {
  if (!t_run_next.valid()) {
    return false;
  }
  task = std::move(t_run_next);
  t_run_next.reset();
  return true;
}

uint64_t Scheduler::addTimer(uint64_t delay_ms, std::function<void()> cb) {
  Timer::ptr timer = TimerManager::addTimer(delay_ms, std::move(cb));
  return timer ? timer->getId() : 0;
}

Timer::ptr Scheduler::addTimerPtr(uint64_t delay_ms, std::function<void()> cb,
                                  bool recurring) {
  return TimerManager::addTimer(delay_ms, std::move(cb), recurring);
}

void Scheduler::onTimerInsertedAtFront() {
  tickle();
}

void Scheduler::processTimers() {
  if (!hasTimer()) {
    return;
  }
  std::vector<std::function<void()>> callbacks;
  listExpiredCb(callbacks);
  for (auto& cb : callbacks) {
    schedule(std::move(cb));
  }
}

bool Scheduler::enqueueTask(ScheduleTask task, int pin_thread) {
  const auto should_tickle = [this](bool was_empty) {
    return was_empty ||
           idle_thread_count_.load(std::memory_order_relaxed) > 0;
  };

  const int dest_thread = (pin_thread != -1) ? pin_thread : task.thread;
  const size_t proc_id = pickProcessorForEnqueue(dest_thread);
  LocalRunQueue& runq = *runqs_[proc_id];

  LocalRunQueue::PushResult pushed = runq.pushBack(task);
  if (pushed.ok) {
    return should_tickle(pushed.was_empty);
  }

  std::deque<ScheduleTask> overflow;
  runq.drainHalfTo(overflow);
  if (!overflow.empty()) {
    const size_t drained = overflow.size();
    global_runq_.pushBatch(overflow);
    overflow_count_.fetch_add(drained, std::memory_order_relaxed);
  }

  pushed = runq.pushBack(task);
  if (pushed.ok) {
    return true;
  }

  global_runq_.push(std::move(task));
  return true;
}

bool Scheduler::taskRunnable(const ScheduleTask& task,
                             int self_thread) const {
  if (!task.valid()) {
    return false;
  }
  if (task.thread != -1 && task.thread != self_thread) {
    return false;
  }
  if (task.fiber) {
    const auto st = task.fiber->getState();
    if (st == Fiber::EXEC || st == Fiber::TERM || st == Fiber::EXCEPT) {
      return false;
    }
  }
  return true;
}

void Scheduler::requeueTask(ScheduleTask task) {
  if (!task.valid()) {
    return;
  }
  const int thr = task.thread;
  enqueueTask(std::move(task), thr);
}

bool Scheduler::dispatchCandidate(ScheduleTask& out, ScheduleTask candidate,
                                  int self_thread, TaskSource source) {
  if (!taskRunnable(candidate, self_thread)) {
    requeueTask(std::move(candidate));
    return false;
  }
  out = std::move(candidate);
  switch (source) {
    case TaskSource::kLocal:
      local_pop_count_.fetch_add(1, std::memory_order_relaxed);
      break;
    case TaskSource::kGlobal:
      global_pop_count_.fetch_add(1, std::memory_order_relaxed);
      break;
    case TaskSource::kSteal:
      steal_count_.fetch_add(1, std::memory_order_relaxed);
      break;
    case TaskSource::kRunnext:
      break;
  }
  return true;
}

bool Scheduler::popGlobalCandidate(ScheduleTask& candidate) {
  if (!t_global_prefetch.empty()) {
    candidate = std::move(t_global_prefetch.front());
    t_global_prefetch.pop_front();
    return true;
  }
  if (global_runq_.popBatch(t_global_prefetch,
                            Scheduler::kGlobalPrefetchBatch) == 0) {
    return false;
  }
  candidate = std::move(t_global_prefetch.front());
  t_global_prefetch.pop_front();
  return true;
}

bool Scheduler::fetchFromGlobal(ScheduleTask& task, int self_thread) {
  const size_t tries = global_runq_.size() + t_global_prefetch.size() + 1;
  ScheduleTask candidate;
  for (size_t i = 0; i < tries; ++i) {
    if (!popGlobalCandidate(candidate)) {
      break;
    }
    if (dispatchCandidate(task, std::move(candidate), self_thread,
                          TaskSource::kGlobal)) {
      return true;
    }
  }
  return false;
}

bool Scheduler::stealTask(ScheduleTask& task, size_t proc_id, int self_thread) {
  const size_t n = runqs_.size();
  if (n <= 1) {
    return false;
  }

  const size_t start =
      steal_seq_.fetch_add(1, std::memory_order_relaxed) % (n - 1);
  ScheduleTask candidate;
  for (size_t i = 0; i < n - 1; ++i) {
    const size_t victim = (proc_id + 1 + start + i) % n;
    if (victim == proc_id) {
      continue;
    }
    if (!runqs_[victim]->stealBack(candidate)) {
      continue;
    }
    if (dispatchCandidate(task, std::move(candidate), self_thread,
                          TaskSource::kSteal)) {
      return true;
    }
  }
  return false;
}

bool Scheduler::fetchTask(ScheduleTask& task, size_t proc_id) {
  const int self_thread = static_cast<int>(utils::GetThreadId());
  ScheduleTask candidate;

  if (takeRunnext(candidate) &&
      dispatchCandidate(task, std::move(candidate), self_thread,
                        TaskSource::kRunnext)) {
    return true;
  }

  if (proc_id < runqs_.size() && runqs_[proc_id]->popFront(candidate) &&
      dispatchCandidate(task, std::move(candidate), self_thread,
                        TaskSource::kLocal)) {
    return true;
  }

  if (fetchFromGlobal(task, self_thread)) {
    return true;
  }

  return stealTask(task, proc_id, self_thread);
}

bool Scheduler::allQueuesEmpty() const {
  if (!global_runq_.empty()) {
    return false;
  }
  for (const auto& runq : runqs_) {
    if (!runq->empty()) {
      return false;
    }
  }
  return true;
}

void Scheduler::runFiberTask(ScheduleTask& task) {
  if (!task.fiber || task.fiber->getState() == Fiber::TERM ||
      task.fiber->getState() == Fiber::EXCEPT) {
    FiberPool::release(task.fiber);
    return;
  }

  task.fiber->swapIn();

  if (task.fiber->getState() == Fiber::READY) {
    setRunnext(ScheduleTask(task.fiber, task.thread));
  } else if (task.fiber->getState() == Fiber::TERM ||
             task.fiber->getState() == Fiber::EXCEPT) {
    FiberPool::release(task.fiber);
  } else if (task.fiber->getState() != Fiber::TERM &&
             task.fiber->getState() != Fiber::EXCEPT) {
    task.fiber->state_ = Fiber::HOLD;
  }
  task.reset();
}

void Scheduler::runCbTask(ScheduleTask& task, Fiber::ptr& cb_fiber) {
  if (!static_cast<bool>(task.cb)) {
    return;
  }

  if (cb_fiber && (cb_fiber->getState() == Fiber::TERM ||
                   cb_fiber->getState() == Fiber::INIT)) {
    cb_fiber->reset(std::move(task.cb));
  } else {
    if (cb_fiber) {
      cb_fiber.reset();
    }
    cb_fiber = FiberPool::acquire(std::move(task.cb));
  }
  task.reset();
  cb_fiber->swapIn();

  if (cb_fiber->getState() == Fiber::READY) {
    setRunnext(ScheduleTask(cb_fiber, -1));
    cb_fiber.reset();
  } else if (cb_fiber->getState() == Fiber::EXCEPT ||
             cb_fiber->getState() == Fiber::TERM) {
    FiberPool::release(cb_fiber);
  } else {
    cb_fiber->state_ = Fiber::HOLD;
    cb_fiber.reset();
  }
}

void Scheduler::ensureIdleFiber(Fiber::ptr& idle_fiber) {
  if (idle_fiber->getState() == Fiber::TERM) {
    if (stopping()) {
      return;
    }
    idle_fiber = newIdleFiber();
  }
}

bool Scheduler::parkIdle(Fiber::ptr& idle_fiber) {
  ensureIdleFiber(idle_fiber);
  if (stopping() && idle_fiber->getState() == Fiber::TERM) {
    return false;
  }

  ++idle_thread_count_;
  idle_fiber->swapIn();
  --idle_thread_count_;

  if (idle_fiber->getState() == Fiber::TERM) {
    if (stopping()) {
      return false;
    }
    idle_fiber = newIdleFiber();
    return true;
  }

  if (idle_fiber->getState() != Fiber::TERM &&
      idle_fiber->getState() != Fiber::EXCEPT) {
    idle_fiber->state_ = Fiber::HOLD;
  }
  return true;
}

void Scheduler::setThis() { t_scheduler = this; }

void Scheduler::run() {
  setThis();
  t_processor_id = resolveProcessorId();

  const int self_tid = static_cast<int>(utils::GetThreadId());
  if (self_tid != root_thread_) {
    t_scheduler_fiber = Fiber::GetThis().get();
  }

  const size_t proc_id = t_processor_id;
  Fiber::ptr idle_fiber = newIdleFiber();
  Fiber::ptr cb_fiber;
  ScheduleTask task;

  while (true) {
    processTimers();
    task.reset();

    const bool got_task = fetchTask(task, proc_id);
    if (got_task) {
      ++active_thread_count_;
    } else if (!allQueuesEmpty()) {
      tickle();
    }

    if (task.fiber) {
      runFiberTask(task);
      --active_thread_count_;
    } else if (static_cast<bool>(task.cb)) {
      runCbTask(task, cb_fiber);
      --active_thread_count_;
    } else if (got_task) {
      --active_thread_count_;
    } else if (!allQueuesEmpty()) {
      continue;
    } else if (!parkIdle(idle_fiber)) {
      break;
    }
  }

  t_run_next.reset();
  t_global_prefetch.clear();
  lemo::memory::StackPool::trim_thread_cache();
}

bool Scheduler::stopping() {
  return auto_stop_ && stopping_ && allQueuesEmpty() && !hasTimer() &&
         active_thread_count_ == 0;
}

void Scheduler::tickle() {
  if (idle_thread_count_.load(std::memory_order_relaxed) > 0) {
    idle_sem_.notify();
  }
}

void Scheduler::idle() {
  int spins = 0;
  while (!stopping()) {
    processTimers();
    if (!allQueuesEmpty()) {
      return;
    }
    if (spins < kIdleSpinRounds) {
      ++spins;
      Fiber::YieldToHold();
      continue;
    }
    if (idle_sem_.tryWait()) {
      return;
    }
    const uint64_t wait_ms = getNextTimer();
    if (wait_ms != UINT64_MAX && wait_ms > 0) {
      const uint64_t capped = wait_ms > 100 ? 100 : wait_ms;
      if (idle_sem_.timedWait(capped)) {
        return;
      }
    }
    Fiber::YieldToHold();
  }
}

Scheduler* Scheduler::GetThis() { return t_scheduler; }

Fiber* Scheduler::GetMainFiber() { return t_scheduler_fiber; }

size_t Scheduler::GetProcessorId() { return t_processor_id; }

std::ostream& Scheduler::dump(std::ostream& os) {
  os << "[Scheduler name=" << name_ << " processors=" << runqs_.size()
     << " global_qsize=" << global_runq_.size()
     << " timer_qsize=" << timerCount()
     << " active_count=" << active_thread_count_
     << " idle_count=" << idle_thread_count_
     << " local_pop=" << local_pop_count_.load()
     << " global_pop=" << global_pop_count_.load()
     << " steal=" << steal_count_.load()
     << " overflow=" << overflow_count_.load() << " stopping=" << stopping_
     << " ]" << std::endl
     << "    ";
  for (size_t i = 0; i < thread_ids_.size(); ++i) {
    if (i != 0) {
      os << ", ";
    }
    os << thread_ids_[i];
  }
  os << std::endl;
  for (size_t i = 0; i < runqs_.size(); ++i) {
    os << "    P" << i << "(tid=" << proc_thread_ids_[i]
       << ") local_qsize=" << runqs_[i]->size() << std::endl;
  }
  return os;
}

void Scheduler::switchTo(int thread) {
  assert(Scheduler::GetThis() != nullptr);
  if (Scheduler::GetThis() == this) {
    if (thread == -1 || thread == static_cast<int>(utils::GetThreadId())) {
      return;
    }
  }
  Fiber::ptr cur = Fiber::GetThis();
  assert(cur->getState() == Fiber::EXEC);
  schedule(cur, thread);
  cur->state_ = Fiber::HOLD;
  cur->swapOut();
}

SchedulerSwitcher::SchedulerSwitcher(Scheduler* target) {
  caller_ = Scheduler::GetThis();
  if (target != nullptr) {
    target->switchTo();
  }
}

SchedulerSwitcher::~SchedulerSwitcher() {
  if (caller_ != nullptr) {
    caller_->switchTo();
  }
}

}  // namespace fiber
}  // namespace lemo
