// rb3_session_trace.cpp — unified session telemetry recorder (HX_NATIVE only).
//
// See rb3_session_trace.h + docs/native/SESSION_TELEMETRY_DESIGN.md
// "Locked v1 contract". This file owns the in-memory event model, the
// append-only string interner, the bounded ring with tiered overflow, the
// monotonic client_seq / sid identity, and the v1 NDJSON serializer.
//
// M0 scope: recorder CORE only. The engine taps (frame/input/nav/boot/song/log)
// are Wave 2 — the one exception is the RB3FrameTraceRecord back-compat shim
// (the existing src/App.cpp:809 call), so a native run already emits hdr+fr.

#ifdef HX_NATIVE

#include "rb3_session_trace.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

#if defined(_WIN32)
#include <process.h>
#define RB3_GETPID() _getpid()
#else
#include <unistd.h>
#define RB3_GETPID() getpid()
#endif

// ---------------------------------------------------------------------------
// Engine-side counters — DEFINED in src/system/utl/Loader.cpp. The increments
// live behind gFrameTraceActive, so the recorder arms that flag when it opens a
// sink and reads+zeroes the per-frame counters from RB3RecordFrame.
// ---------------------------------------------------------------------------
extern bool gFrameTraceActive;
extern int  gFrameTraceLoaderAdds;
extern int  gFrameTraceStreamOpens;

// ---------------------------------------------------------------------------
// Exported toggle gate + frame counter.
// ---------------------------------------------------------------------------
bool gRB3TraceActive = false;
int  gRB3TraceFrame  = 0;

namespace {

// ===========================================================================
// Event model — a single fixed-size POD so the ring is a flat array (no
// per-event alloc). Variable-length strings (screen / nav / log / song id) go
// through the interner and live here only as uint16 ids.
// ===========================================================================
struct TraceEvent {
    double  t;          // monotonic ms since first init (steady_clock)
    int32_t f;          // frame index (gRB3TraceFrame snapshot)
    uint64_t cs;        // client_seq (monotonic per-session)
    float   sm;         // song ms; < 0 => omit "sm" entirely
    uint8_t kind;       // RB3TraceKind
    uint8_t pad;        // joypad index (input) / unused
    union {
        struct { float dt, lp, lpu; uint16_t pend; uint16_t scrId; int ld, st; } fr;
        struct { uint32_t bits, dn, up; int16_t whammy, tilt; } in;
        struct { uint16_t fromId, toId, focusId; uint8_t wentBack; } nav;
        struct { uint16_t phaseId; } boot;
        struct { uint16_t aId; uint32_t u32; float val; } gen;
    };
};

// ===========================================================================
// String interner — append-only vector<string> + map -> uint16 id. The table is
// NOT emitted as its own line; strings are serialized inline (the interner is an
// in-ring size optimization, not part of the wire envelope). id 0 is reserved
// for the empty string so an absent string resolves cleanly.
// ===========================================================================
struct Interner {
    std::vector<std::string>                  strings;
    std::unordered_map<std::string, uint16_t> ids;

    Interner() { reset(); }
    void reset() {
        strings.clear();
        ids.clear();
        strings.push_back("");   // id 0 = empty
        ids[""] = 0;
    }
    uint16_t intern(const char *s) {
        if (!s) return 0;
        std::string key(s);
        std::unordered_map<std::string, uint16_t>::iterator it = ids.find(key);
        if (it != ids.end()) return it->second;
        // Cap at uint16 space; past it, reuse the empty id (extremely unlikely
        // in a dev session — screen/nav/log names are few).
        if (strings.size() >= 0xFFFF) return 0;
        uint16_t id = (uint16_t)strings.size();
        strings.push_back(key);
        ids[key] = id;
        return id;
    }
    const char *str(uint16_t id) const {
        if (id < strings.size()) return strings[id].c_str();
        return "";
    }
};

// ===========================================================================
// Recorder state (single global, reset by RB3TraceShutdown for tests).
// ===========================================================================
struct Recorder {
    FILE       *sink         = nullptr;
    int         state        = 0;     // 0=unchecked, 1=armed, -1=disabled
    uint64_t    clientSeq    = 0;     // next cs to stamp
    std::string sid;                  // 16-hex session id, minted at init
    bool        hdrWritten   = false;

