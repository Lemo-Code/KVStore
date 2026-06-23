// ============================================================================
// zstl general algorithm tests
// ============================================================================
// Tests: for_each, for_each_n, copy/copy_n/copy_if/copy_backward,
//        move/move_backward, fill/fill_n, transform (unary/binary),
//        generate/generate_n, remove/remove_if, replace/replace_if,
//        reverse, rotate, unique, partition/stable_partition/is_partitioned,
//        merge/inplace_merge, includes, set_union/intersection/difference/
//        symmetric_difference, lower_bound/upper_bound/binary_search/equal_range,
//        min_element/max_element/minmax_element,
//        next_permutation/prev_permutation, random_shuffle, iota,
//        lexicographical_compare, is_permutation, sample
// ============================================================================

#include <gtest/gtest.h>
#include "zstl/zstl.h"

#include <string>
#include <vector>
#include <list>
#include <set>
#include <numeric>

using namespace zstl;

// ============================================================================
// for_each tests
// ============================================================================

TEST(ForEach, Empty) {
  vector<int> v;
  int count = 0;
  auto f = zstl::for_each(v.begin(), v.end(), [&count](int& x) { ++count; });
  EXPECT_EQ(count, 0);
}

TEST(ForEach, MutateElements) {
  vector<int> v = {1, 2, 3, 4, 5};
  zstl::for_each(v.begin(), v.end(), [](int& x) { x *= 2; });
  EXPECT_EQ(v[0], 2);
  EXPECT_EQ(v[1], 4);
  EXPECT_EQ(v[2], 6);
  EXPECT_EQ(v[3], 8);
  EXPECT_EQ(v[4], 10);
}

TEST(ForEach, ReturnedFunction) {
  vector<int> v = {1, 2, 3};
  int sum = 0;
  auto f = zstl::for_each(v.begin(), v.end(), [&sum](int x) { sum += x; });
  EXPECT_EQ(sum, 6);
}

TEST(ForEach, Single) {
  vector<int> v = {42};
  bool called = false;
  zstl::for_each(v.begin(), v.end(), [&called](int x) { called = true; });
  EXPECT_TRUE(called);
}

// ============================================================================
// copy / copy_n / copy_if / copy_backward tests
// ============================================================================

TEST(Copy, Empty) {
  vector<int> src;
  vector<int> dst(10, -1);
  auto it = zstl::copy(src.begin(), src.end(), dst.begin());
  EXPECT_EQ(it, dst.begin());
  EXPECT_EQ(dst[0], -1);  // unchanged
}

TEST(Copy, Basic) {
  vector<int> src = {1, 2, 3, 4, 5};
  vector<int> dst(5);
  auto it = zstl::copy(src.begin(), src.end(), dst.begin());
  EXPECT_EQ(it, dst.end());
  for (int i = 0; i < 5; ++i) EXPECT_EQ(dst[i], src[i]);
}

TEST(Copy, Single) {
  vector<int> src = {42};
  vector<int> dst(1);
  zstl::copy(src.begin(), src.end(), dst.begin());
  EXPECT_EQ(dst[0], 42);
}

TEST(CopyN, Basic) {
  vector<int> src = {1, 2, 3, 4, 5};
  vector<int> dst(3);
  auto it = zstl::copy_n(src.begin(), 3, dst.begin());
  EXPECT_EQ(it, dst.end());
  EXPECT_EQ(dst[0], 1);
  EXPECT_EQ(dst[1], 2);
  EXPECT_EQ(dst[2], 3);
}

TEST(CopyN, Zero) {
  vector<int> src = {1, 2, 3};
  vector<int> dst(1, 99);
  auto it = zstl::copy_n(src.begin(), 0, dst.begin());
  EXPECT_EQ(it, dst.begin());
}

TEST(CopyIf, Basic) {
  vector<int> src = {1, 2, 3, 4, 5, 6};
  vector<int> dst(6, 0);
  auto it = zstl::copy_if(src.begin(), src.end(), dst.begin(),
                           [](int x) { return x % 2 == 0; });
  EXPECT_EQ(it, dst.begin() + 3);
  EXPECT_EQ(dst[0], 2);
  EXPECT_EQ(dst[1], 4);
  EXPECT_EQ(dst[2], 6);
}

TEST(CopyIf, NoneMatch) {
  vector<int> src = {1, 3, 5};
  vector<int> dst(3);
  auto it = zstl::copy_if(src.begin(), src.end(), dst.begin(),
                           [](int x) { return x % 2 == 0; });
  EXPECT_EQ(it, dst.begin());
}

TEST(CopyIf, AllMatch) {
  vector<int> src = {2, 4, 6};
  vector<int> dst(3);
  auto it = zstl::copy_if(src.begin(), src.end(), dst.begin(),
                           [](int x) { return true; });
  EXPECT_EQ(it, dst.end());
  for (int i = 0; i < 3; ++i) EXPECT_EQ(dst[i], src[i]);
}

TEST(CopyBackward, Basic) {
  vector<int> src = {1, 2, 3, 4, 5};
  vector<int> dst(7, 0);
  auto it = zstl::copy_backward(src.begin(), src.end(), dst.begin() + 5);
  EXPECT_EQ(it, dst.begin());
  EXPECT_EQ(dst[0], 1);
  EXPECT_EQ(dst[1], 2);
  EXPECT_EQ(dst[2], 3);
  EXPECT_EQ(dst[3], 4);
  EXPECT_EQ(dst[4], 5);
}

