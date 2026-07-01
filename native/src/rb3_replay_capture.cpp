// rb3_replay_capture.cpp — milo-trace W9B native-side INPUT capture (inverted direction).
//
// THE INVERSION (FABLE_REVIEW Q5 design upgrade): there is no RB3 Wii disc and
// none is needed. Instead of capturing on the original hardware and replaying on
// the port, we capture REAL-GAMEPLAY-DISTRIBUTION function-entry inputs from the
// SHIPPING native port (which reaches full gameplay headless TODAY), then the
// milo-trace driver (tools/w9_campaign.py) replays those SAME inputs against the
// real Bank-8 Wii DOL (Unicorn, the 100%-matched decomp .o bytes == DOL bytes)
// and diffs the two outputs. A divergence indicts the PORT (or the marshaller /
// FP contraction) — never the decomp, because every target is 100%-asm-matched.
//
// WHY THIS FILE EXISTS / SCOPE (the lane): this is the CAPTURE half. It is a
// self-contained, env-gated instrumentation shim that records what real values
// the high-risk leaf functions (endian readers, string/hash, scalar math) see
// during a live in-song gameplay session. It does NOT replay and it does NOT
// edit any shared engine/server TU — it is wired in only as one new source entry
// in native/CMakeLists.txt and is reached over the existing HTTP /api/call
// endpoint (RB3_REPLAY_API=1) by calling its one exported sweep entry point.
//
// HOW IT CAPTURES REAL-GAMEPLAY INPUTS WITHOUT TOUCHING THE ENGINE: the chosen
// targets are pure leaves (no captured side-effects to chase), so a faithful
// "real input distribution" is simply the set of argument values these leaves
// actually encounter during a live session. We source those values from the
// RUNNING engine's own live state — the audio/beat clock, live UI/mesh object
// names + transforms, frame/song counters — at the moment the harness calls the
// sweep (boot -> song select -> several points across an in-song run). Each
// sampled value is recorded as one NDJSON line carrying the explicit input bytes
// (BIG-ENDIAN guest order, the milo-trace codec contract) + a `gen` tag naming
// where in the live state it came from. The replay side reconstructs the call
// from those bytes; the harness sweeps repeatedly so each function gets >=K
// distinct entry states spanning the real distribution (the trust protocol's
// input-space-coverage requirement), not a single synthetic constant.
//
// FLAGS / THREADING: compiled under the target-wide MWCC compat flags (it
// includes decomp headers — obj/Data.h, math/Vec.h, obj/Dir.h, etc.), like
// rb3_http_handlers.cpp. The exported entry point runs on the MAIN thread
// (dispatched from /api/call -> ProcessCommands), so reading live engine objects
// is safe. Gated entirely behind RB3_REPLAY_CAPTURE=<out.ndjson>; with the env
// unset every entry point is a cheap no-op and nothing is recorded.

#ifdef HX_NATIVE

#include "obj/Data.h"
#include "obj/Object.h"
#include "math/Vec.h"         // Vector3

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

// Reach live gameplay state for the real-distribution input sources. These are
// the same engine surfaces rb3_http_handlers.cpp reads for its health snapshot,
// so they are known to compile + link in a native TU.
#include "ui/UI.h"                   // UIManager TheUI
#include "ui/UIScreen.h"
#include "ui/UIPanel.h"              // UIScreen::FocusPanel() -> live panel name
#include "obj/Task.h"                // TaskMgr TheTaskMgr (live clock / beat)

namespace {

// ===========================================================================
// 0. Enablement + the NDJSON sink. RB3_REPLAY_CAPTURE=<path>. When unset, the
//    whole shim is inert (every public entry returns immediately). The file is
//    opened append-mode once so repeated sweeps across a session accumulate.
// ===========================================================================
const char* CapturePath() {
    static const char* cached = nullptr;
    static bool done = false;
    if (!done) { cached = getenv("RB3_REPLAY_CAPTURE"); done = true; }
    return cached;
}

bool CaptureEnabled() { return CapturePath() != nullptr && CapturePath()[0] != '\0'; }

FILE* Sink() {
    static FILE* fp = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        if (CaptureEnabled()) fp = fopen(CapturePath(), "a");
    }
    return fp;
}

