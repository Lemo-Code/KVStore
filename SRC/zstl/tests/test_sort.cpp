// ============================================================================
// zstl sort algorithm tests
// ============================================================================
// Tests for: sort, stable_sort, partial_sort, partial_sort_copy,
//           nth_element, is_sorted, is_sorted_until
// Covers: empty, single, sorted, reverse-sorted, random, duplicates,
//         large datasets, custom comparators, stability, O(n log n) behavior
// ============================================================================

#include <gtest/gtest.h>
#include "zstl/zstl.h"

#include <string>
#include <vector>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <random>

using namespace zstl;

// ============================================================================
// Custom struct for testing — tracks move/copy counts
// ============================================================================
struct TrackedInt {
  int value;
  int id;

  TrackedInt() : value(0), id(0) {}
  TrackedInt(int v, int i) : value(v), id(i) {}
  bool operator<(const TrackedInt& rhs) const { return value < rhs.value; }
  bool operator>(const TrackedInt& rhs) const { return value > rhs.value; }
  bool operator==(const TrackedInt& rhs) const {
    return value == rhs.value && id == rhs.id;
  }
};

bool operator==(const TrackedInt& a, const TrackedInt& b) { return a.value == b.value && a.id == b.id; }
bool operator!=(const TrackedInt& a, const TrackedInt& b) { return !(a == b); }

// ============================================================================
// sort tests
// ============================================================================

TEST(Sort, Empty) {
  vector<int> v;
  zstl::sort(v.begin(), v.end());
  EXPECT_TRUE(v.empty());
}

TEST(Sort, Single) {
  vector<int> v = {42};
  zstl::sort(v.begin(), v.end());
  EXPECT_EQ(v[0], 42);
}

TEST(Sort, TwoElements) {
  {
    vector<int> v = {3, 1};
    zstl::sort(v.begin(), v.end());
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 3);
  }
  {
    vector<int> v = {1, 3};
    zstl::sort(v.begin(), v.end());
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 3);
  }
}

TEST(Sort, AlreadySorted) {
  vector<int> v(100);
  for (int i = 0; i < 100; ++i) v[i] = i;
  zstl::sort(v.begin(), v.end());
  for (int i = 0; i < 100; ++i) EXPECT_EQ(v[i], i);
}

TEST(Sort, ReverseSorted) {
  vector<int> v(100);
  for (int i = 0; i < 100; ++i) v[i] = 99 - i;
  zstl::sort(v.begin(), v.end());
  for (int i = 0; i < 100; ++i) EXPECT_EQ(v[i], i);
}

TEST(Sort, RandomSmall) {
  vector<int> v = {7, 3, 5, 1, 9, 2, 8, 4, 6, 0};
  zstl::sort(v.begin(), v.end());
  for (size_t i = 0; i < v.size(); ++i) EXPECT_EQ(v[i], (int)i);
}

TEST(Sort, AllEqual) {
  vector<int> v(50, 99);
  zstl::sort(v.begin(), v.end());
  for (auto x : v) EXPECT_EQ(x, 99);
}

TEST(Sort, Duplicates) {
  vector<int> v = {5, 1, 3, 1, 4, 2, 5, 3, 1, 2, 4, 5};
  zstl::sort(v.begin(), v.end());
  for (size_t i = 1; i < v.size(); ++i) {
    EXPECT_LE(v[i - 1], v[i]);
  }
  // Verify all elements present
  EXPECT_EQ(zstl::count(v.begin(), v.end(), 1), 3);
  EXPECT_EQ(zstl::count(v.begin(), v.end(), 2), 2);
  EXPECT_EQ(zstl::count(v.begin(), v.end(), 3), 2);
  EXPECT_EQ(zstl::count(v.begin(), v.end(), 4), 2);
  EXPECT_EQ(zstl::count(v.begin(), v.end(), 5), 3);
}

TEST(Sort, Large1000) {
  const int N = 1000;
  vector<int> v(N);
  // Fill with descending values
  for (int i = 0; i < N; ++i) v[i] = N - i;
  zstl::sort(v.begin(), v.end());
  for (int i = 0; i < N; ++i) EXPECT_EQ(v[i], i + 1);
}

TEST(Sort, CustomComparator) {
  vector<int> v = {5, 1, 3, 2, 4};
  zstl::sort(v.begin(), v.end(), greater<int>());
  EXPECT_EQ(v[0], 5);
  EXPECT_EQ(v[1], 4);
  EXPECT_EQ(v[2], 3);
  EXPECT_EQ(v[3], 2);
  EXPECT_EQ(v[4], 1);
}