TEST(CopyBackward, Empty) {
  vector<int> src;
  vector<int> dst(5);
  auto it = zstl::copy_backward(src.begin(), src.end(), dst.end());
  EXPECT_EQ(it, dst.end());
}

// ============================================================================
// move / move_backward tests
// ============================================================================

TEST(Move, Basic) {
  vector<std::string> src;
  src.push_back("hello");
  src.push_back("world");
  vector<std::string> dst(2);
  auto it = zstl::move(src.begin(), src.end(), dst.begin());
  EXPECT_EQ(it, dst.end());
  EXPECT_EQ(dst[0], "hello");
  EXPECT_EQ(dst[1], "world");
}

TEST(Move, Empty) {
  vector<int> src;
  vector<int> dst(5);
  auto it = zstl::move(src.begin(), src.end(), dst.begin());
  EXPECT_EQ(it, dst.begin());
}

TEST(MoveBackward, Basic) {
  vector<int> v = {1, 2, 3};
  // Move elements right within same vector
  zstl::move_backward(v.begin(), v.begin() + 2, v.end());
  EXPECT_EQ(v[1], 1);
  EXPECT_EQ(v[2], 2);
}

TEST(MoveBackward, Empty) {
  vector<int> src;
  vector<int> dst(5);
  auto it = zstl::move_backward(src.begin(), src.end(), dst.end());
  EXPECT_EQ(it, dst.end());
}

// ============================================================================
// fill / fill_n tests
// ============================================================================

TEST(Fill, Basic) {
  vector<int> v(5, 0);
  zstl::fill(v.begin(), v.end(), 42);
  for (auto x : v) EXPECT_EQ(x, 42);
}

TEST(Fill, Empty) {
  vector<int> v;
  zstl::fill(v.begin(), v.end(), 42);  // no-op
}

TEST(Fill, Single) {
  vector<int> v(1, 0);
  zstl::fill(v.begin(), v.end(), 99);
  EXPECT_EQ(v[0], 99);
}

TEST(FillN, Basic) {
  vector<int> v(5, 0);
  auto it = zstl::fill_n(v.begin(), 3, 7);
  EXPECT_EQ(it, v.begin() + 3);
  EXPECT_EQ(v[0], 7);
  EXPECT_EQ(v[1], 7);
  EXPECT_EQ(v[2], 7);
  EXPECT_EQ(v[3], 0);
  EXPECT_EQ(v[4], 0);
}

TEST(FillN, Zero) {
  vector<int> v(3, 99);
  auto it = zstl::fill_n(v.begin(), 0, 0);
  EXPECT_EQ(it, v.begin());
  EXPECT_EQ(v[0], 99);  // unchanged
}

// ============================================================================
// transform tests
// ============================================================================

TEST(TransformUnary, Basic) {
  vector<int> src = {1, 2, 3, 4, 5};
  vector<int> dst(5);
  auto it = zstl::transform(src.begin(), src.end(), dst.begin(),
                             [](int x) { return x * 2; });
  EXPECT_EQ(it, dst.end());
  EXPECT_EQ(dst[0], 2);
  EXPECT_EQ(dst[1], 4);
  EXPECT_EQ(dst[2], 6);
  EXPECT_EQ(dst[3], 8);
  EXPECT_EQ(dst[4], 10);
}

TEST(TransformUnary, Empty) {
  vector<int> src;
  vector<int> dst;
  auto it = zstl::transform(src.begin(), src.end(), dst.begin(),
                             [](int x) { return x; });
  EXPECT_EQ(it, dst.begin());
}

TEST(TransformUnary, StringToInt) {
  vector<std::string> src = {"1", "2", "3"};
  vector<int> dst(3);
  zstl::transform(src.begin(), src.end(), dst.begin(),
                  [](const std::string& s) { return s.length(); });
  EXPECT_EQ(dst[0], 1);
  EXPECT_EQ(dst[1], 1);
  EXPECT_EQ(dst[2], 1);
}

TEST(TransformBinary, Basic) {
  vector<int> a = {1, 2, 3};
  vector<int> b = {4, 5, 6};
  vector<int> dst(3);
  auto it = zstl::transform(a.begin(), a.end(), b.begin(), dst.begin(),
                             [](int x, int y) { return x + y; });
  EXPECT_EQ(it, dst.end());
  EXPECT_EQ(dst[0], 5);
  EXPECT_EQ(dst[1], 7);
  EXPECT_EQ(dst[2], 9);
}

TEST(TransformBinary, Empty) {
  vector<int> a, b, dst;
  auto it = zstl::transform(a.begin(), a.end(), b.begin(), dst.begin(),
                             [](int x, int y) { return x + y; });
  EXPECT_EQ(it, dst.begin());
}

// ============================================================================
// generate / generate_n tests
// ============================================================================

TEST(Generate, Basic) {
  vector<int> v(5);
  int counter = 0;
  zstl::generate(v.begin(), v.end(), [&counter]() { return counter++; });
  EXPECT_EQ(v[0], 0);
  EXPECT_EQ(v[1], 1);
  EXPECT_EQ(v[2], 2);
  EXPECT_EQ(v[3], 3);
  EXPECT_EQ(v[4], 4);
}

TEST(Generate, Empty) {
  vector<int> v;
  zstl::generate(v.begin(), v.end(), []() { return 42; });
  // No crash
}

TEST(GenerateN, Basic) {
  vector<int> v(10, -1);
  auto it = zstl::generate_n(v.begin(), 5, []() { return 99; });
  EXPECT_EQ(it, v.begin() + 5);
  for (int i = 0; i < 5; ++i) EXPECT_EQ(v[i], 99);
  for (int i = 5; i < 10; ++i) EXPECT_EQ(v[i], -1);
}

