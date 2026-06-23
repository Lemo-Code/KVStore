// ============================================================================
// zstl string_view tests
// ============================================================================
// Tests: all constructors, operator=, iterators, element access,
//        capacity, modifiers, copy, substr, compare, starts_with/ends_with/
//        contains, find/rfind/find_first_of/find_last_of/find_first_not_of/
//        find_last_not_of, operator<<, literal ""_sv, out_of_range for at()
// ============================================================================

#include <gtest/gtest.h>
#include "zstl/zstl.h"

#include <string>
#include <sstream>
#include <stdexcept>

using namespace zstl;

// Helper: enable the ""_sv literal
using namespace zstl::literals::string_view_literals;

// ============================================================================
// Constructor tests
// ============================================================================

TEST(StringView, DefaultConstructor) {
  string_view sv;
  EXPECT_EQ(sv.data(), nullptr);
  EXPECT_EQ(sv.size(), 0);
  EXPECT_TRUE(sv.empty());
}

TEST(StringView, ConstructFromCString) {
  const char* s = "hello world";
  string_view sv(s);
  EXPECT_EQ(sv.size(), 11);
  EXPECT_STREQ(sv.data(), s);
}

TEST(StringView, ConstructFromCStringEmpty) {
  const char* s = "";
  string_view sv(s);
  EXPECT_EQ(sv.size(), 0);
  EXPECT_TRUE(sv.empty());
}

TEST(StringView, ConstructFromPtrAndLength) {
  const char* s = "hello world";
  string_view sv(s, 5);
  EXPECT_EQ(sv.size(), 5);
  EXPECT_EQ(sv[0], 'h');
  EXPECT_EQ(sv[4], 'o');
}

TEST(StringView, ConstructFromPtrAndZeroLength) {
  string_view sv("test", 0);
  EXPECT_EQ(sv.size(), 0);
  EXPECT_TRUE(sv.empty());
}

TEST(StringView, ConstructFromStdString) {
  std::string str = "hello";
  string_view sv(str);
  EXPECT_EQ(sv.size(), 5);
  EXPECT_EQ(sv, "hello");
}

TEST(StringView, ConstructFromStdStringEmpty) {
  std::string str;
  string_view sv(str);
  EXPECT_EQ(sv.size(), 0);
  EXPECT_TRUE(sv.empty());
}

TEST(StringView, CopyConstructor) {
  string_view a("hello");
  string_view b(a);
  EXPECT_EQ(a.data(), b.data());
  EXPECT_EQ(a.size(), b.size());
}

TEST(StringView, CopyAssignment) {
  string_view a("hello");
  string_view b("world");
  b = a;
  EXPECT_EQ(b, "hello");
  EXPECT_EQ(b.size(), 5);
}

// ============================================================================
// Iterator tests
// ============================================================================

TEST(StringView, BeginEnd) {
  string_view sv("abc");
  EXPECT_EQ(*sv.begin(), 'a');
  EXPECT_EQ(*(sv.end() - 1), 'c');
  EXPECT_EQ(sv.end() - sv.begin(), 3);
}

TEST(StringView, RangeBasedFor) {
  string_view sv("hello");
  std::string result;
  for (auto c : sv) result += c;
  EXPECT_EQ(result, "hello");
}

TEST(StringView, EmptyIterators) {
  string_view sv;
  EXPECT_EQ(sv.begin(), sv.end());
}

TEST(StringView, CBeginCEnd) {
  string_view sv("test");
  EXPECT_EQ(*sv.cbegin(), 't');
  EXPECT_EQ(*(sv.cend() - 1), 't');
}

TEST(StringView, RBeginREnd) {
  string_view sv("abc");
  EXPECT_EQ(*sv.rbegin(), 'c');
  EXPECT_EQ(*(sv.rend() - 1), 'a');
}

TEST(StringView, ReverseIteration) {
  string_view sv("hello");
  std::string result;
  for (auto it = sv.rbegin(); it != sv.rend(); ++it) {
    result += *it;
  }
  EXPECT_EQ(result, "olleh");
}

TEST(StringView, CRBeginCREnd) {
  string_view sv("xyz");
  EXPECT_EQ(*sv.crbegin(), 'z');
}

// ============================================================================
// Element access tests
// ============================================================================

TEST(StringView, OperatorBracket) {
  string_view sv("hello");
  EXPECT_EQ(sv[0], 'h');
  EXPECT_EQ(sv[1], 'e');
  EXPECT_EQ(sv[4], 'o');
}

