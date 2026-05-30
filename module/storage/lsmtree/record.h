#ifndef LSTL_LSM_RECORD_H
#define LSTL_LSM_RECORD_H

namespace lstl {
namespace lsm {

template <typename T>
struct Record {
  bool deleted;
  T value;

  Record() : deleted(false), value() {}
  explicit Record(const T& v) : deleted(false), value(v) {}
  static Record tombstone() {
    Record r;
    r.deleted = true;
    return r;
  }
};

}  // namespace lsm
}  // namespace lstl

#endif  // LSTL_LSM_RECORD_H