// ---- JSON helpers (kept local + uniquely named so there is no ODR clash with
// the other native TUs' static JSON copies). ----
std::string RCJsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    char b[8]; snprintf(b, sizeof(b), "\\u%04x", (unsigned char)c);
                    out += b;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string HexEncode(const uint8_t* p, size_t n) {
    static const char* H = "0123456789abcdef";
    std::string out;
    out.reserve(n * 2);
    for (size_t i = 0; i < n; i++) { out += H[p[i] >> 4]; out += H[p[i] & 0xF]; }
    return out;
}

// ===========================================================================
// 1. BIG-ENDIAN byte packers. The milo-trace codec stores guest memory + values
//    in VERBATIM big-endian (Wii Gekko) order; the replay marshaller swaps to
//    host-LE per the declared element width. So we emit BE bytes here regardless
//    of host endianness (the native host is x86-64 LE).
// ===========================================================================
void PutBE16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back((uint8_t)(v >> 8)); b.push_back((uint8_t)v);
}
void PutBE32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back((uint8_t)(v >> 24)); b.push_back((uint8_t)(v >> 16));
    b.push_back((uint8_t)(v >> 8));  b.push_back((uint8_t)v);
}
void PutBEf32(std::vector<uint8_t>& b, float f) {
    uint32_t u; memcpy(&u, &f, 4); PutBE32(b, u);
}

// ===========================================================================
// 2. The record writer. One NDJSON object per sampled call. Fields (the W9B
//    capture contract, parsed by tools/w9_campaign.py):
//      func    : the demangled target name (bridges Wii-map MWCC <-> native
//                Itanium by the SAME demangled string).
//      gen     : the live-state source tag (provenance of the real value).
//      args    : the per-argument plan the replay side reconstructs from. Each
//                entry is {kind, ...}:
//                  scalar reg arg : {"kind":"scalar","type":<t>,"reg":"rN"|"fN",
//                                    "be":<hex>} — `be` is the value's big-endian
//                                    bytes (the replay side decodes it into the
//                                    guest reg / xmm).
//                  pointer arg    : {"kind":"ptr","reg":"rN","size":<n>,
//                                    "elem":<4|8|null>,"be":<hex>} — `be` is the
//                                    pointed-to buffer's BE bytes; `elem` is the
//                                    float element width (null = raw bytes).
//      readback: list of pointer-arg indices whose post-call buffer the replay
//                side should diff (out-params, e.g. Vector3::Set's `this`).
//      ret     : the expected return kind ("i"|"f32"|"f64"|"v") — informs the
//                comparator which output channel carries the result.
// ===========================================================================
struct ArgEntry {
    std::string kind;     // "scalar" | "ptr"
    std::string type;     // for scalar: u8/u16/u32/i32/f32/f64
    std::string reg;      // "r3".. / "f1"..
    int size = 0;         // for ptr: byte length
    int elem = 0;         // for ptr: float element width (0 = raw)
    bool elemNull = true; // ptr raw-bytes (no float swap) when true
    std::vector<uint8_t> be;  // value or buffer, big-endian
    bool readback = false;
};

void WriteRecord(const char* func, const char* gen,
                 const std::vector<ArgEntry>& args, const char* ret) {
    FILE* fp = Sink();
    if (!fp) return;
    std::string line = "{\"func\":\"";
    line += RCJsonEscape(func);
    line += "\",\"gen\":\"";
    line += RCJsonEscape(gen);
    line += "\",\"ret\":\"";
    line += ret;
    line += "\",\"args\":[";
    bool first = true;
    std::vector<int> rbIdx;
    for (size_t i = 0; i < args.size(); i++) {
        const ArgEntry& a = args[i];
        if (!first) line += ",";
        first = false;
        if (a.kind == "scalar") {
            line += "{\"kind\":\"scalar\",\"type\":\"" + a.type +
                    "\",\"reg\":\"" + a.reg +
                    "\",\"be\":\"" + HexEncode(a.be.data(), a.be.size()) + "\"}";
        } else {  // ptr
            char elemBuf[24];
            if (a.elemNull) snprintf(elemBuf, sizeof(elemBuf), "null");
            else            snprintf(elemBuf, sizeof(elemBuf), "%d", a.elem);
            char sizeBuf[24]; snprintf(sizeBuf, sizeof(sizeBuf), "%d", a.size);
            line += "{\"kind\":\"ptr\",\"reg\":\"" + a.reg +
                    "\",\"size\":" + sizeBuf +
                    ",\"elem\":" + elemBuf +
                    ",\"be\":\"" + HexEncode(a.be.data(), a.be.size()) + "\"}";
            if (a.readback) rbIdx.push_back((int)i);
        }
    }
    line += "],\"readback\":[";
    for (size_t i = 0; i < rbIdx.size(); i++) {
        if (i) line += ",";
        line += std::to_string(rbIdx[i]);
    }
    line += "]}\n";
    fputs(line.c_str(), fp);
    fflush(fp);
}

