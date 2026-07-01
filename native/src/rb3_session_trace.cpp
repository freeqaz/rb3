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

// Web egress (D5 §7): the browser build pushes serialized NDJSON chunks into a
// JS-side array (window.__rb3Trace) that the pre-js flusher drains over a POST.
// Fully guarded so the NATIVE build is byte-unaffected (no emscripten symbols).
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// ---------------------------------------------------------------------------
// Engine-side counters — DEFINED in src/system/utl/Loader.cpp. The increments
// live behind gFrameTraceActive, so the recorder arms that flag when it opens a
// sink and reads+zeroes the per-frame counters from RB3RecordFrame.
// ---------------------------------------------------------------------------
extern bool gFrameTraceActive;
extern int  gFrameTraceLoaderAdds;
extern int  gFrameTraceStreamOpens;

// Incremental-load-perf (PLAN.md T1) per-frame ATTRIBUTION counters. Also DEFINED
// in src/system/utl/Loader.cpp; incremented behind gFrameTraceActive at the
// engine choke points (DirLoader/StandardStream/DataFile/ChunkStream/Loader). We
// only extern them here; RB3RecordFrame reads + zeroes them each frame so the
// RB3_FRAME_TRACE back-compat line carries master's full per-frame attribution
// (field names preserved so scripts/native/frame_profiler.py + the load-perf
// tooling keep working). This subsumes the old rb3_frame_trace.cpp (deleted).
extern float  gFetchSyncMsThisFrame;
extern int    gFetchSyncCountThisFrame;
extern double gFetchSyncBytesThisFrame;
extern float  gDtaParseMsThisFrame;
extern float  gObjLoadMsThisFrame;
extern float  gObjLoadWorstMs;
extern char   gObjLoadWorstName[64];
extern float  gAudioPrimeMsThisFrame;
extern float  gTexUploadMsThisFrame;
extern int    gTexUploadCountThisFrame;
extern float  gMeshUploadMsThisFrame;
extern int    gMeshUploadCountThisFrame;
extern float  gVertUnpackMsThisFrame;
extern int    gVertUnpackCountThisFrame;
extern float  gPipelineCreateMsThisFrame;
extern int    gPipelineCreateCountThisFrame;
extern float  gStreamReadMsThisFrame;

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
        // sdt = sim dt in SECONDS (the menu/UI clock advance this frame, from the
        // TaskMgr.mTime cycle delta), captured so RB3_REPLAY_FIXED_CLOCK can drive
        // seam 1 (Task.cpp) deterministically. Serialized only when > 0.
        // atr = per-frame load-perf ATTRIBUTION (ported from the deleted
        // rb3_frame_trace.cpp). Read+zeroed from the engine gFrameTrace* counters
        // each frame; serialized with master's exact field names so the
        // RB3_FRAME_TRACE back-compat line keeps frame_profiler.py + the load-perf
        // tooling working. objWNm is interned (objWNmId). All zero => a plain fr.
        struct {
            float dt, lp, lpu, sdt; uint16_t pend; uint16_t scrId; int ld, st;
            struct {
                float  fetchMs; int fetchN; double fetchB;
                float  dtaMs, objMs, objWMs; uint16_t objWNmId;
                float  primeMs, texMs; int texN;
                float  meshMs; int meshN; float unpackMs; int unpackN;
                float  pipeMs; int pipeN; float inflMs;
            } atr;
        } fr;
        struct { uint32_t bits, dn, up; int16_t whammy, tilt; } in;
        struct { uint16_t fromId, toId, focusId; uint8_t wentBack; } nav;
        struct { uint16_t phaseId; } boot;
        // song: ev/id/track/diff interned; score/pct omitted when < 0.
        struct { uint16_t evId, idId, trackId, diffId; float score, pct; } song;
        // au: underrun event count + underrun-frame count.
        struct { int under, frames; } au;
        // log: lvl/msg interned; src interned (0 = omit).
        struct { uint16_t lvlId, msgId, srcId; } log;
        // mark: tag interned; note interned (0 = omit).
        struct { uint16_t tagId, noteId; } mark;
        // chk (M4 replay checkpoint): the fast-equality hash + RAW fields. scr/
        // focus interned; taskSec/beat/pct + per-player scores carried raw; the
        // exact scoreSum/nPlayers feed the hash. `sm` rides the envelope (e.sm).
        struct {
            uint64_t h;            // FNV-1a over the quantized state tuple
            int64_t  scoreSum;     // exact sum of all active Player::GetScore()
            int      scores[RB3_CHK_MAX_SCORES];  // per-player raw (capped)
            float    taskSec, beat;
            int32_t  pct;          // GetPercentComplete (-1 = omit)
            uint16_t scrId, focusId;
            uint8_t  nScores, nPlayers;
        } chk;
        // clk (M4 per-frame clock): the un-decimated sim-dt + song-ms for THIS
        // frame. sdt = menu/UI sim seconds advanced this frame (replay seam 1);
        // `sm` rides the envelope (e.sm, < 0 => omitted in menus). Emitted every
        // frame (no decimation) so the replay can feed the EXACT recorded clock at
        // each frame N (vs the decimated fr table's stale carry-forward).
        struct { float sdt; } clk;
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

    // RB3_FRAME_TRACE back-compat alias mode (vs RB3_SESSION_TRACE). When true the
    // hdr is a `#`-comment line (skipped by frame_profiler.py) instead of the
    // {"k":"hdr"} envelope line, so the master frame-trace consumers parse cleanly.
    bool        frameCompat  = false;

    Interner    interner;

    // Monotonic clock base — first init pins t=0.
    std::chrono::steady_clock::time_point clockBase;
    bool        clockBaseSet = false;

    // Input edge-detect: last recorded full bitmask (per pad 0).
    uint32_t    lastInputBits = 0;
    bool        haveInput      = false;

    // Current song ms (D2 §4.5); < 0 => not in a song => envelope omits `sm`.
    // Wave 2 / D6 set this each frame via RB3TraceSetSongMs.
    float       songMs         = -1.0f;

    // Current sim dt in SECONDS (menu/UI clock advance this frame), set by the
    // frame tap via RB3TraceSetSimDt from the TaskMgr.mTime cycle delta. Stamped
    // into the fr row so RB3_REPLAY_FIXED_CLOCK seam 1 can replay it. <= 0 => omit.
    float       simDt          = 0.0f;

    std::string startedIso;   // hdr.started
    std::string buildGit;     // hdr.build.git
    std::string platform = "native";  // hdr.platform ("web" under __EMSCRIPTEN__)

    // M4 GAP 1 — boot RNG seed (gRand). Stamped into the hdr `seed` field so a
    // replay can re-seed gRand identically. seedSet=false => the boot path never
    // reached the SeedRand site (e.g. a recorder-core gtest with no engine) => the
    // hdr omits `seed`.
    int         randSeed       = 0;
    bool        seedSet        = false;

    // M4 GAP 2 — active run aids (autohit/nofail) the recording enabled. Out-of-
    // band toggles (HTTP /api/input verb, or the RB3_GAME_INPUT script) that are
    // NOT replayable `in` edges; summarized in hdr.flags.aids so a reader sees
    // them, and emitted per-aid as a one-shot `mark{tag:"aid"}` at the applied
    // frame (the replayable record — see RB3TraceRecordAid). Bit 0 = autohit,
    // bit 1 = nofail.
    unsigned    aids           = 0;
};