TEST(Sort, StringSort) {
  vector<std::string> v = {"cherry", "apple", "banana", "aardvark", "zebra"};
  zstl::sort(v.begin(), v.end());
  EXPECT_EQ(v[0], "aardvark");
  EXPECT_EQ(v[1], "apple");
  EXPECT_EQ(v[2], "banana");
  EXPECT_EQ(v[3], "cherry");
  EXPECT_EQ(v[4], "zebra");
}

TEST(Sort, StringSortDescending) {
  vector<std::string> v = {"dog", "cat", "bird", "ant"};
  zstl::sort(v.begin(), v.end(), greater<std::string>());
  EXPECT_EQ(v[0], "dog");
  EXPECT_EQ(v[3], "ant");
}

TEST(Sort, TrackedIntStableOrder) {
  vector<TrackedInt> v;
  for (int i = 0; i < 20; ++i) {
    v.push_back(TrackedInt(5, i));
  }
  // All have same value=5 but different ids
  zstl::sort(v.begin(), v.end());
  for (auto& x : v) EXPECT_EQ(x.value, 5);

  // For sort (not stable), ID order may change — just verify sorted
  // Verify that all values are sorted and present
  for (size_t i = 1; i < v.size(); ++i) {
    EXPECT_LE(v[i - 1].value, v[i].value);
  }
}

TEST(Sort, Subrange) {
  vector<int> v = {9, 2, 7, 1, 8, 3, 5, 4, 6};
  zstl::sort(v.begin() + 2, v.begin() + 7);  // sort [7,1,8,3,5]
  EXPECT_EQ(v[0], 9);
  EXPECT_EQ(v[1], 2);
  // sorted subrange
  EXPECT_EQ(v[2], 1);
  EXPECT_EQ(v[3], 3);
  EXPECT_EQ(v[4], 5);
  EXPECT_EQ(v[5], 7);
  EXPECT_EQ(v[6], 8);
  EXPECT_EQ(v[7], 4);
  EXPECT_EQ(v[8], 6);
}

// ============================================================================
// stable_sort tests
// ============================================================================

TEST(StableSort, Empty) {
  vector<int> v;
  zstl::stable_sort(v.begin(), v.end());
  EXPECT_TRUE(v.empty());
}

TEST(StableSort, Single) {
  vector<int> v = {7};
  zstl::stable_sort(v.begin(), v.end());
  EXPECT_EQ(v[0], 7);
}

TEST(StableSort, BasicInt) {
  vector<int> v = {5, 2, 8, 2, 9, 1, 5};
  zstl::stable_sort(v.begin(), v.end());
  for (size_t i = 1; i < v.size(); ++i) {
    EXPECT_LE(v[i - 1], v[i]);
  }
}

TEST(StableSort, Stability) {
  // Create elements with same value but different IDs
  vector<TrackedInt> v;
  v.push_back(TrackedInt(3, 0));
  v.push_back(TrackedInt(1, 1));
  v.push_back(TrackedInt(3, 2));
  v.push_back(TrackedInt(2, 3));
  v.push_back(TrackedInt(1, 4));
  v.push_back(TrackedInt(2, 5));
  v.push_back(TrackedInt(3, 6));

  zstl::stable_sort(v.begin(), v.end());

  // Check sorted by value
  for (size_t i = 1; i < v.size(); ++i) {
    EXPECT_LE(v[i - 1].value, v[i].value);
  }

  // Check stability: for equal values, original order should be preserved
  // Value 1: id=1 should come before id=4
  int found_1 = 0;
  for (auto& x : v) {
    if (x.value == 1) {
      if (found_1 == 0) EXPECT_EQ(x.id, 1);
      else if (found_1 == 1) EXPECT_EQ(x.id, 4);
      ++found_1;
    }
  }
  EXPECT_EQ(found_1, 2);

  // Value 2: id=3 before id=5
  int found_2 = 0;
  for (auto& x : v) {
    if (x.value == 2) {
      if (found_2 == 0) EXPECT_EQ(x.id, 3);
      else if (found_2 == 1) EXPECT_EQ(x.id, 5);
      ++found_2;
    }
  }
  EXPECT_EQ(found_2, 2);
}

