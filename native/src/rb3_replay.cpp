// rb3_replay.cpp — Tier-1 session replay loader + carry-forward input lookup.
//
// See rb3_replay.h for the contract + design rationale. This TU owns ONLY the
// parse + lookup; the actual SendButtonMessages re-drive lives in the JoypadPoll
// hook (native/src/rb3_joypad_native.cpp), which calls RB3ReplayBitsForFrame
// each poll. No matched decomp TU is touched.

#ifdef HX_NATIVE

#include "rb3_replay.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/em_asm.h>
#endif

namespace {

// One carry-forward sample: the FULL held bitmask `bits` becomes active at frame
// `frame` and persists until the next sample's frame.
struct ReplaySample {
    int          frame;
    unsigned int bits;
};

// One recorded clock sample's replay-relevant fields (Tier-2 fixed clock). `sdt`
// = the menu/UI sim seconds advanced THAT frame; `songMs` = the song-ms fed to
// SetSeconds that frame (-1 in menus). Populated from BOTH the `clk` stream (one
// per frame, un-decimated -> an EXACT per-frame lookup) and, as a fallback for
// older traces with no clk, the decimated `fr` rows (sparse -> carry-forward).
struct ReplayFrameSample {
    int   frame;
    float sdt;     // sim seconds this frame; 0 when absent
    float songMs;  // song ms this frame; -1 when absent (menus)
};

// M4 GAP 2 — one recorded run-aid application: the aid name ("autohit"/"nofail")
// and the frame it was applied at. Parsed from the one-shot mark{tag:"aid"} rows
// the recorder emits (rb3_session_trace RB3TraceRecordAid). On replay the
// game-input poll re-applies each aid once its frame is reached, reproducing the
// autoplay that produced the recorded score.
struct ReplayAid {
    int         frame;
    std::string name;
    bool        applied;   // re-applied during this replay (one-shot)
};

struct ReplayState {
    std::vector<ReplaySample> samples;  // sorted ascending by frame (`in` events)
    // Per-frame clock from the un-decimated `clk` stream (sorted ascending by
    // frame). When non-empty, RB3ReplayDtForFrame / RB3ReplaySongMsForFrame do an
    // EXACT per-frame lookup here (no carry-forward staleness) so the fixed-clock
    // replay's song clock tracks the recording frame-for-frame.
    std::vector<ReplayFrameSample> clocks;
    std::vector<ReplayFrameSample> frames;  // sorted ascending by frame (`fr` rows; FALLBACK)
    std::vector<ReplayAid> aids;        // recorded run-aid applications (GAP 2)
    bool inited = false;                // RB3ReplayInit has run (parse attempted)
    bool active = false;                // armed: >= 1 `in` event loaded
    int  lastFrame = 0;                 // frame of the last `in` event
    int  fixedClock = -1;               // RB3_REPLAY_FIXED_CLOCK: -1 unchecked, 0/1
    // M4 GAP 1 — boot RNG seed captured from the trace hdr (`seed`). seedSet=false
    // => the trace carried no seed (older trace / recorder-core) => no re-seed.
    int  seed       = 0;
    bool seedSet    = false;
    // ── M4 (T3) — in-song milestone anchoring ────────────────────────────────
    // The recorded curve's song-start frame: the FIRST recorded clk/fr sample
    // whose songMs >= 0 (in-song begins). -1 if the trace never enters a song
    // (pure-menu trace) -> anchoring stays disabled and the lookup is pure menus.
    // Computed once in Finalize() from the same table RB3ReplaySongMsForFrame reads.
    int  recSongStartFrame    = -1;
    // The replay's own song-start frame, LATCHED lazily the first time
    // RB3ReplaySongMsForFrame is asked at an absolute frame the recorded curve
    // calls in-song (i.e. the seam fires under TheGamePanel->unk150 AND the
    // recorded song-ms-vs-frame curve has reached in-song). -1 until latched.
    // The ~5-frame absolute phase shift between record and replay song-start lives
    // entirely in (replaySongStartFrame - recSongStartFrame); indexing the recorded
    // curve RELATIVE to each side's start cancels it.
    int  replaySongStartFrame = -1;
};

ReplayState gReplay;

// ── Tolerant NDJSON field scanners ───────────────────────────────────────────
// We do NOT need a full JSON parser — the trace's `in` lines are flat objects
// emitted by rb3_session_trace's SerializeEvent (keys "k","f","b","dn","up",...).
// Find `"<key>":` and parse the integer that follows. Returns false if the key
// is absent (so we can skip malformed/foreign lines).

// Locate the value start just past `"<key>":` (skipping optional whitespace).
const char *FindKeyValue(const char *line, const char *key) {
    // Build the needle `"key":` once per call (keys are short).
    char needle[16];
    int n = std::snprintf(needle, sizeof(needle), "\"%s\":", key);
    if (n <= 0 || n >= (int)sizeof(needle))
        return nullptr;
    const char *p = std::strstr(line, needle);
    if (!p)
        return nullptr;
    p += n;
    while (*p == ' ' || *p == '\t')
        ++p;
    return p;
}

bool ParseIntField(const char *line, const char *key, long *out) {
    const char *p = FindKeyValue(line, key);
    if (!p)
        return false;
    char *end = nullptr;
    long v = std::strtol(p, &end, 10);
    if (end == p)
        return false;  // no digits — not the field we want
    *out = v;
    return true;
}

bool ParseUIntField(const char *line, const char *key, unsigned long *out) {
    const char *p = FindKeyValue(line, key);
    if (!p)
        return false;
    char *end = nullptr;
    unsigned long v = std::strtoul(p, &end, 10);
    if (end == p)
        return false;
    *out = v;
    return true;
}

bool ParseFloatField(const char *line, const char *key, float *out) {
    const char *p = FindKeyValue(line, key);
    if (!p)
        return false;
    char *end = nullptr;
    double v = std::strtod(p, &end);
    if (end == p)
        return false;  // no number — not the field we want
    *out = (float)v;
    return true;
}

// Is this line an `in` event? Match `"k":"in"` exactly (after the writer's
// envelope). Cheap substring test; foreign/other-kind lines are skipped.
bool IsInputLine(const char *line) {
    return std::strstr(line, "\"k\":\"in\"") != nullptr;
}

// Is this line an `fr` event? Match `"k":"fr"` exactly.
bool IsFrameLine(const char *line) {
    return std::strstr(line, "\"k\":\"fr\"") != nullptr;
}

// Is this line a `clk` (per-frame clock) event? Match `"k":"clk"` exactly.
bool IsClockLine(const char *line) {
    return std::strstr(line, "\"k\":\"clk\"") != nullptr;
}

// Is this the header line? Match `"k":"hdr"`.
bool IsHeaderLine(const char *line) {
    return std::strstr(line, "\"k\":\"hdr\"") != nullptr;
}

// Is this a run-aid one-shot marker? mark{tag:"aid"} (M4 GAP 2).
bool IsAidMarkLine(const char *line) {
    return std::strstr(line, "\"k\":\"mark\"") != nullptr &&
           std::strstr(line, "\"tag\":\"aid\"") != nullptr;
}

// Pull a quoted string field value into `out` (no escape handling beyond a copy;
// the aid names are simple identifiers). Returns false if the key is absent or
// not a quoted string.
bool ParseStrField(const char *line, const char *key, std::string *out) {
    const char *p = FindKeyValue(line, key);
    if (!p || *p != '"')
        return false;
    ++p;  // past the opening quote
    out->clear();
    bool esc = false;
    for (; *p; ++p) {
        char c = *p;
        if (esc) { out->push_back(c); esc = false; continue; }
        if (c == '\\') { esc = true; continue; }
        if (c == '"') return true;
        out->push_back(c);
    }
    return false;  // unterminated
}

// Ingest the header's `seed` field (M4 GAP 1). Absent => seedSet stays false.
void IngestHeaderLine(const char *line) {
    long seed = 0;
    if (ParseIntField(line, "seed", &seed)) {
        gReplay.seed    = (int)seed;
        gReplay.seedSet = true;
    }
}

// Ingest a run-aid marker (M4 GAP 2): note=<aid name>, f=<frame applied>.
void IngestAidMarkLine(const char *line) {
    long frame = 0;
    std::string name;
    if (!ParseStrField(line, "note", &name) || name.empty())
        return;
    ParseIntField(line, "f", &frame);   // absent => frame 0 (apply ASAP)
    ReplayAid a;
    a.frame   = (int)frame;
    a.name    = name;
    a.applied = false;
    gReplay.aids.push_back(a);
}

// Ingest an `fr` row's clock fields (Tier-2 fixed clock FALLBACK for older traces
// with no clk stream). Needs `f`; `sdt`/`sm` are optional (the recorder omits
// `sdt` at 0 and `sm` in menus). Carries 0/-1 defaults so the carry-forward
// lookups behave (no sim advance, not-in-a-song).
void IngestFrameLine(const char *line) {
    long frame = 0;
    if (!ParseIntField(line, "f", &frame))
        return;
    ReplayFrameSample s;
    s.frame  = (int)frame;
    s.sdt    = 0.0f;
    s.songMs = -1.0f;
    ParseFloatField(line, "sdt", &s.sdt);  // absent => stays 0 (no menu advance)
    ParseFloatField(line, "sm", &s.songMs);  // absent => stays -1 (menus)
    gReplay.frames.push_back(s);
}

// Ingest a `clk` (per-frame clock) row — the EXACT per-frame {sdt, sm} the
// fixed-clock replay feeds at frame N. Un-decimated (one per recorded frame), so
// the per-frame lookups find the exact recorded value and never carry a stale
// decimated-fr sample forward. Needs `f`; `sdt` is always present (recorder emits
// it even at 0); `sm` is omitted in menus (stays -1).
void IngestClockLine(const char *line) {
    long frame = 0;
    if (!ParseIntField(line, "f", &frame))
        return;
    ReplayFrameSample s;
    s.frame  = (int)frame;
    s.sdt    = 0.0f;
    s.songMs = -1.0f;
    ParseFloatField(line, "sdt", &s.sdt);
    ParseFloatField(line, "sm", &s.songMs);  // absent => stays -1 (menus)
    gReplay.clocks.push_back(s);
}

// Parse one already-trimmed line. `in` rows append an input sample; `fr` rows
// append a clock sample (for fixed-clock replay). `#`/blank/unknown lines skip.
void IngestLine(const char *line) {
    // Skip comments + blanks (the recorder writes a leading `#` comment line).
    while (*line == ' ' || *line == '\t')
        ++line;
    if (*line == '\0' || *line == '#')
        return;
    if (IsHeaderLine(line)) {
        IngestHeaderLine(line);   // M4 GAP 1: capture the boot RNG seed
        return;
    }
    if (IsAidMarkLine(line)) {
        IngestAidMarkLine(line);  // M4 GAP 2: capture a run-aid application
        return;
    }
    if (IsClockLine(line)) {
        IngestClockLine(line);   // M4: per-frame un-decimated clock (exact lookup)
        return;
    }
    if (IsFrameLine(line)) {
        IngestFrameLine(line);   // FALLBACK clock for traces with no clk stream
        return;
    }
    if (!IsInputLine(line))
        return;
    long frame = 0;
    unsigned long bits = 0;
    if (!ParseIntField(line, "f", &frame))
        return;
    if (!ParseUIntField(line, "b", &bits))
        return;
    ReplaySample s;
    s.frame = (int)frame;
    s.bits  = (unsigned int)bits;
    gReplay.samples.push_back(s);
}

// Split an NDJSON buffer on '\n' and ingest each line.
void IngestBuffer(const char *buf, size_t len) {
    size_t start = 0;
    for (size_t i = 0; i <= len; ++i) {
        if (i == len || buf[i] == '\n') {
            if (i > start) {
                std::string line(buf + start, i - start);
                // Trim a trailing '\r' (CRLF tolerance).
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                IngestLine(line.c_str());
            }
            start = i + 1;
        }
    }
}

// Finalize: sort by frame (the trace is normally already in order, but a
// decimated/re-ordered stream is tolerated), then arm. The fr clock table is
// sorted whether or not any `in` events exist (fixed-clock replay can drive the
// menu clock from fr rows alone), but `active` still requires >= 1 `in` event.
void Finalize() {
    // Per-frame clk table (preferred): sort ascending by frame for the exact
    // lookup. Stable so same-frame dupes keep recorded order (last one wins on a
    // tie via upper_bound). The clk stream drives the fixed-clock replay when
    // present; fr is the fallback for older traces.
    if (!gReplay.clocks.empty()) {
        std::stable_sort(gReplay.clocks.begin(), gReplay.clocks.end(),
                         [](const ReplayFrameSample &a, const ReplayFrameSample &b) {
                             return a.frame < b.frame;
                         });
    }
    if (!gReplay.frames.empty()) {
        std::stable_sort(gReplay.frames.begin(), gReplay.frames.end(),
                         [](const ReplayFrameSample &a, const ReplayFrameSample &b) {
                             return a.frame < b.frame;
                         });
    }
    // M4 (T3) — compute the recorded AUDIO-START frame: the FIRST sample (in the
    // SAME table RB3ReplaySongMsForFrame reads — prefer clk, fall back to fr) whose
    // songMs is strictly > 0, i.e. the song clock has actually begun ADVANCING.
    //
    // Why > 0 and not >= 0: a real gameplay trace pins songMs at exactly 0.0 for a
    // long pre-roll/countdown plateau (e.g. frames 1777..4697 in the gate trace)
    // BEFORE the audio Play() callback latches and the clock advances. That plateau
    // is deterministic (the unk150 seam onset is the same absolute frame on both
    // sides), so it needs NO anchoring. The non-deterministic ~5-frame phase shift
    // is precisely WHERE the clock starts advancing (rec 4698 vs replay 4703) — so
    // the audio-start (first sm>0) is the milestone the in-song curve is anchored
    // against. The plateau (sm==0) and menus (sm<0) use the absolute curve, which is
    // already aligned. -1 if the trace never advances a song clock (pure menus).
    {
        const std::vector<ReplayFrameSample> &tbl =
            !gReplay.clocks.empty() ? gReplay.clocks : gReplay.frames;
        gReplay.recSongStartFrame = -1;
        for (size_t i = 0; i < tbl.size(); ++i) {
            if (tbl[i].songMs > 0.0f) {
                gReplay.recSongStartFrame = tbl[i].frame;
                break;
            }
        }
    }
    // Sort run-aid applications by frame (M4 GAP 2) so the per-frame re-apply walk
    // fires them in order. Stable so same-frame aids keep recorded order.
    if (!gReplay.aids.empty()) {
        std::stable_sort(gReplay.aids.begin(), gReplay.aids.end(),
                         [](const ReplayAid &a, const ReplayAid &b) {
                             return a.frame < b.frame;
                         });
    }
    if (gReplay.samples.empty())
        return;
    std::stable_sort(gReplay.samples.begin(), gReplay.samples.end(),
                     [](const ReplaySample &a, const ReplaySample &b) {
                         return a.frame < b.frame;
                     });
    gReplay.lastFrame = gReplay.samples.back().frame;
    gReplay.active = true;
}

}  // namespace

