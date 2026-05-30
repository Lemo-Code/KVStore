#ifndef LSTL_KEY_OF_VALUE_H
#define LSTL_KEY_OF_VALUE_H

namespace lstl {
namespace detail {

template <typename T>
struct identity {
  const T& operator()(const T& x) const { return x; }
};

template <typename Pair>
struct select1st {
  typedef typename Pair::first_type first_type;
  const first_type& operator()(const Pair& x) const { return x.first; }
};

}  // namespace detail
}  // namespace lstl

#endif  // LSTL_KEY_OF_VALUE_H
