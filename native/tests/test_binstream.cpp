// BinStream endianness invariants — pure, asset-free, millisecond CI gates.
//
// Locks the polarity the hack-audit flagged as "reverted-to-broken twice":
// BinStream::mLittleEndian describes the FILE's endianness, not the host's. On
// the native LE host, ReadEndian/WriteEndian swap ONLY when the file is
// big-endian (the Xbox/Wii .milo_xbox case). A regression here silently
// corrupts every asset load, so these guard it cheaply.

#include "test_helpers.h"

// ---- Reading ----

TEST(BinStreamEndian, BigEndianFileReadsIntCorrectly) {
    std::vector<uint8_t> buf;
    PutBE32(buf, 42); // 00 00 00 2A on disk
    MemBinStream bs(buf.data(), (int)buf.size(), /*littleEndian=*/false);
    int v = 0;
    bs >> v;
    EXPECT_EQ(v, 42) << "BE file int must swap to host value on native";
}

TEST(BinStreamEndian, LittleEndianFileReadsIntCorrectly) {
    std::vector<uint8_t> buf;
    PutLE32(buf, 42); // 2A 00 00 00 on disk
    MemBinStream bs(buf.data(), (int)buf.size(), /*littleEndian=*/true);
    int v = 0;
    bs >> v;
    EXPECT_EQ(v, 42) << "LE file matches the LE host — no swap";
}

// The decisive polarity test: identical raw bytes interpreted under each flag.
TEST(BinStreamEndian, PolarityIsKeyedToFileNotHost) {
    const uint8_t raw[4] = {0x00, 0x00, 0x00, 0x2A}; // big-endian 42

    MemBinStream asBE(raw, 4, /*littleEndian=*/false);
    int be = 0; asBE >> be;
    EXPECT_EQ(be, 42) << "read as BE file -> swapped -> 42";

    MemBinStream asLE(raw, 4, /*littleEndian=*/true);
    int le = 0; asLE >> le;
    EXPECT_EQ(le, 0x2A000000) << "read as LE file -> no swap -> raw little-endian value";
}

TEST(BinStreamEndian, BigEndianShort) {
    std::vector<uint8_t> buf;
    PutBE16(buf, 0x1234);
    MemBinStream bs(buf.data(), (int)buf.size(), false);
    short v = 0;
    bs >> v;
    EXPECT_EQ((unsigned short)v, 0x1234u);
}

TEST(BinStreamEndian, BigEndianFloat) {
    std::vector<uint8_t> buf;
    PutBEFloat(buf, 1.5f);
    MemBinStream bs(buf.data(), (int)buf.size(), false);
    float v = 0.0f;
    bs >> v;
    EXPECT_FLOAT_EQ(v, 1.5f);
}

// ---- Round trips (write then read with matching file endianness) ----

TEST(BinStreamEndian, RoundTripBigEndian) {
    MemBinStream out(/*littleEndian=*/false);
    int w = 0x0BADF00D;
    out << w;
    // On-disk bytes must be big-endian order.
    ASSERT_EQ(out.Size(), 4);
    const uint8_t *b = (const uint8_t *)out.Buffer();
    EXPECT_EQ(b[0], 0x0Bu);
    EXPECT_EQ(b[1], 0xADu);
    EXPECT_EQ(b[2], 0xF0u);
    EXPECT_EQ(b[3], 0x0Du);

    MemBinStream in(out.Buffer(), out.Size(), false);
    int r = 0; in >> r;
    EXPECT_EQ(r, w);
}

TEST(BinStreamEndian, RoundTripLittleEndian) {
    MemBinStream out(/*littleEndian=*/true);
    int w = 0x0BADF00D;
    out << w;
    const uint8_t *b = (const uint8_t *)out.Buffer();
    EXPECT_EQ(b[0], 0x0Du);
    EXPECT_EQ(b[1], 0xF0u);
    EXPECT_EQ(b[2], 0xADu);
    EXPECT_EQ(b[3], 0x0Bu);

    MemBinStream in(out.Buffer(), out.Size(), true);
    int r = 0; in >> r;
    EXPECT_EQ(r, w);
}
