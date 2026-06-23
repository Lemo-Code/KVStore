// ============================================================================
// zstl bitset tests — fixed-size bitmap with bitwise and shift operations
// ============================================================================
// Tests for: bitset<N> for N=0, N=8, N=64, N=100, N=200
// Covers: constructors, set/reset/flip/test, count/size/all/any/none,
//         to_ulong/to_ullong/to_string, bitwise & | ^ ~, shift << >>,
//         comparison == !=, reference proxy, out-of-range exceptions,
//         multi-word operations, edge bits
// ============================================================================

#include <gtest/gtest.h>
#include "zstl/zstl.h"

#include <string>
#include <stdexcept>

using namespace zstl;

// ============================================================================
// Construction
// ============================================================================

TEST(Bitset, DefaultConstructorAllZero) {
  bitset<8> bs;
  EXPECT_EQ(bs.count(), 0u);
  EXPECT_EQ(bs.size(), 8u);
  EXPECT_TRUE(bs.none());
  EXPECT_FALSE(bs.any());
  EXPECT_FALSE(bs.all());
}

TEST(Bitset, DefaultConstructorSmallN) {
  bitset<1> bs;
  EXPECT_EQ(bs.count(), 0u);
  EXPECT_EQ(bs.size(), 1u);
  EXPECT_TRUE(bs.none());
}

TEST(Bitset, DefaultConstructorLargeN) {
  bitset<200> bs;
  EXPECT_EQ(bs.count(), 0u);
  EXPECT_EQ(bs.size(), 200u);
  EXPECT_TRUE(bs.none());
  EXPECT_FALSE(bs.any());
}

TEST(Bitset, DefaultConstructorN0) {
  bitset<0> bs;
  EXPECT_EQ(bs.count(), 0u);
  EXPECT_EQ(bs.size(), 0u);
  EXPECT_TRUE(bs.none());
  EXPECT_TRUE(bs.all());
}

TEST(Bitset, ULLConstructorZero) {
  bitset<8> bs(0ULL);
  EXPECT_EQ(bs.count(), 0u);
  EXPECT_TRUE(bs.none());
}

TEST(Bitset, ULLConstructorSmallValue) {
  bitset<8> bs(5ULL);  // binary 00000101
  EXPECT_EQ(bs.count(), 2u);
  EXPECT_TRUE(bs.test(0));
  EXPECT_FALSE(bs.test(1));
  EXPECT_TRUE(bs.test(2));
  EXPECT_FALSE(bs.test(3));
  EXPECT_FALSE(bs.test(4));
  EXPECT_FALSE(bs.test(5));
  EXPECT_FALSE(bs.test(6));
  EXPECT_FALSE(bs.test(7));
}

TEST(Bitset, ULLConstructorMaxULL) {
  bitset<64> bs(~0ULL);
  EXPECT_EQ(bs.count(), 64u);
  EXPECT_TRUE(bs.all());
  EXPECT_FALSE(bs.none());
}

TEST(Bitset, ULLConstructorBeyondN) {
  // bitset<4> from 0xFF — only low 4 bits stored
  bitset<4> bs(0xFFULL);
  EXPECT_EQ(bs.count(), 4u);
  EXPECT_TRUE(bs.all());
}

TEST(Bitset, ULLConstructorLargeNOnlyLowBits) {
  bitset<100> bs(0x0FULL);  // only bits 0-3 set
  EXPECT_EQ(bs.count(), 4u);
  for (size_t i = 0; i < 4; ++i) EXPECT_TRUE(bs.test(i));
  for (size_t i = 4; i < 100; ++i) EXPECT_FALSE(bs.test(i));
}

// ============================================================================
// operator[] — non-const proxy reference and const bool
// ============================================================================