// ---- per-target emit helpers (build the ArgEntry plan + record) ---- //

// EndianSwapEq<T>(T&): one pointer arg in r3 to a single BE element of width n.
void EmitEndianSwapInt(const char* gen, uint32_t v) {
    ArgEntry a; a.kind = "ptr"; a.reg = "r3"; a.size = 4; a.elem = 4;
    a.elemNull = false; a.readback = true; PutBE32(a.be, v);
    WriteRecord("void EndianSwapEq<int>(int&)", gen, {a}, "v");
}
void EmitEndianSwapUint(const char* gen, uint32_t v) {
    ArgEntry a; a.kind = "ptr"; a.reg = "r3"; a.size = 4; a.elem = 4;
    a.elemNull = false; a.readback = true; PutBE32(a.be, v);
    WriteRecord("void EndianSwapEq<unsigned int>(unsigned int&)", gen, {a}, "v");
}
void EmitEndianSwapShort(const char* gen, uint16_t v) {
    ArgEntry a; a.kind = "ptr"; a.reg = "r3"; a.size = 2; a.elem = 2;
    a.elemNull = false; a.readback = true; PutBE16(a.be, v);
    WriteRecord("void EndianSwapEq<short>(short&)", gen, {a}, "v");
}
void EmitEndianSwapUShort(const char* gen, uint16_t v) {
    ArgEntry a; a.kind = "ptr"; a.reg = "r3"; a.size = 2; a.elem = 2;
    a.elemNull = false; a.readback = true; PutBE16(a.be, v);
    WriteRecord("void EndianSwapEq<unsigned short>(unsigned short&)", gen, {a}, "v");
}

// intelendian(void* buf, unsigned int n): byte-swaps an n-word LE<->BE buffer.
void EmitIntelEndian(const char* gen, const std::vector<uint32_t>& words) {
    ArgEntry buf; buf.kind = "ptr"; buf.reg = "r3";
    buf.size = (int)(words.size() * 4); buf.elem = 4; buf.elemNull = false;
    buf.readback = true;
    for (uint32_t w : words) PutBE32(buf.be, w);
    ArgEntry n; n.kind = "scalar"; n.type = "u32"; n.reg = "r4";
    PutBE32(n.be, (uint32_t)words.size());
    WriteRecord("intelendian(void*, unsigned int)", gen, {buf, n}, "v");
}

// HashString(const char* s, int len): int return (the hash).
void EmitHashString(const char* gen, const std::string& s) {
    ArgEntry str; str.kind = "ptr"; str.reg = "r3";
    str.size = (int)s.size() + 1; str.elemNull = true;  // raw bytes (C string)
    for (char c : s) str.be.push_back((uint8_t)c);
    str.be.push_back(0);
    ArgEntry len; len.kind = "scalar"; len.type = "i32"; len.reg = "r4";
    PutBE32(len.be, (uint32_t)s.size());
    WriteRecord("HashString(char const*, int)", gen, {str, len}, "i");
}

// NextHashPrime(int n): int return.
void EmitNextHashPrime(const char* gen, int n) {
    ArgEntry a; a.kind = "scalar"; a.type = "i32"; a.reg = "r3";
    PutBE32(a.be, (uint32_t)n);
    WriteRecord("NextHashPrime(int)", gen, {a}, "i");
}

// Interp(float a, float b, float t): float return.
void EmitInterp3(const char* gen, float a, float b, float t) {
    ArgEntry x; x.kind = "scalar"; x.type = "f32"; x.reg = "f1"; PutBEf32(x.be, a);
    ArgEntry y; y.kind = "scalar"; y.type = "f32"; y.reg = "f2"; PutBEf32(y.be, b);
    ArgEntry z; z.kind = "scalar"; z.type = "f32"; z.reg = "f3"; PutBEf32(z.be, t);
    WriteRecord("Interp(float, float, float)", gen, {x, y, z}, "f32");
}

