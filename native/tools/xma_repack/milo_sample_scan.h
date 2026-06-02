// milo_sample_scan.h — minimal, read-only parser for RB3 `.milo_xbox` SFX banks.
//
// RB3 SFX banks (orig-assets/extracted/sfx/gen/*.milo_xbox) are *uncompressed*
// milo containers: a 0x810-byte little-endian ChunkStream::ChunkInfo header
// (id 0xCABEDEAF) followed by the milo object stream (big-endian). Each bank
// holds N SynthSample objects; each SynthSample embeds a SampleData blob whose
// payload is the audio (kXMA / kBigEndPCM / ...). See the project doc
// docs/sessions/native/roadmap-2026-06-02/xma-conversion-impl.md for the full
// byte-layout writeup.
//
// This header ONLY locates + reads SampleData blobs — it never rewrites the
// container (offline conversion uses approach B: decode to sidecar PCM, leave
// the original bank untouched). Layout verified against all 18 RB3 banks.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace milo_scan {

// SampleData::Format (synth/SampleData.h)
enum Format {
    kPCM = 0,
    kBigEndPCM = 1,
    kVAG = 2,
    kXMA = 3,
    kATRAC = 4,
    kMP3 = 5,
    kNintendoADPCM = 6,
};

struct SampleBlob {
    std::string file;     // SynthSample::mFile (e.g. "samples/ach_bed_01_l.wav")
    int rev = 0;          // SampleData rev
    int format = 0;       // SampleData::Format
    int numSamples = 0;
    int sampleRate = 0;
    int sizeBytes = 0;    // payload size (raw codec bytes)
    int numChannels = 1;  // SampleData::mNumChannels (RB3=1; DC3 rev>=0x10 stores it)
    size_t dataOffset = 0; // offset of payload within the milo object stream
};

inline uint32_t rd_be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}
inline uint32_t rd_le32(const uint8_t* p) {
    return (uint32_t(p[3]) << 24) | (uint32_t(p[2]) << 16) |
           (uint32_t(p[1]) << 8) | uint32_t(p[0]);
}

// Content key for a SampleData payload, stable between offline convert and
// runtime lookup: the raw codec bytes drive an FNV-1a hash, mixed with size +
// sample rate. Bank-independent (so the runtime SampleInst, which only has the
// loaded SampleData, can find the matching sidecar without knowing the bank).
inline uint64_t payload_key(const uint8_t* data, int sizeBytes, int sampleRate) {
    uint64_t h = 1469598103934665603ULL; // FNV-1a offset basis
    for (int i = 0; i < sizeBytes; i++) {
        h ^= data[i];
        h *= 1099511628211ULL;
    }
    h ^= (uint64_t)(uint32_t)sizeBytes;
    h *= 1099511628211ULL;
    h ^= (uint64_t)(uint32_t)sampleRate;
    h *= 1099511628211ULL;
    return h;
}