enum { RB3_AID_AUTOHIT = 1u << 0, RB3_AID_NOFAIL = 1u << 1 };

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
        case TK_CHK:   out += "chk";  break;
        case TK_CLK:   out += "clk";  break;
        default:       out += "?";    break;
    }
    out += '"';

    char num[24];
    switch (e.kind) {
        case TK_FRAME: {
            out += ",\"dt\":";  AppendNum3(out, e.fr.dt);
            out += ",\"lp\":";  AppendNum3(out, e.fr.lp);
            out += ",\"lpu\":"; AppendNum3(out, e.fr.lpu);
            // sdt = sim seconds advanced this frame (menu/UI clock); replay seam 1
            // drives kTaskSeconds from it under RB3_REPLAY_FIXED_CLOCK. Omitted at 0.
            if (e.fr.sdt > 0.0f) { out += ",\"sdt\":"; AppendNum3(out, e.fr.sdt); }
            out += ",\"scr\":\"";
            JsonEscape(out, gRec.interner.str(e.fr.scrId));
            out += '"';
            std::snprintf(num, sizeof(num), ",\"ld\":%d", e.fr.ld); out += num;
            std::snprintf(num, sizeof(num), ",\"st\":%d", e.fr.st); out += num;
            std::snprintf(num, sizeof(num), ",\"pend\":%u", (unsigned)e.fr.pend); out += num;
            // Load-perf ATTRIBUTION (ported from the deleted rb3_frame_trace.cpp).
            // Master's exact field names + precision so scripts/native/
            // frame_profiler.py + the load-perf .mjs tooling read the same keys off
            // the RB3_FRAME_TRACE line (they use .get()/|| 0, so the extra session
            // envelope keys are harmless). Emitted for every fr row.
            {
                const char *wn = gRec.interner.str(e.fr.atr.objWNmId);
                out += ",\"fetchMs\":"; AppendNum3(out, e.fr.atr.fetchMs);
                std::snprintf(num, sizeof(num), ",\"fetchN\":%d", e.fr.atr.fetchN); out += num;
                std::snprintf(num, sizeof(num), ",\"fetchB\":%.0f", e.fr.atr.fetchB); out += num;
                out += ",\"dtaMs\":"; AppendNum3(out, e.fr.atr.dtaMs);
                out += ",\"objMs\":"; AppendNum3(out, e.fr.atr.objMs);
                out += ",\"objWMs\":"; AppendNum3(out, e.fr.atr.objWMs);
                out += ",\"objWNm\":\""; JsonEscape(out, wn); out += '"';
                out += ",\"primeMs\":"; AppendNum3(out, e.fr.atr.primeMs);
                out += ",\"texMs\":"; AppendNum3(out, e.fr.atr.texMs);
                std::snprintf(num, sizeof(num), ",\"texN\":%d", e.fr.atr.texN); out += num;
                out += ",\"meshMs\":"; AppendNum3(out, e.fr.atr.meshMs);
                std::snprintf(num, sizeof(num), ",\"meshN\":%d", e.fr.atr.meshN); out += num;
                out += ",\"unpackMs\":"; AppendNum3(out, e.fr.atr.unpackMs);
                std::snprintf(num, sizeof(num), ",\"unpackN\":%d", e.fr.atr.unpackN); out += num;
                out += ",\"pipeMs\":"; AppendNum3(out, e.fr.atr.pipeMs);
                std::snprintf(num, sizeof(num), ",\"pipeN\":%d", e.fr.atr.pipeN); out += num;
                out += ",\"inflMs\":"; AppendNum3(out, e.fr.atr.inflMs);
            }
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
        case TK_SONG: {
            // song{ev,id,track,diff} + score/pct only when >= 0 (OQ7-deferred).
            out += ",\"ev\":\"";    JsonEscape(out, gRec.interner.str(e.song.evId));    out += '"';
            out += ",\"id\":\"";    JsonEscape(out, gRec.interner.str(e.song.idId));    out += '"';
            out += ",\"track\":\""; JsonEscape(out, gRec.interner.str(e.song.trackId)); out += '"';
            out += ",\"diff\":\"";  JsonEscape(out, gRec.interner.str(e.song.diffId));  out += '"';
            if (e.song.score >= 0.0f) {
                out += ",\"score\":";
                AppendNum3(out, e.song.score);
            }
            if (e.song.pct >= 0.0f) {
                out += ",\"pct\":";
                AppendNum3(out, e.song.pct);
            }
            break;
        }
        case TK_AU: {
            // au{under,frames}.
            std::snprintf(num, sizeof(num), ",\"under\":%d", e.au.under);   out += num;
            std::snprintf(num, sizeof(num), ",\"frames\":%d", e.au.frames); out += num;
            break;
        }
        case TK_LOG: {
            // log{lvl,msg} + src when present.
            out += ",\"lvl\":\""; JsonEscape(out, gRec.interner.str(e.log.lvlId)); out += '"';
            out += ",\"msg\":\""; JsonEscape(out, gRec.interner.str(e.log.msgId)); out += '"';
            if (e.log.srcId != 0) {
                out += ",\"src\":\"";
                JsonEscape(out, gRec.interner.str(e.log.srcId));
                out += '"';
            }
            break;
        }
        case TK_MARK: {
            // mark{tag} + note when present.
            out += ",\"tag\":\""; JsonEscape(out, gRec.interner.str(e.mark.tagId)); out += '"';
            if (e.mark.noteId != 0) {
                out += ",\"note\":\"";
                JsonEscape(out, gRec.interner.str(e.mark.noteId));
                out += '"';
            }
            break;
        }
        case TK_CHK: {
            // chk: the fast-equality hash `h` (hex) + RAW state fields. `sm` rides
            // the envelope (omitted in menus). scr/focus/sec/beat/score[]/pct are
            // emitted RAW so trace-diff can ε-classify a hash mismatch field-by-field.
            std::snprintf(num, sizeof(num), ",\"h\":\"%016llx\"",
                          (unsigned long long)e.chk.h);
            out += num;
            out += ",\"scr\":\"";   JsonEscape(out, gRec.interner.str(e.chk.scrId));   out += '"';
            out += ",\"focus\":\""; JsonEscape(out, gRec.interner.str(e.chk.focusId)); out += '"';
            out += ",\"sec\":";  AppendNum3(out, e.chk.taskSec);
            out += ",\"beat\":"; AppendNum3(out, e.chk.beat);
            std::snprintf(num, sizeof(num), ",\"score\":%lld",
                          (long long)e.chk.scoreSum);
            out += num;
            // Per-player raw scores (array). Empty array in menus (nScores=0).
            out += ",\"scores\":[";
            for (int i = 0; i < (int)e.chk.nScores && i < RB3_CHK_MAX_SCORES; ++i) {
                if (i) out += ',';
                std::snprintf(num, sizeof(num), "%d", e.chk.scores[i]);
                out += num;
            }
            out += ']';
            std::snprintf(num, sizeof(num), ",\"np\":%u", (unsigned)e.chk.nPlayers);
            out += num;
            // pct raw only when >= 0 (omit when unknown, mirroring song.pct).
            if (e.chk.pct >= 0) {
                std::snprintf(num, sizeof(num), ",\"pct\":%d", (int)e.chk.pct);
                out += num;
            }
            break;
        }
        case TK_CLK: {
            // clk: the un-decimated per-frame clock. `f` rides the envelope; `sm`
            // rides the envelope too (omitted in menus). `sdt` is the sim seconds
            // advanced this frame (replay seam 1) — ALWAYS emitted (even at 0) so
            // the replay's exact per-frame lookup never has to guess a missing dt.
            out += ",\"sdt\":"; AppendNum3(out, e.clk.sdt);
            break;
        }
        default: break;
    }
    out += "}\n";
}