TEST(Bitset, OperatorBracketNonConstReadWrite) {
  bitset<10> bs;
  EXPECT_FALSE(bs[0]);
  EXPECT_FALSE(bs[5]);
  EXPECT_FALSE(bs[9]);

  bs[0] = true;
  bs[5] = true;
  bs[9] = true;

  EXPECT_TRUE(bs[0]);
  EXPECT_FALSE(bs[1]);
  EXPECT_TRUE(bs[5]);
  EXPECT_FALSE(bs[6]);
  EXPECT_TRUE(bs[9]);
  EXPECT_EQ(bs.count(), 3u);
}

TEST(Bitset, OperatorBracketNonConstAssignFalse) {
  bitset<8> bs(0xFFULL);
  bs[3] = false;
  bs[7] = false;
  EXPECT_FALSE(bs[3]);
  EXPECT_FALSE(bs[7]);
  EXPECT_EQ(bs.count(), 6u);
}

TEST(Bitset, OperatorBracketConstBool) {
  const bitset<8> bs(0xAAULL);  // 10101010
  EXPECT_TRUE(bs[1]);
  EXPECT_FALSE(bs[0]);
  EXPECT_TRUE(bs[3]);
  EXPECT_FALSE(bs[2]);
  EXPECT_TRUE(bs[5]);
  EXPECT_FALSE(bs[4]);
  EXPECT_TRUE(bs[7]);
  EXPECT_FALSE(bs[6]);
}

TEST(Bitset, ProxyFlip) {
  bitset<8> bs;
  bs[3] = true;
  EXPECT_TRUE(bs[3]);
  bs[3].flip();
  EXPECT_FALSE(bs[3]);
  bs[3].flip();
  EXPECT_TRUE(bs[3]);
}

TEST(Bitset, ProxyOperatorNot) {
  bitset<8> bs;
  bs[3] = true;
  EXPECT_FALSE(~bs[3]);
  EXPECT_TRUE(~bs[5]);
}

TEST(Bitset, ProxyAssignFromOtherProxy) {
  bitset<8> a;
  bitset<8> b;
  a[2] = true;
  b[5] = a[2];
  EXPECT_TRUE(b[5]);
  EXPECT_FALSE(b[2]);
}

// ============================================================================
// set()
// ============================================================================

TEST(Bitset, SetAll) {
  bitset<8> bs;
  bs.set();
  EXPECT_EQ(bs.count(), 8u);
  EXPECT_TRUE(bs.all());
  for (size_t i = 0; i < 8; ++i) EXPECT_TRUE(bs[i]);
}

TEST(Bitset, SetAllLarge) {
  bitset<200> bs;
  bs.set();
  EXPECT_EQ(bs.count(), 200u);
  EXPECT_TRUE(bs.all());
}

TEST(Bitset, SetAllThenSetAllAgain) {
  bitset<16> bs;
  bs.set();
  bs.set();  // idempotent
  EXPECT_EQ(bs.count(), 16u);
  EXPECT_TRUE(bs.all());
}

TEST(Bitset, SetSpecificBitToTrue) {
  bitset<16> bs;
  bs.set(7);
  EXPECT_TRUE(bs[7]);
  EXPECT_EQ(bs.count(), 1u);
}

TEST(Bitset, SetSpecificBitToFalse) {
  bitset<8> bs(0xFFULL);
  bs.set(3, false);
  EXPECT_FALSE(bs[3]);
  EXPECT_EQ(bs.count(), 7u);
}

TEST(Bitset, SetBit0) {
  bitset<16> bs;
  bs.set(0);
  EXPECT_TRUE(bs[0]);
  EXPECT_EQ(bs.count(), 1u);
}

TEST(Bitset, SetBitNMinus1) {
  bitset<16> bs;
  bs.set(15);
  EXPECT_TRUE(bs[15]);
  EXPECT_EQ(bs.count(), 1u);
}

TEST(Bitset, SetOutOfRangeThrows) {
  bitset<8> bs;
  EXPECT_THROW(bs.set(8), std::out_of_range);
  EXPECT_THROW(bs.set(100), std::out_of_range);
}