    // Ring (bounded). Default 16384; env RB3_TRACE_RING override.
    std::vector<TraceEvent> ring;
    size_t      ringCap      = 16384;
    size_t      ringHead     = 0;     // next slot to write
    size_t      ringCount    = 0;     // live events not yet flushed
    uint64_t    droppedTotal = 0;     // events dropped (last-resort drop-oldest)

    // fr decimation knobs (§4.7).
    float       frameLongMs  = 20.0f;
    int         frameDecim   = 30;

    Interner    interner;

    // Monotonic clock base — first init pins t=0.
    std::chrono::steady_clock::time_point clockBase;
    bool        clockBaseSet = false;

    // Input edge-detect: last recorded full bitmask (per pad 0).
    uint32_t    lastInputBits = 0;
    bool        haveInput      = false;

    std::string startedIso;   // hdr.started
    std::string buildGit;     // hdr.build.git
};

Recorder gRec;

// ---------------------------------------------------------------------------
// Monotonic ms since first init.
// ---------------------------------------------------------------------------
double NowMs() {
    if (!gRec.clockBaseSet) {
        gRec.clockBase    = std::chrono::steady_clock::now();
        gRec.clockBaseSet = true;
        return 0.0;
    }
    std::chrono::duration<double, std::milli> d =
        std::chrono::steady_clock::now() - gRec.clockBase;
    return d.count();
}

// Seconds since the Unix epoch, via std::chrono (NOT libc time(): the engine
// link overrides the libc `time` PLT entry with a Wii/SDK shim that faults on
// the native host, so we avoid that symbol entirely).
uint64_t UnixSeconds() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// ---------------------------------------------------------------------------
// ISO-8601 UTC, e.g. 2026-06-09T17:04:11Z. Computed from a raw epoch-second
// count (no localtime/gmtime libc dependency — see UnixSeconds above).
// ---------------------------------------------------------------------------
std::string IsoUtcNow() {
    uint64_t s   = UnixSeconds();
    uint64_t days = s / 86400;
    uint32_t secOfDay = (uint32_t)(s % 86400);
    uint32_t hh = secOfDay / 3600;
    uint32_t mm = (secOfDay % 3600) / 60;
    uint32_t ss = secOfDay % 60;
    // Civil-from-days (Howard Hinnant's algorithm), valid for any Gregorian date.
    int64_t z = (int64_t)days + 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    uint64_t doe = (uint64_t)(z - era * 146097);
    uint64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t y = (int64_t)yoe + era * 400;
    uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    uint64_t mp = (5 * doy + 2) / 153;
    uint32_t d = (uint32_t)(doy - (153 * mp + 2) / 5 + 1);
    uint32_t m = (uint32_t)(mp < 10 ? mp + 3 : mp - 9);
    y += (m <= 2);
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04lld-%02u-%02uT%02u:%02u:%02uZ",
                  (long long)y, m, d, hh, mm, ss);
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// 16-hex session id from time ^ pid ^ a per-process counter.
// ---------------------------------------------------------------------------
std::string MintSid() {
    static uint64_t sCounter = 0;
    uint64_t t   = UnixSeconds();
    uint64_t pid = (uint64_t)RB3_GETPID();
    uint64_t c   = ++sCounter;
    // Spread the clock into the high bits so two sessions in the same second
    // (same t, same pid) still differ via the counter.
    uint64_t mix = (t << 20) ^ (pid << 8) ^ (c * 0x9E3779B97F4A7C15ULL);
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)mix);
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// JSON string escaper. Appends an escaped copy of `s` to `out`. Guarantees one
// event = one physical line: \n / \r / control chars never pass through raw.
// ---------------------------------------------------------------------------
void JsonEscape(std::string &out, const char *s) {
    if (!s) return;
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        unsigned char c = *p;
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char u[8];
                    std::snprintf(u, sizeof(u), "\\u%04x", (unsigned)c);
                    out += u;
                } else {
                    out += (char)c;
                }
        }
    }
}

