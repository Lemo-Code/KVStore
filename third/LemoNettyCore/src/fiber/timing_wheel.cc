#include "lemo/fiber/timing_wheel.h"

#include <algorithm>

namespace lemo {
namespace fiber {

namespace {

uint64_t CeilDiv(uint64_t a, uint64_t b) {
  return (a + b - 1) / b;
}

}  // namespace

HierarchicalTimingWheel::HierarchicalTimingWheel() = default;

WheelPlacement HierarchicalTimingWheel::computePlacement(uint64_t global_tick,
                                                         uint64_t delay_ms) {
  WheelPlacement p;
  uint64_t ticks = CeilDiv(delay_ms, kTickMs);
  if (ticks == 0) {
    ticks = 1;
  }

  if (delay_ms < kL0SpanMs) {
    p.level = 0;
    p.rounds = static_cast<uint32_t>((ticks - 1) / kL0Slots);
    p.slot = static_cast<size_t>((global_tick + ticks) % kL0Slots);
    return p;
  }

  ticks = CeilDiv(delay_ms, kL0SpanMs);
  if (delay_ms < kL1SpanMs) {
    p.level = 1;
    p.rounds = static_cast<uint32_t>((ticks - 1) / kL1Slots);
    const uint64_t l1_tick = global_tick / kL0Slots;
    p.slot = static_cast<size_t>((l1_tick + ticks) % kL1Slots);
    return p;
  }

  ticks = CeilDiv(delay_ms, kL1SpanMs);
  if (delay_ms < kL2SpanMs) {
    p.level = 2;
    p.rounds = static_cast<uint32_t>((ticks - 1) / kL2Slots);
    const uint64_t l2_tick = global_tick / kL0Slots / kL1Slots;
    p.slot = static_cast<size_t>((l2_tick + ticks) % kL2Slots);
    return p;
  }

  ticks = CeilDiv(delay_ms, kL2SpanMs);
  p.level = 3;
  p.rounds = static_cast<uint32_t>((ticks - 1) / kL3Slots);
  const uint64_t l3_tick = global_tick / kL0Slots / kL1Slots / kL2Slots;
  p.slot = static_cast<size_t>((l3_tick + ticks) % kL3Slots);
  return p;
}

void HierarchicalTimingWheel::add(uint8_t level, size_t slot, uint64_t timer_id) {
  switch (level) {
    case 0:
      level0_[slot % kL0Slots].push_back(timer_id);
      break;
    case 1:
      level1_[slot % kL1Slots].push_back(timer_id);
      break;
    case 2:
      level2_[slot % kL2Slots].push_back(timer_id);
      break;
    case 3:
      level3_[slot % kL3Slots].push_back(timer_id);
      break;
    default:
      break;
  }
}

void HierarchicalTimingWheel::remove(uint8_t level, size_t slot,
                                     uint64_t timer_id) {
  std::vector<uint64_t>* bucket = nullptr;
  switch (level) {
    case 0:
      bucket = &level0_[slot % kL0Slots];
      break;
    case 1:
      bucket = &level1_[slot % kL1Slots];
      break;
    case 2:
      bucket = &level2_[slot % kL2Slots];
      break;
    case 3:
      bucket = &level3_[slot % kL3Slots];
      break;
    default:
      return;
  }
  bucket->erase(std::remove(bucket->begin(), bucket->end(), timer_id),
                bucket->end());
}

std::vector<uint64_t>& HierarchicalTimingWheel::l0Bucket(size_t slot) {
  return level0_[slot % kL0Slots];
}

const std::vector<uint64_t>& HierarchicalTimingWheel::l0Bucket(
    size_t slot) const {
  return level0_[slot % kL0Slots];
}

TickEvent HierarchicalTimingWheel::tickOneMs() {
  TickEvent ev;
  last_advance_ms_ += kTickMs;
  ++global_tick_;
  ev.l0_slot = static_cast<size_t>(global_tick_ % kL0Slots);

  if (global_tick_ % kL0Slots == 0) {
    const size_t l1_slot =
        static_cast<size_t>((global_tick_ / kL0Slots) % kL1Slots);
    std::vector<uint64_t>& b1 = level1_[l1_slot];
    ev.cascade_ids.insert(ev.cascade_ids.end(), b1.begin(), b1.end());
    b1.clear();

    if ((global_tick_ / kL0Slots) % kL1Slots == 0) {
      const size_t l2_slot = static_cast<size_t>(
          (global_tick_ / kL0Slots / kL1Slots) % kL2Slots);
      std::vector<uint64_t>& b2 = level2_[l2_slot];
      ev.cascade_ids.insert(ev.cascade_ids.end(), b2.begin(), b2.end());
      b2.clear();

      if ((global_tick_ / kL0Slots / kL1Slots) % kL2Slots == 0) {
        const size_t l3_slot = static_cast<size_t>(
            (global_tick_ / kL0Slots / kL1Slots / kL2Slots) % kL3Slots);
        std::vector<uint64_t>& b3 = level3_[l3_slot];
        ev.cascade_ids.insert(ev.cascade_ids.end(), b3.begin(), b3.end());
        b3.clear();
      }
    }
  }
  return ev;
}

void HierarchicalTimingWheel::advanceTo(uint64_t now_ms,
                                        std::vector<TickEvent>& events) {
  if (last_advance_ms_ == 0) {
    last_advance_ms_ = now_ms;
    return;
  }
  while (last_advance_ms_ + kTickMs <= now_ms) {
    events.push_back(tickOneMs());
  }
}

}  // namespace fiber
}  // namespace lemo