// Parse an in-memory `.milo_xbox` file, appending every SampleData blob (any
// format) found inside its SynthSample objects to `out`. Returns false on a
// header that isn't an uncompressed milo (we only handle id 0xCABEDEAF banks —
// every RB3 SFX bank is uncompressed single/multi-chunk). The parser is a
// targeted scan: it walks the object stream for SynthSample::mFile FilePath
// strings ("*.wav") and decodes the fixed SynthSample tail that follows each.
inline bool scan_bank(const uint8_t* file, size_t fileLen,
                      std::vector<SampleBlob>& out) {
    if (fileLen < 0x810 + 4) return false;
    uint32_t id = rd_le32(file);
    if (id != 0xCABEDEAFu) {
        // Not an uncompressed milo (compressed milos id 0xCDBEDEAF/0xCCBEDEAF
        // would need zlib decompression first). RB3 SFX banks are all 0xCABEDEAF.
        return false;
    }
    uint32_t infoSize = rd_le32(file + 4); // ChunkInfoSize (== payload start)
    if (infoSize >= fileLen) return false;

    const uint8_t* p = file + infoSize;
    size_t n = fileLen - infoSize;

    // Walk for FilePath strings: 4-byte BE length prefix + ASCII ending ".wav".
    // mFile in a SynthSample is stored as a milo String (len + bytes). We accept
    // any "*.wav" path; the SampleData header that follows is sanity-checked, so
    // false positives (e.g. a .wav name in a different object) self-reject.
    for (size_t i = 0; i + 4 < n; i++) {
        uint32_t len = rd_be32(p + i);
        if (len < 5 || len > 256 || i + 4 + len + 25 > n) continue;
        const uint8_t* s = p + i + 4;
        // Must end with ".wav" and be printable ASCII.
        if (memcmp(s + len - 4, ".wav", 4) != 0) continue;
        bool ascii = true;
        for (uint32_t k = 0; k < len; k++)
            if (s[k] < 0x20 || s[k] > 0x7e) { ascii = false; break; }
        if (!ascii) continue;

        // SynthSample tail after mFile string:
        //   bool mIsLooped (1)  int mLoopStartSamp (4)  int mLoopEndSamp (4)
        // then SampleData:
        //   int rev  int fmt  int numSamples  int sampleRate  int sizeBytes
        //   bool hasData (rev>=0xB)  [payload]
        size_t o = i + 4 + len;
        o += 1;        // mIsLooped
        o += 4;        // mLoopStartSamp
        o += 4;        // mLoopEndSamp
        if (o + 25 > n) continue;
        int rev = (int)rd_be32(p + o); o += 4;
        int fmt = (int)rd_be32(p + o); o += 4;
        int ns = (int)rd_be32(p + o); o += 4;
        int sr = (int)rd_be32(p + o); o += 4;
        int sz = (int)rd_be32(p + o); o += 4;
        if (rev <= 0 || rev > 0xE) continue;       // sane SampleData rev range
        if (fmt < 0 || fmt > 6) continue;          // valid Format enum
        if (sr < 4000 || sr > 48000) continue;     // plausible sample rate
        if (sz < 0 || (size_t)sz > n) continue;
        int hasData = 1;
        if (rev >= 0xB) { hasData = p[o]; o += 1; }
        if (hasData != 1) continue;
        if (o + (size_t)sz > n) continue;

        SampleBlob b;
        b.file.assign((const char*)s, len);
        b.rev = rev;
        b.format = fmt;
        b.numSamples = ns;
        b.sampleRate = sr;
        b.sizeBytes = sz;
        b.dataOffset = o; // within object stream (add infoSize for file offset)
        out.push_back(b);
        // Skip past the payload so we don't re-scan its bytes.
        i = o + sz - 1;
    }
    return true;
}