TEST(Bitset, SetMultiWordBit) {
  bitset<100> bs;
  bs.set(70);
  EXPECT_TRUE(bs[70]);
  EXPECT_EQ(bs.count(), 1u);
  // Verify other words not affected
  EXPECT_FALSE(bs[0]);
  EXPECT_FALSE(bs[10]);
}

// ============================================================================
// reset()
// ============================================================================

TEST(Bitset, ResetAll) {
  bitset<8> bs(0xFFULL);
  bs.reset();
  EXPECT_EQ(bs.count(), 0u);
  EXPECT_TRUE(bs.none());
}

TEST(Bitset, ResetAllLarge) {
  bitset<200> bs;
  bs.set();
  EXPECT_TRUE(bs.all());
  bs.reset();
  EXPECT_TRUE(bs.none());
  EXPECT_EQ(bs.count(), 0u);
}

TEST(Bitset, ResetSpecificBit) {
  bitset<8> bs(0xFFULL);
  bs.reset(3);
  EXPECT_FALSE(bs[3]);
  EXPECT_EQ(bs.count(), 7u);
  // Other bits unchanged
  for (size_t i = 0; i < 8; ++i) {
    if (i != 3) EXPECT_TRUE(bs[i]);
  }
}

TEST(Bitset, ResetBit0) {
  bitset<8> bs(0xFFULL);
  bs.reset(0);
  EXPECT_FALSE(bs[0]);
  EXPECT_EQ(bs.count(), 7u);
}

TEST(Bitset, ResetBitNMinus1) {
  bitset<16> bs(0xFFFFULL);
  bs.reset(15);
  EXPECT_FALSE(bs[15]);
  EXPECT_EQ(bs.count(), 15u);
}

TEST(Bitset, ResetOutOfRangeThrows) {
  bitset<8> bs;
  EXPECT_THROW(bs.reset(8), std::out_of_range);
  EXPECT_THROW(bs.reset(999), std::out_of_range);
}

TEST(Bitset, ResetMultiWordBit) {
  bitset<100> bs;
  bs.set();
  bs.reset(80);
  EXPECT_FALSE(bs[80]);
  EXPECT_EQ(bs.count(), 99u);
}

// ============================================================================
// flip()
// ============================================================================

TEST(Bitset, FlipAllZeroToAll) {
  bitset<8> bs;
  bs.flip();
  EXPECT_EQ(bs.count(), 8u);
  EXPECT_TRUE(bs.all());
}

TEST(Bitset, FlipAllToZero) {
  bitset<8> bs(0xFFULL);
  bs.flip();
  EXPECT_EQ(bs.count(), 0u);
  EXPECT_TRUE(bs.none());
}

TEST(Bitset, FlipSpecificBit) {
  bitset<8> bs;
  bs.flip(3);
  EXPECT_TRUE(bs[3]);
  EXPECT_EQ(bs.count(), 1u);

  bs.flip(3);
  EXPECT_FALSE(bs[3]);
  EXPECT_EQ(bs.count(), 0u);
}

TEST(Bitset, FlipBit0) {
  bitset<8> bs;
  bs.flip(0);
  EXPECT_TRUE(bs[0]);
  bs.flip(0);
  EXPECT_FALSE(bs[0]);
}

TEST(Bitset, FlipBitNMinus1) {
  bitset<16> bs;
  bs.flip(15);
  EXPECT_TRUE(bs[15]);
  bs.flip(15);
  EXPECT_FALSE(bs[15]);
}

TEST(Bitset, FlipOutOfRangeThrows) {
  bitset<8> bs;
  EXPECT_THROW(bs.flip(8), std::out_of_range);
  EXPECT_THROW(bs.flip(100), std::out_of_range);
}

TEST(Bitset, FlipLargeMultiWord) {
  bitset<200> bs;
  bs.flip();
  EXPECT_TRUE(bs.all());
  bs.flip();
  EXPECT_TRUE(bs.none());
}

// ============================================================================
// test() with bounds checking
// ============================================================================