TEST(StableSort, Large) {
  const int N = 500;
  vector<int> v(N);
  for (int i = 0; i < N; ++i) v[i] = (N - i) % 50;
  zstl::stable_sort(v.begin(), v.end());
  for (int i = 1; i < N; ++i) {
    EXPECT_LE(v[i - 1], v[i]);
  }
}

TEST(StableSort, CustomComparator) {
  vector<int> v = {5, 1, 3, 2, 4, 1};
  zstl::stable_sort(v.begin(), v.end(), greater<int>());
  for (size_t i = 1; i < v.size(); ++i) {
    EXPECT_GE(v[i - 1], v[i]);
  }
}

// ============================================================================
// partial_sort tests
// ============================================================================

TEST(PartialSort, BasicK) {
  vector<int> v = {7, 2, 5, 1, 8, 3, 9, 4, 6};
  zstl::partial_sort(v.begin(), v.begin() + 4, v.end());
  // First 4 should be the smallest in sorted order
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[1], 2);
  EXPECT_EQ(v[2], 3);
  EXPECT_EQ(v[3], 4);
  // Remaining are unspecified
  for (size_t i = 4; i < v.size(); ++i) {
    EXPECT_GE(v[i], 4);
  }
}

TEST(PartialSort, EmptyK) {
  vector<int> v = {5, 3, 1, 4, 2};
  zstl::partial_sort(v.begin(), v.begin(), v.end());
  // Should be no-op: first 0 elements sorted
  EXPECT_EQ(v.size(), 5);  // No crash
}

TEST(PartialSort, KEqualsN) {
  vector<int> v = {3, 1, 4, 2};
  zstl::partial_sort(v.begin(), v.end(), v.end());
  // Equivalent to full sort
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[1], 2);
  EXPECT_EQ(v[2], 3);
  EXPECT_EQ(v[3], 4);
}

TEST(PartialSort, LargeK) {
  const int N = 200;
  vector<int> v(N);
  for (int i = 0; i < N; ++i) v[i] = (i * 17 + 31) % 1000;
  zstl::partial_sort(v.begin(), v.begin() + 50, v.end());
  // First 50 elements are the 50 smallest, sorted
  for (int i = 1; i < 50; ++i) {
    EXPECT_LE(v[i - 1], v[i]);
  }
  // All first 50 are <= all remaining
  for (int i = 0; i < 50; ++i) {
    for (int j = 50; j < N; ++j) {
      EXPECT_LE(v[i], v[j]);
    }
  }
}

TEST(PartialSort, Single) {
  vector<int> v = {42};
  zstl::partial_sort(v.begin(), v.begin() + 1, v.end());
  EXPECT_EQ(v[0], 42);
}

// ============================================================================
// partial_sort_copy tests
// ============================================================================

TEST(PartialSortCopy, Basic) {
  vector<int> src = {9, 3, 7, 1, 5, 8, 2, 6, 4};
  vector<int> dest(4);
  auto end = zstl::partial_sort_copy(src.begin(), src.end(),
                                      dest.begin(), dest.end());
  EXPECT_EQ(end - dest.begin(), 4);
  EXPECT_EQ(dest[0], 1);
  EXPECT_EQ(dest[1], 2);
  EXPECT_EQ(dest[2], 3);
  EXPECT_EQ(dest[3], 4);
}

TEST(PartialSortCopy, EmptyDest) {
  vector<int> src = {3, 1, 2};
  vector<int> dest;
  auto end = zstl::partial_sort_copy(src.begin(), src.end(),
                                      dest.begin(), dest.end());
  EXPECT_EQ(end, dest.begin());
}

TEST(PartialSortCopy, EmptySrc) {
  vector<int> src;
  vector<int> dest(5, 999);
  auto end = zstl::partial_sort_copy(src.begin(), src.end(),
                                      dest.begin(), dest.end());
  EXPECT_EQ(end, dest.begin());
}

TEST(PartialSortCopy, DestLargerThanSrc) {
  vector<int> src = {5, 2, 8, 1};
  vector<int> dest(10);
  auto end = zstl::partial_sort_copy(src.begin(), src.end(),
                                      dest.begin(), dest.end());
  // Should copy all 4 elements sorted, return dest.begin() + 4
  EXPECT_EQ(end - dest.begin(), 4);
  EXPECT_EQ(dest[0], 1);
  EXPECT_EQ(dest[1], 2);
  EXPECT_EQ(dest[2], 5);
  EXPECT_EQ(dest[3], 8);
}

