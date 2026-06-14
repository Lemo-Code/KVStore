#ifndef LEMO_FIBER_TIMING_WHEEL_H
#define LEMO_FIBER_TIMING_WHEEL_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lemo {
namespace fiber {

struct WheelPlacement {
  uint8_t level = 0;
  uint32_t rounds = 0;
  size_t slot = 0;
};

struct TickEvent {
  size_t l0_slot = 0;
  std::vector<uint64_t> cascade_ids;
};

class HierarchicalTimingWheel {
 public:
  static constexpr size_t kNumLevels = 4;
  static constexpr size_t kL0Slots = 512;
  static constexpr size_t kL1Slots = 64;
  static constexpr size_t kL2Slots = 64;
  static constexpr size_t kL3Slots = 64;
  static constexpr uint64_t kTickMs = 1;
  static constexpr uint64_t kL0SpanMs = kL0Slots * kTickMs;
  static constexpr uint64_t kL1SpanMs = kL1Slots * kL0SpanMs;
  static constexpr uint64_t kL2SpanMs = kL2Slots * kL1SpanMs;
  static constexpr uint64_t kL3SpanMs = kL3Slots * kL2SpanMs;

  HierarchicalTimingWheel();

  static WheelPlacement computePlacement(uint64_t global_tick, uint64_t delay_ms);

  void add(uint8_t level, size_t slot, uint64_t timer_id);
  void remove(uint8_t level, size_t slot, uint64_t timer_id);

  std::vector<uint64_t>& l0Bucket(size_t slot);
  const std::vector<uint64_t>& l0Bucket(size_t slot) const;

  TickEvent tickOneMs();

  uint64_t globalTick() const { return global_tick_; }
  void setGlobalTick(uint64_t tick) { global_tick_ = tick; }
  uint64_t lastAdvanceMs() const { return last_advance_ms_; }
  void setLastAdvanceMs(uint64_t ms) { last_advance_ms_ = ms; }

  void advanceTo(uint64_t now_ms, std::vector<TickEvent>& events);

 private:
  std::array<std::vector<uint64_t>, kL0Slots> level0_;
  std::array<std::vector<uint64_t>, kL1Slots> level1_;
  std::array<std::vector<uint64_t>, kL2Slots> level2_;
  std::array<std::vector<uint64_t>, kL3Slots> level3_;
  uint64_t global_tick_ = 0;
  uint64_t last_advance_ms_ = 0;
};

}  // namespace fiber
}  // namespace lemo

#endif  // LEMO_FIBER_TIMING_WHEEL_H