// ---------------------------------------------------------------------------
// Web egress sink (D5 §7) — FROZEN JS-global contract for agent F's pre-js
// flusher: window.__rb3Sid (string), window.__rb3Trace (array of NDJSON-string
// chunks), window.__rb3TraceOn (bool). On native these are no-ops (no
// emscripten symbols), so the native build is byte-unaffected.
// ---------------------------------------------------------------------------

// True when the web build should emit (web sink active). Native: always false
// here — native uses the FILE* sink. (gRec.sink stays null on web.)
bool HaveWebSink() {
#ifdef __EMSCRIPTEN__
    return true;
#else
    return false;
#endif
}

// Publish the C++-minted sid to window.__rb3Sid (string) so the pre-js POST
// route, the hdr line, and the SQLite PK all agree. Read window.__rb3TraceOn
// (default true) → return whether tracing should be armed.
bool WebEgressPublishSidAndReadToggle(const char *sid) {
#ifdef __EMSCRIPTEN__
    // Publish sid; create __rb3Trace as a plain array if pre-js hasn't yet.
    EM_ASM({
        var s = UTF8ToString($0);
        window.__rb3Sid = s;
        if (!Array.isArray(window.__rb3Trace)) window.__rb3Trace = [];
    }, sid);
    // __rb3TraceOn defaults to true when unset.
    int on = EM_ASM_INT({
        return (typeof window.__rb3TraceOn === 'undefined' ||
                window.__rb3TraceOn) ? 1 : 0;
    });
    return on != 0;
#else
    (void)sid;
    return false;
#endif
}