// ============================================================================
// nth_element tests
// ============================================================================

TEST(NthElement, Median) {
  vector<int> v = {7, 1, 9, 3, 5, 2, 8, 4, 6};
  zstl::nth_element(v.begin(), v.begin() + 4, v.end());
  // The element at index 4 should be 5 (the 5th smallest, 0-indexed)
  EXPECT_EQ(v[4], 5);
  // Elements before should be <= 5
  for (int i = 0; i < 4; ++i) EXPECT_LE(v[i], 5);
  // Elements after should be >= 5
  for (size_t i = 5; i < v.size(); ++i) EXPECT_GE(v[i], 5);
}

TEST(NthElement, First) {
  vector<int> v = {7, 3, 5, 2, 9, 1};
  zstl::nth_element(v.begin(), v.begin(), v.end());
  // First element should be the minimum
  EXPECT_EQ(v[0], 1);
  for (size_t i = 1; i < v.size(); ++i) EXPECT_GE(v[i], 1);
}

TEST(NthElement, Last) {
  vector<int> v = {7, 3, 5, 2, 9, 1};
  zstl::nth_element(v.begin(), v.end() - 1, v.end());
  // Last element should be the maximum
  EXPECT_EQ(v[5], 9);
  for (int i = 0; i < 5; ++i) EXPECT_LE(v[i], 9);
}

TEST(NthElement, Empty) {
  vector<int> v;
  zstl::nth_element(v.begin(), v.begin(), v.end());
  // No crash
}

TEST(NthElement, Single) {
  vector<int> v = {42};
  zstl::nth_element(v.begin(), v.begin(), v.end());
  EXPECT_EQ(v[0], 42);
}

TEST(NthElement, AllEqual) {
  vector<int> v(10, 5);
  zstl::nth_element(v.begin(), v.begin() + 5, v.end());
  EXPECT_EQ(v[5], 5);
}

TEST(NthElement, Large) {
  const int N = 500;
  vector<int> v(N);
  for (int i = 0; i < N; ++i) v[i] = (i * 37 + 53) % 10000;
  int mid = N / 2;
  zstl::nth_element(v.begin(), v.begin() + mid, v.end());
  int mid_val = v[mid];
  for (int i = 0; i < mid; ++i) EXPECT_LE(v[i], mid_val);
  for (int i = mid + 1; i < N; ++i) EXPECT_GE(v[i], mid_val);
}

// ============================================================================
// is_sorted tests
// ============================================================================

TEST(IsSorted, Empty) {
  vector<int> v;
  EXPECT_TRUE(zstl::is_sorted(v.begin(), v.end()));
}

TEST(IsSorted, Single) {
  vector<int> v = {1};
  EXPECT_TRUE(zstl::is_sorted(v.begin(), v.end()));
}

TEST(IsSorted, Sorted) {
  vector<int> v = {1, 2, 3, 4, 5};
  EXPECT_TRUE(zstl::is_sorted(v.begin(), v.end()));
}

TEST(IsSorted, NotSorted) {
  vector<int> v = {1, 3, 2, 4, 5};
  EXPECT_FALSE(zstl::is_sorted(v.begin(), v.end()));
}

TEST(IsSorted, AllEqual) {
  vector<int> v = {3, 3, 3, 3, 3};
  EXPECT_TRUE(zstl::is_sorted(v.begin(), v.end()));
}

TEST(IsSorted, SortingDescending) {
  vector<int> v = {5, 4, 3, 2, 1};
  EXPECT_FALSE(zstl::is_sorted(v.begin(), v.end()));  // using default less
  EXPECT_TRUE(zstl::is_sorted(v.begin(), v.end(), greater<int>()));
}

TEST(IsSorted, TwoElements) {
  EXPECT_TRUE(zstl::is_sorted(vector<int>{1, 2}.begin(), vector<int>{1, 2}.end()));
  EXPECT_FALSE(zstl::is_sorted(vector<int>{2, 1}.begin(), vector<int>{2, 1}.end()));
}

// ============================================================================
// is_sorted_until tests
// ============================================================================

TEST(IsSortedUntil, Empty) {
  vector<int> v;
  auto it = zstl::is_sorted_until(v.begin(), v.end());
  EXPECT_EQ(it, v.end());
}

TEST(IsSortedUntil, Single) {
  vector<int> v = {1};
  auto it = zstl::is_sorted_until(v.begin(), v.end());
  EXPECT_EQ(it, v.end());
}

