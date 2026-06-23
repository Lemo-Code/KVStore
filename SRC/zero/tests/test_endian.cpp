// test_endian.cpp — Comprehensive endian conversion unit tests
// Tests byte-order detection, byte swapping, host<->network conversion,
// and little/big-endian load/store with known values and round-trips.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <cstring>
#include <cstdint>

using namespace zero;

// ============================================================
// Compile-time endianness detection
// ============================================================

TEST(EndianDetection, IsLittleOrBigEndian) {
    // Must be exactly one of them
    EXPECT_TRUE(is_little_endian() != is_big_endian());
    // Static assertions
    constexpr bool le = is_little_endian();
    constexpr bool be = is_big_endian();
    EXPECT_TRUE(le || be);
    EXPECT_FALSE(le && be);
}

TEST(EndianDetection, ConsistentResults) {
    bool r1 = is_little_endian();
    bool r2 = is_little_endian();
    EXPECT_EQ(r1, r2);
    r1 = is_big_endian();
    r2 = is_big_endian();
    EXPECT_EQ(r1, r2);
}

// ============================================================
// Byteswap16
// ============================================================

TEST(Byteswap16, Zero) {
    EXPECT_EQ(byteswap16(0x0000), 0x0000);
}

TEST(Byteswap16, Max) {
    EXPECT_EQ(byteswap16(0xFFFF), 0xFFFF);
}

TEST(Byteswap16, KnownValue) {
    EXPECT_EQ(byteswap16(0x1234), 0x3412);
    EXPECT_EQ(byteswap16(0x00FF), 0xFF00);
    EXPECT_EQ(byteswap16(0xFF00), 0x00FF);
    EXPECT_EQ(byteswap16(0x0100), 0x0001);
}

TEST(Byteswap16, SymmetricRoundTrip) {
    // byteswap is self-inverse
    for (uint16_t v : {uint16_t(0), uint16_t(1),
                       uint16_t(0x1234), uint16_t(0xABCD),
                       uint16_t(0xFF00), uint16_t(0x00FF),
                       uint16_t(0xFFFF), uint16_t(0x8000),
                       uint16_t(0x0001)}) {
        EXPECT_EQ(byteswap16(byteswap16(v)), v);
    }
}

TEST(Byteswap16, IdentityValues) {
    // Values that are symmetric: 0x0000, 0xFFFF, 0xAA55->0x55AA, etc. are
    // self-inverse only in double application
    EXPECT_EQ(byteswap16(0xAAAA), 0xAAAA); // Palindromic
    EXPECT_EQ(byteswap16(0x5555), 0x5555);
}

// ============================================================
// Byteswap32
// ============================================================

TEST(Byteswap32, Zero) {
    EXPECT_EQ(byteswap32(0x00000000u), 0x00000000u);
}

TEST(Byteswap32, Max) {
    EXPECT_EQ(byteswap32(0xFFFFFFFFu), 0xFFFFFFFFu);
}

TEST(Byteswap32, KnownValue) {
    EXPECT_EQ(byteswap32(0x12345678u), 0x78563412u);
    EXPECT_EQ(byteswap32(0x000000FFu), 0xFF000000u);
    EXPECT_EQ(byteswap32(0xFF000000u), 0x000000FFu);
    EXPECT_EQ(byteswap32(0x00FF0000u), 0x0000FF00u);
    EXPECT_EQ(byteswap32(0x01020304u), 0x04030201u);
}

TEST(Byteswap32, SymmetricRoundTrip) {
    uint32_t values[] = {0u, 1u, 0x12345678u, 0xDEADBEEFu,
                         0xFFFFFFFFu, 0x80000000u, 0x00000001u,
                         0x00FF00FFu, 0xAA55AA55u};
    for (auto v : values) {
        EXPECT_EQ(byteswap32(byteswap32(v)), v);
    }
}

// ============================================================
// Byteswap64
// ============================================================

TEST(Byteswap64, Zero) {
    EXPECT_EQ(byteswap64(0ULL), 0ULL);
}

TEST(Byteswap64, Max) {
    EXPECT_EQ(byteswap64(~0ULL), ~0ULL);
}