// Push one serialized NDJSON chunk (one-or-more '\n'-terminated lines) into the
// window.__rb3Trace array that the pre-js flusher drains.
void WebEgressPush(const std::string &chunk) {
#ifdef __EMSCRIPTEN__
    EM_ASM({
        var t = window.__rb3Trace;
        if (!Array.isArray(t)) { t = window.__rb3Trace = []; }
        t.push(UTF8ToString($0, $1));
    }, chunk.data(), (int)chunk.size());
#else
    (void)chunk;
#endif
}

// ---------------------------------------------------------------------------
// Header line. k=hdr,v=1,sid,cs=0,platform,started,build{git},flags{}.
// No t/f/sm. cs is 0 (the first stamped client_seq). Goes to whichever sink is
// active (native FILE* or the web __rb3Trace array).
// ---------------------------------------------------------------------------
void WriteHeader() {
    if (gRec.hdrWritten) return;
    if (!gRec.sink && !HaveWebSink()) return;
    // RB3_FRAME_TRACE back-compat alias: master's frame trace opened with a
    // `#`-comment header line that scripts/native/frame_profiler.py (and the
    // load-perf .mjs tooling) SKIP (they do `line.startswith("#")`). Emitting the
    // {"k":"hdr"} JSON envelope instead would make frame_profiler's unconditional
    // r["dt"] read KeyError on the header row. So in frameCompat mode we write the
    // master-shaped comment header (carrying the sid for provenance) and suppress
    // the JSON hdr envelope. The per-frame `fr` rows still carry every attribution
    // field with master's exact names.
    if (gRec.frameCompat) {
        std::string h = "# rb3 session trace (RB3_FRAME_TRACE alias) sid=";
        h += gRec.sid;
        h += " f=frame dt=frameMs lp=loadPollMs lpu=pollUntilMs scr=screen "
             "ld=loaderAdds st=streamOpens pend=pendingLoaders "
             "fetchMs/fetchN/fetchB dtaMs objMs objWMs/objWNm primeMs texMs/texN "
             "meshMs/meshN unpackMs/unpackN pipeMs/pipeN inflMs\n";
        if (gRec.sink) {
            std::fwrite(h.data(), 1, h.size(), gRec.sink);
            std::fflush(gRec.sink);
        } else {
            WebEgressPush(h);
        }
        gRec.hdrWritten = true;
        if (gRec.clientSeq == 0) gRec.clientSeq = 1;
        return;
    }
    std::string out;
    out += "{\"k\":\"hdr\",\"v\":1,";
    AppendQuotedKV(out, "sid", gRec.sid.c_str());
    out += ",\"cs\":0,";
    AppendQuotedKV(out, "platform", gRec.platform.c_str());
    out += ',';
    AppendQuotedKV(out, "started", gRec.startedIso.c_str());
    out += ",\"build\":{";
    AppendQuotedKV(out, "git", gRec.buildGit.c_str());
    out += "}";
    // M4 GAP 1 — boot RNG seed (gRand). Additive; omitted if the boot path never
    // set it (recorder-core tests with no engine). A replay reads this back to
    // re-seed gRand before the first RandomInt (rb3_replay.cpp RB3ReplaySeed).
    if (gRec.seedSet) {
        char sb[32];
        std::snprintf(sb, sizeof(sb), ",\"seed\":%d", gRec.randSeed);
        out += sb;
    }
    // flags{} — summarizes out-of-band run state for a human reader. aids carries
    // the run aids (autohit/nofail) that were enabled; the per-aid replay record
    // is the one-shot mark{tag:"aid"} events (RB3TraceRecordAid). aids is the
    // bitmask AT FLUSH/SHUTDOWN time; since aids only ever turn ON in a session,
    // the hdr's snapshot is a best-effort summary and the marks are authoritative.
    out += ",\"flags\":{";
    if (gRec.aids) {
        out += "\"aids\":[";
        bool first = true;
        if (gRec.aids & RB3_AID_AUTOHIT) { out += "\"autohit\""; first = false; }
        if (gRec.aids & RB3_AID_NOFAIL)  { if (!first) out += ','; out += "\"nofail\""; }
        out += "]";
    }
    out += "}}\n";
    if (gRec.sink) {
        std::fwrite(out.data(), 1, out.size(), gRec.sink);
        std::fflush(gRec.sink);
    } else {
        WebEgressPush(out);
    }
    gRec.hdrWritten = true;
    // hdr consumes cs=0; subsequent events start at cs=1.
    if (gRec.clientSeq == 0) gRec.clientSeq = 1;
}