TEST(GenerateN, Zero) {
  vector<int> v(3, 7);
  auto it = zstl::generate_n(v.begin(), 0, []() { return 42; });
  EXPECT_EQ(it, v.begin());
  EXPECT_EQ(v[0], 7);
}

// ============================================================================
// remove / remove_if tests
// ============================================================================

TEST(Remove, Basic) {
  vector<int> v = {1, 2, 3, 2, 4, 2, 5};
  auto it = zstl::remove(v.begin(), v.end(), 2);
  EXPECT_EQ(it, v.begin() + 4);
  // Remaining elements should be non-2
  for (auto p = v.begin(); p != it; ++p) EXPECT_NE(*p, 2);
}

TEST(Remove, None) {
  vector<int> v = {1, 3, 5, 7};
  auto it = zstl::remove(v.begin(), v.end(), 2);
  EXPECT_EQ(it, v.end());
}

TEST(Remove, All) {
  vector<int> v = {2, 2, 2, 2};
  auto it = zstl::remove(v.begin(), v.end(), 2);
  EXPECT_EQ(it, v.begin());
}

TEST(Remove, Empty) {
  vector<int> v;
  auto it = zstl::remove(v.begin(), v.end(), 1);
  EXPECT_EQ(it, v.end());
}

TEST(RemoveIf, Basic) {
  vector<int> v = {1, 2, 3, 4, 5, 6};
  auto it = zstl::remove_if(v.begin(), v.end(), [](int x) { return x % 2 == 0; });
  EXPECT_EQ(it, v.begin() + 3);
  for (auto p = v.begin(); p != it; ++p) EXPECT_NE(*p % 2, 0);
}

TEST(RemoveIf, None) {
  vector<int> v = {1, 3, 5};
  auto it = zstl::remove_if(v.begin(), v.end(), [](int x) { return x % 2 == 0; });
  EXPECT_EQ(it, v.end());
}

// ============================================================================
// replace / replace_if tests
// ============================================================================

TEST(Replace, Basic) {
  vector<int> v = {1, 2, 3, 2, 4};
  zstl::replace(v.begin(), v.end(), 2, 99);
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[1], 99);
  EXPECT_EQ(v[2], 3);
  EXPECT_EQ(v[3], 99);
  EXPECT_EQ(v[4], 4);
}

TEST(Replace, Empty) {
  vector<int> v;
  zstl::replace(v.begin(), v.end(), 0, 1);  // no-op
}

TEST(Replace, None) {
  vector<int> v = {1, 3, 5};
  zstl::replace(v.begin(), v.end(), 2, 99);
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[1], 3);
  EXPECT_EQ(v[2], 5);
}

TEST(ReplaceIf, Basic) {
  vector<int> v = {1, 2, 3, 4, 5};
  zstl::replace_if(v.begin(), v.end(), [](int x) { return x % 2 == 0; }, -1);
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[1], -1);
  EXPECT_EQ(v[2], 3);
  EXPECT_EQ(v[3], -1);
  EXPECT_EQ(v[4], 5);
}

// ============================================================================
// reverse tests
// ============================================================================

TEST(Reverse, Basic) {
  vector<int> v = {1, 2, 3, 4, 5};
  zstl::reverse(v.begin(), v.end());
  EXPECT_EQ(v[0], 5);
  EXPECT_EQ(v[1], 4);
  EXPECT_EQ(v[2], 3);
  EXPECT_EQ(v[3], 2);
  EXPECT_EQ(v[4], 1);
}

TEST(Reverse, Empty) {
  vector<int> v;
  zstl::reverse(v.begin(), v.end());  // no-op
}

TEST(Reverse, Single) {
  vector<int> v = {42};
  zstl::reverse(v.begin(), v.end());
  EXPECT_EQ(v[0], 42);
}

TEST(Reverse, Two) {
  vector<int> v = {1, 2};
  zstl::reverse(v.begin(), v.end());
  EXPECT_EQ(v[0], 2);
  EXPECT_EQ(v[1], 1);
}

TEST(Reverse, Even) {
  vector<int> v = {1, 2, 3, 4};
  zstl::reverse(v.begin(), v.end());
  EXPECT_EQ(v[0], 4);
  EXPECT_EQ(v[1], 3);
  EXPECT_EQ(v[2], 2);
  EXPECT_EQ(v[3], 1);
}

// ============================================================================
// rotate tests
// ============================================================================

TEST(Rotate, Basic) {
  vector<int> v = {1, 2, 3, 4, 5};
  auto it = zstl::rotate(v.begin(), v.begin() + 2, v.end());
  EXPECT_EQ(it, v.begin() + 3);
  EXPECT_EQ(v[0], 3);
  EXPECT_EQ(v[1], 4);
  EXPECT_EQ(v[2], 5);
  EXPECT_EQ(v[3], 1);
  EXPECT_EQ(v[4], 2);
}

TEST(Rotate, ZeroShift) {
  vector<int> v = {1, 2, 3};
  auto it = zstl::rotate(v.begin(), v.begin(), v.end());
  EXPECT_EQ(it, v.end());
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[1], 2);
  EXPECT_EQ(v[2], 3);
}

TEST(Rotate, FullShift) {
  vector<int> v = {1, 2, 3};
  auto it = zstl::rotate(v.begin(), v.end(), v.end());
  EXPECT_EQ(it, v.begin());
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[1], 2);
  EXPECT_EQ(v[2], 3);
}

