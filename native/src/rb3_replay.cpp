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

// One recorded fr row's replay-relevant clock fields (Tier-2 fixed clock). `sdt`
// = the menu/UI sim seconds advanced THAT frame; `songMs` = the song-ms fed to
// SetSeconds that frame (-1 in menus). fr rows are decimated, so these are sparse
// and consumed by RB3ReplayDtForFrame / RB3ReplaySongMsForFrame via carry-forward.
struct ReplayFrameSample {
    int   frame;
    float sdt;     // sim seconds this frame; 0 when absent
    float songMs;  // song ms this frame; -1 when absent (menus)
};

struct ReplayState {
    std::vector<ReplaySample> samples;  // sorted ascending by frame (`in` events)
    std::vector<ReplayFrameSample> frames;  // sorted ascending by frame (`fr` rows)
    bool inited = false;                // RB3ReplayInit has run (parse attempted)
    bool active = false;                // armed: >= 1 `in` event loaded
    int  lastFrame = 0;                 // frame of the last `in` event
    int  fixedClock = -1;               // RB3_REPLAY_FIXED_CLOCK: -1 unchecked, 0/1
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

// Ingest an `fr` row's clock fields (Tier-2 fixed clock). Needs `f`; `sdt`/`sm`
// are optional (the recorder omits `sdt` at 0 and `sm` in menus). Carries 0/-1
// defaults so the carry-forward lookups behave (no sim advance, not-in-a-song).
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

// Parse one already-trimmed line. `in` rows append an input sample; `fr` rows
// append a clock sample (for fixed-clock replay). `#`/blank/unknown lines skip.
void IngestLine(const char *line) {
    // Skip comments + blanks (the recorder writes a leading `#` comment line).
    while (*line == ' ' || *line == '\t')
        ++line;
    if (*line == '\0' || *line == '#')
        return;
    if (IsFrameLine(line)) {
        IngestFrameLine(line);
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
    if (!gReplay.frames.empty()) {
        std::stable_sort(gReplay.frames.begin(), gReplay.frames.end(),
                         [](const ReplayFrameSample &a, const ReplayFrameSample &b) {
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
    gReplay.inited = true;

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

    Finalize();
    if (gReplay.active) {
        std::printf("[rb3-replay] ARMED — %zu edges, %zu fr clock samples, "
                    "lastFrame=%d. Live input is overridden; replayed input is "
                    "re-recorded as fresh `in` rows.\n",
                    gReplay.samples.size(), gReplay.frames.size(),
                    gReplay.lastFrame);
        if (RB3ReplayFixedClock()) {
            std::printf("[rb3-replay] FIXED CLOCK — RB3_REPLAY_FIXED_CLOCK set: "
                        "sim clock driven from recorded {sdt,sm} (Task.cpp seam 1 "
                        "+ Game.cpp seam 2), bypassing wall-clock + live audio.\n");
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

// Carry-forward lookup over the sorted fr clock table: the fr sample at-or-before
// `frame`, or nullptr if `frame` precedes the first fr (or no fr rows exist).
namespace {
const ReplayFrameSample *FrameSampleAtOrBefore(int frame) {
    const std::vector<ReplayFrameSample> &v = gReplay.frames;
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
        return nullptr;  // before the first recorded fr
    --it;
    return &*it;
}
}  // namespace

float RB3ReplayDtForFrame(int frame) {
    const ReplayFrameSample *s = FrameSampleAtOrBefore(frame);
    return s ? s->sdt : 0.0f;  // 0 => no menu-clock advance (default/before first fr)
}

float RB3ReplaySongMsForFrame(int frame) {
    const ReplayFrameSample *s = FrameSampleAtOrBefore(frame);
    return s ? s->songMs : -1.0f;  // -1 => menus / not in a song
}

#endif  // HX_NATIVE