TEST(Bitset, TestReturnsBool) {
  bitset<8> bs(0x0AULL);  // bits 1 and 3 set
  EXPECT_TRUE(bs.test(1));
  EXPECT_TRUE(bs.test(3));
  EXPECT_FALSE(bs.test(0));
  EXPECT_FALSE(bs.test(2));
  EXPECT_FALSE(bs.test(7));
}

TEST(Bitset, TestAfterSetAndReset) {
  bitset<16> bs;
  bs.set(5);
  EXPECT_TRUE(bs.test(5));
  bs.reset(5);
  EXPECT_FALSE(bs.test(5));
}

TEST(Bitset, TestOutOfRangeThrows) {
  bitset<8> bs;
  EXPECT_THROW(bs.test(8), std::out_of_range);
  EXPECT_THROW(bs.test(200), std::out_of_range);
}

TEST(Bitset, TestMultiWord) {
  bitset<100> bs;
  bs.set(75);
  EXPECT_TRUE(bs.test(75));
  EXPECT_FALSE(bs.test(74));
  EXPECT_FALSE(bs.test(76));
}

// ============================================================================
// count(), size(), all(), any(), none()
// ============================================================================

TEST(Bitset, CountAfterVariousOps) {
  bitset<64> bs;
  EXPECT_EQ(bs.count(), 0u);

  bs.set(3);
  bs.set(7);
  bs.set(60);
  EXPECT_EQ(bs.count(), 3u);

  bs.set(7, false);
  EXPECT_EQ(bs.count(), 2u);

  bs.flip();
  EXPECT_EQ(bs.count(), 62u);
}

TEST(Bitset, SizeDifferentN) {
  EXPECT_EQ((bitset<0>().size()), 0u);
  EXPECT_EQ((bitset<1>().size()), 1u);
  EXPECT_EQ((bitset<8>().size()), 8u);
  EXPECT_EQ((bitset<63>().size()), 63u);
  EXPECT_EQ((bitset<64>().size()), 64u);
  EXPECT_EQ((bitset<65>().size()), 65u);
  EXPECT_EQ((bitset<128>().size()), 128u);
  EXPECT_EQ((bitset<1000>().size()), 1000u);
}

TEST(Bitset, AllAnyNoneEmpty) {
  bitset<8> bs;
  EXPECT_TRUE(bs.none());
  EXPECT_FALSE(bs.any());
  EXPECT_FALSE(bs.all());

  bs[0] = true;
  EXPECT_FALSE(bs.none());
  EXPECT_TRUE(bs.any());
  EXPECT_FALSE(bs.all());

  bs.set();
  EXPECT_FALSE(bs.none());
  EXPECT_TRUE(bs.any());
  EXPECT_TRUE(bs.all());
}

TEST(Bitset, AllAnyNoneMultiWord) {
  bitset<130> bs;
  EXPECT_TRUE(bs.none());
  EXPECT_FALSE(bs.any());
  EXPECT_FALSE(bs.all());

  bs.set(100);
  EXPECT_FALSE(bs.none());
  EXPECT_TRUE(bs.any());
  EXPECT_FALSE(bs.all());

  bs.set();
  EXPECT_FALSE(bs.none());
  EXPECT_TRUE(bs.any());
  EXPECT_TRUE(bs.all());
}

// ============================================================================
// to_ulong() / to_ullong()
// ============================================================================

TEST(Bitset, ToUlongSmallValue) {
  bitset<8> bs(42ULL);
  EXPECT_EQ(bs.to_ulong(), 42UL);
  EXPECT_EQ(bs.to_ullong(), 42ULL);
}

TEST(Bitset, ToUlongMax8Bit) {
  bitset<8> bs(0xFFULL);
  EXPECT_EQ(bs.to_ulong(), 255UL);
  EXPECT_EQ(bs.to_ullong(), 255ULL);
}

TEST(Bitset, ToUlongOverflowThrows) {
  bitset<100> bs;
  bs.set(70);  // bit beyond unsigned long range
  EXPECT_THROW(bs.to_ulong(), std::overflow_error);
  EXPECT_THROW(bs.to_ullong(), std::overflow_error);
}

