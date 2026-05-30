#ifndef LSTL_FUNCTIONAL_H
#define LSTL_FUNCTIONAL_H

namespace lstl {

template <typename T>
struct less {
  bool operator()(const T& a, const T& b) const { return a < b; }
};

template <typename T>
struct greater {
  bool operator()(const T& a, const T& b) const { return a > b; }
};

template <typename T>
struct equal_to {
  bool operator()(const T& a, const T& b) const { return a == b; }
};

template <typename T>
struct not_equal_to {
  bool operator()(const T& a, const T& b) const { return a != b; }
};

template <typename T>
struct less_equal {
  bool operator()(const T& a, const T& b) const { return a <= b; }
};

template <typename T>
struct greater_equal {
  bool operator()(const T& a, const T& b) const { return a >= b; }
};

}  // namespace lstl

#endif  // LSTL_FUNCTIONAL_H
