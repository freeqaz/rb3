// mtrace_wrap.h — milo-trace W3 per-call-site capture wrapper.
//
// INCLUDE ONLY from inside #ifdef HX_NATIVE blocks. Header-only to avoid any
// CMakeLists.txt edit (that file is concurrently modified). Gated on the
// RB3_MTRACE_WRAP env var; when unset every macro is a zero-cost no-op.
//
// Usage in BinStream::ReadEndian (BinStream.cpp, inside #ifdef HX_NATIVE):
//   #include "mtrace_wrap.h"   // one new include line (top of TU, HX_NATIVE-guarded)
//   MTRACE_WRAP_READENDIAN_PRE(data, bytes);   // after Read(), before if-block
//   if (!mLittleEndian) { SwapData(data, data, bytes); }
//   MTRACE_WRAP_READENDIAN_POST(data, bytes, mLittleEndian, Tell(), *this);
//   // <-- end of #ifdef HX_NATIVE block
//
// Do NOT include from non-HX_NATIVE compilation units or MWCC-compiled TUs.
// NOTE (W4 land deviation): per the W3 patch doc §4 fallback + W4 plan §0
// check 3, the #include is placed at the TOP of each TU under an #ifdef
// HX_NATIVE guard, NOT inside the function bodies (an anonymous namespace /
// function definitions inside a function body is invalid C++).

#pragma once
#ifdef HX_NATIVE

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <typeinfo>

namespace {  // anonymous — no ODR clash with rb3_replay_capture.cpp helpers

// ---------------------------------------------------------------------------
// Enablement + sink
// ---------------------------------------------------------------------------

static const char* MtwrapEnabled() {
    static const char* cached = nullptr;
    static bool done = false;
    if (!done) {
        done = true;
        const char* v = ::getenv("RB3_MTRACE_WRAP");
        cached = (v && v[0] && v[0] != '0') ? v : nullptr;
    }
    return cached;
}

static FILE* MtwrapSink() {
    static FILE* fp = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        if (MtwrapEnabled()) {
            const char* outPath = ::getenv("RB3_MTRACE_WRAP_OUT");
            if (!outPath || !outPath[0])
                outPath = "/tmp/milo_w3/wrap.ndjson";
            fp = ::fopen(outPath, "a");
        }
    }
    return fp;
}

static std::atomic<uint64_t>& MtwrapSeq() {
    static std::atomic<uint64_t> seq{0};
    return seq;
}

// ---------------------------------------------------------------------------
// JSON helpers (local prefix MTWRAP_ avoids ODR with rb3_replay_capture.cpp)
// ---------------------------------------------------------------------------

static std::string MtwrapHex(const void* p, int n) {
    static const char H[] = "0123456789abcdef";
    const uint8_t* b = reinterpret_cast<const uint8_t*>(p);
    std::string out;
    out.reserve(n * 2);
    for (int i = 0; i < n; i++) {
        out += H[b[i] >> 4];
        out += H[b[i] & 0xF];
    }
    return out;
}

static std::string MtwrapEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '"')  { out += "\\\""; }
        else if (c == '\\') { out += "\\\\"; }
        else           { out += c; }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Per-call-site pre-snapshot storage.
// The macro pair (PRE / POST) must execute on the same logical call.
// We store the pre-bytes in a small stack buffer captured by the PRE macro and
// passed to the POST macro via the same scope — both macros are in the same
// function body, so a local array works. See macro definitions below.
// ---------------------------------------------------------------------------

static void MtwrapEmitReadEndian(
        const char* site, uint64_t seq, bool le_flag, int n, int tell,
        const char* cls, const void* pre_buf, const void* post_buf) {
    FILE* fp = MtwrapSink();
    if (!fp) return;
    char buf[8];
    std::string line;
    line.reserve(200);
    line += "{\"site\":\"";
    line += MtwrapEscape(site);
    line += "\",\"seq\":";
    snprintf(buf, sizeof(buf), "%llu", (unsigned long long)seq);
    line += buf;
    line += ",\"le_flag\":";
    line += (le_flag ? "true" : "false");
    line += ",\"n\":";
    snprintf(buf, sizeof(buf), "%d", n);
    line += buf;
    line += ",\"tell\":";
    snprintf(buf, sizeof(buf), "%d", tell);
    line += buf;
    line += ",\"cls\":\"";
    line += MtwrapEscape(cls ? cls : "?");
    line += "\",\"pre\":\"";
    line += MtwrapHex(pre_buf, n);
    line += "\",\"post\":\"";
    line += MtwrapHex(post_buf, n);
    line += "\"}\n";
    ::fputs(line.c_str(), fp);
    ::fflush(fp);
}