TEST(Bitset, ToUllongSingleWordSafe) {
  bitset<64> bs(0xDEADBEEFULL);
  EXPECT_EQ(bs.to_ullong(), 0xDEADBEEFULL);
}

TEST(Bitset, ToUllongMultiWordOverflow) {
  bitset<100> bs;
  bs.set(65);  // bit 65 requires word 1
  EXPECT_THROW(bs.to_ullong(), std::overflow_error);
}

// ============================================================================
// to_string()
// ============================================================================

TEST(Bitset, ToStringDefault) {
  bitset<4> bs(0x05ULL);  // binary 0101
  // MSB first: bit 3='0', bit 2='1', bit 1='0', bit 0='1'
  EXPECT_EQ(bs.to_string(), "0101");
}

TEST(Bitset, ToStringAllZero) {
  bitset<8> bs;
  EXPECT_EQ(bs.to_string(), "00000000");
}

TEST(Bitset, ToStringAllOne) {
  bitset<8> bs(0xFFULL);
  EXPECT_EQ(bs.to_string(), "11111111");
}

TEST(Bitset, ToStringCustomChars) {
  bitset<4> bs(0x06ULL);  // 0110
  EXPECT_EQ(bs.to_string('F', 'T'), "FTTF");
}

TEST(Bitset, ToStringCustomCharsYesNo) {
  bitset<3> bs(0x05ULL);  // 101
  EXPECT_EQ(bs.to_string('N', 'Y'), "YNY");
}

TEST(Bitset, ToStringLargeMultiWord) {
  bitset<130> bs;
  bs.set(0);
  bs.set(129);
  string s = bs.to_string();
  EXPECT_EQ(s.size(), 130u);
  // First char is MSB (bit 129)
  EXPECT_EQ(s[0], '1');
  for (size_t i = 1; i < 129; ++i) EXPECT_EQ(s[i], '0');
  // Last char is LSB (bit 0)
  EXPECT_EQ(s[129], '1');
}

TEST(Bitset, ToStringEmpty) {
  bitset<0> bs;
  EXPECT_EQ(bs.to_string(), "");
}

// ============================================================================
// Bitwise operators — member
// ============================================================================

TEST(Bitset, AndEquals) {
  bitset<8> a(0xF0ULL);  // 11110000
  bitset<8> b(0xCCULL);  // 11001100
  a &= b;                 // 11000000
  EXPECT_EQ(a.to_ulong(), 0xC0UL);
}

TEST(Bitset, OrEquals) {
  bitset<8> a(0xF0ULL);  // 11110000
  bitset<8> b(0xCCULL);  // 11001100
  a |= b;                 // 11111100
  EXPECT_EQ(a.to_ulong(), 0xFCUL);
}

TEST(Bitset, XorEquals) {
  bitset<8> a(0xF0ULL);  // 11110000
  bitset<8> b(0xCCULL);  // 11001100
  a ^= b;                 // 00111100
  EXPECT_EQ(a.to_ulong(), 0x3CUL);
}

TEST(Bitset, NotOperator) {
  bitset<8> a(0xF0ULL);  // 11110000
  bitset<8> b = ~a;      // 00001111
  EXPECT_EQ(b.to_ulong(), 0x0FUL);
  // Original unchanged
  EXPECT_EQ(a.to_ulong(), 0xF0UL);
}

TEST(Bitset, BitwiseMultiWord) {
  bitset<100> a;
  bitset<100> b;
  a.set(10);
  a.set(80);
  b.set(10);
  b.set(45);

  bitset<100> and_result = a & b;
  EXPECT_TRUE(and_result.test(10));
  EXPECT_FALSE(and_result.test(80));
  EXPECT_FALSE(and_result.test(45));

  bitset<100> or_result = a | b;
  EXPECT_TRUE(or_result.test(10));
  EXPECT_TRUE(or_result.test(80));
  EXPECT_TRUE(or_result.test(45));

  bitset<100> xor_result = a ^ b;
  EXPECT_FALSE(xor_result.test(10));  // both set, xor clears
  EXPECT_TRUE(xor_result.test(80));
  EXPECT_TRUE(xor_result.test(45));
}