TEST(Byteswap64, KnownValue) {
    EXPECT_EQ(byteswap64(0x0123456789ABCDEFULL), 0xEFCDAB8967452301ULL);
    EXPECT_EQ(byteswap64(0x00000000000000FFULL), 0xFF00000000000000ULL);
    EXPECT_EQ(byteswap64(0xFF00000000000000ULL), 0x00000000000000FFULL);
}

TEST(Byteswap64, SymmetricRoundTrip) {
    uint64_t values[] = {0ULL, 1ULL, 0x0123456789ABCDEFULL,
                         0xDEADBEEFCAFEBABEULL, ~0ULL,
                         0x8000000000000000ULL, 0x0000000000000001ULL};
    for (auto v : values) {
        EXPECT_EQ(byteswap64(byteswap64(v)), v);
    }
}

// ============================================================
// Host-to-Network / Network-to-Host
// ============================================================

TEST(NetworkOrder, HtonNtoh16RoundTrip) {
    uint16_t values[] = {0, 1, 0x1234, 0xABCD, 0xFFFF, 0x8000, 0x00FF};
    for (auto v : values) {
        EXPECT_EQ(ntoh16(hton16(v)), v);
        EXPECT_EQ(hton16(ntoh16(v)), v);
    }
}

TEST(NetworkOrder, HtonNtoh32RoundTrip) {
    uint32_t values[] = {0u, 1u, 0x12345678u, 0xDEADBEEFu,
                         0xFFFFFFFFu, 0x80000000u, 42u};
    for (auto v : values) {
        EXPECT_EQ(ntoh32(hton32(v)), v);
        EXPECT_EQ(hton32(ntoh32(v)), v);
    }
}

TEST(NetworkOrder, HtonNtoh64RoundTrip) {
    uint64_t values[] = {0ULL, 1ULL, 0x0123456789ABCDEFULL,
                         0xDEADBEEFCAFEBABEULL, ~0ULL, 999ULL};
    for (auto v : values) {
        EXPECT_EQ(ntoh64(hton64(v)), v);
        EXPECT_EQ(hton64(ntoh64(v)), v);
    }
}

// ============================================================
// Legacy aliases
// ============================================================

TEST(NetworkOrder, LegacyAliases) {
    EXPECT_EQ(zero::htons(0x1234), zero::hton16(0x1234));
    EXPECT_EQ(zero::htonl(0x12345678u), zero::hton32(0x12345678u));
    EXPECT_EQ(zero::ntohs(0x1234), zero::ntoh16(0x1234));
    EXPECT_EQ(zero::ntohl(0x12345678u), zero::ntoh32(0x12345678u));
}

// ============================================================
// Template hton/ntoh
// ============================================================

TEST(NetworkOrder, TemplateHtonNtoh) {
    EXPECT_EQ(hton<uint16_t>(0xABCD), hton16(0xABCD));
    EXPECT_EQ(hton<uint32_t>(0xDEADBEEFu), hton32(0xDEADBEEFu));
    EXPECT_EQ(hton<uint64_t>(0xCAFEu), hton64(0xCAFEu));
    EXPECT_EQ(hton<uint8_t>(42), 42); // 1-byte: identity

    EXPECT_EQ(ntoh<uint16_t>(hton<uint16_t>(42)), 42u);
    EXPECT_EQ(ntoh<uint32_t>(hton<uint32_t>(100)), 100u);
    EXPECT_EQ(ntoh<uint64_t>(hton<uint64_t>(9999)), 9999u);
}

// ============================================================
// Little-endian load/store
// ============================================================

TEST(LittleEndianLoadStore, LoadLe16Known) {
    uint8_t buf[2] = {0x78, 0x56};
    EXPECT_EQ(load_le16(buf), 0x5678u);
}

TEST(LittleEndianLoadStore, LoadLe32Known) {
    alignas(4) uint8_t buf[4] = {0x78, 0x56, 0x34, 0x12};
    EXPECT_EQ(load_le32(buf), 0x12345678u);
}