TEST(Rotate, Single) {
  vector<int> v = {42};
  zstl::rotate(v.begin(), v.begin(), v.end());
  EXPECT_EQ(v[0], 42);
}

TEST(Rotate, Empty) {
  vector<int> v;
  zstl::rotate(v.begin(), v.begin(), v.end());  // no-op
}

TEST(Rotate, SingleShift) {
  vector<int> v = {1, 2, 3, 4};
  zstl::rotate(v.begin(), v.begin() + 1, v.end());
  EXPECT_EQ(v[0], 2);
  EXPECT_EQ(v[1], 3);
  EXPECT_EQ(v[2], 4);
  EXPECT_EQ(v[3], 1);
}

// ============================================================================
// unique tests
// ============================================================================

TEST(Unique, Basic) {
  vector<int> v = {1, 1, 2, 2, 2, 3, 3, 4};
  auto it = zstl::unique(v.begin(), v.end());
  EXPECT_EQ(it, v.begin() + 4);
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[1], 2);
  EXPECT_EQ(v[2], 3);
  EXPECT_EQ(v[3], 4);
}

TEST(Unique, Empty) {
  vector<int> v;
  auto it = zstl::unique(v.begin(), v.end());
  EXPECT_EQ(it, v.end());
}

TEST(Unique, Single) {
  vector<int> v = {42};
  auto it = zstl::unique(v.begin(), v.end());
  EXPECT_EQ(it, v.end());
}

TEST(Unique, AllSame) {
  vector<int> v = {5, 5, 5, 5, 5};
  auto it = zstl::unique(v.begin(), v.end());
  EXPECT_EQ(it, v.begin() + 1);
  EXPECT_EQ(v[0], 5);
}

TEST(Unique, NoDuplicates) {
  vector<int> v = {1, 2, 3, 4, 5};
  auto it = zstl::unique(v.begin(), v.end());
  EXPECT_EQ(it, v.end());
}

TEST(Unique, CustomPredicate) {
  vector<int> v = {1, 3, 5, 2, 4, 6};
  // Treat elements as "equal" if they have same parity
  auto it = zstl::unique(v.begin(), v.end(), [](int a, int b) {
    return (a % 2) == (b % 2);
  });
  EXPECT_EQ(it, v.begin() + 2);
}

// ============================================================================
// partition / stable_partition / is_partitioned tests
// ============================================================================

TEST(Partition, Basic) {
  vector<int> v = {1, 2, 3, 4, 5, 6, 7, 8};
  auto it = zstl::partition(v.begin(), v.end(), [](int x) { return x % 2 == 0; });
  // All evens before it, all odds after it
  for (auto p = v.begin(); p != it; ++p) EXPECT_EQ(*p % 2, 0);
  for (auto p = it; p != v.end(); ++p) EXPECT_EQ(*p % 2, 1);
}

TEST(Partition, AllTrue) {
  vector<int> v = {2, 4, 6, 8};
  auto it = zstl::partition(v.begin(), v.end(), [](int x) { return x > 0; });
  EXPECT_EQ(it, v.end());
}

TEST(Partition, AllFalse) {
  vector<int> v = {1, 3, 5, 7};
  auto it = zstl::partition(v.begin(), v.end(), [](int x) { return x % 2 == 0; });
  EXPECT_EQ(it, v.begin());
}

TEST(Partition, Empty) {
  vector<int> v;
  auto it = zstl::partition(v.begin(), v.end(), [](int) { return true; });
  EXPECT_EQ(it, v.end());
}

TEST(StablePartition, Basic) {
  vector<int> v = {1, 2, 3, 4, 5, 6};
  auto it = zstl::stable_partition(v.begin(), v.end(),
                                    [](int x) { return x % 2 == 0; });
  // Evens first: 2, 4, 6 (preserving original relative order)
  for (auto p = v.begin(); p != it; ++p) EXPECT_EQ(*p % 2, 0);
  for (auto p = it; p != v.end(); ++p) EXPECT_EQ(*p % 2, 1);
}

TEST(StablePartition, Empty) {
  vector<int> v;
  auto it = zstl::stable_partition(v.begin(), v.end(), [](int) { return true; });
  EXPECT_EQ(it, v.end());
}

TEST(IsPartitioned, True) {
  vector<int> v = {2, 4, 6, 1, 3, 5};
  EXPECT_TRUE(zstl::is_partitioned(v.begin(), v.end(),
                                    [](int x) { return x % 2 == 0; }));
}

TEST(IsPartitioned, False) {
  vector<int> v = {2, 1, 4, 3, 6, 5};
  EXPECT_FALSE(zstl::is_partitioned(v.begin(), v.end(),
                                     [](int x) { return x % 2 == 0; }));
}

TEST(IsPartitioned, Empty) {
  vector<int> v;
  EXPECT_TRUE(zstl::is_partitioned(v.begin(), v.end(), [](int) { return true; }));
}

TEST(IsPartitioned, AllTrue) {
  vector<int> v = {2, 4, 6, 8};
  EXPECT_TRUE(zstl::is_partitioned(v.begin(), v.end(), [](int x) { return x > 0; }));
}

// ============================================================================
// merge / inplace_merge tests
// ============================================================================

TEST(Merge, Basic) {
  vector<int> a = {1, 3, 5, 7};
  vector<int> b = {2, 4, 6, 8};
  vector<int> dst(8);
  auto it = zstl::merge(a.begin(), a.end(), b.begin(), b.end(), dst.begin());
  EXPECT_EQ(it, dst.end());
  for (int i = 0; i < 8; ++i) EXPECT_EQ(dst[i], i + 1);
}