void RB3ReplayInit() {
    if (gReplay.inited)
        return;

    // Only LATCH inited once a replay source is actually present. M4 GAP 1 made
    // the engine boot (System.cpp SystemInit -> RB3ReplaySeed) an early caller;
    // if RB3_REPLAY / window.__rb3ReplayData is not set yet at that point, leave
    // inited=false so a later real caller (JoypadPoll, or a re-armed env in a
    // gtest) re-attempts. The per-call env read is cheap.
    bool foundSource = false;

#ifdef __EMSCRIPTEN__
    // Web: rb3_pre.js fetches GET /api/telemetry/<sid> into window.__rb3ReplayData
    // (a single NDJSON string) when the URL carries ?replay=<sid>. Read it back
    // via EM_ASM. Returns a malloc'd C string we own (or 0 if unset).
    char *data = (char *)EM_ASM_PTR({
        var s = window.__rb3ReplayData;
        if (typeof s !== 'string' || s.length === 0)
            return 0;
        var len = lengthBytesUTF8(s) + 1;
        var ptr = _malloc(len);
        stringToUTF8(s, ptr, len);
        return ptr;
    });
    if (data) {
        foundSource = true;
        IngestBuffer(data, std::strlen(data));
        std::free(data);
        std::printf("[rb3-replay] web: loaded %zu input edges from "
                    "window.__rb3ReplayData\n", gReplay.samples.size());
    }
#else
    // Native: RB3_REPLAY=<path.jsonl>. Read the whole file (traces are small —
    // a few hundred KB at most) and ingest.
    const char *path = std::getenv("RB3_REPLAY");
    if (path && *path) {
        foundSource = true;
        FILE *f = std::fopen(path, "rb");
        if (!f) {
            std::fprintf(stderr, "[rb3-replay] could not open RB3_REPLAY='%s'\n",
                         path);
        } else {
            std::fseek(f, 0, SEEK_END);
            long sz = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
            if (sz > 0) {
                std::string buf;
                buf.resize((size_t)sz);
                size_t rd = std::fread(&buf[0], 1, (size_t)sz, f);
                IngestBuffer(buf.data(), rd);
            }
            std::fclose(f);
            std::printf("[rb3-replay] native: loaded %zu input edges from '%s'\n",
                        gReplay.samples.size(), path);
        }
    }
#endif

    // Latch only if a source was present; otherwise stay un-inited so a later call
    // (env armed afterwards) re-attempts. Once a source is found we always latch,
    // even if it parsed to zero events (a malformed/empty trace shouldn't loop).
    if (foundSource)
        gReplay.inited = true;

    Finalize();
    if (gReplay.active) {
        std::printf("[rb3-replay] ARMED — %zu edges, %zu clk samples, "
                    "%zu fr clock samples (fallback), lastFrame=%d. Live input is "
                    "overridden; replayed input is re-recorded as fresh `in` rows.\n",
                    gReplay.samples.size(), gReplay.clocks.size(),
                    gReplay.frames.size(), gReplay.lastFrame);
        if (RB3ReplayFixedClock()) {
            std::printf("[rb3-replay] FIXED CLOCK — RB3_REPLAY_FIXED_CLOCK set: "
                        "sim clock driven from recorded {sdt,sm} via the %s "
                        "(Task.cpp seam 1 + Game.cpp seam 2), bypassing wall-clock "
                        "+ live audio.\n",
                        gReplay.clocks.empty()
                            ? "DECIMATED fr stream (older trace, no clk)"
                            : "per-frame clk stream (frame-locked)");
        }
    }
}