// ============================================================================
// Bitwise operators — non-member & | ^
// ============================================================================

TEST(Bitset, NonMemberAnd) {
  bitset<8> a(0x0FULL);  // 00001111
  bitset<8> b(0xF0ULL);  // 11110000
  bitset<8> c = a & b;
  EXPECT_EQ(c.to_ulong(), 0x00UL);
}

TEST(Bitset, NonMemberOr) {
  bitset<8> a(0x0FULL);
  bitset<8> b(0xF0ULL);
  bitset<8> c = a | b;
  EXPECT_EQ(c.to_ulong(), 0xFFUL);
}

TEST(Bitset, NonMemberXor) {
  bitset<8> a(0x0FULL);
  bitset<8> b(0xF0ULL);
  bitset<8> c = a ^ b;
  EXPECT_EQ(c.to_ulong(), 0xFFUL);
}

TEST(Bitset, NonMemberOperatorsDoNotModifySources) {
  bitset<8> a(0x55ULL);
  bitset<8> b(0x33ULL);
  bitset<8> r = a & b;
  EXPECT_EQ(a.to_ulong(), 0x55UL);  // unchanged
  EXPECT_EQ(b.to_ulong(), 0x33UL);  // unchanged
  EXPECT_EQ(r.to_ulong(), 0x11UL);
}

// ============================================================================
// Shift operations — member and non-member
// ============================================================================

TEST(Bitset, ShiftLeftMember) {
  bitset<8> bs(0x01ULL);  // 00000001
  bs <<= 3;               // 00001000
  EXPECT_EQ(bs.to_ulong(), 0x08UL);
}

TEST(Bitset, ShiftLeftNonMember) {
  bitset<8> bs(0x07ULL);  // 00000111
  bitset<8> shifted = bs << 2;
  EXPECT_EQ(shifted.to_ulong(), 0x1CUL);  // 00011100
  EXPECT_EQ(bs.to_ulong(), 0x07UL);       // unchanged
}

TEST(Bitset, ShiftLeftByZero) {
  bitset<8> bs(0xFFULL);
  bs <<= 0;
  EXPECT_EQ(bs.to_ulong(), 0xFFUL);
  EXPECT_TRUE(bs.all());
}

TEST(Bitset, ShiftLeftBeyondSize) {
  bitset<8> bs(0xFFULL);
  bs <<= 10;  // >= N, all zero
  EXPECT_EQ(bs.to_ulong(), 0x00UL);
  EXPECT_TRUE(bs.none());
}

TEST(Bitset, ShiftLeftExactSize) {
  bitset<8> bs(0xFFULL);
  bitset<8> shifted = bs << 8;
  EXPECT_TRUE(shifted.none());
}

TEST(Bitset, ShiftRightMember) {
  bitset<8> bs(0x80ULL);  // 10000000
  bs >>= 4;               // 00001000
  EXPECT_EQ(bs.to_ulong(), 0x08UL);
}

TEST(Bitset, ShiftRightNonMember) {
  bitset<8> bs(0xF0ULL);   // 11110000
  bitset<8> shifted = bs >> 3;
  EXPECT_EQ(shifted.to_ulong(), 0x1EUL);  // 00011110
  EXPECT_EQ(bs.to_ulong(), 0xF0UL);       // unchanged
}

TEST(Bitset, ShiftRightByZero) {
  bitset<8> bs(0xFFULL);
  bitset<8> shifted = bs >> 0;
  EXPECT_EQ(shifted.to_ulong(), 0xFFUL);
}

TEST(Bitset, ShiftRightBeyondSize) {
  bitset<8> bs(0xFFULL);
  bs >>= 20;
  EXPECT_TRUE(bs.none());
}

