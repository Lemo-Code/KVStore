// ============================================================================
// zstl string (basic_string<char>) Unit Tests
// Tests all constructors, assignment, element access, iterators, capacity,
// modifiers (append/insert/erase/replace/push_back/pop_back), search operations,
// comparison, SSO boundary behavior, and conversion functions.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

using namespace zstl;

// ============================================================
// Constructors
// ============================================================

TEST(StringTest, DefaultConstructor) {
    string s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_EQ(s.length(), 0u);
    EXPECT_STREQ(s.c_str(), "");
}

TEST(StringTest, FillCharConstructor) {
    string s(5, 'x');
    EXPECT_EQ(s.size(), 5u);
    EXPECT_EQ(s.length(), 5u);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(s[i], 'x');
    }
    EXPECT_EQ(s[5], '\0');
}

TEST(StringTest, FillCharConstructorZero) {
    string s(0, 'x');
    EXPECT_TRUE(s.empty());
    EXPECT_STREQ(s.c_str(), "");
}

TEST(StringTest, CStrConstructor) {
    string s("hello");
    EXPECT_EQ(s.size(), 5u);
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(StringTest, CStrConstructorEmpty) {
    string s("");
    EXPECT_TRUE(s.empty());
    EXPECT_STREQ(s.c_str(), "");
}

TEST(StringTest, CStrLengthConstructor) {
    string s("hello world", 5);
    EXPECT_EQ(s.size(), 5u);
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(StringTest, CStrLengthConstructorZero) {
    string s("hello", 0);
    EXPECT_TRUE(s.empty());
}

TEST(StringTest, CopyConstructor) {
    string src("original");
    string s(src);
    EXPECT_EQ(s.size(), 8u);
    EXPECT_STREQ(s.c_str(), "original");
    // Deep copy
    src[0] = 'X';
    EXPECT_EQ(s[0], 'o');
}

TEST(StringTest, MoveConstructor) {
    string src("move me");
    string s(std::move(src));
    EXPECT_STREQ(s.c_str(), "move me");
    EXPECT_TRUE(src.empty());  // moved-from is empty
}

TEST(StringTest, InitializerListConstructor) {
    string s = {'h', 'e', 'l', 'l', 'o'};
    EXPECT_EQ(s.size(), 5u);
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(StringTest, InitializerListConstructorEmpty) {
    string s = {};
    EXPECT_TRUE(s.empty());
}

TEST(StringTest, IteratorRangeConstructor) {
    std::vector<char> v = {'a', 'b', 'c', 'd'};
    string s(v.begin(), v.end());
    EXPECT_EQ(s.size(), 4u);
    EXPECT_STREQ(s.c_str(), "abcd");
}

TEST(StringTest, IteratorRangeConstructorEmpty) {
    std::vector<char> v;
    string s(v.begin(), v.end());
    EXPECT_TRUE(s.empty());
}

// ============================================================
// operator=
// ============================================================

TEST(StringTest, CopyAssignment) {
    string a("source");
    string b("destination");
    b = a;
    EXPECT_STREQ(b.c_str(), "source");
    a[0] = 'X';
    EXPECT_EQ(b[0], 's');  // deep copy
}

TEST(StringTest, CopyAssignmentSelf) {
    string s("test");
    s = s;
    EXPECT_STREQ(s.c_str(), "test");
}

TEST(StringTest, MoveAssignment) {
    string a("move source");
    string b("longer destination string");
    b = std::move(a);
    EXPECT_STREQ(b.c_str(), "move source");
    EXPECT_TRUE(a.empty());
}

TEST(StringTest, MoveAssignmentSelf) {
    string s("test");
    s = std::move(s);  // self-move should not crash
    SUCCEED();
}

TEST(StringTest, CStrAssignment) {
    string s("old");
    s = "new value";
    EXPECT_STREQ(s.c_str(), "new value");
}

TEST(StringTest, CharAssignment) {
    string s("old");
    s = 'X';
    EXPECT_EQ(s.size(), 1u);
    EXPECT_EQ(s[0], 'X');
}

TEST(StringTest, InitializerListAssignment) {
    string s("old");
    s = {'n', 'e', 'w'};
    EXPECT_STREQ(s.c_str(), "new");
}

// ============================================================
// assign
// ============================================================

TEST(StringTest, AssignFillChar) {
    string s;
    s.assign(5, 'z');
    EXPECT_EQ(s.size(), 5u);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(s[i], 'z');
    }
}

TEST(StringTest, AssignString) {
    string s("old");
    string t("new string");
    s.assign(t);
    EXPECT_STREQ(s.c_str(), "new string");
}

TEST(StringTest, AssignSubstring) {
    string s;
    string t("hello world");
    s.assign(t, 6, 5);  // "world"
    EXPECT_STREQ(s.c_str(), "world");
}

TEST(StringTest, AssignSubstringOverflow) {
    string s;
    string t("hello");
    s.assign(t, 2, 100);  // count exceeds remaining length
    EXPECT_STREQ(s.c_str(), "llo");
}

TEST(StringTest, AssignSubstringPosOutOfRange) {
    string s;
    string t("hello");
    EXPECT_THROW(s.assign(t, 10, 1), std::out_of_range);
}

TEST(StringTest, AssignCStrLength) {
    string s;
    s.assign("hello world", 5);
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(StringTest, AssignCStr) {
    string s("old stuff");
    s.assign("new");
    EXPECT_STREQ(s.c_str(), "new");
}

TEST(StringTest, AssignRange) {
    string s;
    std::vector<char> v = {'a', 'b', 'c'};
    s.assign(v.begin(), v.end());
    EXPECT_STREQ(s.c_str(), "abc");
}

TEST(StringTest, AssignInitializerList) {
    string s;
    s.assign({'x', 'y', 'z'});
    EXPECT_STREQ(s.c_str(), "xyz");
}

// ============================================================
// Element access
// ============================================================

TEST(StringTest, AtValid) {
    string s("hello");
    EXPECT_EQ(s.at(0), 'h');
    EXPECT_EQ(s.at(4), 'o');
    s.at(0) = 'H';
    EXPECT_EQ(s.at(0), 'H');
}

TEST(StringTest, AtOutOfRange) {
    string s("abc");
    EXPECT_THROW(s.at(3), std::out_of_range);
    EXPECT_THROW(s.at(100), std::out_of_range);
    const auto& cs = s;
    EXPECT_THROW(cs.at(3), std::out_of_range);
}

TEST(StringTest, AtOnEmpty) {
    string s;
    EXPECT_THROW(s.at(0), std::out_of_range);
}

TEST(StringTest, OperatorBracket) {
    string s("test");
    EXPECT_EQ(s[0], 't');
    EXPECT_EQ(s[3], 't');
    s[1] = 'E';
    EXPECT_EQ(s[1], 'E');
    const auto& cs = s;
    EXPECT_EQ(cs[0], 't');
    // operator[] allows reading null terminator
    EXPECT_EQ(s[s.size()], '\0');
}

TEST(StringTest, FrontBack) {
    string s("hello");
    EXPECT_EQ(s.front(), 'h');
    EXPECT_EQ(s.back(), 'o');
    s.front() = 'H';
    s.back() = 'O';
    EXPECT_EQ(s.front(), 'H');
    EXPECT_EQ(s.back(), 'O');
    const auto& cs = s;
    EXPECT_EQ(cs.front(), 'H');
    EXPECT_EQ(cs.back(), 'O');
}

TEST(StringTest, FrontBackSingleChar) {
    string s("X");
    EXPECT_EQ(s.front(), 'X');
    EXPECT_EQ(s.back(), 'X');
}

// ============================================================
// c_str / data
// ============================================================

TEST(StringTest, CStr) {
    string s("hello");
    const char* p = s.c_str();
    EXPECT_STREQ(p, "hello");
    EXPECT_EQ(p[5], '\0');
}

TEST(StringTest, Data) {
    string s("world");
    EXPECT_STREQ(s.data(), "world");
    // Non-const data
    char* p = s.data();
    p[0] = 'W';
    EXPECT_STREQ(s.c_str(), "World");
}

TEST(StringTest, DataCStrOnEmpty) {
    string s;
    EXPECT_STREQ(s.c_str(), "");
    EXPECT_STREQ(s.data(), "");
    EXPECT_EQ(s.c_str()[0], '\0');
}

// ============================================================
// Iterators
// ============================================================

TEST(StringTest, BeginEnd) {
    string s("abc");
    auto it = s.begin();
    EXPECT_EQ(*it, 'a');
    ++it;
    EXPECT_EQ(*it, 'b');
    ++it;
    EXPECT_EQ(*it, 'c');
    ++it;
    EXPECT_EQ(it, s.end());
}

TEST(StringTest, ConstIterators) {
    const string s("abc");
    EXPECT_EQ(*s.cbegin(), 'a');
    EXPECT_EQ(*(s.cend() - 1), 'c');
}

TEST(StringTest, RangeForLoop) {
    string s("hello");
    std::string result;
    for (char c : s) {
        result += c;
    }
    EXPECT_EQ(result, "hello");
}

TEST(StringTest, IteratorOnEmpty) {
    string s;
    EXPECT_EQ(s.begin(), s.end());
    EXPECT_EQ(s.cbegin(), s.cend());
}

// ============================================================
// Capacity
// ============================================================

TEST(StringTest, Empty) {
    string s;
    EXPECT_TRUE(s.empty());
    s = "x";
    EXPECT_FALSE(s.empty());
    s.clear();
    EXPECT_TRUE(s.empty());
}

TEST(StringTest, SizeLength) {
    string s("hello");
    EXPECT_EQ(s.size(), 5u);
    EXPECT_EQ(s.length(), 5u);
    EXPECT_EQ(s.size(), s.length());
}

TEST(StringTest, MaxSize) {
    string s;
    EXPECT_GT(s.max_size(), 0u);
}

TEST(StringTest, CapacityDefault) {
    string s;
    // SSO capacity is 15
    EXPECT_EQ(s.capacity(), 15u);
}

TEST(StringTest, CapacityAfterReserve) {
    string s;
    s.reserve(50);
    EXPECT_GE(s.capacity(), 50u);
    EXPECT_EQ(s.size(), 0u);  // size unchanged
}

TEST(StringTest, ReserveLarger) {
    string s("hello");
    size_t old_cap = s.capacity();
    s.reserve(old_cap * 2 + 10);
    EXPECT_GE(s.capacity(), old_cap * 2 + 10);
    EXPECT_STREQ(s.c_str(), "hello");  // content preserved
}

TEST(StringTest, ReserveSmaller) {
    string s("hello");
    size_t old_cap = s.capacity();
    s.reserve(3);
    // reserve should not shrink if smaller than current capacity
    EXPECT_GE(s.capacity(), old_cap);
}

TEST(StringTest, ShrinkToFit) {
    string s;
    s.reserve(100);
    s = "short";
    size_t old_cap = s.capacity();
    s.shrink_to_fit();
    EXPECT_LE(s.capacity(), old_cap);
    EXPECT_STREQ(s.c_str(), "short");
}

TEST(StringTest, Clear) {
    string s("hello world");
    s.clear();
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_STREQ(s.c_str(), "");
    // Reusable after clear
    s += "reused";
    EXPECT_STREQ(s.c_str(), "reused");
}

// ============================================================
// operator+=
// ============================================================

TEST(StringTest, OperatorPlusEqualsString) {
    string s("hello ");
    string t("world");
    s += t;
    EXPECT_STREQ(s.c_str(), "hello world");
}

TEST(StringTest, OperatorPlusEqualsCStr) {
    string s("hello ");
    s += "world";
    EXPECT_STREQ(s.c_str(), "hello world");
}

TEST(StringTest, OperatorPlusEqualsChar) {
    string s("abc");
    s += 'd';
    EXPECT_STREQ(s.c_str(), "abcd");
}

TEST(StringTest, OperatorPlusEqualsInitializerList) {
    string s("ab");
    s += {'c', 'd', 'e'};
    EXPECT_STREQ(s.c_str(), "abcde");
}

// ============================================================
// append
// ============================================================

TEST(StringTest, AppendString) {
    string s("hello ");
    string t("world");
    s.append(t);
    EXPECT_STREQ(s.c_str(), "hello world");
}

TEST(StringTest, AppendSubstring) {
    string s("hello ");
    string t("beautiful world");
    s.append(t, 10, 5);  // "world"
    EXPECT_STREQ(s.c_str(), "hello world");
}

TEST(StringTest, AppendSubstringPosOutOfRange) {
    string s("hello ");
    string t("world");
    EXPECT_THROW(s.append(t, 10, 5), std::out_of_range);
}

TEST(StringTest, AppendCStrLength) {
    string s("hello ");
    s.append("world!!!", 5);
    EXPECT_STREQ(s.c_str(), "hello world");
}

TEST(StringTest, AppendCStr) {
    string s("hello ");
    s.append("world");
    EXPECT_STREQ(s.c_str(), "hello world");
}

TEST(StringTest, AppendFillChar) {
    string s("x");
    s.append(5, 'y');
    EXPECT_STREQ(s.c_str(), "xyyyyy");
}

TEST(StringTest, AppendFillCharZero) {
    string s("x");
    s.append(0, 'y');
    EXPECT_STREQ(s.c_str(), "x");
}

TEST(StringTest, AppendRange) {
    string s("start-");
    std::vector<char> v = {'e', 'n', 'd'};
    s.append(v.begin(), v.end());
    EXPECT_STREQ(s.c_str(), "start-end");
}

TEST(StringTest, AppendInitializerList) {
    string s("ab");
    s.append({'c', 'd', 'e'});
    EXPECT_STREQ(s.c_str(), "abcde");
}

// ============================================================
// push_back / pop_back
// ============================================================

TEST(StringTest, PushBack) {
    string s;
    s.push_back('a');
    EXPECT_EQ(s.size(), 1u);
    EXPECT_EQ(s[0], 'a');
    EXPECT_EQ(s[1], '\0');
    s.push_back('b');
    s.push_back('c');
    EXPECT_STREQ(s.c_str(), "abc");
}

TEST(StringTest, PushBackMany) {
    string s;
    for (int i = 0; i < 100; ++i) {
        s.push_back(static_cast<char>('a' + (i % 26)));
    }
    EXPECT_EQ(s.size(), 100u);
    EXPECT_EQ(s[0], 'a');
    EXPECT_EQ(s[99], 'v');  // 'a' + (99 % 26) = 'a' + 21 = 'v'
}

TEST(StringTest, PopBack) {
    string s("hello");
    s.pop_back();
    EXPECT_STREQ(s.c_str(), "hell");
    s.pop_back();
    s.pop_back();
    EXPECT_STREQ(s.c_str(), "he");
    s.pop_back();
    s.pop_back();
    EXPECT_TRUE(s.empty());
    // Pop on empty should not crash
    s.pop_back();
    EXPECT_TRUE(s.empty());
}

// ============================================================
// insert
// ============================================================

TEST(StringTest, InsertFillChar) {
    string s("abef");
    s.insert(2, 2, 'c');  // insert "cc" at position 2 -> "abccdef" wait... let me recalculate
    // Actually: "abef".insert(2, 2, 'c') = "ab" + "cc" + "ef" = "abccef"
    EXPECT_STREQ(s.c_str(), "abccef");
}

TEST(StringTest, InsertFillCharAtEnd) {
    string s("abc");
    s.insert(3, 2, 'd');
    EXPECT_STREQ(s.c_str(), "abcdd");
}

TEST(StringTest, InsertFillCharAtBeginning) {
    string s("abc");
    s.insert(0, 2, 'x');
    EXPECT_STREQ(s.c_str(), "xxabc");
}

TEST(StringTest, InsertFillCharIndexOutOfRange) {
    string s("abc");
    EXPECT_THROW(s.insert(4, 1, 'x'), std::out_of_range);
}

TEST(StringTest, InsertCStr) {
    string s("hello world");
    s.insert(5, " cruel");
    EXPECT_STREQ(s.c_str(), "hello cruel world");
}

TEST(StringTest, InsertCStrLength) {
    string s("abcf");
    s.insert(3, "def", 2);  // Insert first 2 chars of "def" = "de"
    EXPECT_STREQ(s.c_str(), "abcdef");
}

TEST(StringTest, InsertString) {
    string s("hello world");
    string t(" cruel");
    s.insert(5, t);
    EXPECT_STREQ(s.c_str(), "hello cruel world");
}

// ============================================================
// erase
// ============================================================

TEST(StringTest, EraseIndexCount) {
    string s("hello world");
    s.erase(5, 1);  // erase the space
    EXPECT_STREQ(s.c_str(), "helloworld");
}

TEST(StringTest, EraseIndexOverflow) {
    string s("hello");
    s.erase(2, 100);  // erase from position 2 to end
    EXPECT_STREQ(s.c_str(), "he");
}

TEST(StringTest, EraseIndexOutOfRange) {
    string s("abc");
    EXPECT_THROW(s.erase(5, 1), std::out_of_range);
}

TEST(StringTest, EraseDefaultParams) {
    string s("hello");
    s.erase();  // erase all
    EXPECT_TRUE(s.empty());
}

TEST(StringTest, EraseIterator) {
    string s("hello");
    auto it = s.erase(s.begin() + 1);  // erase 'e'
    EXPECT_STREQ(s.c_str(), "hllo");
    EXPECT_EQ(*it, 'l');
}

TEST(StringTest, EraseIteratorRange) {
    string s("hello world");
    auto it = s.erase(s.begin() + 6, s.end());  // erase "world"
    EXPECT_STREQ(s.c_str(), "hello ");
    EXPECT_EQ(it, s.end());
}

// ============================================================
// replace
// ============================================================

TEST(StringTest, ReplaceWithString) {
    string s("hello world");
    string t("cruel");
    s.replace(6, 5, t);  // replace "world" with "cruel"
    EXPECT_STREQ(s.c_str(), "hello cruel");
}

TEST(StringTest, ReplaceWithCStrLength) {
    string s("hello world");
    s.replace(6, 5, "earthlings", 5);  // replace "world" with "earth"
    EXPECT_STREQ(s.c_str(), "hello earth");
}

TEST(StringTest, ReplaceWithCStr) {
    string s("hello world");
    s.replace(6, 5, "there");
    EXPECT_STREQ(s.c_str(), "hello there");
}

TEST(StringTest, ReplaceWithFillChar) {
    string s("hello world");
    s.replace(6, 5, 3, '*');  // replace "world" with "***"
    EXPECT_STREQ(s.c_str(), "hello ***");
}

TEST(StringTest, ReplaceLargerThanOriginal) {
    string s("abf");
    s.replace(2, 1, "cde");  // replace 1 char with 3 chars
    EXPECT_STREQ(s.c_str(), "abcdef");
}

TEST(StringTest, ReplaceSmallerThanOriginal) {
    string s("abzzzf");
    s.replace(2, 3, "c");  // replace 3 chars with 1 char
    EXPECT_STREQ(s.c_str(), "abcf");
}

TEST(StringTest, ReplacePosOutOfRange) {
    string s("abc");
    EXPECT_THROW(s.replace(5, 1, "x"), std::out_of_range);
}

// ============================================================
// substr
// ============================================================

TEST(StringTest, SubstrBasic) {
    string s("hello world");
    string sub = s.substr(6, 5);
    EXPECT_STREQ(sub.c_str(), "world");
}

TEST(StringTest, SubstrDefaultParams) {
    string s("hello");
    string sub = s.substr();  // full string
    EXPECT_STREQ(sub.c_str(), "hello");
}

TEST(StringTest, SubstrCountOverflow) {
    string s("hello");
    string sub = s.substr(2, 100);
    EXPECT_STREQ(sub.c_str(), "llo");
}

TEST(StringTest, SubstrPosOutOfRange) {
    string s("abc");
    EXPECT_THROW(s.substr(5, 1), std::out_of_range);
}

TEST(StringTest, SubstrPosEqualsSize) {
    string s("abc");
    string sub = s.substr(3, 0);  // pos == size is allowed, returns empty
    EXPECT_TRUE(sub.empty());
}

// ============================================================
// resize
// ============================================================

// Note: basic_string has push_back/pop_back but no resize() member — check

// ============================================================
// swap
// ============================================================

TEST(StringTest, SwapMember) {
    string a("hello");
    string b("world");
    a.swap(b);
    EXPECT_STREQ(a.c_str(), "world");
    EXPECT_STREQ(b.c_str(), "hello");
}

TEST(StringTest, SwapNonMember) {
    string a("first");
    string b("second");
    swap(a, b);
    EXPECT_STREQ(a.c_str(), "second");
    EXPECT_STREQ(b.c_str(), "first");
}

TEST(StringTest, SwapSame) {
    string s("test");
    s.swap(s);
    EXPECT_STREQ(s.c_str(), "test");
}

TEST(StringTest, SwapEmpty) {
    string a("content");
    string b;
    a.swap(b);
    EXPECT_TRUE(a.empty());
    EXPECT_STREQ(b.c_str(), "content");
}

// ============================================================
// find
// ============================================================

TEST(StringTest, FindString) {
    string s("hello world hello");
    string needle("world");
    EXPECT_EQ(s.find(needle), 6u);
    EXPECT_EQ(s.find(string("xyz")), string::npos);
}

TEST(StringTest, FindStringWithPos) {
    string s("hello world hello");
    string needle("hello");
    EXPECT_EQ(s.find(needle, 1), 12u);
}

TEST(StringTest, FindChar) {
    string s("hello");
    EXPECT_EQ(s.find('h'), 0u);
    EXPECT_EQ(s.find('l'), 2u);
    EXPECT_EQ(s.find('z'), string::npos);
}

TEST(StringTest, FindCharWithPos) {
    string s("hello hello");
    EXPECT_EQ(s.find('h', 1), 6u);
}

TEST(StringTest, FindCStr) {
    string s("hello world");
    EXPECT_EQ(s.find("world"), 6u);
    EXPECT_EQ(s.find("xyz"), string::npos);
}

TEST(StringTest, FindCStrLength) {
    string s("hello world");
    EXPECT_EQ(s.find("worldwide", 0, 5), 6u);  // find "world" (first 5 chars)
}

TEST(StringTest, FindEmptyString) {
    string s("hello");
    EXPECT_EQ(s.find(string()), 0u);
    EXPECT_EQ(s.find(""), 0u);
}

TEST(StringTest, FindOnEmpty) {
    string s;
    EXPECT_EQ(s.find('a'), string::npos);
    EXPECT_EQ(s.find(""), 0u);
}

// ============================================================
// rfind
// ============================================================

TEST(StringTest, RfindString) {
    string s("hello world hello");
    string needle("hello");
    EXPECT_EQ(s.rfind(needle), 12u);
}

TEST(StringTest, RfindStringNotFound) {
    string s("hello");
    EXPECT_EQ(s.rfind(string("xyz")), string::npos);
}

TEST(StringTest, RfindChar) {
    string s("hello");
    EXPECT_EQ(s.rfind('l'), 3u);
    EXPECT_EQ(s.rfind('h'), 0u);
    EXPECT_EQ(s.rfind('z'), string::npos);
}

TEST(StringTest, RfindCStr) {
    string s("ababab");
    EXPECT_EQ(s.rfind("ab"), 4u);
}

TEST(StringTest, RfindEmptyString) {
    string s("hello");
    EXPECT_EQ(s.rfind(""), 5u);  // returns size() for empty search string
}

// ============================================================
// find_first_of / find_last_of
// ============================================================

TEST(StringTest, FindFirstOfString) {
    string s("hello world");
    string chars("aeiou");
    EXPECT_EQ(s.find_first_of(chars), 1u);  // 'e'
}

TEST(StringTest, FindFirstOfNotFound) {
    string s("bcdfg");
    string chars("aeiou");
    EXPECT_EQ(s.find_first_of(chars), string::npos);
}

TEST(StringTest, FindFirstOfChar) {
    string s("hello");
    EXPECT_EQ(s.find_first_of('e'), 1u);
    EXPECT_EQ(s.find_first_of('z'), string::npos);
}

TEST(StringTest, FindLastOfString) {
    string s("hello world");
    string chars("aeiou");
    EXPECT_EQ(s.find_last_of(chars), 7u);  // 'o' in "world"
}

TEST(StringTest, FindLastOfNotFound) {
    string s("bcdfg");
    string chars("aeiou");
    EXPECT_EQ(s.find_last_of(chars), string::npos);
}

// ============================================================
// find_first_not_of / find_last_not_of
// ============================================================

TEST(StringTest, FindFirstNotOfString) {
    string s("aaabbbccc");
    string chars("a");
    EXPECT_EQ(s.find_first_not_of(chars), 3u);  // first 'b'
}

TEST(StringTest, FindFirstNotOfAllMatch) {
    string s("aaaa");
    string chars("a");
    EXPECT_EQ(s.find_first_not_of(chars), string::npos);
}

TEST(StringTest, FindFirstNotOfChar) {
    string s("aaaabc");
    EXPECT_EQ(s.find_first_not_of('a'), 4u);
    EXPECT_EQ(s.find_first_not_of('a', 2), 4u);
}

TEST(StringTest, FindLastNotOfString) {
    string s("hello");
    string chars("lo");
    EXPECT_EQ(s.find_last_not_of(chars), 1u);  // 'e'
}

TEST(StringTest, FindLastNotOfAllMatch) {
    string s("aaa");
    EXPECT_EQ(s.find_last_not_of('a'), string::npos);
}

TEST(StringTest, FindLastNotOfChar) {
    string s("abcaaa");
    EXPECT_EQ(s.find_last_not_of('a'), 3u);  // 'c'
}

// ============================================================
// contains (via find)
// ============================================================

TEST(StringTest, ContainsViaFind) {
    string s("hello world");
    EXPECT_NE(s.find("world"), string::npos);
    EXPECT_NE(s.find('w'), string::npos);
    EXPECT_EQ(s.find("xyz"), string::npos);
}

// ============================================================
// starts_with / ends_with
// ============================================================

TEST(StringTest, StartsWithString) {
    string s("hello world");
    string prefix("hello");
    EXPECT_TRUE(s.starts_with(prefix));
    EXPECT_FALSE(s.starts_with(string("world")));
}

TEST(StringTest, StartsWithChar) {
    string s("hello");
    EXPECT_TRUE(s.starts_with('h'));
    EXPECT_FALSE(s.starts_with('e'));
}

TEST(StringTest, StartsWithCStr) {
    string s("hello world");
    EXPECT_TRUE(s.starts_with("hello"));
    EXPECT_FALSE(s.starts_with("world"));
}

TEST(StringTest, StartsWithEmpty) {
    string s("hello");
    EXPECT_TRUE(s.starts_with(string()));
    EXPECT_TRUE(s.starts_with(""));
}

TEST(StringTest, StartsWithOnEmpty) {
    string s;
    EXPECT_TRUE(s.starts_with(""));
    EXPECT_FALSE(s.starts_with("a"));
    EXPECT_FALSE(s.starts_with('a'));
}

TEST(StringTest, StartsWithLongerThanString) {
    string s("hi");
    EXPECT_FALSE(s.starts_with("hello"));
}

TEST(StringTest, EndsWithString) {
    string s("hello world");
    string suffix("world");
    EXPECT_TRUE(s.ends_with(suffix));
    EXPECT_FALSE(s.ends_with(string("hello")));
}

TEST(StringTest, EndsWithChar) {
    string s("hello");
    EXPECT_TRUE(s.ends_with('o'));
    EXPECT_FALSE(s.ends_with('l'));
}

TEST(StringTest, EndsWithCStr) {
    string s("hello world");
    EXPECT_TRUE(s.ends_with("world"));
    EXPECT_FALSE(s.ends_with("hello"));
}

TEST(StringTest, EndsWithEmpty) {
    string s("hello");
    EXPECT_TRUE(s.ends_with(string()));
    EXPECT_TRUE(s.ends_with(""));
}

TEST(StringTest, EndsWithOnEmpty) {
    string s;
    EXPECT_TRUE(s.ends_with(""));
    EXPECT_FALSE(s.ends_with("a"));
    EXPECT_FALSE(s.ends_with('a'));
}

TEST(StringTest, EndsWithLongerThanString) {
    string s("hi");
    EXPECT_FALSE(s.ends_with("hello"));
}

// ============================================================
// compare
// ============================================================

TEST(StringTest, CompareEqual) {
    string s1("hello");
    string s2("hello");
    EXPECT_EQ(s1.compare(s2), 0);
}

TEST(StringTest, CompareLess) {
    string s1("abc");
    string s2("abd");
    EXPECT_LT(s1.compare(s2), 0);
}

TEST(StringTest, CompareGreater) {
    string s1("xyz");
    string s2("abc");
    EXPECT_GT(s1.compare(s2), 0);
}

TEST(StringTest, CompareShorterString) {
    string s1("ab");
    string s2("abc");
    EXPECT_LT(s1.compare(s2), 0);  // shorter is less when prefix matches
    EXPECT_GT(s2.compare(s1), 0);
}

TEST(StringTest, CompareCStr) {
    string s("hello");
    EXPECT_EQ(s.compare("hello"), 0);
    EXPECT_LT(s.compare("abc"), 0);   // "hello" > "abc"
}

TEST(StringTest, CompareWithPos) {
    string s("hello world");
    EXPECT_EQ(s.compare(6, 5, string("world")), 0);
    EXPECT_EQ(s.compare(0, 5, string("hello")), 0);
}

// ============================================================
// Comparison operators
// ============================================================

TEST(StringTest, OperatorEquals) {
    string a("hello");
    string b("hello");
    string c("world");
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a == "hello");
    EXPECT_TRUE("hello" == a);
}

TEST(StringTest, OperatorNotEquals) {
    string a("hello");
    string b("world");
    EXPECT_TRUE(a != b);
    EXPECT_FALSE(a != string("hello"));
    EXPECT_TRUE(a != "world");
    EXPECT_TRUE("world" != a);
}

TEST(StringTest, OperatorLess) {
    string a("abc");
    string b("abd");
    string c("aaa");
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
    EXPECT_TRUE(c < a);
    EXPECT_TRUE(a < "abd");
}

TEST(StringTest, OperatorGreater) {
    string a("xyz");
    string b("abc");
    EXPECT_TRUE(a > b);
    EXPECT_FALSE(b > a);
}

TEST(StringTest, OperatorLessEqual) {
    string a("abc");
    string b("abc");
    string c("abd");
    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(a <= c);
    EXPECT_FALSE(c <= a);
}

TEST(StringTest, OperatorGreaterEqual) {
    string a("xyz");
    string b("abc");
    string c("xyz");
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(a >= c);
    EXPECT_FALSE(b >= a);
}

// ============================================================
// operator+ (concatenation)
// ============================================================

TEST(StringTest, OperatorPlusStringString) {
    string a("hello ");
    string b("world");
    string c = a + b;
    EXPECT_STREQ(c.c_str(), "hello world");
    // Originals unchanged
    EXPECT_STREQ(a.c_str(), "hello ");
    EXPECT_STREQ(b.c_str(), "world");
}

TEST(StringTest, OperatorPlusStringCStr) {
    string a("hello ");
    string c = a + "world";
    EXPECT_STREQ(c.c_str(), "hello world");
}

TEST(StringTest, OperatorPlusCStrString) {
    string b(" world");
    string c = "hello" + b;
    EXPECT_STREQ(c.c_str(), "hello world");
}

TEST(StringTest, OperatorPlusStringChar) {
    string a("test");
    string c = a + 's';
    EXPECT_STREQ(c.c_str(), "tests");
}

TEST(StringTest, OperatorPlusCharString) {
    string b("ello");
    string c = 'h' + b;
    EXPECT_STREQ(c.c_str(), "hello");
}

TEST(StringTest, OperatorPlusMoveStringString) {
    string a("hello ");
    string b("world");
    string c = std::move(a) + b;
    EXPECT_STREQ(c.c_str(), "hello world");
}

TEST(StringTest, OperatorPlusMoveStringCStr) {
    string a("hello ");
    string c = std::move(a) + "world";
    EXPECT_STREQ(c.c_str(), "hello world");
}

TEST(StringTest, OperatorPlusMoveStringChar) {
    string a("test");
    string c = std::move(a) + 's';
    EXPECT_STREQ(c.c_str(), "tests");
}

// ============================================================
// SSO boundary tests
// ============================================================

TEST(StringTest, SSOSize0) {
    string s;
    EXPECT_EQ(s.size(), 0u);
    EXPECT_LE(s.capacity(), 15u);
    EXPECT_STREQ(s.c_str(), "");
}

TEST(StringTest, SSOSize1) {
    string s("a");
    EXPECT_EQ(s.size(), 1u);
    EXPECT_EQ(s[0], 'a');
    EXPECT_EQ(s[1], '\0');
    EXPECT_STREQ(s.c_str(), "a");
}

TEST(StringTest, SSOSize5) {
    string s("hello");
    EXPECT_EQ(s.size(), 5u);
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(StringTest, SSOSize10) {
    string s("0123456789");
    EXPECT_EQ(s.size(), 10u);
    EXPECT_STREQ(s.c_str(), "0123456789");
    EXPECT_EQ(s[9], '9');
    EXPECT_EQ(s[10], '\0');
}

TEST(StringTest, SSOSize15_SSOLimit) {
    // 15 chars = SSO capacity limit
    string s("123456789012345");
    EXPECT_EQ(s.size(), 15u);
    EXPECT_EQ(s.capacity(), 15u);  // still in SSO
    EXPECT_STREQ(s.c_str(), "123456789012345");
    EXPECT_EQ(s[15], '\0');
}

TEST(StringTest, SSOTransitionToHeapSize16) {
    // 16 chars exceeds SSO, triggers heap allocation
    string s("1234567890123456");
    EXPECT_EQ(s.size(), 16u);
    EXPECT_GT(s.capacity(), 15u);  // now on heap (cap > SSO limit)
    EXPECT_STREQ(s.c_str(), "1234567890123456");
    EXPECT_EQ(s[16], '\0');
}

TEST(StringTest, HeapSize20) {
    string s("12345678901234567890");
    EXPECT_EQ(s.size(), 20u);
    EXPECT_GT(s.capacity(), 15u);
    EXPECT_STREQ(s.c_str(), "12345678901234567890");
}

TEST(StringTest, HeapSize50) {
    std::string std_s(50, 'x');
    string s(std_s.c_str());
    EXPECT_EQ(s.size(), 50u);
    EXPECT_GT(s.capacity(), 15u);
    for (size_t i = 0; i < 50; ++i) {
        EXPECT_EQ(s[i], 'x');
    }
}

TEST(StringTest, HeapSize100) {
    std::string std_s(100, 'z');
    string s(std_s.c_str());
    EXPECT_EQ(s.size(), 100u);
    EXPECT_GT(s.capacity(), 15u);
    EXPECT_EQ(s[99], 'z');
    EXPECT_EQ(s[100], '\0');
}

TEST(StringTest, SSOBackToSSOShrink) {
    string s;
    s.reserve(50);
    ASSERT_GT(s.capacity(), 15u);
    s = "short";  // 5 chars, should fit in SSO
    EXPECT_STREQ(s.c_str(), "short");
    s.shrink_to_fit();
    EXPECT_LE(s.capacity(), 15u);  // should move back to SSO
}

TEST(StringTest, SSOCopyPreservesContent) {
    string s("hello");
    string t = s;  // copy SSO string
    EXPECT_STREQ(t.c_str(), "hello");
    s[0] = 'H';
    EXPECT_EQ(t[0], 'h');  // deep copy
}

TEST(StringTest, HeapCopyPreservesContent) {
    string s("this is a longer string on the heap");
    ASSERT_GT(s.capacity(), 15u);
    string t = s;  // copy heap string
    EXPECT_STREQ(t.c_str(), s.c_str());
    s[0] = 'X';
    EXPECT_EQ(t[0], 't');  // deep copy
}

TEST(StringTest, SSOMoveLeavesSourceEmpty) {
    string s("hello");
    string t = std::move(s);
    EXPECT_STREQ(t.c_str(), "hello");
    EXPECT_TRUE(s.empty());
}

TEST(StringTest, HeapMoveLeavesSourceEmpty) {
    string s("this is a longer string on the heap");
    ASSERT_GT(s.capacity(), 15u);
    string t = std::move(s);
    EXPECT_STREQ(t.c_str(), "this is a longer string on the heap");
    EXPECT_TRUE(s.empty());
}

TEST(StringTest, AppendAcrossSSOBoundary) {
    string s("start");  // 5 chars, SSO
    s.append(20, 'x');  // push past SSO limit
    EXPECT_EQ(s.size(), 25u);
    EXPECT_GT(s.capacity(), 15u);
    EXPECT_STREQ(s.c_str() + 5, "xxxxxxxxxxxxxxxxxxxx");
}

// ============================================================
// to_string
// ============================================================

TEST(StringTest, ToStringInt) {
    string s = to_string(42);
    EXPECT_STREQ(s.c_str(), "42");
    string s2 = to_string(-123);
    EXPECT_STREQ(s2.c_str(), "-123");
    string s3 = to_string(0);
    EXPECT_STREQ(s3.c_str(), "0");
}

TEST(StringTest, ToStringLong) {
    string s = to_string(123456789L);
    EXPECT_STREQ(s.c_str(), "123456789");
    string s2 = to_string(-999L);
    EXPECT_STREQ(s2.c_str(), "-999");
}

TEST(StringTest, ToStringUnsigned) {
    string s = to_string(42u);
    EXPECT_STREQ(s.c_str(), "42");
    string s2 = to_string(0u);
    EXPECT_STREQ(s2.c_str(), "0");
}

TEST(StringTest, ToStringFloat) {
    string s = to_string(3.14f);
    EXPECT_FALSE(s.empty());
    // Just verify it produces something non-empty and parsable
    EXPECT_GE(s.size(), 3u);
}

TEST(StringTest, ToStringDouble) {
    string s = to_string(2.718281828);
    EXPECT_FALSE(s.empty());
    EXPECT_GE(s.size(), 3u);
}

// ============================================================
// stoi / stol / stof / stod
// ============================================================

TEST(StringTest, StoiValid) {
    EXPECT_EQ(stoi(string("42")), 42);
    EXPECT_EQ(stoi(string("-100")), -100);
    EXPECT_EQ(stoi(string("0")), 0);
    EXPECT_EQ(stoi(string("  99")), 99);  // leading whitespace
}

TEST(StringTest, StoiInvalid) {
    EXPECT_EQ(stoi(string("abc")), 0);  // returns 0 for non-numeric
    EXPECT_EQ(stoi(string("")), 0);
}

TEST(StringTest, StoiWithIdx) {
    size_t idx;
    stoi(string("42abc"), &idx);
    EXPECT_EQ(idx, 2u);  // parsed 2 chars
}

TEST(StringTest, StolValid) {
    EXPECT_EQ(stol(string("123456")), 123456L);
    EXPECT_EQ(stol(string("-789")), -789L);
}

TEST(StringTest, StolInvalid) {
    EXPECT_EQ(stol(string("not a number")), 0L);
}

TEST(StringTest, StofValid) {
    float f = stof(string("3.14"));
    EXPECT_NEAR(f, 3.14f, 0.01f);
}

TEST(StringTest, StofInvalid) {
    float f = stof(string("abc"));
    EXPECT_EQ(f, 0.0f);
}

TEST(StringTest, StodValid) {
    double d = stod(string("2.718281828"));
    EXPECT_NEAR(d, 2.718281828, 0.0001);
}

TEST(StringTest, StodInvalid) {
    double d = stod(string("not a double"));
    EXPECT_EQ(d, 0.0);
}

// ============================================================
// to_int / to_long / to_float / to_double (member conversion)
// ============================================================

TEST(StringTest, ToInt) {
    string s("42");
    EXPECT_EQ(s.to_int(), 42);
    string s2("-99");
    EXPECT_EQ(s2.to_int(), -99);
    string s3("abc");
    EXPECT_EQ(s3.to_int(), 0);
}

TEST(StringTest, ToLong) {
    string s("12345678");
    EXPECT_EQ(s.to_long(), 12345678L);
}

TEST(StringTest, ToFloat) {
    string s("3.14");
    EXPECT_NEAR(s.to_float(), 3.14f, 0.01f);
}

TEST(StringTest, ToDouble) {
    string s("2.718281828");
    EXPECT_NEAR(s.to_double(), 2.718281828, 0.0001);
}

// ============================================================
// Edge cases with null terminators and embedded zeros
// ============================================================

TEST(StringTest, StringWithEmbeddedNull) {
    // basic_string(const CharT* s, size_type count) constructs with explicit length
    const char data[] = {'h', 'e', '\0', 'l', 'o'};
    string s(data, 5);
    EXPECT_EQ(s.size(), 5u);
    EXPECT_EQ(s[0], 'h');
    EXPECT_EQ(s[2], '\0');
    EXPECT_EQ(s[4], 'o');
}

TEST(StringTest, VeryLongString) {
    std::string std_s(1000, 'x');
    string s(std_s.c_str());
    EXPECT_EQ(s.size(), 1000u);
    for (size_t i = 0; i < 1000; ++i) {
        EXPECT_EQ(s[i], 'x');
    }
}

// ============================================================
// npos constant
// ============================================================

TEST(StringTest, NposValue) {
    EXPECT_EQ(string::npos, static_cast<size_t>(-1));
}