TEST(Merge, EmptyLeft) {
  vector<int> a;
  vector<int> b = {1, 2, 3};
  vector<int> dst(3);
  auto it = zstl::merge(a.begin(), a.end(), b.begin(), b.end(), dst.begin());
  EXPECT_EQ(it, dst.end());
  EXPECT_EQ(dst[0], 1);
  EXPECT_EQ(dst[1], 2);
  EXPECT_EQ(dst[2], 3);
}

TEST(Merge, EmptyRight) {
  vector<int> a = {1, 2, 3};
  vector<int> b;
  vector<int> dst(3);
  auto it = zstl::merge(a.begin(), a.end(), b.begin(), b.end(), dst.begin());
  EXPECT_EQ(it, dst.end());
  EXPECT_EQ(dst[0], 1);
  EXPECT_EQ(dst[1], 2);
  EXPECT_EQ(dst[2], 3);
}

TEST(Merge, WithDuplicates) {
  vector<int> a = {1, 2, 2, 3};
  vector<int> b = {2, 3, 3, 4};
  vector<int> dst(8);
  zstl::merge(a.begin(), a.end(), b.begin(), b.end(), dst.begin());
  EXPECT_EQ(dst[0], 1);
  EXPECT_EQ(dst[1], 2);
  EXPECT_EQ(dst[2], 2);  // from a (stable: a before b for equals)
  EXPECT_EQ(dst[3], 2);  // from b
  EXPECT_EQ(dst[4], 3);
}

TEST(InplaceMerge, Basic) {
  vector<int> v = {1, 3, 5, 2, 4, 6};
  zstl::inplace_merge(v.begin(), v.begin() + 3, v.end());
  for (int i = 0; i < 6; ++i) EXPECT_EQ(v[i], i + 1);
}

TEST(InplaceMerge, EmptyLeft) {
  vector<int> v = {1, 2, 3};
  zstl::inplace_merge(v.begin(), v.begin(), v.end());
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[1], 2);
  EXPECT_EQ(v[2], 3);
}

TEST(InplaceMerge, EmptyRight) {
  vector<int> v = {1, 2, 3};
  zstl::inplace_merge(v.begin(), v.end(), v.end());
  EXPECT_EQ(v[0], 1);
}

// ============================================================================
// set operations tests
// ============================================================================

TEST(Includes, True) {
  vector<int> a = {1, 2, 3, 4, 5, 6};
  vector<int> b = {2, 4, 6};
  EXPECT_TRUE(zstl::includes(a.begin(), a.end(), b.begin(), b.end()));
}

TEST(Includes, False) {
  vector<int> a = {1, 2, 3, 4, 5};
  vector<int> b = {2, 4, 7};
  EXPECT_FALSE(zstl::includes(a.begin(), a.end(), b.begin(), b.end()));
}

TEST(Includes, EmptySubset) {
  vector<int> a = {1, 2, 3};
  vector<int> b;
  EXPECT_TRUE(zstl::includes(a.begin(), a.end(), b.begin(), b.end()));
}

TEST(SetUnion, Basic) {
  vector<int> a = {1, 3, 5};
  vector<int> b = {2, 4, 6};
  vector<int> dst(6);
  auto it = zstl::set_union(a.begin(), a.end(), b.begin(), b.end(), dst.begin());
  EXPECT_EQ(it, dst.end());
  for (int i = 0; i < 6; ++i) EXPECT_EQ(dst[i], i + 1);
}

TEST(SetUnion, WithDuplicates) {
  vector<int> a = {1, 2, 2, 3};
  vector<int> b = {2, 3, 3, 4};
  vector<int> dst(8);
  auto it = zstl::set_union(a.begin(), a.end(), b.begin(), b.end(), dst.begin());
  // Union: 1, 2, 3, 4 (no duplicates in union)
  int count = it - dst.begin();
  EXPECT_GE(count, 4);
}

TEST(SetIntersection, Basic) {
  vector<int> a = {1, 2, 3, 4, 5};
  vector<int> b = {3, 4, 5, 6, 7};
  vector<int> dst(5);
  auto it = zstl::set_intersection(a.begin(), a.end(), b.begin(), b.end(), dst.begin());
  EXPECT_EQ(it, dst.begin() + 3);
  EXPECT_EQ(dst[0], 3);
  EXPECT_EQ(dst[1], 4);
  EXPECT_EQ(dst[2], 5);
}

TEST(SetIntersection, EmptyIntersection) {
  vector<int> a = {1, 2, 3};
  vector<int> b = {4, 5, 6};
  vector<int> dst(5);
  auto it = zstl::set_intersection(a.begin(), a.end(), b.begin(), b.end(), dst.begin());
  EXPECT_EQ(it, dst.begin());
}

TEST(SetDifference, Basic) {
  vector<int> a = {1, 2, 3, 4, 5};
  vector<int> b = {3, 4};
  vector<int> dst(5);
  auto it = zstl::set_difference(a.begin(), a.end(), b.begin(), b.end(), dst.begin());
  EXPECT_EQ(it, dst.begin() + 3);
  EXPECT_EQ(dst[0], 1);
  EXPECT_EQ(dst[1], 2);
  EXPECT_EQ(dst[2], 5);
}

TEST(SetDifference, NoDifference) {
  vector<int> a = {1, 2, 3};
  vector<int> b = {1, 2, 3};
  vector<int> dst(3);
  auto it = zstl::set_difference(a.begin(), a.end(), b.begin(), b.end(), dst.begin());
  EXPECT_EQ(it, dst.begin());
}

