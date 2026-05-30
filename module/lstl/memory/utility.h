#ifndef LSTL_UTILITY_H
#define LSTL_UTILITY_H

namespace lstl {

template <typename T>
struct remove_reference {
  typedef T type;
};

template <typename T>
struct remove_reference<T&> {
  typedef T type;
};

template <typename T>
struct remove_reference<T&&> {
  typedef T type;
};

template <typename T>
inline T&& move(T& t) throw() {
  return static_cast<T&&>(t);
}

template <typename T>
inline T&& forward(typename remove_reference<T>::type& t) throw() {
  return static_cast<T&&>(t);
}

template <typename T>
inline T&& forward(typename remove_reference<T>::type&& t) throw() {
  return static_cast<T&&>(t);
}

template <typename T>
inline void swap(T& a, T& b) {
  T tmp = move(a);
  a = move(b);
  b = move(tmp);
}

}  // namespace lstl

#endif  // LSTL_UTILITY_H
