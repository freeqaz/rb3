// RB3 native convergence test-suite helpers.
//
// Mirrors dc3-decomp/native/tests/test_helpers.h, adapted to RB3's engine API
// (Symbol::Init() rather than DC3's Symbol::PreInit). Provides:
//   - MemBinStream     : in-memory BinStream for endianness/serialization tests
//   - Put{BE,LE}{16,32}/Float/String : synthetic binary-data builders
//   - EnsureSymbolInit / SymbolTestFixture : minimal Symbol+MakeString bring-up
//   - EnsureEngineInit / EngineTestFixture : RunBoot-style headless engine boot
#pragma once

#include <gtest/gtest.h>

// glibc <sys/stat.h> (pulled transitively by <gtest/gtest.h>) #defines
// st_atime/st_mtime/st_ctime as macros (-> st_atim.tv_sec, ...). The decomp
// os/File.h declares `struct FileStat { ... st_ctime; st_atime; st_mtime; }`
// using those as MEMBER names, so the macros corrupt it whenever a decomp
// header is included after gtest. Neutralize them before any decomp header.
#include <sys/stat.h>
#undef st_atime
#undef st_mtime
#undef st_ctime

#include <cstring>
#include <vector>
#include <cstdint>

#include "utl/BinStream.h"

// ============================================================================
// MemBinStream — in-memory BinStream for unit testing without files.
// Wraps a byte buffer and implements ReadImpl/WriteImpl/SeekImpl directly.
// `littleEndian` controls the endian swap BinStream::ReadEndian applies.
// ============================================================================
class MemBinStream : public BinStream {
public:
    // Construct from existing data (for reading).
    MemBinStream(const void *data, int size, bool littleEndian = false)
        : BinStream(littleEndian), mTell(0), mFail(false) {
        mBuffer.resize(size);
        if (size > 0)
            memcpy(mBuffer.data(), data, size);
    }

    // Construct empty (for writing).
    explicit MemBinStream(bool littleEndian = false)
        : BinStream(littleEndian), mTell(0), mFail(false) {
        mBuffer.reserve(4096);
    }

    virtual ~MemBinStream() {}
    virtual void Flush() {}
    virtual int Tell() { return mTell; }
    virtual EofType Eof() {
        return (mTell >= (int)mBuffer.size()) ? RealEof : NotEof;
    }
    virtual bool Fail() { return mFail; }

    int Size() const { return (int)mBuffer.size(); }
    const char *Buffer() const { return mBuffer.data(); }

private:
    virtual void ReadImpl(void *data, int bytes) {
        if (mTell + bytes > (int)mBuffer.size()) {
            mFail = true;
            int avail = (int)mBuffer.size() - mTell;
            if (avail > 0) {
                memcpy(data, mBuffer.data() + mTell, avail);
                memset((char *)data + avail, 0, bytes - avail);
            } else {
                memset(data, 0, bytes);
            }
            mTell = (int)mBuffer.size();
            return;
        }
        memcpy(data, mBuffer.data() + mTell, bytes);
        mTell += bytes;
    }

    virtual void WriteImpl(const void *data, int bytes) {
        if (mTell + bytes > (int)mBuffer.size())
            mBuffer.resize(mTell + bytes);
        memcpy(mBuffer.data() + mTell, data, bytes);
        mTell += bytes;
    }

    virtual void SeekImpl(int offset, SeekType type) {
        int pos;
        switch (type) {
        case kSeekBegin: pos = offset; break;
        case kSeekCur: pos = mTell + offset; break;
        case kSeekEnd: pos = (int)mBuffer.size() + offset; break;
        default: return;
        }
        if (pos < 0 || pos > (int)mBuffer.size()) {
            mFail = true;
        } else {
            mTell = pos;
        }
    }

    int mTell;
    bool mFail;
    std::vector<char> mBuffer;
};

// ============================================================================
// Byte-buffer builders — construct synthetic binary test data.
// ============================================================================
inline void PutBE32(std::vector<uint8_t> &buf, uint32_t v) {
    buf.push_back((v >> 24) & 0xFF); buf.push_back((v >> 16) & 0xFF);
    buf.push_back((v >> 8) & 0xFF);  buf.push_back(v & 0xFF);
}
inline void PutLE32(std::vector<uint8_t> &buf, uint32_t v) {
    buf.push_back(v & 0xFF);         buf.push_back((v >> 8) & 0xFF);
    buf.push_back((v >> 16) & 0xFF); buf.push_back((v >> 24) & 0xFF);
}
inline void PutBE16(std::vector<uint8_t> &buf, uint16_t v) {
    buf.push_back((v >> 8) & 0xFF); buf.push_back(v & 0xFF);
}
inline void PutLE16(std::vector<uint8_t> &buf, uint16_t v) {
    buf.push_back(v & 0xFF); buf.push_back((v >> 8) & 0xFF);
}
inline void PutBEFloat(std::vector<uint8_t> &buf, float val) {
    uint32_t bits; memcpy(&bits, &val, 4); PutBE32(buf, bits);
}

// ============================================================================
// Minimal init — Symbol + MakeString only (pure unit tests that intern Symbols
// but don't need the full engine). RB3 boots these via Symbol::Init().
// ============================================================================
void EnsureSymbolInit();

class SymbolTestFixture : public ::testing::Test {
protected:
    static void SetUpTestSuite() { EnsureSymbolInit(); }
};

// ============================================================================
// Engine init — RunBoot-style headless boot (SystemPreInit/SystemInit from the
// on-disc config DTAs + common object-factory registration). Needs the
// extracted assets; point RB3_DATA at them (defaults to orig-assets/extracted).
// Returns false (and the fixture SKIPs) if the data dir is unavailable.
// ============================================================================
bool EnsureEngineInit();

class EngineTestFixture : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!EnsureEngineInit())
            GTEST_SKIP() << "engine init unavailable (set RB3_DATA to the extracted assets)";
    }
};
