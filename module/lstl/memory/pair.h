#ifndef LSTL_PAIR_H
#define LSTL_PAIR_H

namespace lstl {

template <typename T1, typename T2>
struct pair {
  typedef T1 first_type;
  typedef T2 second_type;

  T1 first;
  T2 second;

  pair() : first(), second() {}
  pair(const T1& a, const T2& b) : first(a), second(b) {}
};

template <typename T1, typename T2>
inline pair<T1, T2> make_pair(const T1& x, const T2& y) {
  return pair<T1, T2>(x, y);
}

template <typename T1, typename T2>
inline bool operator==(const pair<T1, T2>& a, const pair<T1, T2>& b) {
  return a.first == b.first && a.second == b.second;
}

template <typename T1, typename T2>
inline bool operator!=(const pair<T1, T2>& a, const pair<T1, T2>& b) {
  return !(a == b);
}

template <typename T1, typename T2>
inline bool operator<(const pair<T1, T2>& a, const pair<T1, T2>& b) {
  return a.first < b.first || (!(b.first < a.first) && a.second < b.second);
}

}  // namespace lstl

#endif  // LSTL_PAIR_H