bool RB3ReplayActive() {
    return gReplay.active;
}

unsigned int RB3ReplayBitsForFrame(int frame) {
    if (!gReplay.active)
        return 0;
    // Held bitmask at `frame` = the `b` of the last `in` with sampleFrame <= frame
    // (carry-forward). upper_bound finds the first sample with frame > target;
    // the one before it is our answer. Default 0 before the first event.
    const std::vector<ReplaySample> &v = gReplay.samples;
    ReplaySample key;
    key.frame = frame;
    key.bits  = 0;
    auto it = std::upper_bound(v.begin(), v.end(), key,
                               [](const ReplaySample &a, const ReplaySample &b) {
                                   return a.frame < b.frame;
                               });
    if (it == v.begin())
        return 0;  // frame precedes the first recorded edge
    --it;
    return it->bits;
}

int RB3ReplayLastFrame() {
    return gReplay.lastFrame;
}

// ── Tier-2 fixed-clock replay (M4) ──────────────────────────────────────────

bool RB3ReplayFixedClock() {
    // Parse RB3_REPLAY_FIXED_CLOCK once. Any non-empty, non-"0" value enables it.
    // Independent of RB3ReplayInit so the seams can query it cheaply; it is only
    // MEANINGFUL when RB3ReplayActive() (the seams gate on both).
    if (gReplay.fixedClock < 0) {
#ifdef __EMSCRIPTEN__
        // Web: a JS global set by rb3_pre.js from ?fixedclock=1 (browser-deferred,
        // mirrors the ?replay= path). Absent => off.
        int on = EM_ASM_INT({
            var v = window.__rb3ReplayFixedClock;
            return (v === true || v === 1 || v === '1') ? 1 : 0;
        });
        gReplay.fixedClock = on ? 1 : 0;
#else
        const char *v = std::getenv("RB3_REPLAY_FIXED_CLOCK");
        gReplay.fixedClock = (v && *v && std::strcmp(v, "0") != 0) ? 1 : 0;
#endif
    }
    return gReplay.fixedClock != 0;
}