void AppendQuotedKV(std::string &out, const char *key, const char *val) {
    out += '"';
    out += key;
    out += "\":\"";
    JsonEscape(out, val);
    out += '"';
}

// Append a number with 1 decimal place (envelope t / sm; fr fields use 3).
void AppendNum1(std::string &out, double v) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.1f", v);
    out += buf;
}
void AppendNum3(std::string &out, double v) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.3f", v);
    out += buf;
}

// ---------------------------------------------------------------------------
// Serialize one event to a single NDJSON line (terminated with '\n').
// ---------------------------------------------------------------------------
void SerializeEvent(std::string &out, const TraceEvent &e) {
    out += '{';
    // Envelope: t, f, sm(omit when <0), cs, k. (hdr has its own path.)
    out += "\"t\":";
    AppendNum1(out, e.t);
    out += ",\"f\":";
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", e.f);
        out += buf;
    }
    if (e.sm >= 0.0f) {
        out += ",\"sm\":";
        AppendNum1(out, e.sm);
    }
    out += ",\"cs\":";
    {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%llu", (unsigned long long)e.cs);
        out += buf;
    }
    out += ",\"k\":\"";
    switch (e.kind) {
        case TK_BOOT:  out += "boot"; break;
        case TK_FRAME: out += "fr";   break;
        case TK_INPUT: out += "in";   break;
        case TK_NAV:   out += "nav";  break;
        case TK_SONG:  out += "song"; break;
        case TK_AU:    out += "au";   break;
        case TK_LOG:   out += "log";  break;
        case TK_MARK:  out += "mark"; break;
        default:       out += "?";    break;
    }
    out += '"';

    char num[24];
    switch (e.kind) {
        case TK_FRAME: {
            out += ",\"dt\":";  AppendNum3(out, e.fr.dt);
            out += ",\"lp\":";  AppendNum3(out, e.fr.lp);
            out += ",\"lpu\":"; AppendNum3(out, e.fr.lpu);
            out += ",\"scr\":\"";
            JsonEscape(out, gRec.interner.str(e.fr.scrId));
            out += '"';
            std::snprintf(num, sizeof(num), ",\"ld\":%d", e.fr.ld); out += num;
            std::snprintf(num, sizeof(num), ",\"st\":%d", e.fr.st); out += num;
            std::snprintf(num, sizeof(num), ",\"pend\":%u", (unsigned)e.fr.pend); out += num;
            break;
        }
        case TK_INPUT: {
            std::snprintf(num, sizeof(num), ",\"pad\":%u", (unsigned)e.pad); out += num;
            std::snprintf(num, sizeof(num), ",\"b\":%u",  (unsigned)e.in.bits); out += num;
            std::snprintf(num, sizeof(num), ",\"dn\":%u", (unsigned)e.in.dn);  out += num;
            std::snprintf(num, sizeof(num), ",\"up\":%u", (unsigned)e.in.up);  out += num;
            // ax{} is sparse: only present when whammy||tilt is non-zero.
            if (e.in.whammy != 0 || e.in.tilt != 0) {
                out += ",\"ax\":{";
                bool first = true;
                if (e.in.whammy != 0) {
                    std::snprintf(num, sizeof(num), "\"wh\":%d", (int)e.in.whammy);
                    out += num; first = false;
                }
                if (e.in.tilt != 0) {
                    if (!first) out += ',';
                    std::snprintf(num, sizeof(num), "\"ti\":%d", (int)e.in.tilt);
                    out += num;
                }
                out += '}';
            }
            break;
        }
        case TK_NAV: {
            out += ",\"from\":\""; JsonEscape(out, gRec.interner.str(e.nav.fromId)); out += '"';
            out += ",\"to\":\"";   JsonEscape(out, gRec.interner.str(e.nav.toId));   out += '"';
            out += ",\"focus\":\"";JsonEscape(out, gRec.interner.str(e.nav.focusId));out += '"';
            if (e.nav.wentBack) out += ",\"wb\":true";
            break;
        }
        case TK_BOOT: {
            out += ",\"ph\":\"";
            JsonEscape(out, gRec.interner.str(e.boot.phaseId));
            out += '"';
            break;
        }
        case TK_SONG:
        case TK_AU:
        case TK_LOG:
        case TK_MARK: {
            // Generic payload: interned string + u32 + val. Field name keyed by
            // kind to match the v1 schema's first field (song.ev / log.msg /
            // mark.tag). Numeric extras are emitted as u/val when set.
            const char *strKey =
                (e.kind == TK_LOG)  ? "msg" :
                (e.kind == TK_MARK) ? "tag" :
                (e.kind == TK_SONG) ? "ev"  : "k2";
            out += ",\"";
            out += strKey;
            out += "\":\"";
            JsonEscape(out, gRec.interner.str(e.gen.aId));
            out += '"';
            if (e.gen.u32 != 0) {
                std::snprintf(num, sizeof(num), ",\"u\":%u", (unsigned)e.gen.u32);
                out += num;
            }
            if (e.gen.val != 0.0f) {
                out += ",\"val\":";
                AppendNum3(out, e.gen.val);
            }
            break;
        }
        default: break;
    }
    out += "}\n";
}