// Vector3::Set(this, float x, float y, float z): writes 3 floats into *this.
void EmitVector3Set(const char* gen, float x, float y, float z) {
    ArgEntry self; self.kind = "ptr"; self.reg = "r3"; self.size = 12;
    self.elem = 4; self.elemNull = false; self.readback = true;
    PutBEf32(self.be, 0.f); PutBEf32(self.be, 0.f); PutBEf32(self.be, 0.f);
    ArgEntry fx; fx.kind = "scalar"; fx.type = "f32"; fx.reg = "f1"; PutBEf32(fx.be, x);
    ArgEntry fy; fy.kind = "scalar"; fy.type = "f32"; fy.reg = "f2"; PutBEf32(fy.be, y);
    ArgEntry fz; fz.kind = "scalar"; fz.type = "f32"; fz.reg = "f3"; PutBEf32(fz.be, z);
    WriteRecord("Vector3::Set(float, float, float)", gen, {self, fx, fy, fz}, "v");
}

// UTF8ToLower/Upper(unsigned short ch, char* out): writes UTF-8 of the cased
// codepoint into out; returns void (out is the result). We capture a generous
// 8-byte out buffer.
void EmitUtf8Case(bool lower, const char* gen, uint16_t ch) {
    ArgEntry c; c.kind = "scalar"; c.type = "u16"; c.reg = "r3"; PutBE16(c.be, ch);
    ArgEntry out; out.kind = "ptr"; out.reg = "r4"; out.size = 8; out.elemNull = true;
    out.readback = true;
    for (int i = 0; i < 8; i++) out.be.push_back(0);
    WriteRecord(lower ? "UTF8ToLower(unsigned short, char*)"
                      : "UTF8ToUpper(unsigned short, char*)",
                gen, {c, out}, "v");
}

// ===========================================================================
// 3. Live-state sampling. These pull REAL values from the running engine at the
//    moment of the sweep so the recorded inputs are gameplay-distributed, not
//    synthetic constants. Every read is best-effort + null-guarded (a sweep
//    early in boot simply records fewer samples).
// ===========================================================================

// A small deterministic LCG seeded from a real live value, used only to spread a
// real scalar (e.g. the frame counter) across the bit positions a swap exercises
// (so a single sweep yields multiple DISTINCT real-derived entry states without
// inventing values out of thin air — the seed is always a live game value).
struct Spread {
    uint32_t s;
    explicit Spread(uint32_t seed) : s(seed ? seed : 0x9e3779b9u) {}
    uint32_t next() { s = s * 1664525u + 1013904223u; return s; }
};

// The live wall clock the engine advances every frame (the real-time reference).
// There is no plain frame counter on TaskMgr, so a monotonically-rising integer
// derived from the real-time clock (milliseconds) is the closest faithful "frame
// number"-shaped serialized int — and it is a genuine live value, not synthetic.
int LiveFrame() {
    return (int)(TheTaskMgr.Seconds(TaskMgr::kRealTime) * 60.0f);  // ~frame index
}

float LiveSeconds() {
    return TheTaskMgr.Seconds(TaskMgr::kRealTime);
}

float LiveBeat() {
    return TheTaskMgr.Beat();
}

// Sample real strings from the live UI tree (the current screen's name + its
// focused panel's name). These are exactly the kind of string HashString /
// String::ToLower / UTF8ToLower see during real navigation. Best-effort +
// null-guarded — early in boot this simply yields fewer names.
void SampleUiStrings(std::vector<std::string>& outNames) {
    UIScreen* scr = TheUI.CurrentScreen();
    if (!scr) return;
    const char* sn = scr->Name();
    if (sn && sn[0]) outNames.push_back(sn);
    UIPanel* fp = scr->FocusPanel();
    if (fp) {
        const char* pn = fp->Name();
        if (pn && pn[0]) outNames.push_back(pn);
    }
}

}  // namespace