// ── W0.3b — Trace-free fixed sim clock (headless-determinism harness) ─────────
// A file-static cached presence flag + dt, INDEPENDENT of gReplay.fixedClock
// (which is the trace-gated RB3_REPLAY_FIXED_CLOCK; the semantics differ — this
// one engages with no trace loaded). Both parse once, mirroring the idiom above.
namespace {
int   gFixedClockActive = -1;      // RB3_FIXED_CLOCK: -1 unchecked, 0/1
float gFixedClockDt      = -1.0f;  // seconds; -1 unchecked, else >= 0
}  // namespace

bool RB3FixedClockActive() {
    // Parse RB3_FIXED_CLOCK once. Any non-empty, non-"0" value enables it. Unlike
    // RB3ReplayFixedClock this is TRACE-FREE: it is meaningful on a plain boot.
    if (gFixedClockActive < 0) {
#ifdef __EMSCRIPTEN__
        // Web: a JS global (browser-deferred, mirrors window.__rb3ReplayFixedClock).
        // Absent => off.
        int on = EM_ASM_INT({
            var v = window.__rb3FixedClock;
            return (v === true || v === 1 || v === '1') ? 1 : 0;
        });
        gFixedClockActive = on ? 1 : 0;
#else
        const char *v = std::getenv("RB3_FIXED_CLOCK");
        gFixedClockActive = (v && *v && std::strcmp(v, "0") != 0) ? 1 : 0;
#endif
    }
    return gFixedClockActive != 0;
}

