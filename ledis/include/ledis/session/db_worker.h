#pragma once

#include <atomic>
#include <thread>

namespace ledis {

class LedisEngine;

/** DB Worker 线程：消费 InboundQueue，dispatch，推送 ReplyRouter。 */
class DbWorker {
 public:
  explicit DbWorker(LedisEngine* engine);
  ~DbWorker();

  void start();
  void stop();
  bool running() const { return running_; }

 private:
  void runLoop();
  void runActiveExpire();

  LedisEngine* engine_;
  std::atomic<bool> running_{false};
  std::thread thread_;
};

}  // namespace ledis