TEST(LittleEndianLoadStore, LoadLe64Known) {
    alignas(8) uint8_t buf[8] = {0xEF, 0xBE, 0xAD, 0xDE,
                                  0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(load_le64(buf), 0xDEADBEEFu);
}

TEST(LittleEndianLoadStore, StoreLe16RoundTrip) {
    uint8_t buf[2] = {};
    store_le16(buf, 0xABCD);
    EXPECT_EQ(load_le16(buf), 0xABCDu);
}

TEST(LittleEndianLoadStore, StoreLe32RoundTrip) {
    alignas(4) uint8_t buf[4] = {};
    store_le32(buf, 0xAABBCCDDu);
    EXPECT_EQ(load_le32(buf), 0xAABBCCDDu);
}

TEST(LittleEndianLoadStore, StoreLe64RoundTrip) {
    alignas(8) uint8_t buf[8] = {};
    store_le64(buf, 0x1122334455667788ULL);
    EXPECT_EQ(load_le64(buf), 0x1122334455667788ULL);
}

TEST(LittleEndianLoadStore, LeZero) {
    alignas(8) uint8_t buf[8] = {};
    EXPECT_EQ(load_le16(buf), 0u);
    EXPECT_EQ(load_le32(buf), 0u);
    EXPECT_EQ(load_le64(buf), 0u);
}

TEST(LittleEndianLoadStore, LeMax) {
    alignas(8) uint8_t buf[8] = {};
    store_le16(buf, 0xFFFF);
    EXPECT_EQ(load_le16(buf), 0xFFFFu);
    store_le32(buf, 0xFFFFFFFFu);
    EXPECT_EQ(load_le32(buf), 0xFFFFFFFFu);
    store_le64(buf, UINT64_MAX);
    EXPECT_EQ(load_le64(buf), UINT64_MAX);
}

// ============================================================
// Big-endian load/store
// ============================================================

TEST(BigEndianLoadStore, LoadBe16Known) {
    uint8_t buf[2] = {0x12, 0x34};
    EXPECT_EQ(load_be16(buf), 0x1234u);
}

TEST(BigEndianLoadStore, LoadBe32Known) {
    alignas(4) uint8_t buf[4] = {0x12, 0x34, 0x56, 0x78};
    EXPECT_EQ(load_be32(buf), 0x12345678u);
}

TEST(BigEndianLoadStore, LoadBe64Known) {
    alignas(8) uint8_t buf[8] = {0x00, 0x00, 0x00, 0x00,
                                  0xDE, 0xAD, 0xBE, 0xEF};
    // Big-endian: first bytes are most significant
    EXPECT_EQ(load_be64(buf), 0x00000000DEADBEEFULL);
}

TEST(BigEndianLoadStore, StoreBe16RoundTrip) {
    alignas(2) uint8_t buf[2] = {};
    store_be16(buf, 0xABCD);
    EXPECT_EQ(load_be16(buf), 0xABCDu);
}

TEST(BigEndianLoadStore, StoreBe32RoundTrip) {
    alignas(4) uint8_t buf[4] = {};
    store_be32(buf, 0xAABBCCDDu);
    EXPECT_EQ(load_be32(buf), 0xAABBCCDDu);
}

TEST(BigEndianLoadStore, StoreBe64RoundTrip) {
    alignas(8) uint8_t buf[8] = {};
    store_be64(buf, 0x1122334455667788ULL);
    EXPECT_EQ(load_be64(buf), 0x1122334455667788ULL);
}

TEST(BigEndianLoadStore, BeZero) {
    alignas(8) uint8_t buf[8] = {};
    EXPECT_EQ(load_be16(buf), 0u);
    EXPECT_EQ(load_be32(buf), 0u);
    EXPECT_EQ(load_be64(buf), 0u);
}

// ============================================================
// Signed integer load/store
// ============================================================

TEST(SignedLoadStore, LoadStoreLeI16) {
    alignas(2) uint8_t buf[2] = {};
    store_le_i16(buf, -42);
    EXPECT_EQ(load_le_i16(buf), -42);
    store_le_i16(buf, 0);
    EXPECT_EQ(load_le_i16(buf), 0);
    store_le_i16(buf, 32767);
    EXPECT_EQ(load_le_i16(buf), 32767);
    store_le_i16(buf, -32768);
    EXPECT_EQ(load_le_i16(buf), -32768);
}

TEST(SignedLoadStore, LoadStoreLeI32) {
    alignas(4) uint8_t buf[4] = {};
    store_le_i32(buf, -1000);
    EXPECT_EQ(load_le_i32(buf), -1000);
    store_le_i32(buf, 0);
    EXPECT_EQ(load_le_i32(buf), 0);
    store_le_i32(buf, INT32_MAX);
    EXPECT_EQ(load_le_i32(buf), INT32_MAX);
    store_le_i32(buf, INT32_MIN + 1);
    EXPECT_EQ(load_le_i32(buf), INT32_MIN + 1);
}

TEST(SignedLoadStore, LoadStoreLeI64) {
    alignas(8) uint8_t buf[8] = {};
    store_le_i64(buf, INT64_MIN + 1);
    EXPECT_EQ(load_le_i64(buf), INT64_MIN + 1);
    store_le_i64(buf, INT64_MAX);
    EXPECT_EQ(load_le_i64(buf), INT64_MAX);
    store_le_i64(buf, -1);
    EXPECT_EQ(load_le_i64(buf), -1);
    store_le_i64(buf, 0);
    EXPECT_EQ(load_le_i64(buf), 0);
}

TEST(SignedLoadStore, LoadStoreBeI32) {
    alignas(4) uint8_t buf[4] = {};
    store_be_i32(buf, -100);
    EXPECT_EQ(load_be_i32(buf), -100);
    store_be_i32(buf, 42);
    EXPECT_EQ(load_be_i32(buf), 42);
}

TEST(SignedLoadStore, LoadStoreBeI64) {
    alignas(8) uint8_t buf[8] = {};
    store_be_i64(buf, -9999);
    EXPECT_EQ(load_be_i64(buf), -9999);
    store_be_i64(buf, 0x7FFFFFFFFFFFFFFF);
    EXPECT_EQ(load_be_i64(buf), 0x7FFFFFFFFFFFFFFF);
}

// ============================================================
// Cross-endian consistency: LE store + BE load and vice versa
// ============================================================

TEST(CrossEndian, LeStoreBeLoad16) {
    alignas(2) uint8_t buf[2] = {};
    store_le16(buf, 0x1234);
    // Loading as BE should give the swapped value
    uint16_t be_val = load_be16(buf);
    EXPECT_EQ(be_val, byteswap16(0x1234));
}

TEST(CrossEndian, BeStoreLeLoad32) {
    alignas(4) uint8_t buf[4] = {};
    store_be32(buf, 0x12345678u);
    uint32_t le_val = load_le32(buf);
    EXPECT_EQ(le_val, byteswap32(0x12345678u));
}

TEST(CrossEndian, LeBeRoundTrip) {
    // Store as LE, load as LE, store as BE, load as BE — should recover original
    uint32_t original = 0xDEADBEEFu;
    alignas(4) uint8_t buf[4] = {};
    store_le32(buf, original);
    uint32_t le_read = load_le32(buf);
    EXPECT_EQ(le_read, original);
    // Now take those bytes, interpret as BE, store back as BE
    store_be32(buf, le_read);
    uint32_t be_read = load_be32(buf);
    EXPECT_EQ(be_read, le_read);
}

// ============================================================
// Unaligned access safety
// ============================================================

TEST(UnalignedAccess, LeLoadUnaligned) {
    // Place data at an unaligned offset
    uint8_t raw[9] = {};
    raw[1] = 0x78;
    raw[2] = 0x56;
    raw[3] = 0x34;
    raw[4] = 0x12;
    EXPECT_EQ(load_le32(&raw[1]), 0x12345678u);
}

TEST(UnalignedAccess, LeStoreUnaligned) {
    uint8_t raw[9] = {};
    store_le32(&raw[2], 0xAABBCCDDu);
    EXPECT_EQ(load_le32(&raw[2]), 0xAABBCCDDu);
}

TEST(UnalignedAccess, BeLoadUnaligned) {
    uint8_t raw[9] = {};
    raw[2] = 0x12;
    raw[3] = 0x34;
    raw[4] = 0x56;
    raw[5] = 0x78;
    EXPECT_EQ(load_be32(&raw[2]), 0x12345678u);
}