// ---------------------------------------------------------------------------
// Header line. k=hdr,v=1,sid,cs=0,platform="native",started,build{git},flags{}.
// No t/f/sm. cs is 0 (the first stamped client_seq).
// ---------------------------------------------------------------------------
void WriteHeader() {
    if (!gRec.sink || gRec.hdrWritten) return;
    std::string out;
    out += "{\"k\":\"hdr\",\"v\":1,";
    AppendQuotedKV(out, "sid", gRec.sid.c_str());
    out += ",\"cs\":0,";
    AppendQuotedKV(out, "platform", "native");
    out += ',';
    AppendQuotedKV(out, "started", gRec.startedIso.c_str());
    out += ",\"build\":{";
    AppendQuotedKV(out, "git", gRec.buildGit.c_str());
    out += "},\"flags\":{}}\n";
    std::fwrite(out.data(), 1, out.size(), gRec.sink);
    std::fflush(gRec.sink);
    gRec.hdrWritten = true;
    // hdr consumes cs=0; subsequent events start at cs=1.
    if (gRec.clientSeq == 0) gRec.clientSeq = 1;
}

// ---------------------------------------------------------------------------
// Drain the ring to the file sink (oldest first), then fflush.
// ---------------------------------------------------------------------------
void DrainRing() {
    if (!gRec.sink || gRec.ringCount == 0) return;
    std::string out;
    out.reserve(gRec.ringCount * 64);
    size_t start = (gRec.ringHead + gRec.ringCap - gRec.ringCount) % gRec.ringCap;
    for (size_t i = 0; i < gRec.ringCount; ++i) {
        const TraceEvent &e = gRec.ring[(start + i) % gRec.ringCap];
        SerializeEvent(out, e);
    }
    std::fwrite(out.data(), 1, out.size(), gRec.sink);
    std::fflush(gRec.sink);
    gRec.ringCount = 0;
}

// ---------------------------------------------------------------------------
// Tiered overflow when the ring is full and not yet drained. The native sink
// drains on every push when present, so this only fires if the sink is absent
// (e.g. open failed) or a future buffered mode. Protected kinds (in/nav/song/
// boot/mark/log) are never decimated; fr is dropped first; last resort is a
// drop-oldest with a {"k":"log","drop":N} marker.
// ---------------------------------------------------------------------------
bool IsProtectedKind(uint8_t k) {
    return k == TK_INPUT || k == TK_NAV || k == TK_SONG ||
           k == TK_BOOT  || k == TK_MARK || k == TK_LOG;
}