float RB3FixedClockDt() {
    // The constant per-frame sim dt (SECONDS). Default 1/60s (a real fixed
    // timestep so animation progresses). RB3_FIXED_CLOCK_DT_MS overrides it with a
    // millisecond value; any finite >= 0 value is honoured (0.0 => a true freeze).
    // Parsed once.
    if (gFixedClockDt < 0.0f) {
        float dt = 1.0f / 60.0f;
#ifdef __EMSCRIPTEN__
        // Web: window.__rb3FixedClockDtMs (milliseconds) overrides; absent => default.
        double ms = EM_ASM_DOUBLE({
            var v = window.__rb3FixedClockDtMs;
            return (v === undefined || v === null) ? -1.0 : Number(v);
        });
        if (ms >= 0.0 && ms == ms)  // finite (NaN != NaN) and non-negative
            dt = (float)(ms / 1000.0);
#else
        const char *v = std::getenv("RB3_FIXED_CLOCK_DT_MS");
        if (v && *v) {
            char *end = nullptr;
            double ms = std::strtod(v, &end);
            if (end != v && ms >= 0.0 && ms == ms)  // parsed, finite, non-negative
                dt = (float)(ms / 1000.0);
        }
#endif
        gFixedClockDt = dt;
    }
    return gFixedClockDt;
}