TEST(StringView, At) {
  string_view sv("hello");
  EXPECT_EQ(sv.at(0), 'h');
  EXPECT_EQ(sv.at(2), 'l');
  EXPECT_EQ(sv.at(4), 'o');
}

TEST(StringView, AtOutOfRange) {
  string_view sv("hello");
  EXPECT_THROW(sv.at(5), std::out_of_range);
  EXPECT_THROW(sv.at(100), std::out_of_range);
}

TEST(StringView, AtOnEmpty) {
  string_view sv;
  EXPECT_THROW(sv.at(0), std::out_of_range);
}

TEST(StringView, Front) {
  string_view sv("abc");
  EXPECT_EQ(sv.front(), 'a');
}

TEST(StringView, Back) {
  string_view sv("abc");
  EXPECT_EQ(sv.back(), 'c');
}

TEST(StringView, Data) {
  string_view sv("hello");
  EXPECT_STREQ(sv.data(), "hello");
}

TEST(StringView, DataOnEmpty) {
  string_view sv;
  EXPECT_EQ(sv.data(), nullptr);
}

// ============================================================================
// Capacity tests
// ============================================================================

TEST(StringView, Size) {
  string_view sv("hello");
  EXPECT_EQ(sv.size(), 5);
  string_view empty;
  EXPECT_EQ(empty.size(), 0);
}

TEST(StringView, Length) {
  string_view sv("hello world");
  EXPECT_EQ(sv.length(), sv.size());
  EXPECT_EQ(sv.length(), 11);
}

TEST(StringView, MaxSize) {
  string_view sv;
  EXPECT_GT(sv.max_size(), 0);
}

TEST(StringView, Empty) {
  string_view sv;
  EXPECT_TRUE(sv.empty());
  string_view sv2("x");
  EXPECT_FALSE(sv2.empty());
}

// ============================================================================
// Modifier tests
// ============================================================================

TEST(StringView, RemovePrefix) {
  string_view sv("hello world");
  sv.remove_prefix(6);
  EXPECT_EQ(sv, "world");
  EXPECT_EQ(sv.size(), 5);
}

TEST(StringView, RemovePrefixEntireView) {
  string_view sv("hi");
  sv.remove_prefix(2);
  EXPECT_EQ(sv.size(), 0);
  EXPECT_TRUE(sv.empty());
}

TEST(StringView, RemovePrefixZero) {
  string_view sv("hello");
  sv.remove_prefix(0);
  EXPECT_EQ(sv, "hello");
  EXPECT_EQ(sv.size(), 5);
}

TEST(StringView, RemoveSuffix) {
  string_view sv("hello world");
  sv.remove_suffix(6);
  EXPECT_EQ(sv, "hello");
  EXPECT_EQ(sv.size(), 5);
}

TEST(StringView, RemoveSuffixEntireView) {
  string_view sv("hi");
  sv.remove_suffix(2);
  EXPECT_EQ(sv.size(), 0);
  EXPECT_TRUE(sv.empty());
}

TEST(StringView, RemoveSuffixZero) {
  string_view sv("hello");
  sv.remove_suffix(0);
  EXPECT_EQ(sv, "hello");
}

TEST(StringView, Swap) {
  string_view a("hello");
  string_view b("world");
  a.swap(b);
  EXPECT_EQ(a, "world");
  EXPECT_EQ(b, "hello");
}

TEST(StringView, NonMemberSwap) {
  string_view a("abc");
  string_view b("xyz");
  zstl::swap(a, b);
  EXPECT_EQ(a, "xyz");
  EXPECT_EQ(b, "abc");
}

// ============================================================================
// Copy and substr tests
// ============================================================================

TEST(StringView, Copy) {
  string_view sv("hello world");
  char buf[12] = {0};
  auto n = sv.copy(buf, 5, 0);
  EXPECT_EQ(n, 5);
  EXPECT_STREQ(buf, "hello");
}

TEST(StringView, CopyWithPos) {
  string_view sv("hello world");
  char buf[12] = {0};
  auto n = sv.copy(buf, 5, 6);
  EXPECT_EQ(n, 5);
  EXPECT_STREQ(buf, "world");
}

TEST(StringView, CopyOutOfRange) {
  string_view sv("hello");
  char buf[10];
  EXPECT_THROW(sv.copy(buf, 3, 10), std::out_of_range);
}

TEST(StringView, CopyTruncated) {
  string_view sv("hello");
  char buf[3];
  auto n = sv.copy(buf, 3, 4);  // only 1 char available from pos 4
  EXPECT_EQ(n, 1);
  EXPECT_EQ(buf[0], 'o');
}