// Make room for one event. Returns true if a slot is free to write at ringHead.
bool MakeRoom() {
    if (gRec.ringCount < gRec.ringCap) return true;
    // Full. First: scan oldest->newest for an fr to drop (decimate fr first).
    size_t start = (gRec.ringHead + gRec.ringCap - gRec.ringCount) % gRec.ringCap;
    for (size_t i = 0; i < gRec.ringCount; ++i) {
        size_t idx = (start + i) % gRec.ringCap;
        if (gRec.ring[idx].kind == TK_FRAME) {
            // Compact: shift everything after idx back by one, freeing a slot.
            for (size_t j = i; j + 1 < gRec.ringCount; ++j) {
                size_t a = (start + j) % gRec.ringCap;
                size_t b = (start + j + 1) % gRec.ringCap;
                gRec.ring[a] = gRec.ring[b];
            }
            gRec.ringHead = (gRec.ringHead + gRec.ringCap - 1) % gRec.ringCap;
            gRec.ringCount--;
            return true;
        }
    }
    // No fr to drop — last resort: drop the oldest (protected) event + count it.
    gRec.droppedTotal++;
    gRec.ringHead  = (gRec.ringHead + gRec.ringCap - 1) % gRec.ringCap;
    gRec.ringCount--;
    return true;
}

// ---------------------------------------------------------------------------
// Push one event into the ring (stamps t/cs), then (native) drain to the sink.
// ---------------------------------------------------------------------------
void PushEvent(TraceEvent &e) {
    if (!gRec.hdrWritten) WriteHeader();
    e.t  = NowMs();
    e.cs = gRec.clientSeq++;
    MakeRoom();
    gRec.ring[gRec.ringHead] = e;
    gRec.ringHead = (gRec.ringHead + 1) % gRec.ringCap;
    if (gRec.ringCount < gRec.ringCap) gRec.ringCount++;
    // Native: stream to the file each push so a SIGTERM leaves valid NDJSON.
    if (gRec.sink) DrainRing();
}

// Snapshot the current song ms (Wave 2 wires the real accessor; v1 core has no
// engine dependency, so default to "not in a song" = omit sm).
float CurrentSongMs() {
    return -1.0f;
}

int ParseIntEnv(const char *name, int def) {
    const char *v = std::getenv(name);
    if (!v || !v[0]) return def;
    return std::atoi(v);
}
float ParseFloatEnv(const char *name, float def) {
    const char *v = std::getenv(name);
    if (!v || !v[0]) return def;
    return (float)std::atof(v);
}

} // namespace

// ===========================================================================
// Public API
// ===========================================================================