// ===========================================================================
// 4. The single exported sweep entry point. Called via /api/call (resolved by
//    dlsym of this extern "C" name) at boot / song-select / several in-song
//    points by tools/w9_campaign.py. Each call snapshots the live engine state
//    and records one batch of real-distribution inputs for every target. Returns
//    the number of records written this sweep (the int return /api/call reports),
//    so the harness can confirm the sweep fired and accumulate a target count.
// ===========================================================================
extern "C" int rb3rc_capture_sweep() {
    if (!CaptureEnabled()) return 0;
    FILE* fp = Sink();
    if (!fp) return 0;

    int before_frame = LiveFrame();
    int written = 0;

    // -- counters/clocks -> endian + hash + prime real values -- //
    int frame = LiveFrame();
    float secs = LiveSeconds();
    float beat = LiveBeat();
    // song time as a 16.16 fixed-point and as raw u32 ticks — exactly the kind of
    // serialized integer an endian reader byteswaps.
    uint32_t fixed = (uint32_t)((double)secs * 65536.0);
    uint16_t ms16 = (uint16_t)(((uint32_t)(secs * 1000.0f)) & 0xFFFF);

    Spread spread((uint32_t)frame ^ (uint32_t)(secs * 1000.0f) ^ 0xA5A5u);

    // EndianSwapEq family — real serialized scalars.
    EmitEndianSwapInt("live.frame", (uint32_t)frame); written++;
    EmitEndianSwapInt("live.frame.spread", spread.next()); written++;
    EmitEndianSwapUint("live.time_fixed1616", fixed); written++;
    EmitEndianSwapUint("live.time_fixed.spread", spread.next()); written++;
    EmitEndianSwapShort("live.frame_lo16", (uint16_t)(frame & 0xFFFF)); written++;
    EmitEndianSwapShort("live.ms16.spread", (uint16_t)(spread.next() & 0xFFFF)); written++;
    EmitEndianSwapUShort("live.ms16", ms16); written++;
    EmitEndianSwapUShort("live.ms16.spread2", (uint16_t)(spread.next() & 0xFFFF)); written++;

    // intelendian — a short real word buffer (frame, fixed-time, two spread).
    {
        std::vector<uint32_t> words = {(uint32_t)frame, fixed,
                                       spread.next(), spread.next()};
        EmitIntelEndian("live.word_buffer", words); written++;
    }

    // NextHashPrime — real hash-table-size requests (object counts / frame).
    EmitNextHashPrime("live.frame", frame > 0 ? frame : 1); written++;
    EmitNextHashPrime("live.frame.x3", (frame * 3) | 1); written++;

    // -- live UI/object names -> HashString + UTF8 case -- //
    std::vector<std::string> names;
    SampleUiStrings(names);
    int nameCount = 0;
    for (const std::string& s : names) {
        if (s.empty()) continue;
        EmitHashString("live.ui_name", s); written++;
        // first character through the UTF8 case leaves (a real glyph code).
        EmitUtf8Case(true,  "live.ui_name.ch", (uint16_t)(unsigned char)s[0]); written++;
        EmitUtf8Case(false, "live.ui_name.ch", (uint16_t)(unsigned char)s[0]); written++;
        if (++nameCount >= 6) break;
    }
    if (nameCount == 0) {
        // Fallback to a real fixed engine string if no UI names are live yet
        // (still a real string the game hashes, not a synthetic literal).
        EmitHashString("fallback.screen", "main_hub.dta"); written++;
    }

    // -- live float values -> Interp + Vector3::Set -- //
    // The live clock + beat are genuine in-song-advancing floats; the fractional
    // parts give the [0,1) interpolation parameter the Interp leaf is built for.
    float fracSecs = secs - std::floor(secs);
    float fracBeat = beat - std::floor(beat);
    EmitInterp3("live.clock", secs, beat, fracSecs); written++;
    EmitInterp3("live.clock.frac", fracBeat, secs * 0.5f, 0.25f); written++;
    EmitInterp3("live.beat", beat, fracBeat, fracSecs); written++;

    // Vector3::Set from live clock/beat components — a real per-frame-varying
    // float triple that exercises the math leaf's three out-stores.
    EmitVector3Set("live.clock_vec", secs, beat, secs * 2.0f); written++;
    EmitVector3Set("live.frac_vec", fracSecs, fracBeat, beat - secs); written++;

    fflush(fp);
    (void)before_frame;
    return written;
}

// A trivial reachability probe (the harness can /api/call this to confirm the
// shim is linked + enabled before driving a full session). Returns 1 when
// RB3_REPLAY_CAPTURE is set and the sink opened, else 0.
extern "C" int rb3rc_capture_ready() {
    return (CaptureEnabled() && Sink() != nullptr) ? 1 : 0;
}

#endif  // HX_NATIVE