TEST(StringView, Substr) {
  string_view sv("hello world");
  auto sub = sv.substr(0, 5);
  EXPECT_EQ(sub, "hello");
  auto sub2 = sv.substr(6, 5);
  EXPECT_EQ(sub2, "world");
}

TEST(StringView, SubstrDefaultNpos) {
  string_view sv("hello world");
  auto sub = sv.substr(6);
  EXPECT_EQ(sub, "world");
}

TEST(StringView, SubstrPosOutOfRange) {
  string_view sv("hello");
  EXPECT_THROW(sv.substr(10), std::out_of_range);
}

TEST(StringView, SubstrEmpty) {
  string_view sv("hello");
  auto sub = sv.substr(0, 0);
  EXPECT_TRUE(sub.empty());
}

// ============================================================================
// Compare tests
// ============================================================================

TEST(StringView, CompareLess) {
  string_view a("abc");
  string_view b("abd");
  EXPECT_LT(a.compare(b), 0);
}

TEST(StringView, CompareGreater) {
  string_view a("abd");
  string_view b("abc");
  EXPECT_GT(a.compare(b), 0);
}

TEST(StringView, CompareEqual) {
  string_view a("hello");
  string_view b("hello");
  EXPECT_EQ(a.compare(b), 0);
}

TEST(StringView, CompareShorterLess) {
  string_view a("ab");
  string_view b("abc");
  EXPECT_LT(a.compare(b), 0);
}

TEST(StringView, CompareLongerGreater) {
  string_view a("abcd");
  string_view b("abc");
  EXPECT_GT(a.compare(b), 0);
}

TEST(StringView, CompareWithPos) {
  string_view sv("hello world");
  EXPECT_EQ(sv.compare(6, 5, string_view("world")), 0);
}

TEST(StringView, CompareSubstrVsSubstr) {
  string_view sv1("hello world");
  string_view sv2("hello there");
  EXPECT_EQ(sv1.compare(0, 5, sv2, 0, 5), 0);
  EXPECT_NE(sv1.compare(6, 5, sv2, 6, 5), 0);
}

TEST(StringView, CompareCString) {
  string_view sv("hello");
  EXPECT_EQ(sv.compare("hello"), 0);
  EXPECT_GT(sv.compare("hella"), 0);
  EXPECT_LT(sv.compare("hellp"), 0);
}

// ============================================================================
// Comparison operators tests
// ============================================================================

TEST(StringView, OperatorEq) {
  string_view a("hello"), b("hello");
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
}

TEST(StringView, OperatorNeq) {
  string_view a("hello"), b("world");
  EXPECT_TRUE(a != b);
  EXPECT_FALSE(a == b);
}

TEST(StringView, OperatorLess) {
  string_view a("abc"), b("abd");
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(b < a);
}

TEST(StringView, OperatorGreater) {
  string_view a("abd"), b("abc");
  EXPECT_TRUE(a > b);
}

TEST(StringView, OperatorLE) {
  string_view a("abc"), b("abc"), c("abd");
  EXPECT_TRUE(a <= b);
  EXPECT_TRUE(a <= c);
}

TEST(StringView, OperatorGE) {
  string_view a("abd"), b("abc");
  EXPECT_TRUE(a >= b);
}

// ============================================================================
// starts_with / ends_with tests
// ============================================================================

TEST(StringView, StartsWithStringView) {
  string_view sv("hello world");
  EXPECT_TRUE(sv.starts_with(string_view("hello")));
  EXPECT_TRUE(sv.starts_with(string_view("h")));
  EXPECT_FALSE(sv.starts_with(string_view("world")));
  EXPECT_FALSE(sv.starts_with(string_view("hello world!")));
}

TEST(StringView, StartsWithChar) {
  string_view sv("hello");
  EXPECT_TRUE(sv.starts_with('h'));
  EXPECT_FALSE(sv.starts_with('x'));
}

TEST(StringView, StartsWithCString) {
  string_view sv("hello world");
  EXPECT_TRUE(sv.starts_with("hello"));
  EXPECT_FALSE(sv.starts_with("world"));
}

TEST(StringView, StartsWithEmpty) {
  string_view sv;
  EXPECT_FALSE(sv.starts_with('x'));
  EXPECT_FALSE(sv.starts_with("x"));
}