void RB3TraceInit() {
    if (gRec.state == 1) return;   // already armed
    // Resolve the master toggle: RB3_SESSION_TRACE, else RB3_FRAME_TRACE alias.
    const char *sessPath = std::getenv("RB3_SESSION_TRACE");
    const char *framePath = std::getenv("RB3_FRAME_TRACE");
    bool frameAlias = false;
    const char *path = nullptr;
    if (sessPath && sessPath[0]) {
        path = sessPath;
    } else if (framePath && framePath[0]) {
        path = framePath;
        frameAlias = true;   // full-frame capture (decimate=1)
    }

    if (!path) {
        gRec.state = -1;     // not requested
        return;
    }

    // "1" => a default path; anything else is taken literally.
    const char *resolved = path;
    if (std::strcmp(path, "1") == 0) resolved = "rb3_session_trace.jsonl";

    gRec.sink = std::fopen(resolved, "w");
    if (!gRec.sink) {
        gRec.state = -1;     // open failed; never retry
        return;
    }

    gRec.state = 1;

    // Knobs.
    gRec.ringCap = (size_t)ParseIntEnv("RB3_TRACE_RING", 16384);
    if (gRec.ringCap < 16) gRec.ringCap = 16;
    gRec.ring.assign(gRec.ringCap, TraceEvent());
    gRec.ringHead = gRec.ringCount = 0;
    gRec.droppedTotal = 0;

    gRec.frameLongMs = ParseFloatEnv("RB3_TRACE_FRAME_MS", 20.0f);
    gRec.frameDecim  = ParseIntEnv("RB3_TRACE_FRAME_DECIMATE", 30);
    if (frameAlias) gRec.frameDecim = 1;   // back-compat: every frame

    // Identity + header fields.
    gRec.clientSeq  = 0;
    gRec.sid        = MintSid();
    gRec.startedIso = IsoUtcNow();
    const char *git = std::getenv("RB3_BUILD_SHA");
    gRec.buildGit   = (git && git[0]) ? std::string(git) : std::string();
    gRec.hdrWritten = false;

    gRec.interner.reset();
    gRec.lastInputBits = 0;
    gRec.haveInput     = false;

    // Pin the monotonic clock base now (t=0 at init).
    gRec.clockBaseSet = false;
    NowMs();

    // Arm both gates: ours + the engine's ld/st counters.
    gRB3TraceActive  = true;
    gFrameTraceActive = true;

    WriteHeader();
}

void RB3TraceSetFrame(int frame) {
    gRB3TraceFrame = frame;
}

void RB3RecordFrame(float dt, float lp, float lpu, const char *scr, int pend) {
    if (!gRB3TraceActive) return;
    // Read + ZERO the engine asset-event counters (same contract as the old TU).
    int ld = gFrameTraceLoaderAdds;
    int st = gFrameTraceStreamOpens;
    gFrameTraceLoaderAdds  = 0;
    gFrameTraceStreamOpens = 0;

    // fr decimation (§4.7): always emit long frames + frames with asset events;
    // otherwise sample 1/N (decim 0 => long-frames-only; decim 1 => every frame).
    bool isLong  = dt > gRec.frameLongMs;
    bool hasAsset = (ld > 0 || st > 0);
    bool sampled = false;
    if (gRec.frameDecim == 1) {
        sampled = true;
    } else if (gRec.frameDecim > 1) {
        sampled = (gRB3TraceFrame % gRec.frameDecim) == 0;
    }
    if (!isLong && !hasAsset && !sampled) return;

    TraceEvent e;
    std::memset(&e, 0, sizeof(e));
    e.kind = TK_FRAME;
    e.f    = gRB3TraceFrame;
    e.sm   = CurrentSongMs();
    e.fr.dt  = dt;
    e.fr.lp  = lp;
    e.fr.lpu = lpu;
    e.fr.pend = (uint16_t)(pend < 0 ? 0 : (pend > 0xFFFF ? 0xFFFF : pend));
    e.fr.scrId = gRec.interner.intern(scr ? scr : "?");
    e.fr.ld = ld;
    e.fr.st = st;
    PushEvent(e);
}

void RB3RecordInput(int pad, uint32_t bits, uint32_t dn, uint32_t up,
                    float whammy, float tilt) {
    if (!gRB3TraceActive) return;
    // Edge-only: skip if the bitmask is unchanged vs the last recorded input.
    if (gRec.haveInput && bits == gRec.lastInputBits) return;
    gRec.lastInputBits = bits;
    gRec.haveInput     = true;

    TraceEvent e;
    std::memset(&e, 0, sizeof(e));
    e.kind = TK_INPUT;
    e.f    = gRB3TraceFrame;
    e.sm   = CurrentSongMs();
    e.pad  = (uint8_t)pad;
    e.in.bits = bits;
    e.in.dn   = dn;
    e.in.up   = up;
    e.in.whammy = (int16_t)(whammy * 1000.0f + (whammy >= 0 ? 0.5f : -0.5f));
    e.in.tilt   = (int16_t)(tilt   * 1000.0f + (tilt   >= 0 ? 0.5f : -0.5f));
    PushEvent(e);
}