static void MtwrapEmitSeekNonce(
        uint64_t seq, int seek_target, int byte_offset,
        const void* nonce_pre, const void* nonce_post, int nonce_len) {
    FILE* fp = MtwrapSink();
    if (!fp) return;
    char buf[16];
    std::string line;
    line.reserve(200);
    line += "{\"site\":\"VorbisReader::SeekNonce\",\"seq\":";
    snprintf(buf, sizeof(buf), "%llu", (unsigned long long)seq);
    line += buf;
    line += ",\"seek_target\":";
    snprintf(buf, sizeof(buf), "%d", seek_target);
    line += buf;
    line += ",\"byte_offset\":";
    snprintf(buf, sizeof(buf), "%d", byte_offset);
    line += buf;
    line += ",\"nonce_pre\":\"";
    line += MtwrapHex(nonce_pre, nonce_len);
    line += "\",\"nonce_post\":\"";
    line += MtwrapHex(nonce_post, nonce_len);
    line += "\"}\n";
    ::fputs(line.c_str(), fp);
    ::fflush(fp);
}

}  // namespace (anonymous)

// ---------------------------------------------------------------------------
// Public macros — these are what BinStream.cpp (and VorbisReader.cpp) use.
// ---------------------------------------------------------------------------

// ReadEndian site — two macros, both required:
//
//   MTRACE_WRAP_READENDIAN_PRE(data_ptr, byte_count)
//     Place immediately after Read(data, bytes) in the #ifdef HX_NATIVE block.
//     Snapshots raw bytes into a local stack array `_mtwrap_pre`.
//
//   MTRACE_WRAP_READENDIAN_POST(data_ptr, byte_count, le_flag, tell_val, this_ref)
//     Place at the END of the #ifdef HX_NATIVE block (after the swap decision).
//     Emits the NDJSON record.  `_mtwrap_pre` must be in scope.

#define MTRACE_WRAP_READENDIAN_PRE(data_ptr, byte_count)                        \
    uint8_t _mtwrap_pre[8] = {};                                                \
    uint64_t _mtwrap_seq = 0;                                                   \
    do {                                                                        \
        if (MtwrapSink() && (byte_count) >= 2 && (byte_count) <= 8) {          \
            _mtwrap_seq = MtwrapSeq().fetch_add(1, std::memory_order_relaxed);  \
            ::memcpy(_mtwrap_pre, (data_ptr), (byte_count));                    \
        }                                                                       \
    } while (0)

#define MTRACE_WRAP_READENDIAN_POST(data_ptr, byte_count, le_flag, tell_val, this_ref)  \
    do {                                                                                 \
        if (MtwrapSink() && (byte_count) >= 2 && (byte_count) <= 8) {                   \
            MtwrapEmitReadEndian(                                                         \
                "BinStream::ReadEndian",                                                  \
                _mtwrap_seq,                                                             \
                static_cast<bool>(le_flag),                                              \
                static_cast<int>(byte_count),                                            \
                static_cast<int>(tell_val),                                              \
                typeid(this_ref).name(),                                                 \
                _mtwrap_pre,                                                             \
                (data_ptr));                                                             \
        }                                                                                \
    } while (0)

// SeekNonce site — single macro (pre/post in the same expression scope):
//
//   MTRACE_WRAP_SEEKNONCE(seek_target, byte_offset, nonce_ptr, nonce_len)
//     Place BEFORE the `*(unsigned int*)mNonce = ...` write. The macro snapshots
//     the nonce, executes the write via a helper lambda, then snapshots again.
//     Because this macro expands to a compound statement it MUST appear in place
//     of the existing write line (which is replaced by this macro call + the
//     `ctr_reinit` line immediately after). See the unified diff below.

#define MTRACE_WRAP_SEEKNONCE(seek_target_val, byte_offset_val, nonce_ptr, nonce_len, write_expr) \
    do {                                                                                          \
        uint8_t _mtwrap_nonce_pre[16] = {};                                                       \
        ::memcpy(_mtwrap_nonce_pre, (nonce_ptr), (nonce_len));                                    \
        (write_expr);  /* the original write: *(unsigned int*)mNonce = (unsigned int)(byte/16) */  \
        if (MtwrapSink()) {                                                                       \
            uint64_t _seq = MtwrapSeq().fetch_add(1, std::memory_order_relaxed);                  \
            MtwrapEmitSeekNonce(_seq,                                                             \
                static_cast<int>(seek_target_val),                                               \
                static_cast<int>(byte_offset_val),                                               \
                _mtwrap_nonce_pre,                                                               \
                (nonce_ptr),                                                                     \
                (nonce_len));                                                                     \
        }                                                                                        \
    } while (0)

#endif  // HX_NATIVE