// ---------------------------------------------------------------------------
// Drain the ring to the active sink (oldest first): native FILE* (+fflush) or
// the web __rb3Trace array (one pushed chunk per drain).
// ---------------------------------------------------------------------------
void DrainRing() {
    if (gRec.ringCount == 0) return;
    if (!gRec.sink && !HaveWebSink()) return;
    std::string out;
    out.reserve(gRec.ringCount * 64);
    size_t start = (gRec.ringHead + gRec.ringCap - gRec.ringCount) % gRec.ringCap;
    for (size_t i = 0; i < gRec.ringCount; ++i) {
        const TraceEvent &e = gRec.ring[(start + i) % gRec.ringCap];
        SerializeEvent(out, e);
    }
    if (gRec.sink) {
        std::fwrite(out.data(), 1, out.size(), gRec.sink);
        std::fflush(gRec.sink);
    } else {
        WebEgressPush(out);
    }
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
    // Full. First: scan oldest->newest for an fr/clk to drop (decimate the high-
    // volume statistical kinds first; clk is per-frame like fr, so it is equally
    // droppable under pressure — never a protected replay/diagnostic kind).
    size_t start = (gRec.ringHead + gRec.ringCap - gRec.ringCount) % gRec.ringCap;
    for (size_t i = 0; i < gRec.ringCount; ++i) {
        size_t idx = (start + i) % gRec.ringCap;
        if (gRec.ring[idx].kind == TK_FRAME || gRec.ring[idx].kind == TK_CLK) {
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
// Push one event into the ring (stamps t/cs), then drain to the active sink.
//  - Native: stream to the FILE* each push so a SIGTERM leaves valid NDJSON.
//  - Web: amortize the EM_ASM crossing — accumulate in the ring and only drain
//    a multi-line chunk to window.__rb3Trace once it half-fills (RB3TraceFlush,
//    driven by the pre-js timer, drains the remainder on cadence). (D5 §7)
// ---------------------------------------------------------------------------
void PushEvent(TraceEvent &e) {
    // RB3_FRAME_TRACE back-compat alias: master's frame trace was a PURE per-frame
    // JSONL (only `fr` rows after the `#` header). scripts/native/frame_profiler.py
    // reads r["dt"] on EVERY non-`#` line, so interleaving the session stream's
    // clk/log/nav/boot/chk rows (which have no `dt`) would KeyError it. In this
    // alias mode we emit ONLY `fr` rows — the full multi-kind stream is what
    // RB3_SESSION_TRACE is for. (The fr rows still carry every attribution field.)
    if (gRec.frameCompat && e.kind != TK_FRAME) return;
    if (!gRec.hdrWritten) WriteHeader();
    e.t  = NowMs();
    e.cs = gRec.clientSeq++;
    MakeRoom();
    gRec.ring[gRec.ringHead] = e;
    gRec.ringHead = (gRec.ringHead + 1) % gRec.ringCap;
    if (gRec.ringCount < gRec.ringCap) gRec.ringCount++;
    if (gRec.sink) {
        DrainRing();
    } else if (HaveWebSink() && gRec.ringCount * 2 >= gRec.ringCap) {
        DrainRing();
    }
}

// Snapshot the current song ms. RB3TraceSetSongMs stores the latest value
// (Wave 2 wires the real GetBeatMaster()->GetAudio()->GetTime() chain into it);
// < 0 => "not in a song" => the envelope omits `sm` (D2 §4.5).
float CurrentSongMs() {
    return gRec.songMs;
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

    bool frameAlias = false;
    gRec.platform = "native";

#ifdef __EMSCRIPTEN__
    // ----- Web sink (D5 §7): no file. Mint sid in C++, publish it to JS, read
    // the JS opt-out toggle. The pre-js flusher drains window.__rb3Trace. -----
    gRec.sid = MintSid();
    bool armed = WebEgressPublishSidAndReadToggle(gRec.sid.c_str());
    if (!armed) {
        gRec.state = -1;     // ?notrace / __rb3TraceOn=false
        return;
    }
    gRec.platform = "web";
    gRec.sink = nullptr;     // web has no FILE* sink
#else
    // ----- Native sink: RB3_SESSION_TRACE, else RB3_FRAME_TRACE alias. -----
    const char *sessPath = std::getenv("RB3_SESSION_TRACE");
    const char *framePath = std::getenv("RB3_FRAME_TRACE");
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
    gRec.sid = MintSid();
#endif

    gRec.state = 1;

    // Knobs.
    gRec.ringCap = (size_t)ParseIntEnv("RB3_TRACE_RING", 16384);
    if (gRec.ringCap < 16) gRec.ringCap = 16;
    gRec.ring.assign(gRec.ringCap, TraceEvent());
    gRec.ringHead = gRec.ringCount = 0;
    gRec.droppedTotal = 0;

    gRec.frameLongMs = ParseFloatEnv("RB3_TRACE_FRAME_MS", 20.0f);
    gRec.frameDecim  = ParseIntEnv("RB3_TRACE_FRAME_DECIMATE", 30);
    gRec.frameCompat = frameAlias;
    if (frameAlias) gRec.frameDecim = 1;   // back-compat: every frame

    // Identity + header fields. (sid is minted per-sink above.)
    gRec.clientSeq  = 0;
    gRec.startedIso = IsoUtcNow();
    const char *git = std::getenv("RB3_BUILD_SHA");
    gRec.buildGit   = (git && git[0]) ? std::string(git) : std::string();
    gRec.hdrWritten = false;

    gRec.interner.reset();
    gRec.lastInputBits = 0;
    gRec.haveInput     = false;
    gRec.songMs        = -1.0f;

    // Pin the monotonic clock base now (t=0 at init).
    gRec.clockBaseSet = false;
    NowMs();

    // Arm both gates: ours + the engine's ld/st counters.
    gRB3TraceActive  = true;
    gFrameTraceActive = true;

    // NOTE: the hdr is written LAZILY on the first event (PushEvent calls
    // WriteHeader). M4 GAP 1 requires the boot RNG seed — set by SeedRand deep in
    // the App ctor's SystemInit, which runs AFTER RB3TraceInit — to land in the
    // hdr. So we deliberately DO NOT eagerly write the hdr here; the first real
    // event (a boot mark recorded post-ctor, see main_native.cpp) flushes a hdr
    // that already carries the seed. Recorder-core gtests that never set a seed
    // simply omit the `seed` field (seedSet stays false).
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

    // Read + ZERO the per-frame load-perf ATTRIBUTION counters (ported from the
    // deleted rb3_frame_trace.cpp). Snapshot ALL of them BEFORE the decimation
    // early-return so they never leak into a later frame (exactly the old TU's
    // read-then-zero-every-frame contract). The snapshot is emitted with master's
    // field names in SerializeEvent's fr path.
    float  aFetchMs = gFetchSyncMsThisFrame;   int aFetchN = gFetchSyncCountThisFrame;
    double aFetchB  = gFetchSyncBytesThisFrame;
    float  aDtaMs   = gDtaParseMsThisFrame;
    float  aObjMs   = gObjLoadMsThisFrame;     float aObjWMs = gObjLoadWorstMs;
    const char *aObjWNm = gObjLoadWorstName;
    float  aPrimeMs = gAudioPrimeMsThisFrame;
    float  aTexMs   = gTexUploadMsThisFrame;   int aTexN = gTexUploadCountThisFrame;
    float  aMeshMs  = gMeshUploadMsThisFrame;  int aMeshN = gMeshUploadCountThisFrame;
    float  aUnpMs   = gVertUnpackMsThisFrame;  int aUnpN = gVertUnpackCountThisFrame;
    float  aPipeMs  = gPipelineCreateMsThisFrame; int aPipeN = gPipelineCreateCountThisFrame;
    float  aInflMs  = gStreamReadMsThisFrame;
    gFetchSyncMsThisFrame = 0.0f;    gFetchSyncCountThisFrame = 0;
    gFetchSyncBytesThisFrame = 0.0;
    gDtaParseMsThisFrame = 0.0f;
    gObjLoadMsThisFrame = 0.0f;      gObjLoadWorstMs = 0.0f;  gObjLoadWorstName[0] = '\0';
    gAudioPrimeMsThisFrame = 0.0f;
    gTexUploadMsThisFrame = 0.0f;    gTexUploadCountThisFrame = 0;
    gMeshUploadMsThisFrame = 0.0f;   gMeshUploadCountThisFrame = 0;
    gVertUnpackMsThisFrame = 0.0f;   gVertUnpackCountThisFrame = 0;
    gPipelineCreateMsThisFrame = 0.0f; gPipelineCreateCountThisFrame = 0;
    gStreamReadMsThisFrame = 0.0f;

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
    e.fr.sdt = gRec.simDt;   // sim seconds this frame (replay seam 1 source)
    e.fr.pend = (uint16_t)(pend < 0 ? 0 : (pend > 0xFFFF ? 0xFFFF : pend));
    e.fr.scrId = gRec.interner.intern(scr ? scr : "?");
    e.fr.ld = ld;
    e.fr.st = st;
    // Attribution snapshot (master field names). objWNm interned like other strings.
    e.fr.atr.fetchMs = aFetchMs; e.fr.atr.fetchN = aFetchN; e.fr.atr.fetchB = aFetchB;
    e.fr.atr.dtaMs = aDtaMs; e.fr.atr.objMs = aObjMs; e.fr.atr.objWMs = aObjWMs;
    e.fr.atr.objWNmId = gRec.interner.intern((aObjWNm && aObjWNm[0]) ? aObjWNm : "");
    e.fr.atr.primeMs = aPrimeMs; e.fr.atr.texMs = aTexMs; e.fr.atr.texN = aTexN;
    e.fr.atr.meshMs = aMeshMs; e.fr.atr.meshN = aMeshN;
    e.fr.atr.unpackMs = aUnpMs; e.fr.atr.unpackN = aUnpN;
    e.fr.atr.pipeMs = aPipeMs; e.fr.atr.pipeN = aPipeN; e.fr.atr.inflMs = aInflMs;
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

void RB3RecordSong(const char *ev, const char *id, const char *track,
                   const char *diff, float score, float pct) {
    if (!gRB3TraceActive) return;
    TraceEvent e;
    std::memset(&e, 0, sizeof(e));
    e.kind = TK_SONG;
    e.f    = gRB3TraceFrame;
    e.sm   = CurrentSongMs();
    e.song.evId    = gRec.interner.intern(ev);
    e.song.idId    = gRec.interner.intern(id);
    e.song.trackId = gRec.interner.intern(track);
    e.song.diffId  = gRec.interner.intern(diff);
    e.song.score   = score;   // < 0 => omitted by the serializer
    e.song.pct     = pct;      // < 0 => omitted by the serializer
    PushEvent(e);
}

void RB3RecordAudio(int under, int frames) {
    if (!gRB3TraceActive) return;
    TraceEvent e;
    std::memset(&e, 0, sizeof(e));
    e.kind = TK_AU;
    e.f    = gRB3TraceFrame;
    e.sm   = CurrentSongMs();
    e.au.under  = under;
    e.au.frames = frames;
    PushEvent(e);
}

void RB3RecordLog(const char *lvl, const char *msg, const char *src) {
    if (!gRB3TraceActive) return;
    TraceEvent e;
    std::memset(&e, 0, sizeof(e));
    e.kind = TK_LOG;
    e.f    = gRB3TraceFrame;
    e.sm   = CurrentSongMs();
    e.log.lvlId = gRec.interner.intern(lvl);
    e.log.msgId = gRec.interner.intern(msg);
    // src is optional: a null pointer interns to id 0 => the serializer omits it.
    e.log.srcId = src ? gRec.interner.intern(src) : 0;
    PushEvent(e);
}

void RB3RecordMark(const char *tag, const char *note) {
    if (!gRB3TraceActive) return;
    TraceEvent e;
    std::memset(&e, 0, sizeof(e));
    e.kind = TK_MARK;
    e.f    = gRB3TraceFrame;
    e.sm   = CurrentSongMs();
    e.mark.tagId  = gRec.interner.intern(tag);
    // note is optional: null => id 0 => omitted.
    e.mark.noteId = note ? gRec.interner.intern(note) : 0;
    PushEvent(e);
}

// ---------------------------------------------------------------------------
// M4 GAP 1 — boot RNG seed. The boot path (System.cpp HX_NATIVE SeedRand site)
// hands us the EXACT scalar it fed to gRand. Stored for the lazily-written hdr's
// `seed` field. Always recorded (even when tracing is off-armed but seedSet is
// only read in WriteHeader, which no-ops when not armed) so the value is captured
// regardless of arm-order; the seed must be set BEFORE the first event so the hdr
// carries it. Idempotent-ish: a later call overwrites (the boot path calls once).
// ---------------------------------------------------------------------------
void RB3TraceSetSeed(int seed) {
    gRec.randSeed = seed;
    gRec.seedSet  = true;
}

// ---------------------------------------------------------------------------
// M4 GAP 2 — run aid (autohit/nofail) capture. Called from the native game-input
// path the instant an aid is actually applied (ExecAutohit/ExecNoFail). Records:
//   (a) a one-shot mark{tag:"aid",note:<aid>} at the CURRENT frame — the
//       authoritative, replayable record (replay re-applies the aid when it
//       reaches this frame; see rb3_replay.cpp RB3ReplayAids), and
//   (b) sets the hdr.flags.aids summary bit (best-effort: only reflected in the
//       hdr if the hdr is still unwritten, but the mark is always emitted).
// De-duped: the same aid re-applied (HTTP verb fired twice) records once.
// ---------------------------------------------------------------------------
void RB3TraceRecordAid(const char *aid) {
    if (!gRB3TraceActive || !aid) return;
    unsigned bit = 0;
    if (std::strcmp(aid, "autohit") == 0)     bit = RB3_AID_AUTOHIT;
    else if (std::strcmp(aid, "nofail") == 0) bit = RB3_AID_NOFAIL;
    if (bit && (gRec.aids & bit)) return;   // already recorded this aid
    gRec.aids |= bit;
    // One-shot session marker at the current frame. tag "aid" + note=<aid name>.
    // RB3RecordMark interns + stamps t/f/cs and pushes (which lazily writes the
    // hdr if still pending — by which point seed/aids summary are already set).
    RB3RecordMark("aid", aid);
}

// ---------------------------------------------------------------------------
// M4 checkpoint helpers (file-local). FNV-1a over the quantized state tuple, so
// run-to-run equality survives benign x86-vs-x86 float drift while the exact
// integer scoreSum + nPlayers stay un-quantized (a 1-point score divergence must
// trip the hash). MUST stay byte-stable across record + replay runs of the same
// build, so it hashes a fixed-width little-endian byte stream (no struct padding,
// no host-endian ambiguity).
// ---------------------------------------------------------------------------
namespace {

const uint64_t kFnvOffset = 1469598103934665603ULL;
const uint64_t kFnvPrime  = 1099511628257ULL;

void FnvBytes(uint64_t &h, const void *p, size_t n) {
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < n; ++i) {
        h ^= (uint64_t)b[i];
        h *= kFnvPrime;
    }
}
void FnvStr(uint64_t &h, const char *s) {
    if (s) FnvBytes(h, s, std::strlen(s));
    h ^= 0; h *= kFnvPrime;   // NUL terminator: separates "ab"|"c" from "a"|"bc"
}
void FnvI64(uint64_t &h, int64_t v) {
    unsigned char le[8];
    uint64_t u = (uint64_t)v;
    for (int i = 0; i < 8; ++i) le[i] = (unsigned char)((u >> (8 * i)) & 0xFF);
    FnvBytes(h, le, 8);
}
// Quantize a float to integer units of `step` (e.g. 1ms => step=1.0 over ms,
// 0.01 beat => step=0.01), then hash the int64. round-half-away-from-zero.
void FnvQuant(uint64_t &h, float v, float step) {
    double q = (double)v / (double)step;
    int64_t qi = (int64_t)(q >= 0 ? q + 0.5 : q - 0.5);
    FnvI64(h, qi);
}

} // namespace

void RB3RecordCheckpoint(const char *scr, const char *focus,
                         float taskSec, float beat, float songMs,
                         long scoreSum, const int *scores, int nScores,
                         int nPlayers, int pct) {
    if (!gRB3TraceActive) return;

    // Fast-equality hash over the quantized tuple (task contract order):
    //   [ scr, focus, q(taskSec,1ms), q(beat,0.01), q(songMs,1ms),
    //     scoreSum(exact), nPlayers ]
    // taskSec is in SECONDS -> quantize to 1ms == step 0.001s. songMs is already
    // in ms -> step 1.0. < 0 songMs (menus) hashes as -1 (sentinel) so a
    // menu->song transition perturbs the hash.
    uint64_t h = kFnvOffset;
    FnvStr(h, scr ? scr : "");
    FnvStr(h, focus ? focus : "");
    FnvQuant(h, taskSec, 0.001f);                 // 1ms
    FnvQuant(h, beat, 0.01f);                      // 0.01 beat
    FnvI64(h, songMs >= 0.0f ? (int64_t)(songMs + 0.5f) : (int64_t)-1);  // 1ms
    FnvI64(h, (int64_t)scoreSum);                  // exact
    FnvI64(h, (int64_t)nPlayers);                  // exact

    TraceEvent e;
    std::memset(&e, 0, sizeof(e));
    e.kind = TK_CHK;
    e.f    = gRB3TraceFrame;
    // Hash uses the caller-supplied songMs; the envelope `sm` follows the same
    // value (so a chk row's sm is the checkpointed song clock, not a stale frame
    // snapshot). < 0 => menus => omitted from the wire.
    e.sm   = songMs;
    e.chk.h        = h;
    e.chk.scoreSum = (int64_t)scoreSum;
    e.chk.taskSec  = taskSec;
    e.chk.beat     = beat;
    e.chk.pct      = pct;
    e.chk.scrId    = gRec.interner.intern(scr ? scr : "");
    e.chk.focusId  = gRec.interner.intern(focus ? focus : "");
    int n = nScores;
    if (n < 0) n = 0;
    if (n > RB3_CHK_MAX_SCORES) n = RB3_CHK_MAX_SCORES;
    for (int i = 0; i < n; ++i) e.chk.scores[i] = scores ? scores[i] : 0;
    e.chk.nScores  = (uint8_t)n;
    e.chk.nPlayers = (uint8_t)(nPlayers < 0 ? 0 : (nPlayers > 255 ? 255 : nPlayers));
    PushEvent(e);
}

void RB3TraceSetSongMs(float ms) {
    gRec.songMs = ms;
}

void RB3TraceSetSimDt(float seconds) {
    gRec.simDt = seconds;
}

// ---------------------------------------------------------------------------
// M4 PER-FRAME CLOCK SAMPLE (clk). Emitted EVERY frame (no §4.7 decimation) so a
// fixed-clock replay (RB3_REPLAY_FIXED_CLOCK) feeds the EXACT recorded {sdt, sm}
// at each frame, instead of carrying a STALE decimated-fr sample forward (which
// drifts the song clock ~1.5s by song end -> gem-strike vs autoplay desync). The
// fr stream stays decimated as-is. Tiny (~30 bytes/frame): just the envelope
// (t/f/cs) + sdt + the song-ms `sm` (omitted in menus). The passed simDt/songMs
// are also mirrored into the recorder cache so a later fr row this same frame
// carries the matching sdt/sm even if the tap order differs.
// ---------------------------------------------------------------------------
void RB3RecordClock(float simDt, float songMs) {
    if (!gRB3TraceActive) return;
    gRec.simDt  = simDt;     // keep the cache coherent with this frame's clock
    gRec.songMs = songMs;
    TraceEvent e;
    std::memset(&e, 0, sizeof(e));
    e.kind     = TK_CLK;
    e.f        = gRB3TraceFrame;
    e.sm       = songMs;     // < 0 => envelope omits "sm" (menus)
    e.clk.sdt  = simDt;
    PushEvent(e);
}

void RB3TraceFlush() {
    if (gRec.state != 1) return;
    // The hdr is written lazily (M4 GAP 1: deferred so the boot RNG seed lands in
    // it). Ensure it is emitted even if no event was ever recorded, so a session
    // with only a hdr is still a valid trace.
    if (!gRec.hdrWritten) WriteHeader();
    DrainRing();
    if (gRec.sink) std::fflush(gRec.sink);
}

void RB3TraceShutdown() {
    if (gRec.state == 1) {
        // Flush a hdr even if nothing was recorded (lazy-hdr; see RB3TraceFlush).
        if (!gRec.hdrWritten) WriteHeader();
        DrainRing();   // flush the remaining ring to the active sink (file or web)
    }
    if (gRec.sink) {
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
    gRec.frameCompat = false;
    gRec.interner.reset();
    gRec.clockBaseSet  = false;
    gRec.lastInputBits = 0;
    gRec.haveInput     = false;
    gRec.songMs        = -1.0f;
    gRec.simDt         = 0.0f;
    gRec.platform      = "native";
    gRec.startedIso.clear();
    gRec.buildGit.clear();
    gRec.randSeed      = 0;
    gRec.seedSet       = false;
    gRec.aids          = 0;

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
