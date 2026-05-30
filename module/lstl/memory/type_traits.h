#ifndef LSTL_TYPE_TRAITS_H
#define LSTL_TYPE_TRAITS_H

namespace lstl {

struct __true_type {};
struct __false_type {};

template <typename T>
struct __type_traits {
  typedef __false_type this_type_has_default_constructor;
  typedef __false_type this_type_has_copy_constructor;
  typedef __false_type this_type_has_assignment_operator;
  typedef __false_type this_type_has_destructor;
  typedef __false_type this_type_is_POD_type;
};

template <typename T>
struct __type_traits<T*> {
  typedef __true_type this_type_has_default_constructor;
  typedef __true_type this_type_has_copy_constructor;
  typedef __true_type this_type_has_assignment_operator;
  typedef __true_type this_type_has_destructor;
  typedef __true_type this_type_is_POD_type;
};

template <>
struct __type_traits<char> {
  typedef __true_type this_type_has_default_constructor;
  typedef __true_type this_type_has_copy_constructor;
  typedef __true_type this_type_has_assignment_operator;
  typedef __true_type this_type_has_destructor;
  typedef __true_type this_type_is_POD_type;
};

template <>
struct __type_traits<unsigned char> {
  typedef __true_type this_type_has_default_constructor;
  typedef __true_type this_type_has_copy_constructor;
  typedef __true_type this_type_has_assignment_operator;
  typedef __true_type this_type_has_destructor;
  typedef __true_type this_type_is_POD_type;
};

template <>
struct __type_traits<signed char> {
  typedef __true_type this_type_has_default_constructor;
  typedef __true_type this_type_has_copy_constructor;
  typedef __true_type this_type_has_assignment_operator;
  typedef __true_type this_type_has_destructor;
  typedef __true_type this_type_is_POD_type;
};

template <>
struct __type_traits<short> {
  typedef __true_type this_type_has_default_constructor;
  typedef __true_type this_type_has_copy_constructor;
  typedef __true_type this_type_has_assignment_operator;
  typedef __true_type this_type_has_destructor;
  typedef __true_type this_type_is_POD_type;
};

template <>
struct __type_traits<unsigned short> {
  typedef __true_type this_type_has_default_constructor;
  typedef __true_type this_type_has_copy_constructor;
  typedef __true_type this_type_has_assignment_operator;
  typedef __true_type this_type_has_destructor;
  typedef __true_type this_type_is_POD_type;
};

template <>
struct __type_traits<int> {
  typedef __true_type this_type_has_default_constructor;
  typedef __true_type this_type_has_copy_constructor;
  typedef __true_type this_type_has_assignment_operator;
  typedef __true_type this_type_has_destructor;
  typedef __true_type this_type_is_POD_type;
};

template <>
struct __type_traits<unsigned int> {
  typedef __true_type this_type_has_default_constructor;
  typedef __true_type this_type_has_copy_constructor;
  typedef __true_type this_type_has_assignment_operator;
  typedef __true_type this_type_has_destructor;
  typedef __true_type this_type_is_POD_type;
};

template <>
struct __type_traits<long> {
  typedef __true_type this_type_has_default_constructor;
  typedef __true_type this_type_has_copy_constructor;
  typedef __true_type this_type_has_assignment_operator;
  typedef __true_type this_type_has_destructor;
  typedef __true_type this_type_is_POD_type;
};

template <>
struct __type_traits<unsigned long> {
  typedef __true_type this_type_has_default_constructor;
  typedef __true_type this_type_has_copy_constructor;
  typedef __true_type this_type_has_assignment_operator;
  typedef __true_type this_type_has_destructor;
  typedef __true_type this_type_is_POD_type;
};

template <>
struct __type_traits<float> {
  typedef __true_type this_type_has_default_constructor;
  typedef __true_type this_type_has_copy_constructor;
  typedef __true_type this_type_has_assignment_operator;
  typedef __true_type this_type_has_destructor;
  typedef __true_type this_type_is_POD_type;
};

template <>
struct __type_traits<double> {
  typedef __true_type this_type_has_default_constructor;
  typedef __true_type this_type_has_copy_constructor;
  typedef __true_type this_type_has_assignment_operator;
  typedef __true_type this_type_has_destructor;
  typedef __true_type this_type_is_POD_type;
};

template <>
struct __type_traits<long double> {
  typedef __true_type this_type_has_default_constructor;
  typedef __true_type this_type_has_copy_constructor;
  typedef __true_type this_type_has_assignment_operator;
  typedef __true_type this_type_has_destructor;
  typedef __true_type this_type_is_POD_type;
};

// 区分 (n, value) 与 (first, last) 构造/赋值（SGI __is_integer）
template <typename T>
struct __is_integer {
  typedef __false_type _Integral;
};

template <>
struct __is_integer<bool> {
  typedef __true_type _Integral;
};
template <>
struct __is_integer<char> {
  typedef __true_type _Integral;
};
template <>
struct __is_integer<signed char> {
  typedef __true_type _Integral;
};
template <>
struct __is_integer<unsigned char> {
  typedef __true_type _Integral;
};
template <>
struct __is_integer<short> {
  typedef __true_type _Integral;
};
template <>
struct __is_integer<unsigned short> {
  typedef __true_type _Integral;
};
template <>
struct __is_integer<int> {
  typedef __true_type _Integral;
};
template <>
struct __is_integer<unsigned int> {
  typedef __true_type _Integral;
};
template <>
struct __is_integer<long> {
  typedef __true_type _Integral;
};
template <>
struct __is_integer<unsigned long> {
  typedef __true_type _Integral;
};
template <>
struct __is_integer<long long> {
  typedef __true_type _Integral;
};
template <>
struct __is_integer<unsigned long long> {
  typedef __true_type _Integral;
};

}  // namespace lstl

#endif  // LSTL_TYPE_TRAITS_H