TEST(SetSymmetricDifference, Basic) {
  vector<int> a = {1, 2, 3, 4};
  vector<int> b = {3, 4, 5, 6};
  vector<int> dst(8);
  auto it = zstl::set_symmetric_difference(a.begin(), a.end(),
                                            b.begin(), b.end(), dst.begin());
  EXPECT_EQ(it, dst.begin() + 4);
  // Should be {1, 2, 5, 6}
  EXPECT_EQ(dst[0], 1);
  EXPECT_EQ(dst[1], 2);
  EXPECT_EQ(dst[2], 5);
  EXPECT_EQ(dst[3], 6);
}

// ============================================================================
// binary search tests
// ============================================================================

TEST(LowerBound, Basic) {
  vector<int> v = {1, 2, 2, 2, 3, 4, 5};
  auto it = zstl::lower_bound(v.begin(), v.end(), 2);
  EXPECT_EQ(it, v.begin() + 1);
  EXPECT_EQ(*it, 2);
}

TEST(LowerBound, NotFoundGreater) {
  vector<int> v = {1, 2, 3};
  auto it = zstl::lower_bound(v.begin(), v.end(), 99);
  EXPECT_EQ(it, v.end());
}

TEST(LowerBound, AllLess) {
  vector<int> v = {10, 20, 30};
  auto it = zstl::lower_bound(v.begin(), v.end(), 5);
  EXPECT_EQ(it, v.begin());
}

TEST(UpperBound, Basic) {
  vector<int> v = {1, 2, 2, 2, 3, 4, 5};
  auto it = zstl::upper_bound(v.begin(), v.end(), 2);
  EXPECT_EQ(it, v.begin() + 4);
  EXPECT_EQ(*it, 3);
}

TEST(UpperBound, NotFound) {
  vector<int> v = {1, 2, 3};
  auto it = zstl::upper_bound(v.begin(), v.end(), 99);
  EXPECT_EQ(it, v.end());
}

TEST(BinarySearch, Found) {
  vector<int> v = {1, 2, 3, 4, 5};
  EXPECT_TRUE(zstl::binary_search(v.begin(), v.end(), 3));
  EXPECT_TRUE(zstl::binary_search(v.begin(), v.end(), 1));
  EXPECT_TRUE(zstl::binary_search(v.begin(), v.end(), 5));
}

TEST(BinarySearch, NotFound) {
  vector<int> v = {1, 2, 3, 4, 5};
  EXPECT_FALSE(zstl::binary_search(v.begin(), v.end(), 0));
  EXPECT_FALSE(zstl::binary_search(v.begin(), v.end(), 6));
}

TEST(EqualRange, Basic) {
  vector<int> v = {1, 2, 2, 2, 3};
  auto [lo, hi] = zstl::equal_range(v.begin(), v.end(), 2);
  EXPECT_EQ(lo, v.begin() + 1);
  EXPECT_EQ(hi, v.begin() + 4);
  EXPECT_EQ(hi - lo, 3);
}

TEST(EqualRange, NotFound) {
  vector<int> v = {1, 2, 3};
  auto [lo, hi] = zstl::equal_range(v.begin(), v.end(), 5);
  EXPECT_EQ(lo, hi);
}

// ============================================================================
// min_element / max_element / minmax_element tests
// ============================================================================

TEST(MinElement, Basic) {
  vector<int> v = {5, 3, 9, 1, 7};
  auto it = zstl::min_element(v.begin(), v.end());
  EXPECT_EQ(it, v.begin() + 3);
  EXPECT_EQ(*it, 1);
}

TEST(MinElement, Empty) {
  vector<int> v;
  auto it = zstl::min_element(v.begin(), v.end());
  EXPECT_EQ(it, v.end());
}

TEST(MinElement, Single) {
  vector<int> v = {42};
  auto it = zstl::min_element(v.begin(), v.end());
  EXPECT_EQ(it, v.begin());
  EXPECT_EQ(*it, 42);
}

TEST(MaxElement, Basic) {
  vector<int> v = {5, 3, 9, 1, 7};
  auto it = zstl::max_element(v.begin(), v.end());
  EXPECT_EQ(it, v.begin() + 2);
  EXPECT_EQ(*it, 9);
}

TEST(MaxElement, Empty) {
  vector<int> v;
  auto it = zstl::max_element(v.begin(), v.end());
  EXPECT_EQ(it, v.end());
}

TEST(MinMaxElement, Basic) {
  vector<int> v = {5, 3, 9, 1, 7};
  auto [min_it, max_it] = zstl::minmax_element(v.begin(), v.end());
  EXPECT_EQ(*min_it, 1);
  EXPECT_EQ(*max_it, 9);
}

TEST(MinMaxElement, Single) {
  vector<int> v = {42};
  auto [min_it, max_it] = zstl::minmax_element(v.begin(), v.end());
  EXPECT_EQ(*min_it, 42);
  EXPECT_EQ(*max_it, 42);
}

TEST(MinMaxElement, Empty) {
  vector<int> v;
  auto [min_it, max_it] = zstl::minmax_element(v.begin(), v.end());
  EXPECT_EQ(min_it, v.end());
  EXPECT_EQ(max_it, v.end());
}

TEST(MinMaxElement, TwoElements) {
  vector<int> v = {5, 3};
  auto [min_it, max_it] = zstl::minmax_element(v.begin(), v.end());
  EXPECT_EQ(*min_it, 3);
  EXPECT_EQ(*max_it, 5);
}

// ============================================================================
// permutation tests
// ============================================================================