// Carry-forward lookup over a sorted (frame,sdt,songMs) table: the sample
// at-or-before `frame`, or nullptr if `frame` precedes the first sample (or the
// table is empty). For the un-decimated clk table this resolves to the EXACT
// recorded sample at frame N (every frame has one); past the last recorded frame
// it holds the final sample (so a replay that runs a few frames long doesn't snap
// the clock back to menus). For the decimated fr fallback it is a true
// carry-forward (the nearest preceding decimated sample).
namespace {
const ReplayFrameSample *SampleAtOrBefore(const std::vector<ReplayFrameSample> &v,
                                          int frame) {
    if (v.empty())
        return nullptr;
    ReplayFrameSample key;
    key.frame  = frame;
    key.sdt    = 0.0f;
    key.songMs = -1.0f;
    auto it = std::upper_bound(
        v.begin(), v.end(), key,
        [](const ReplayFrameSample &a, const ReplayFrameSample &b) {
            return a.frame < b.frame;
        });
    if (it == v.begin())
        return nullptr;  // before the first recorded sample
    --it;
    return &*it;
}

// The clock sample for `frame`: PREFER the un-decimated clk table (exact per-frame
// value, frame-locked to the recording), FALL BACK to the decimated fr table for
// older traces that carry no clk stream. nullptr only when neither table has a
// sample at-or-before `frame` (i.e. before the first recorded frame).
const ReplayFrameSample *ClockSampleForFrame(int frame) {
    if (!gReplay.clocks.empty())
        return SampleAtOrBefore(gReplay.clocks, frame);
    return SampleAtOrBefore(gReplay.frames, frame);
}
}  // namespace

float RB3ReplayDtForFrame(int frame) {
    // Menu/UI clock — UNCHANGED from Wave 7: absolute-frame lookup, drift-0 in
    // menus. (Only the in-song song-ms is milestone-anchored below; the per-frame
    // sim-dt the menu/UI clock accumulates is correct against the absolute frame.)
    const ReplayFrameSample *s = ClockSampleForFrame(frame);
    return s ? s->sdt : 0.0f;  // 0 => no menu-clock advance (default/before first sample)
}

namespace {
// The recorded song-ms curve sampled at an ABSOLUTE recorded frame (the Wave-7
// behavior): exact per-frame from clk, carry-forward from the decimated fr
// fallback, holds the final sample past the last recorded frame, -1 before the
// first recorded sample. Used to read the recorded value at the translated index
// AND as the FALLBACK audio-start signal when the live accessor is unavailable.
float RecordedSongMsAtAbsFrame(int absFrame) {
    const ReplayFrameSample *s = ClockSampleForFrame(absFrame);
    return s ? s->songMs : -1.0f;
}
}  // namespace