TEST(StringView, EndsWithStringView) {
  string_view sv("hello world");
  EXPECT_TRUE(sv.ends_with(string_view("world")));
  EXPECT_TRUE(sv.ends_with(string_view("d")));
  EXPECT_FALSE(sv.ends_with(string_view("hello")));
  EXPECT_FALSE(sv.ends_with(string_view("hello world!")));
}

TEST(StringView, EndsWithChar) {
  string_view sv("hello");
  EXPECT_TRUE(sv.ends_with('o'));
  EXPECT_FALSE(sv.ends_with('x'));
}

TEST(StringView, EndsWithCString) {
  string_view sv("hello world");
  EXPECT_TRUE(sv.ends_with("world"));
  EXPECT_FALSE(sv.ends_with("hello"));
}

TEST(StringView, EndsWithEmpty) {
  string_view sv;
  EXPECT_FALSE(sv.ends_with('x'));
  EXPECT_FALSE(sv.ends_with("x"));
}

// ============================================================================
// contains tests
// ============================================================================

TEST(StringView, ContainsStringView) {
  string_view sv("hello world");
  EXPECT_TRUE(sv.contains(string_view("hello")));
  EXPECT_TRUE(sv.contains(string_view("world")));
  EXPECT_TRUE(sv.contains(string_view("lo wo")));
  EXPECT_TRUE(sv.contains(string_view("")));
  EXPECT_FALSE(sv.contains(string_view("xyz")));
}

TEST(StringView, ContainsChar) {
  string_view sv("hello");
  EXPECT_TRUE(sv.contains('h'));
  EXPECT_TRUE(sv.contains('l'));
  EXPECT_TRUE(sv.contains('o'));
  EXPECT_FALSE(sv.contains('x'));
}

TEST(StringView, ContainsCString) {
  string_view sv("hello world");
  EXPECT_TRUE(sv.contains("world"));
  EXPECT_TRUE(sv.contains(""));
  EXPECT_FALSE(sv.contains("xyz"));
}

// ============================================================================
// find tests
// ============================================================================

TEST(StringView, FindStringView) {
  string_view sv("hello world, hello universe");
  auto pos = sv.find(string_view("hello"));
  EXPECT_EQ(pos, 0);
  auto pos2 = sv.find(string_view("hello"), 1);
  EXPECT_EQ(pos2, 13);
}

TEST(StringView, FindChar) {
  string_view sv("hello world");
  EXPECT_EQ(sv.find('h'), 0);
  EXPECT_EQ(sv.find('o'), 4);
  EXPECT_EQ(sv.find('x'), string_view::npos);
}

TEST(StringView, FindCString) {
  string_view sv("hello world");
  EXPECT_EQ(sv.find("world"), 6);
  EXPECT_EQ(sv.find("xyz"), string_view::npos);
}

TEST(StringView, FindEmpty) {
  string_view sv("hello");
  EXPECT_EQ(sv.find(string_view("")), 0);
  EXPECT_EQ(sv.find(""), 0);
}

TEST(StringView, FindPastEnd) {
  string_view sv("hi");
  EXPECT_EQ(sv.find('h', 10), string_view::npos);
}

// ============================================================================
// rfind tests
// ============================================================================

TEST(StringView, RFindStringView) {
  string_view sv("hello world, hello");
  auto pos = sv.rfind(string_view("hello"));
  EXPECT_EQ(pos, 13);
}

TEST(StringView, RFindChar) {
  string_view sv("hello world");
  EXPECT_EQ(sv.rfind('o'), 7);
  EXPECT_EQ(sv.rfind('h'), 0);
  EXPECT_EQ(sv.rfind('x'), string_view::npos);
}

TEST(StringView, RFindCString) {
  string_view sv("ababab");
  EXPECT_EQ(sv.rfind("ab"), 4);
  EXPECT_EQ(sv.rfind("ba"), 3);
}

TEST(StringView, RFindEmpty) {
  string_view sv("hello");
  EXPECT_EQ(sv.rfind(string_view("")), 5);
}

// ============================================================================
// find_first_of tests
// ============================================================================

TEST(StringView, FindFirstOfStringView) {
  string_view sv("hello world");
  auto pos = sv.find_first_of(string_view("aeiou"));
  EXPECT_EQ(pos, 1);  // 'e'
}

TEST(StringView, FindFirstOfChar) {
  string_view sv("hello");
  EXPECT_EQ(sv.find_first_of('l'), 2);
  EXPECT_EQ(sv.find_first_of('z'), string_view::npos);
}