TEST(NextPermutation, Basic) {
  vector<int> v = {1, 2, 3};
  EXPECT_TRUE(zstl::next_permutation(v.begin(), v.end()));
  EXPECT_EQ(v[0], 1); EXPECT_EQ(v[1], 3); EXPECT_EQ(v[2], 2);
}

TEST(NextPermutation, AllPermutations) {
  vector<int> v = {1, 2, 3, 4};
  int count = 0;
  do { ++count; } while (zstl::next_permutation(v.begin(), v.end()));
  // 4! = 24 permutations
  EXPECT_EQ(count, 24);
}

TEST(NextPermutation, LastReturnsFalse) {
  vector<int> v = {3, 2, 1};
  EXPECT_FALSE(zstl::next_permutation(v.begin(), v.end()));
  // Should wrap to first permutation
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[1], 2);
  EXPECT_EQ(v[2], 3);
}

TEST(NextPermutation, Empty) {
  vector<int> v;
  EXPECT_FALSE(zstl::next_permutation(v.begin(), v.end()));
}

TEST(NextPermutation, Single) {
  vector<int> v = {1};
  EXPECT_FALSE(zstl::next_permutation(v.begin(), v.end()));
  EXPECT_EQ(v[0], 1);
}

TEST(NextPermutation, ThreeElementsAll) {
  vector<int> v = {1, 2, 3};
  std::set<std::string> seen;
  int count = 0;
  do {
    std::string s;
    for (int x : v) s += std::to_string(x);
    seen.insert(s);
    ++count;
  } while (zstl::next_permutation(v.begin(), v.end()));
  EXPECT_EQ(count, 6);
  EXPECT_EQ(seen.size(), 6);
}

TEST(PrevPermutation, Basic) {
  vector<int> v = {3, 2, 1};
  EXPECT_TRUE(zstl::prev_permutation(v.begin(), v.end()));
  EXPECT_EQ(v[0], 3); EXPECT_EQ(v[1], 1); EXPECT_EQ(v[2], 2);
}

TEST(PrevPermutation, All) {
  vector<int> v = {4, 3, 2, 1};
  int count = 0;
  do { ++count; } while (zstl::prev_permutation(v.begin(), v.end()));
  EXPECT_EQ(count, 24);
}

TEST(PrevPermutation, FirstReturnsFalse) {
  vector<int> v = {1, 2, 3};
  EXPECT_FALSE(zstl::prev_permutation(v.begin(), v.end()));
  // Wraps to last permutation
  EXPECT_EQ(v[0], 3);
  EXPECT_EQ(v[1], 2);
  EXPECT_EQ(v[2], 1);
}

// ============================================================================
// random_shuffle tests
// ============================================================================

TEST(RandomShuffle, ElementsPreserved) {
  vector<int> v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  vector<int> orig = v;
  zstl::random_shuffle(v.begin(), v.end());
  // All original elements exist
  int sum_before = 0, sum_after = 0;
  for (auto x : orig) sum_before += x;
  for (auto x : v) sum_after += x;
  EXPECT_EQ(sum_before, sum_after);
  EXPECT_EQ(v.size(), orig.size());
  // Sort and compare
  zstl::sort(v.begin(), v.end());
  EXPECT_TRUE(zstl::equal(v.begin(), v.end(), orig.begin()));
}

TEST(RandomShuffle, Empty) {
  vector<int> v;
  zstl::random_shuffle(v.begin(), v.end());  // no-op
}

TEST(RandomShuffle, Single) {
  vector<int> v = {42};
  zstl::random_shuffle(v.begin(), v.end());
  EXPECT_EQ(v[0], 42);
}

TEST(RandomShuffle, CustomGenerator) {
  vector<int> v = {1, 2, 3, 4, 5};
  vector<int> orig = v;
  // Use a deterministic generator
  int seed = 42;
  auto gen = [&seed]() { return static_cast<unsigned>(seed = seed * 1103515245 + 12345); };
  zstl::random_shuffle(v.begin(), v.end(), gen);
  zstl::sort(v.begin(), v.end());
  EXPECT_TRUE(zstl::equal(v.begin(), v.end(), orig.begin()));
}

// ============================================================================
// iota tests
// ============================================================================

TEST(Iota, Basic) {
  vector<int> v(5);
  zstl::iota(v.begin(), v.end(), 10);
  EXPECT_EQ(v[0], 10);
  EXPECT_EQ(v[1], 11);
  EXPECT_EQ(v[2], 12);
  EXPECT_EQ(v[3], 13);
  EXPECT_EQ(v[4], 14);
}

TEST(Iota, Empty) {
  vector<int> v;
  zstl::iota(v.begin(), v.end(), 0);
  // no-op, no crash
}

TEST(Iota, Single) {
  vector<int> v(1);
  zstl::iota(v.begin(), v.end(), 100);
  EXPECT_EQ(v[0], 100);
}

TEST(Iota, NegativeValues) {
  vector<int> v(5);
  zstl::iota(v.begin(), v.end(), -2);
  EXPECT_EQ(v[0], -2);
  EXPECT_EQ(v[1], -1);
  EXPECT_EQ(v[2], 0);
  EXPECT_EQ(v[3], 1);
  EXPECT_EQ(v[4], 2);
}

// ============================================================================
// lexicographical_compare tests
// ============================================================================

TEST(LexicographicalCompare, FirstSmaller) {
  vector<int> a = {1, 2, 3};
  vector<int> b = {1, 2, 4};
  EXPECT_TRUE(zstl::lexicographical_compare(a.begin(), a.end(),
                                             b.begin(), b.end()));
}