TEST(Bitset, ShiftMultiWord) {
  bitset<130> bs;
  bs.set(0);
  bs.set(100);
  bs.set(129);

  // Left shift
  bitset<130> left = bs << 20;
  EXPECT_TRUE(left.test(20));
  EXPECT_TRUE(left.test(120));
  EXPECT_FALSE(left.test(129));  // shifted out

  // Right shift
  bitset<130> right = bs >> 20;
  EXPECT_FALSE(right.test(0));     // shifted out
  EXPECT_TRUE(right.test(80));
  EXPECT_TRUE(right.test(109));
}

TEST(Bitset, ShiftLeftBitShuffle) {
  // Test shift with non-multiple-of-64 bit_shift
  bitset<100> bs;
  bs.set(0);
  bs.set(63);
  bs.set(80);
  bs <<= 10;
  EXPECT_TRUE(bs.test(10));
  EXPECT_TRUE(bs.test(73));
  EXPECT_TRUE(bs.test(90));
  EXPECT_EQ(bs.count(), 3u);
}

TEST(Bitset, ShiftRightBitShuffle) {
  bitset<100> bs;
  bs.set(20);
  bs.set(70);
  bs.set(90);
  bs >>= 15;
  EXPECT_TRUE(bs.test(5));
  EXPECT_TRUE(bs.test(55));
  EXPECT_TRUE(bs.test(75));
  EXPECT_EQ(bs.count(), 3u);
}

// ============================================================================
// Comparison operators
// ============================================================================

TEST(Bitset, EqualityTrue) {
  bitset<8> a(0xAAULL);
  bitset<8> b(0xAAULL);
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
}

TEST(Bitset, EqualityFalse) {
  bitset<8> a(0xAAULL);
  bitset<8> b(0x55ULL);
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
}

TEST(Bitset, EqualityDifferentN) {
  // Different sizes are fundamentally different types, so cannot compare.
  // Just verify that same-size bitsets with same values are equal.
  bitset<64> a(12345ULL);
  bitset<64> b(12345ULL);
  EXPECT_TRUE(a == b);
}

TEST(Bitset, EqualityWithMultiWord) {
  bitset<100> a;
  bitset<100> b;
  a.set(10);
  a.set(80);
  b.set(10);
  b.set(80);
  EXPECT_TRUE(a == b);

  b.set(45);
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
}

TEST(Bitset, SelfEquality) {
  bitset<16> bs(0xABCDULL);
  EXPECT_TRUE(bs == bs);
  EXPECT_FALSE(bs != bs);
}

// ============================================================================
// Edge bits: bit 0 and bit N-1
// ============================================================================

TEST(Bitset, EdgeBit0) {
  bitset<16> bs;

  bs.set(0);
  EXPECT_TRUE(bs.test(0));
  EXPECT_EQ(bs.to_ulong(), 1UL);

  bs.reset(0);
  EXPECT_FALSE(bs.test(0));

  bs.flip(0);
  EXPECT_TRUE(bs.test(0));
  bs.flip(0);
  EXPECT_FALSE(bs.test(0));
}

TEST(Bitset, EdgeBitNMinus1) {
  bitset<16> bs;

  bs.set(15);
  EXPECT_TRUE(bs.test(15));
  EXPECT_EQ(bs.to_ulong(), 0x8000UL);

  bs.reset(15);
  EXPECT_FALSE(bs.test(15));

  bs.flip(15);
  EXPECT_TRUE(bs.test(15));
  bs.flip(15);
  EXPECT_FALSE(bs.test(15));
}

TEST(Bitset, EdgeBitNMinus1MultiWord) {
  bitset<100> bs;

  bs.set(99);
  EXPECT_TRUE(bs.test(99));
  EXPECT_EQ(bs.count(), 1u);

  bs.reset(99);
  EXPECT_TRUE(bs.none());

  bs.flip(99);
  EXPECT_TRUE(bs.test(99));
}

// ============================================================================
// Large bitset multi-word operations
// ============================================================================