TEST(StringView, FindFirstOfCString) {
  string_view sv("hello world");
  EXPECT_EQ(sv.find_first_of("xyzol"), 2);  // 'l'
  EXPECT_EQ(sv.find_first_of("xyz"), string_view::npos);
}

TEST(StringView, FindFirstOfWithPos) {
  string_view sv("hello world");
  EXPECT_EQ(sv.find_first_of(string_view("aeiou"), 5), 7);  // 'o' after pos 5
}

TEST(StringView, FindFirstOfEmpty) {
  string_view sv("hello");
  EXPECT_EQ(sv.find_first_of(string_view("")), string_view::npos);
}

// ============================================================================
// find_last_of tests
// ============================================================================

TEST(StringView, FindLastOfStringView) {
  string_view sv("hello world");
  auto pos = sv.find_last_of(string_view("aeiou"));
  EXPECT_EQ(pos, 7);  // last vowel is 'o' at index 7
}

TEST(StringView, FindLastOfChar) {
  string_view sv("hello");
  EXPECT_EQ(sv.find_last_of('l'), 3);
  EXPECT_EQ(sv.find_last_of('z'), string_view::npos);
}

TEST(StringView, FindLastOfCString) {
  string_view sv("hello world");
  EXPECT_EQ(sv.find_last_of("dlh"), 10);  // 'd' at index 10
}

// ============================================================================
// find_first_not_of tests
// ============================================================================

TEST(StringView, FindFirstNotOfStringView) {
  string_view sv("   hello");
  auto pos = sv.find_first_not_of(string_view(" "));
  EXPECT_EQ(pos, 3);  // 'h'
}

TEST(StringView, FindFirstNotOfChar) {
  string_view sv("aaaaab");
  EXPECT_EQ(sv.find_first_not_of('a'), 5);
  EXPECT_EQ(sv.find_first_not_of('b'), 0);
}

TEST(StringView, FindFirstNotOfCString) {
  string_view sv("hello world");
  EXPECT_EQ(sv.find_first_not_of("helo"), 5);  // space at index 5
}

TEST(StringView, FindFirstNotOfAllMatch) {
  string_view sv("aaaa");
  EXPECT_EQ(sv.find_first_not_of('a'), string_view::npos);
}

// ============================================================================
// find_last_not_of tests
// ============================================================================

TEST(StringView, FindLastNotOfStringView) {
  string_view sv("hello   ");
  auto pos = sv.find_last_not_of(string_view(" "));
  EXPECT_EQ(pos, 4);  // 'o' at index 4
}

TEST(StringView, FindLastNotOfChar) {
  string_view sv("baaaaa");
  EXPECT_EQ(sv.find_last_not_of('a'), 0);  // 'b'
  EXPECT_EQ(string_view("aaaa").find_last_not_of('a'), string_view::npos);
}

TEST(StringView, FindLastNotOfCString) {
  string_view sv("  hello  ");
  EXPECT_EQ(sv.find_last_not_of(" "), 6);  // 'o' at index 6
}

// ============================================================================
// literal ""_sv tests
// ============================================================================

TEST(StringView, LiteralSv) {
  auto sv = "hello"_sv;
  EXPECT_EQ(sv.size(), 5);
  EXPECT_EQ(sv, "hello");
  EXPECT_EQ(sv[0], 'h');
}

TEST(StringView, LiteralSvCompare) {
  auto sv = "test"_sv;
  EXPECT_EQ(sv.compare(string_view("test")), 0);
  EXPECT_TRUE(sv.starts_with("te"_sv));
  EXPECT_TRUE(sv.ends_with("st"_sv));
}

TEST(StringView, LiteralSvEmpty) {
  auto sv = ""_sv;
  EXPECT_TRUE(sv.empty());
  EXPECT_EQ(sv.size(), 0);
}

// ============================================================================
// operator<< tests
// ============================================================================

TEST(StringView, OStreamOperator) {
  string_view sv("hello world");
  std::ostringstream oss;
  oss << sv;
  EXPECT_EQ(oss.str(), "hello world");
}

TEST(StringView, OStreamOperatorEmpty) {
  string_view sv;
  std::ostringstream oss;
  oss << sv;
  EXPECT_EQ(oss.str(), "");
}

// ============================================================================
// hash test
// ============================================================================

TEST(StringView, Hash) {
  string_view a("hello");
  string_view b("hello");
  string_view c("world");
  std::hash<string_view> hasher;
  EXPECT_EQ(hasher(a), hasher(b));
  EXPECT_NE(hasher(a), hasher(c));
}