// The LIVE song-ms (TheGame->GetBeatMaster()->GetAudio()->GetTime(), null-guarded)
// — the replay's OWN audio clock, which free-runs on the miniaudio callback thread
// even under the fixed-clock seam (design §M4: the mixer still free-runs; the sim
// reads the recorded songMs). Its first ADVANCE past 0 marks the replay's true
// audio-start — the replay-side signal that defeats the pre-roll-plateau ambiguity
// (the recorded curve alone can't tell the replay's longer plateau from its
// advancing region). Defined in rb3_trace_taps.cpp (linked into BOTH rb3-native and
// rb3-tests); returns -1 when unbooted (TheGame==NULL), so the unit tests safely
// fall back to the recorded-curve signal below.
extern float RB3TraceCurrentSongMs();

namespace {
// Test-only injection of the live audio clock so a gtest can exercise the
// live-signal audio-start latch (the real-engine path) without booting audio.
// -2 = "use the real RB3TraceCurrentSongMs()" (production default). Any other
// value overrides it. Set via RB3ReplaySetLiveSongMsForTest below.
float gReplayLiveSongMsOverride = -2.0f;

// The replay-side live audio clock, honoring the test override when armed.
float ReplayLiveSongMs() {
    if (gReplayLiveSongMsOverride > -2.0f)
        return gReplayLiveSongMsOverride;
    return RB3TraceCurrentSongMs();
}
}  // namespace

// Test-only: inject the live audio clock value the next audio-start latch reads.
// Pass -2 to restore the real accessor. Not in the public header (the test
// forward-declares it). No-op in normal runs (never called).
void RB3ReplaySetLiveSongMsForTest(float ms) {
    gReplayLiveSongMsOverride = ms;
}

// ── M4 (T3) — milestone-anchored in-song song-ms ─────────────────────────────
// Wave-7 fed the recorded curve at the ABSOLUTE frame straight into SetSeconds.
// But the song AUDIO-START (when the song clock begins advancing) fires at a
// non-deterministic absolute frame — record vs replay differ by ~5 frames of
// async audio-callback latency (gate trace: rec 4698 vs replay 4703) — so the
// recorded song-ms-vs-frame curve was indexed against a phase-shifted replay
// song lifecycle and drifted to ~-754 ms in-song. T3 cancels that phase shift by
// indexing the recorded curve RELATIVE to each side's AUDIO-START:
//
//   * Menus (recorded curve < 0 at this replay frame): return -1 — seam-2 then
//     keeps the live-audio path. (Aligned; no anchoring needed.)
//   * Pre-roll plateau (recorded curve == 0.0, before the clock advances): pass
//     the absolute 0.0 straight through. The plateau onset (the unk150 seam) is
//     the SAME absolute frame on both sides, so it is already aligned — no
//     anchoring, and the song clock is correctly pinned at 0 during the countdown.
//   * Audio-start: the FIRST replay frame the replay's OWN live audio clock has
//     advanced past 0 (RB3TraceCurrentSongMs() > 0) — the replay-side signal that
//     correctly fires at the replay's longer-plateau end, NOT where the recorded
//     curve happens to advance at the same absolute frame. (Unbooted unit tests:
//     fall back to the recorded curve advancing at this absolute frame.) Latch
//     replaySongStartFrame = that frame. recSongStartFrame is the recorded analogue
//     (first recorded sample sm>0, computed in Finalize). The ~5-frame phase shift
//     is now (replaySongStart - recSongStart).
//   * In-song (clock advancing): return the recorded curve at the RELATIVE index
//     recSongStartFrame + (frame - replaySongStartFrame), so record and replay
//     read the same point on the recorded song-ms curve regardless of which
//     absolute frame each side's audio actually began on. Hold/clamp at the end.
float RB3ReplaySongMsForFrame(int frame) {
    // Pure-menu trace (song clock never advances) -> never anchors. Pass the
    // absolute curve through unchanged (menus -1; an all-zero plateau stays 0).
    if (gReplay.recSongStartFrame < 0)
        return RecordedSongMsAtAbsFrame(frame);

    if (gReplay.replaySongStartFrame < 0) {
        // Not yet latched (pre audio-start on the replay side). Until the replay's
        // OWN audio clock begins advancing, mirror the absolute recorded curve
        // (menus -1 / aligned plateau 0). The seam only reaches here under unk150,
        // so the replay's live audio advancing past 0 means its OWN audio just
        // began — the milestone we anchor the relative index against.
        float here = RecordedSongMsAtAbsFrame(frame);
        if (here < 0.0f)
            return -1.0f;                           // menus -> seam keeps live audio
        // Replay-side audio-start signal. PREFER the live audio clock: it free-runs
        // independently, so its advance past 0 marks the replay's OWN audio-start
        // regardless of the recorded curve's (phase-shifted) absolute frame. The
        // recorded-curve advance is a FALLBACK used ONLY when the live accessor is
        // unavailable (live < 0: unbooted unit test, or audio not yet live) — never
        // when the engine is live, so it can't latch early on the recorded curve's
        // earlier (rec-side) audio-start during the replay's longer plateau.
        float live = ReplayLiveSongMs();
        bool audioStarted = (live > 0.0f) || (live < 0.0f && here > 0.0f);
        if (!audioStarted)
            return 0.0f;                            // plateau: feed the aligned 0
        gReplay.replaySongStartFrame = frame;       // LATCH the replay audio-start
    }

    // A replay frame BEFORE the latched audio-start is still pre-advance -> mirror
    // the absolute curve (the aligned plateau/menus). (In a live monotonic replay
    // frames never go backwards past the latch; this also keeps the boundary exact
    // when a lookup is queried out of order.)
    if (frame < gReplay.replaySongStartFrame)
        return RecordedSongMsAtAbsFrame(frame);

    // In-song (clock advancing): index the recorded curve relative to each side's
    // audio-start so the absolute phase shift (replaySongStart - recSongStart)
    // cancels. Frames at-or-after recSongStartFrame are all in the advancing
    // segment, so RecordedSongMsAtAbsFrame(relIndex) is a valid positive value
    // (and holds the final recorded sample past the recorded end).
    int relIndex = gReplay.recSongStartFrame +
                   (frame - gReplay.replaySongStartFrame);
    return RecordedSongMsAtAbsFrame(relIndex);
}