TEST(Bitset, LargeBitsetSetAndResetAcrossWords) {
  bitset<200> bs;
  // Set bits in word 0, word 1, word 2, word 3
  bs.set(0);
  bs.set(63);
  bs.set(64);
  bs.set(127);
  bs.set(128);
  bs.set(199);
  EXPECT_EQ(bs.count(), 6u);
  EXPECT_TRUE(bs.test(0));
  EXPECT_TRUE(bs.test(63));
  EXPECT_TRUE(bs.test(64));
  EXPECT_TRUE(bs.test(127));
  EXPECT_TRUE(bs.test(128));
  EXPECT_TRUE(bs.test(199));
}

TEST(Bitset, LargeBitsetFlipAndCount) {
  bitset<150> bs;
  bs.flip();
  EXPECT_TRUE(bs.all());
  EXPECT_EQ(bs.count(), 150u);

  // Flip every other bit
  for (size_t i = 0; i < 150; i += 2) {
    bs.flip(i);
  }
  // Half of the bits should be zero now
  EXPECT_EQ(bs.count(), 75u);
}

TEST(Bitset, LargeBitsetSanitizeLastWord) {
  // N=100 means last word has only bits 0-35 meaningful (100 % 64 = 36)
  bitset<100> bs;
  bs.set();
  // Bits 36-63 of the last word should be zero after sanitization
  EXPECT_TRUE(bs.all());
  // Test that sanitize works on flip as well
  bs.flip();
  EXPECT_TRUE(bs.none());
}

TEST(Bitset, LargeBitsetAllCheck) {
  bitset<200> bs;
  EXPECT_FALSE(bs.all());

  bs.set();  // all 200 should be set
  EXPECT_TRUE(bs.all());

  // Clear one bit on each word boundary
  bs.reset(0);
  EXPECT_FALSE(bs.all());
  bs.set(0);

  bs.reset(63);
  EXPECT_FALSE(bs.all());
  bs.set(63);

  bs.reset(64);
  EXPECT_FALSE(bs.all());
}

// ============================================================================
// Stress/consistency tests
// ============================================================================

TEST(Bitset, ChainedOperations) {
  bitset<32> bs;
  bs.set().reset(5).flip(10).set(15, false);
  EXPECT_TRUE(bs.all());
  EXPECT_FALSE(bs[5]);   // reset
  EXPECT_TRUE(bs[10]);   // flipped from 1 to 1? No, set() makes all 1, flip(10) makes it 0. Wait.
  // set() -> all 1s. reset(5) -> bit5=0. flip(10) -> bit10 = 0. set(15,false) -> bit15=0.
  EXPECT_FALSE(bs[10]);
  EXPECT_FALSE(bs[15]);
  EXPECT_EQ(bs.count(), 28u);
}

TEST(Bitset, BackAndForth) {
  bitset<16> bs;
  for (size_t i = 0; i < 16; ++i) {
    bs.set(i);
    EXPECT_TRUE(bs.test(i));
  }
  for (size_t i = 0; i < 16; ++i) {
    bs.reset(i);
    EXPECT_FALSE(bs.test(i));
  }
  EXPECT_TRUE(bs.none());
}

TEST(Bitset, ComplexBitwisePattern) {
  bitset<8> a(0x0FULL);  // 00001111
  bitset<8> b(0x33ULL);  // 00110011
  bitset<8> c(0x55ULL);  // 01010101

  // (a | b) & ~c
  bitset<8> result = (a | b) & ~c;
  EXPECT_EQ(result.to_ulong(), 0x2AUL);  // 00101010
}

TEST(Bitset, ShiftAndBitwiseCombined) {
  bitset<8> bs(0x0FULL);  // 00001111
  bs <<= 2;                // 00111100
  bs |= bitset<8>(0x03UL); // 00111111
  bs >>= 1;                // 00011111 = 0x1F
  EXPECT_EQ(bs.to_ulong(), 0x1FUL);
}