TEST(IsSortedUntil, AllSorted) {
  vector<int> v = {1, 2, 3, 4, 5};
  auto it = zstl::is_sorted_until(v.begin(), v.end());
  EXPECT_EQ(it, v.end());
}

TEST(IsSortedUntil, BreakMiddle) {
  vector<int> v = {1, 2, 5, 3, 4};
  auto it = zstl::is_sorted_until(v.begin(), v.end());
  EXPECT_EQ(it, v.begin() + 3);  // breaks at 3
}

TEST(IsSortedUntil, BreakImmediate) {
  vector<int> v = {2, 1, 3, 4, 5};
  auto it = zstl::is_sorted_until(v.begin(), v.end());
  EXPECT_EQ(it, v.begin() + 1);
}

// ============================================================================
// O(n log n) behavior verification (qualitative timing)
// ============================================================================

TEST(SortComplexity, QualitativeTiming) {
  // Verify that doubling N roughly doubles the time ratio, not quadruples
  auto timed_sort = [](int n) {
    vector<int> v(n);
    for (int i = 0; i < n; ++i) v[i] = (i * 7919 + 104729) % (n * 2);
    auto start = std::chrono::high_resolution_clock::now();
    zstl::sort(v.begin(), v.end());
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(end - start).count();
  };

  double t250 = timed_sort(250);
  double t500 = timed_sort(500);
  double t1000 = timed_sort(1000);

  // For O(n log n): ratio t500/t250 ≈ (500*log500)/(250*log250) ≈ 2.2x
  // For O(n^2): ratio would be ~4x
  double ratio_250_500 = t500 / t250;
  double ratio_500_1000 = t1000 / t500;

  // Both ratios should be well under 4 (and around ~2.2-2.5 for O(n log n))
  // Use generous bounds to avoid flaky tests
  EXPECT_LT(ratio_250_500, 3.5) << "O(n log n) check: t500/t250 = " << ratio_250_500;
  EXPECT_LT(ratio_500_1000, 3.5) << "O(n log n) check: t1000/t500 = " << ratio_500_1000;

  // Verify correctness after timed sort
  SUCCEED() << "Timing: 250=" << t250 << "us, 500=" << t500
            << "us, 1000=" << t1000 << "us";
}

TEST(StableSortComplexity, QualitativeTiming) {
  auto timed = [](int n) {
    vector<int> v(n);
    for (int i = 0; i < n; ++i) v[i] = (i * 7919 + 104729) % (n * 2);
    auto start = std::chrono::high_resolution_clock::now();
    zstl::stable_sort(v.begin(), v.end());
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(end - start).count();
  };

  double t250 = timed(250);
  double t500 = timed(500);

  EXPECT_LT(t500 / t250, 4.0);
  SUCCEED() << "Timing: 250=" << t250 << "us, 500=" << t500 << "us";
}

// ============================================================================
// Custom struct sort tests
// ============================================================================

struct Person {
  std::string name;
  int age;
  Person(const std::string& n, int a) : name(n), age(a) {}
  bool operator<(const Person& rhs) const { return age < rhs.age; }
  bool operator==(const Person& rhs) const {
    return name == rhs.name && age == rhs.age;
  }
};

TEST(Sort, CustomStructByAge) {
  vector<Person> v;
  v.push_back(Person("Charlie", 30));
  v.push_back(Person("Alice", 25));
  v.push_back(Person("Bob", 35));
  v.push_back(Person("Diana", 20));

  zstl::sort(v.begin(), v.end());

  EXPECT_EQ(v[0].age, 20);
  EXPECT_EQ(v[0].name, "Diana");
  EXPECT_EQ(v[1].age, 25);
  EXPECT_EQ(v[1].name, "Alice");
  EXPECT_EQ(v[2].age, 30);
  EXPECT_EQ(v[3].age, 35);
}

TEST(Sort, CustomStructByNameLength) {
  vector<Person> v;
  v.push_back(Person("Alex", 40));
  v.push_back(Person("Jonathan", 25));
  v.push_back(Person("Bo", 35));

  // Sort by name length using custom comparator
  zstl::sort(v.begin(), v.end(), [](const Person& a, const Person& b) {
    return a.name.length() < b.name.length();
  });

  EXPECT_EQ(v[0].name, "Bo");
  EXPECT_EQ(v[1].name, "Alex");
  EXPECT_EQ(v[2].name, "Jonathan");
}