// ── M4 GAP 1 — boot RNG seed re-seed ─────────────────────────────────────────

bool RB3ReplaySeed(int *out) {
    // Self-arming: the seed lives in the hdr, which the boot path needs BEFORE the
    // lazy JoypadPoll RB3ReplayInit. RB3ReplayInit is idempotent, so trigger it
    // here so the seed is available even when the boot path is the first caller.
    if (!gReplay.inited)
        RB3ReplayInit();
    if (!gReplay.seedSet)
        return false;
    if (out)
        *out = gReplay.seed;
    return true;
}

// ── M4 GAP 2 — run-aid re-application ────────────────────────────────────────

int RB3ReplayPendingAids(int frame, const char **outAids, int maxAids) {
    if (gReplay.aids.empty())
        return 0;
    int n = 0;
    // Return aids whose recorded frame has been reached and that have NOT yet
    // taken effect. We deliberately do NOT latch here: an aid (autohit) can no-op
    // if players aren't ready yet at the recorded frame (load-skew), so the caller
    // confirms effectiveness via RB3ReplayMarkAidApplied. Until then the aid stays
    // pending and is re-offered every frame (idempotent re-apply is harmless).
    for (size_t i = 0; i < gReplay.aids.size() && n < maxAids; ++i) {
        ReplayAid &a = gReplay.aids[i];
        if (!a.applied && frame >= a.frame)
            outAids[n++] = a.name.c_str();
    }
    return n;
}

void RB3ReplayMarkAidApplied(const char *aid) {
    if (!aid)
        return;
    // Latch the earliest still-pending aid of this name (one-shot). Re-applying an
    // already-latched aid on a later frame is then skipped.
    for (size_t i = 0; i < gReplay.aids.size(); ++i) {
        ReplayAid &a = gReplay.aids[i];
        if (!a.applied && a.name == aid) {
            a.applied = true;
            return;
        }
    }
}

bool RB3ReplayHasAids() {
    return !gReplay.aids.empty();
}

// Test-only: clear ALL replay state so a gtest can RB3ReplayInit a fresh fixture
// repeatedly (the global gReplay latches `inited` once a source is parsed). Not
// declared in the public header; the test forward-declares it. No-op in normal
// runs (never called).
void RB3ReplayResetForTest() {
    gReplay = ReplayState();
    gReplayLiveSongMsOverride = -2.0f;   // restore the real live-audio accessor
}

#endif  // HX_NATIVE