TEST(LexicographicalCompare, Equal) {
  vector<int> a = {1, 2, 3};
  vector<int> b = {1, 2, 3};
  EXPECT_FALSE(zstl::lexicographical_compare(a.begin(), a.end(),
                                              b.begin(), b.end()));
}

TEST(LexicographicalCompare, ShorterSmaller) {
  vector<int> a = {1, 2};
  vector<int> b = {1, 2, 3};
  EXPECT_TRUE(zstl::lexicographical_compare(a.begin(), a.end(),
                                             b.begin(), b.end()));
}

TEST(LexicographicalCompare, SecondSmaller) {
  vector<int> a = {2, 1};
  vector<int> b = {1, 999};
  EXPECT_FALSE(zstl::lexicographical_compare(a.begin(), a.end(),
                                              b.begin(), b.end()));
}

TEST(LexicographicalCompare, Empty) {
  vector<int> a, b;
  EXPECT_FALSE(zstl::lexicographical_compare(a.begin(), a.end(),
                                              b.begin(), b.end()));
}

TEST(LexicographicalCompare, EmptyVsNonEmpty) {
  vector<int> a, b = {1};
  EXPECT_TRUE(zstl::lexicographical_compare(a.begin(), a.end(),
                                            b.begin(), b.end()));
}

// ============================================================================
// is_permutation tests
// ============================================================================

TEST(IsPermutation, True) {
  vector<int> a = {1, 2, 3, 4, 5};
  vector<int> b = {3, 5, 1, 4, 2};
  EXPECT_TRUE(zstl::is_permutation(a.begin(), a.end(), b.begin()));
}

TEST(IsPermutation, False) {
  vector<int> a = {1, 2, 3, 4, 5};
  vector<int> b = {1, 2, 3, 4, 6};
  EXPECT_FALSE(zstl::is_permutation(a.begin(), a.end(), b.begin()));
}

TEST(IsPermutation, Empty) {
  vector<int> a, b;
  EXPECT_TRUE(zstl::is_permutation(a.begin(), a.end(), b.begin()));
}

TEST(IsPermutation, Single) {
  vector<int> a = {42};
  vector<int> b = {42};
  EXPECT_TRUE(zstl::is_permutation(a.begin(), a.end(), b.begin()));
  b[0] = 99;
  EXPECT_FALSE(zstl::is_permutation(a.begin(), a.end(), b.begin()));
}

TEST(IsPermutation, DifferentLengths) {
  vector<int> a = {1, 2, 3};
  vector<int> b = {1, 2};
  // is_permutation checks the [first1, last1) range against first2...
  // with mismatch skip, it should detect length mismatch
  EXPECT_FALSE(zstl::is_permutation(a.begin(), a.end(), b.begin()));
}

TEST(IsPermutation, Duplicates) {
  vector<int> a = {1, 1, 2, 2, 3};
  vector<int> b = {2, 1, 3, 2, 1};
  EXPECT_TRUE(zstl::is_permutation(a.begin(), a.end(), b.begin()));
}

// ============================================================================
// sample tests
// ============================================================================

TEST(Sample, Basic) {
  vector<int> src = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  vector<int> dst(4);
  auto it = zstl::sample(src.begin(), src.end(), dst.begin(), 4);
  EXPECT_EQ(it, dst.end());
  // Verify all sampled elements come from the source
  for (auto x : dst) {
    EXPECT_GE(x, 1);
    EXPECT_LE(x, 10);
  }
}

TEST(Sample, KEqualsN) {
  vector<int> src = {1, 2, 3};
  vector<int> dst(3);
  auto it = zstl::sample(src.begin(), src.end(), dst.begin(), 3);
  EXPECT_EQ(it, dst.end());
  zstl::sort(dst.begin(), dst.end());
  EXPECT_TRUE(zstl::equal(dst.begin(), dst.end(), src.begin()));
}

TEST(Sample, KLargerThanN) {
  vector<int> src = {1, 2, 3};
  vector<int> dst(5);
  auto it = zstl::sample(src.begin(), src.end(), dst.begin(), 5);
  EXPECT_EQ(it, dst.begin() + 3);  // Only 3 elements available
}

TEST(Sample, Empty) {
  vector<int> src;
  vector<int> dst(3);
  auto it = zstl::sample(src.begin(), src.end(), dst.begin(), 3);
  EXPECT_EQ(it, dst.begin());
}

TEST(Sample, KEqualsZero) {
  vector<int> src = {1, 2, 3, 4, 5};
  vector<int> dst(1);
  auto it = zstl::sample(src.begin(), src.end(), dst.begin(), 0);
  EXPECT_EQ(it, dst.begin());
}

// ============================================================================
// Tests with list and array containers
// ============================================================================

TEST(Algorithm, ForEachOnList) {
  list<int> lst;
  lst.push_back(1);
  lst.push_back(2);
  lst.push_back(3);
  int sum = 0;
  zstl::for_each(lst.begin(), lst.end(), [&sum](int x) { sum += x; });
  EXPECT_EQ(sum, 6);
}

TEST(Algorithm, FillOnArray) {
  array<int, 5> arr;
  zstl::fill(arr.begin(), arr.end(), 10);
  for (int i = 0; i < 5; ++i) EXPECT_EQ(arr[i], 10);
}

TEST(Algorithm, ReverseOnArray) {
  array<int, 5> arr = {1, 2, 3, 4, 5};
  zstl::reverse(arr.begin(), arr.end());
  EXPECT_EQ(arr[0], 5);
  EXPECT_EQ(arr[4], 1);
}