void RB3RecordNav(const char *from, const char *to, const char *focus,
                  bool wentBack) {
    if (!gRB3TraceActive) return;
    TraceEvent e;
    std::memset(&e, 0, sizeof(e));
    e.kind = TK_NAV;
    e.f    = gRB3TraceFrame;
    e.sm   = CurrentSongMs();
    e.nav.fromId   = gRec.interner.intern(from);
    e.nav.toId     = gRec.interner.intern(to);
    e.nav.focusId  = gRec.interner.intern(focus);
    e.nav.wentBack = wentBack ? 1 : 0;
    PushEvent(e);
}

void RB3RecordBootMark(const char *phase) {
    if (!gRB3TraceActive) return;
    TraceEvent e;
    std::memset(&e, 0, sizeof(e));
    e.kind = TK_BOOT;
    e.f    = gRB3TraceFrame;
    e.sm   = CurrentSongMs();
    e.boot.phaseId = gRec.interner.intern(phase);
    PushEvent(e);
}

void RB3RecordEvent(RB3TraceKind k, const char *sym, uint32_t u32, float val) {
    if (!gRB3TraceActive) return;
    TraceEvent e;
    std::memset(&e, 0, sizeof(e));
    e.kind = (uint8_t)k;
    e.f    = gRB3TraceFrame;
    e.sm   = CurrentSongMs();
    e.gen.aId = gRec.interner.intern(sym);
    e.gen.u32 = u32;
    e.gen.val = val;
    PushEvent(e);
}

void RB3TraceFlush() {
    if (gRec.state != 1) return;
    DrainRing();
    if (gRec.sink) std::fflush(gRec.sink);
}

void RB3TraceShutdown() {
    if (gRec.sink) {
        DrainRing();
        std::fflush(gRec.sink);
        std::fclose(gRec.sink);
    }
    // Full reset so a test can RB3TraceInit a fresh file again.
    gRec.sink        = nullptr;
    gRec.state       = 0;
    gRec.clientSeq   = 0;
    gRec.sid.clear();
    gRec.hdrWritten  = false;
    gRec.ring.clear();
    gRec.ringCap     = 16384;
    gRec.ringHead    = 0;
    gRec.ringCount   = 0;
    gRec.droppedTotal = 0;
    gRec.frameLongMs = 20.0f;
    gRec.frameDecim  = 30;
    gRec.interner.reset();
    gRec.clockBaseSet  = false;
    gRec.lastInputBits = 0;
    gRec.haveInput     = false;
    gRec.startedIso.clear();
    gRec.buildGit.clear();

    gRB3TraceActive   = false;
    gFrameTraceActive = false;
    gRB3TraceFrame    = 0;
}

// ===========================================================================
// Back-compat shim — the existing src/App.cpp:809 call. Lazy-inits on first call
// (so a native run with only RB3_FRAME_TRACE set still arms the recorder), sets
// the frame, then forwards to RB3RecordFrame (which reads+zeroes the engine
// counters). If the recorder isn't armed (no env), this resets the counters and
// returns — exactly the old TU's not-requested behavior.
// ===========================================================================
void RB3FrameTraceRecord(int frame, float dtMs, float loadPollMs,
                         float loadPollUntilMs, const char *screen,
                         int pendingLoaders) {
    if (gRec.state == 0) RB3TraceInit();
    if (gRec.state != 1) {
        // Tracing off: still reset the per-frame counters so they don't grow.
        gFrameTraceLoaderAdds  = 0;
        gFrameTraceStreamOpens = 0;
        return;
    }
    RB3TraceSetFrame(frame);
    RB3RecordFrame(dtMs, loadPollMs, loadPollUntilMs,
                   screen ? screen : "?", pendingLoaders);
}

#endif // HX_NATIVE