// ---------------------------------------------------------------------------
// DC3 (.milo_xbox) SampleData layout
// ---------------------------------------------------------------------------
//
// DC3's SynthSample (rev 6) and SampleData (rev up to 0x10) differ from RB3:
//
//   * SynthSample::PreLoad writes `mFile` then SampleData directly — there is NO
//     RB3-style mIsLooped/mLoopStartSamp/mLoopEndSamp tail (those exist only at
//     SynthSample rev <= 5). So the fixed RB3 string-offset parser does not fit.
//   * SampleData (DC3, rev 0x10) is:
//       int  rev (packed revs; low16 = rev)
//       int  mCRC                 (ONLY when rev > 0xE)
//       int  fmt
//       int  mNumSamples
//       int  mSampleRate
//       int  mSizeBytes
//       bool hasData              (rev >= 0xB)
//       [payload]                 (hasData)
//       SampleMarker[] mMarkers   (rev >= 0xE): int count, then count * {String name, int sample}
//       int  mNumChannels         (rev >= 0x10)  <-- needed for the decode (mono/stereo)
//
// The content-hash KEY (payload_key) uses only the payload bytes + mSizeBytes +
// mSampleRate, all of which precede the payload — so numChannels is NOT in the
// key (matching dc3_xma::PayloadKey / rb3_xma::PayloadKey exactly). numChannels
// IS needed to feed DecodeXMAToPCM the right XMA2 channel mask.
//
// Rather than reconstruct the exact object-stream framing (directory headers,
// per-object class/name entries, ADDEADDE sentinels), this scans the object
// stream for a self-consistent SampleData header and validates it strictly by
// fully parsing through markers + numChannels. The strict multi-field validation
// (rev==0x10, valid fmt, plausible rate, in-bounds size, parseable markers,
// 1<=numChannels<=8) makes false positives vanishingly unlikely, and the parser
// resumes after each accepted blob's numChannels so payload bytes are never
// re-scanned. Verified across all 25 DC3 sfx/gen banks (mono + stereo mix).
inline bool scan_bank_dc3(const uint8_t* file, size_t fileLen,
                          std::vector<SampleBlob>& out) {
    if (fileLen < 0x810 + 4) return false;
    uint32_t id = rd_le32(file);
    if (id != 0xCABEDEAFu) return false; // uncompressed milo only
    uint32_t infoSize = rd_le32(file + 4);
    if (infoSize >= fileLen) return false;

    const uint8_t* p = file + infoSize;
    size_t n = fileLen - infoSize;

    for (size_t i = 0; i + 24 < n; i++) {
        // rev (low 16 bits of the packed-revs int). DC3 SampleData tops out at
        // 0x10; require exactly the channel-bearing rev so the parse below is
        // unambiguous (mCRC present, markers present, numChannels present).
        int rev = (int)rd_be32(p + i);
        if (rev != 0x10) continue;
        size_t q = i + 4 + 4; // skip rev + mCRC (mCRC present since rev > 0xE)
        if (q + 16 > n) continue;
        int fmt = (int)rd_be32(p + q);
        int ns = (int)rd_be32(p + q + 4);
        int sr = (int)rd_be32(p + q + 8);
        int sz = (int)rd_be32(p + q + 12);
        if (fmt < 0 || fmt > 6) continue;
        if (ns <= 0 || ns > 200000000) continue;
        if (sr < 4000 || sr > 48000) continue;
        if (sz < 256 || (size_t)sz > n) continue; // skip tiny/implausible

        size_t o = q + 16;
        if (o >= n) continue;
        int hasData = p[o]; o += 1; // rev >= 0xB
        if (hasData != 1) continue; // require an embedded payload
        if (o + (size_t)sz > n) continue;
        size_t payloadOff = o;
        size_t after = o + (size_t)sz;

        // mMarkers (rev >= 0xE): int count, then count * { String name, int sample }.
        if (after + 4 > n) continue;
        int mcount = (int)rd_be32(p + after); after += 4;
        if (mcount < 0 || mcount > 4096) continue;
        bool ok = true;
        for (int m = 0; m < mcount; m++) {
            if (after + 4 > n) { ok = false; break; }
            uint32_t nameLen = rd_be32(p + after); after += 4;
            if (nameLen > 1024) { ok = false; break; }
            after += nameLen;
            if (after + 4 > n) { ok = false; break; } // int sample
            after += 4;
        }
        if (!ok) continue;

        // mNumChannels (rev >= 0x10).
        if (after + 4 > n) continue;
        int nch = (int)rd_be32(p + after); after += 4;
        if (nch < 1 || nch > 8) continue;

        SampleBlob b;
        b.file = "";          // DC3 SampleData carries no inline file path here
        b.rev = rev;
        b.format = fmt;
        b.numSamples = ns;
        b.sampleRate = sr;
        b.sizeBytes = sz;
        b.numChannels = nch;
        b.dataOffset = payloadOff; // within object stream (add infoSize for file offset)
        out.push_back(b);
        // Resume just past numChannels so the payload bytes are never re-scanned.
        i = after - 1;
    }
    return true;
}

} // namespace milo_scan
