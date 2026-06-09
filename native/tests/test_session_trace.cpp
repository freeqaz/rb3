// test_session_trace.cpp — M0 session-telemetry recorder CORE tests.
//
// Drives the recorder DIRECTLY (no booted engine): setenv RB3_SESSION_TRACE to a
// temp path, RB3TraceInit, record, flush/shutdown, then read + parse the file.
// Asserts the Locked v1 wire contract (docs/native/SESSION_TELEMETRY_DESIGN.md):
//   (1) line 1 is k=hdr,v=1,sid,platform
//   (2) every non-hdr line has t,f,cs,k; cs strictly increases
//   (3) sm omitted when <0, present when >=0
//   (4) in(whammy=0,tilt=0) has no ax; in(whammy=0.5) -> ax.wh~=500, no ti
//   (5) a screen name with a quote + newline round-trips as ONE valid-JSON line
//   (6) RB3FrameTraceRecord yields an fr row with dt/lp/lpu/scr/ld/st/pend
//
// No nlohmann/json in the build tree, so this uses a small strict line-oriented
// JSON checker: each line must be brace-balanced with no UNESCAPED control char,
// and a tiny key extractor pulls scalar values. The escaping test (5) is the
// decisive "one event = one physical line" guard.

#include "gtest/gtest.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif

// ---- recorder API under test ---------------------------------------------
extern bool gRB3TraceActive;
extern int  gRB3TraceFrame;
void RB3TraceInit();
void RB3TraceSetFrame(int frame);
void RB3RecordFrame(float dt, float lp, float lpu, const char *scr, int pend);
void RB3RecordInput(int pad, unsigned bits, unsigned dn, unsigned up,
                    float whammy, float tilt);
void RB3RecordNav(const char *from, const char *to, const char *focus,
                  bool wentBack);
void RB3RecordBootMark(const char *phase);
void RB3RecordSong(const char *ev, const char *id, const char *track,
                   const char *diff, float score, float pct);
void RB3RecordAudio(int under, int frames);
void RB3RecordLog(const char *lvl, const char *msg, const char *src);
void RB3RecordMark(const char *tag, const char *note);
void RB3RecordCheckpoint(const char *scr, const char *focus,
                         float taskSec, float beat, float songMs,
                         long scoreSum, const int *scores, int nScores,
                         int nPlayers, int pct);
void RB3RecordClock(float simDt, float songMs);  // M4 — per-frame un-decimated clock
void RB3TraceSetSongMs(float ms);
void RB3TraceSetSeed(int seed);                  // M4 GAP 1 — boot RNG seed capture
void RB3TraceRecordAid(const char *aid);          // M4 GAP 2 — run-aid capture
void RB3TraceFlush();
void RB3TraceShutdown();
void RB3FrameTraceRecord(int frame, float dtMs, float loadPollMs,
                         float loadPollUntilMs, const char *screen,
                         int pendingLoaders);

// Engine counters (so test (6) can prove ld/st flow through the shim).
extern int gFrameTraceLoaderAdds;
extern int gFrameTraceStreamOpens;

// ---- Tier-1 replay API under test (rb3_replay.cpp) ------------------------
// RB3ReplayInit parses RB3_REPLAY=<path> into an ordered (frame,bits) table;
// RB3ReplayBitsForFrame is the carry-forward lookup the JoypadPoll hook drives.
void         RB3ReplayInit();
bool         RB3ReplayActive();
unsigned int RB3ReplayBitsForFrame(int frame);
int          RB3ReplayLastFrame();
// M4 Tier-2 fixed-clock per-frame lookups (clk stream; fr fallback).
float        RB3ReplayDtForFrame(int frame);
float        RB3ReplaySongMsForFrame(int frame);
// M4 GAP 1 / GAP 2 replay accessors.
bool         RB3ReplaySeed(int *out);
int          RB3ReplayPendingAids(int frame, const char **outAids, int maxAids);
void         RB3ReplayMarkAidApplied(const char *aid);
bool         RB3ReplayHasAids();
void         RB3ReplayResetForTest();   // clear the latched global replay state
void         RB3ReplaySetLiveSongMsForTest(float ms);  // inject the live audio clock

// ===========================================================================
// Minimal NDJSON test harness — strict-enough to prove the wire contract
// without a JSON library. Each line is validated brace/string-balanced and
// free of raw control chars; scalar/string values are pulled by key.
// ===========================================================================
namespace {

// Resolve the temp-dir base to an ABSOLUTE path, captured at static-init time
// (before any test body runs). The full rb3-tests binary also runs engine-backed
// tests (NativeSubsystems) whose boot `chdir`s into RB3_DATA (native_file.cpp);
// a *relative* TMPDIR (the ctest ENVIRONMENT default is `build-native/tmp`) would
// then no longer resolve from the new cwd, so RB3TraceInit's fopen would ENOENT
// and every SessionTrace test that ran AFTER an engine test would fail. Snapshot
// cwd+TMPDIR once, up front, so the path is stable regardless of later chdir.
std::string AbsTempBase() {
    const char *dir = std::getenv("TMPDIR");
    std::string base = (dir && dir[0]) ? std::string(dir) : std::string("/tmp");
    if (!base.empty() && base[0] != '/') {
        // Relative TMPDIR — anchor it to the cwd as it is RIGHT NOW (static init,
        // before the engine chdir).
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd))) {
            std::string abs(cwd);
            if (abs.empty() || abs[abs.size() - 1] != '/') abs += '/';
            base = abs + base;
        }
    }
    if (base.empty() || base[base.size() - 1] != '/') base += '/';
    return base;
}

// Captured at program start (before the engine boot's chdir).
const std::string kAbsTempBase = AbsTempBase();

std::string TempTracePath() {
    static int sCounter = 0;
    std::ostringstream os;
    os << kAbsTempBase << "rb3_trace_test_" << (long)getpid() << "_"
       << (++sCounter) << ".jsonl";
    return os.str();
}

// Validate a single physical NDJSON line: brace-balanced (tracking string
// context so braces inside strings don't count), and no UNESCAPED control char
// inside a string. Returns false on any violation.
bool LineIsWellFormed(const std::string &line) {
    int depth = 0;
    bool inStr = false;
    bool esc = false;
    bool sawOpen = false;
    for (size_t i = 0; i < line.size(); ++i) {
        unsigned char c = (unsigned char)line[i];
        if (inStr) {
            if (esc) { esc = false; continue; }
            if (c == '\\') { esc = true; continue; }
            if (c == '"') { inStr = false; continue; }
            // Raw control char inside a string => the escaper failed.
            if (c < 0x20) return false;
        } else {
            if (c == '"') { inStr = true; continue; }
            if (c == '{') { depth++; sawOpen = true; }
            else if (c == '}') { depth--; if (depth < 0) return false; }
        }
    }
    return sawOpen && depth == 0 && !inStr && !esc;
}

// Read all non-empty physical lines from the file.
std::vector<std::string> ReadLines(const std::string &path) {
    std::vector<std::string> out;
    std::ifstream f(path.c_str());
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) out.push_back(line);
    }
    return out;
}

// True if `line` contains the top-level token `"key":` (cheap presence check).
bool HasKey(const std::string &line, const char *key) {
    std::string needle = std::string("\"") + key + "\":";
    return line.find(needle) != std::string::npos;
}

// Extract the raw token after "key": up to the next top-level ',' or '}'.
// Handles a quoted string value (returns it WITHOUT the surrounding quotes, but
// keeps escape sequences verbatim) or a bare number/bool. Empty if missing.
std::string GetRaw(const std::string &line, const char *key) {
    std::string needle = std::string("\"") + key + "\":";
    size_t p = line.find(needle);
    if (p == std::string::npos) return std::string();
    p += needle.size();
    if (p >= line.size()) return std::string();
    if (line[p] == '"') {
        // Quoted string — read to the matching unescaped quote.
        std::string out;
        bool esc = false;
        for (size_t i = p + 1; i < line.size(); ++i) {
            char c = line[i];
            if (esc) { out += c; esc = false; continue; }
            if (c == '\\') { out += c; esc = true; continue; }
            if (c == '"') break;
            out += c;
        }
        return out;
    }
    // Bare token.
    std::string out;
    for (size_t i = p; i < line.size(); ++i) {
        char c = line[i];
        if (c == ',' || c == '}') break;
        out += c;
    }
    return out;
}

// Object-valued key: returns the {...} block (including braces). Empty if absent.
std::string GetObject(const std::string &line, const char *key) {
    std::string needle = std::string("\"") + key + "\":{";
    size_t p = line.find(needle);
    if (p == std::string::npos) return std::string();
    size_t start = p + needle.size() - 1;  // at the '{'
    int depth = 0;
    bool inStr = false, esc = false;
    for (size_t i = start; i < line.size(); ++i) {
        char c = line[i];
        if (inStr) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') inStr = false;
            continue;
        }
        if (c == '"') inStr = true;
        else if (c == '{') depth++;
        else if (c == '}') { depth--; if (depth == 0) return line.substr(start, i - start + 1); }
    }
    return std::string();
}

// Test fixture: each test gets a fresh temp file + a clean recorder.
class SessionTrace : public ::testing::Test {
protected:
    std::string path;
    void SetUp() override {
        RB3TraceShutdown();                 // clean slate (idempotent)
        path = TempTracePath();
        std::remove(path.c_str());
        setenv("RB3_SESSION_TRACE", path.c_str(), 1);
        unsetenv("RB3_FRAME_TRACE");
        unsetenv("RB3_TRACE_FRAME_DECIMATE");
        unsetenv("RB3_TRACE_FRAME_MS");
        unsetenv("RB3_TRACE_RING");
        gFrameTraceLoaderAdds = 0;
        gFrameTraceStreamOpens = 0;
    }
    void TearDown() override {
        RB3TraceShutdown();
        std::remove(path.c_str());
        unsetenv("RB3_SESSION_TRACE");
    }
};

} // namespace

// ---------------------------------------------------------------------------
// (1) hdr is line 1: k=hdr, v=1, sid present, platform=native.
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, HeaderIsFirstLine) {
    RB3TraceInit();
    EXPECT_TRUE(gRB3TraceActive) << "RB3_SESSION_TRACE must arm the recorder";
    RB3TraceShutdown();

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_FALSE(lines.empty()) << "trace file must have at least the hdr line";
    const std::string &h = lines[0];
    EXPECT_TRUE(LineIsWellFormed(h)) << "hdr must be one well-formed JSON line: " << h;
    EXPECT_EQ(GetRaw(h, "k"), "hdr");
    EXPECT_EQ(GetRaw(h, "v"), "1");
    EXPECT_EQ(GetRaw(h, "platform"), "native");
    EXPECT_FALSE(GetRaw(h, "sid").empty()) << "hdr must carry a minted sid";
    EXPECT_EQ(GetRaw(h, "sid").size(), 16u) << "sid is 16 hex chars";
    EXPECT_FALSE(GetObject(h, "build").empty()) << "hdr must carry build{}";
    EXPECT_EQ(GetRaw(h, "cs"), "0") << "hdr client_seq is 0";
}

// ---------------------------------------------------------------------------
// (2) every non-hdr line has t,f,cs,k AND cs strictly increases.
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, EnvelopeAndClientSeqMonotonic) {
    RB3TraceInit();
    RB3TraceSetFrame(1);
    RB3RecordBootMark("engine_init_done");
    RB3TraceSetFrame(2);
    RB3RecordNav("a", "b", "btn", false);
    RB3TraceSetFrame(3);
    RB3RecordInput(0, 0x1, 0x1, 0x0, 0.0f, 0.0f);
    RB3TraceShutdown();

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_GE(lines.size(), 4u);
    long long prevCs = -1;
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string &ln = lines[i];
        EXPECT_TRUE(LineIsWellFormed(ln)) << "line " << i << " malformed: " << ln;
        if (i == 0) {
            EXPECT_EQ(GetRaw(ln, "k"), "hdr");
            continue;
        }
        EXPECT_TRUE(HasKey(ln, "t")) << "non-hdr needs t: " << ln;
        EXPECT_TRUE(HasKey(ln, "f")) << "non-hdr needs f: " << ln;
        EXPECT_TRUE(HasKey(ln, "cs")) << "non-hdr needs cs: " << ln;
        EXPECT_TRUE(HasKey(ln, "k")) << "non-hdr needs k: " << ln;
        long long cs = std::atoll(GetRaw(ln, "cs").c_str());
        EXPECT_GT(cs, prevCs) << "client_seq must strictly increase: " << ln;
        prevCs = cs;
    }
}

// ---------------------------------------------------------------------------
// (3) sm omitted when <0 (menus); present when >=0. The core's CurrentSongMs()
//     returns -1, so every recorded row omits sm. We additionally drive the
//     serializer directly via a frame whose sm we can't set here — so this test
//     asserts the OMIT path (the >=0 path is exercised by construction in the
//     serializer; menu rows must never carry "sm":-1).
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, SongMsOmittedInMenus) {
    RB3TraceInit();
    RB3TraceSetFrame(10);
    RB3RecordNav("menu_a", "menu_b", "focus", false);
    RB3TraceShutdown();

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_GE(lines.size(), 2u);
    const std::string &nav = lines[1];
    EXPECT_FALSE(HasKey(nav, "sm")) << "menu rows omit sm entirely (no \"sm\":-1): " << nav;
    EXPECT_EQ(nav.find("\"sm\":-1"), std::string::npos) << "must never emit sm:-1";
}

// ---------------------------------------------------------------------------
// (4) in(0,0) has no ax; in(whammy=0.5) -> ax.wh~=500 and no ti.
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, InputAxesSparse) {
    RB3TraceInit();
    RB3TraceSetFrame(1);
    RB3RecordInput(0, 0x2, 0x2, 0x0, 0.0f, 0.0f);   // no analog
    RB3TraceSetFrame(2);
    RB3RecordInput(0, 0x4, 0x4, 0x0, 0.5f, 0.0f);   // whammy only
    RB3TraceShutdown();

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_GE(lines.size(), 3u);
    const std::string &in0 = lines[1];
    const std::string &in1 = lines[2];

    EXPECT_EQ(GetRaw(in0, "k"), "in");
    EXPECT_FALSE(HasKey(in0, "ax")) << "all-zero axes => no ax: " << in0;

    EXPECT_EQ(GetRaw(in1, "k"), "in");
    std::string ax = GetObject(in1, "ax");
    ASSERT_FALSE(ax.empty()) << "non-zero whammy => ax present: " << in1;
    int wh = std::atoi(GetRaw(ax, "wh").c_str());
    EXPECT_NEAR(wh, 500, 1) << "whammy 0.5 => ax.wh ~= 500: " << ax;
    EXPECT_FALSE(HasKey(ax, "ti")) << "tilt 0 => no ti key: " << ax;
}

// ---------------------------------------------------------------------------
// (5) a screen name with a quote + newline round-trips as ONE valid JSON line.
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, EscapingKeepsOneLine) {
    RB3TraceInit();
    RB3TraceSetFrame(1);
    // Nasty name: embedded quote, backslash, newline, tab, control char.
    // NB: write the control byte as its own string literal so the following
    // 'c'/'t'/'l' chars aren't swallowed by C++'s greedy \x hex escape.
    const char *nasty = "scr\"ee\\n\nname\twith\x01" "ctl";
    RB3RecordNav("from", nasty, "focus", true);
    RB3TraceShutdown();

    std::vector<std::string> lines = ReadLines(path);
    // The whole event MUST be on a single physical line — no extra line from the
    // embedded '\n'. So lines = [hdr, nav] exactly.
    ASSERT_EQ(lines.size(), 2u) << "embedded newline must not split the event";
    const std::string &nav = lines[1];
    EXPECT_TRUE(LineIsWellFormed(nav)) << "escaped nav must be valid JSON: " << nav;
    EXPECT_EQ(GetRaw(nav, "k"), "nav");
    // wentBack=true must serialize as wb:true.
    EXPECT_EQ(GetRaw(nav, "wb"), "true");
    // The raw newline must NOT appear unescaped; the escaped form \n (backslash
    // + n) must be present in the `to` field.
    std::string to = GetRaw(nav, "to");
    EXPECT_NE(to.find("\\n"), std::string::npos) << "newline must be escaped: " << to;
    EXPECT_NE(to.find("\\\""), std::string::npos) << "quote must be escaped: " << to;
    EXPECT_NE(to.find("\\u0001"), std::string::npos) << "ctl char must be \\u-escaped: " << to;
}

// ---------------------------------------------------------------------------
// (6) RB3FrameTraceRecord (the back-compat shim) yields an fr row carrying
//     dt/lp/lpu/scr/ld/st/pend, with ld/st pulled from the engine counters.
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, BackCompatFrameShim) {
    RB3TraceInit();
    // Simulate the engine choke points firing this frame.
    gFrameTraceLoaderAdds = 3;
    gFrameTraceStreamOpens = 1;
    RB3FrameTraceRecord(0, 16.7f, 2.0f, 0.5f, "main_hub", 4);
    RB3TraceShutdown();

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_GE(lines.size(), 2u);
    const std::string &fr = lines[1];
    EXPECT_TRUE(LineIsWellFormed(fr)) << fr;
    EXPECT_EQ(GetRaw(fr, "k"), "fr");
    EXPECT_TRUE(HasKey(fr, "dt"));
    EXPECT_TRUE(HasKey(fr, "lp"));
    EXPECT_TRUE(HasKey(fr, "lpu"));
    EXPECT_EQ(GetRaw(fr, "scr"), "main_hub");
    EXPECT_EQ(std::atoi(GetRaw(fr, "ld").c_str()), 3) << "ld pulled from engine counter";
    EXPECT_EQ(std::atoi(GetRaw(fr, "st").c_str()), 1) << "st pulled from engine counter";
    EXPECT_EQ(std::atoi(GetRaw(fr, "pend").c_str()), 4);
    // The shim must have ZEROED the engine counters after reading them.
    EXPECT_EQ(gFrameTraceLoaderAdds, 0);
    EXPECT_EQ(gFrameTraceStreamOpens, 0);
}

// ---------------------------------------------------------------------------
// (bonus) RB3_FRAME_TRACE alias arms the recorder with full-frame capture.
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, FrameTraceAliasFullCapture) {
    unsetenv("RB3_SESSION_TRACE");
    setenv("RB3_FRAME_TRACE", path.c_str(), 1);
    RB3TraceInit();
    EXPECT_TRUE(gRB3TraceActive);
    // decimate=1 under the alias: every frame emits even off-multiples.
    RB3TraceSetFrame(7);   // 7 % 30 != 0, so only the alias' decim=1 lets it pass
    RB3RecordFrame(8.0f, 0.0f, 0.0f, "scr", 0);
    RB3TraceShutdown();
    unsetenv("RB3_FRAME_TRACE");

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_GE(lines.size(), 2u) << "alias must full-capture (decimate=1)";
    EXPECT_EQ(GetRaw(lines[1], "k"), "fr");
}

// ---------------------------------------------------------------------------
// (7) RB3RecordSong emits song{ev,id,track,diff}. score/pct are present when
//     >=0 and OMITTED when -1 (the OQ7-deferred accessor passes -1).
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, SongScorePctOmittedWhenNegative) {
    RB3TraceInit();
    RB3TraceSetFrame(1);
    // start: score/pct unknown -> pass -1 -> omitted.
    RB3RecordSong("start", "song_abc", "guitar", "expert", -1.0f, -1.0f);
    RB3TraceSetFrame(2);
    // end: real score + pct (0..1 fraction).
    RB3RecordSong("end", "song_abc", "guitar", "expert", 98250.0f, 0.873f);
    RB3TraceShutdown();

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_GE(lines.size(), 3u);
    const std::string &s0 = lines[1];
    const std::string &s1 = lines[2];

    // Reader-expected keys: k=song, ev/id/track/diff.
    EXPECT_TRUE(LineIsWellFormed(s0)) << s0;
    EXPECT_EQ(GetRaw(s0, "k"), "song");
    EXPECT_EQ(GetRaw(s0, "ev"), "start");
    EXPECT_EQ(GetRaw(s0, "id"), "song_abc");
    EXPECT_EQ(GetRaw(s0, "track"), "guitar");
    EXPECT_EQ(GetRaw(s0, "diff"), "expert");
    // score/pct OMITTED when -1 (never serialize "score":-1).
    EXPECT_FALSE(HasKey(s0, "score")) << "score omitted when <0: " << s0;
    EXPECT_FALSE(HasKey(s0, "pct")) << "pct omitted when <0: " << s0;

    // end row carries score + pct.
    EXPECT_EQ(GetRaw(s1, "ev"), "end");
    ASSERT_TRUE(HasKey(s1, "score")) << "score present when >=0: " << s1;
    ASSERT_TRUE(HasKey(s1, "pct")) << "pct present when >=0: " << s1;
    EXPECT_NEAR(std::atof(GetRaw(s1, "score").c_str()), 98250.0, 0.5);
    EXPECT_NEAR(std::atof(GetRaw(s1, "pct").c_str()), 0.873, 0.001);
}

// ---------------------------------------------------------------------------
// (8) RB3RecordAudio emits au{under,frames} (both always present).
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, AudioUnderrunRow) {
    RB3TraceInit();
    RB3TraceSetFrame(5);
    RB3RecordAudio(2, 384);
    RB3TraceShutdown();

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_GE(lines.size(), 2u);
    const std::string &au = lines[1];
    EXPECT_TRUE(LineIsWellFormed(au)) << au;
    EXPECT_EQ(GetRaw(au, "k"), "au");
    EXPECT_EQ(std::atoi(GetRaw(au, "under").c_str()), 2);
    EXPECT_EQ(std::atoi(GetRaw(au, "frames").c_str()), 384);
}

// ---------------------------------------------------------------------------
// (9) RB3RecordLog emits log{lvl,msg} + src only when non-null.
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, LogRowSrcOptional) {
    RB3TraceInit();
    RB3TraceSetFrame(1);
    RB3RecordLog("warn", "low memory", "Heap.cpp:204");   // with src
    RB3TraceSetFrame(2);
    RB3RecordLog("assert", "bad index", nullptr);          // no src
    RB3TraceShutdown();

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_GE(lines.size(), 3u);
    const std::string &l0 = lines[1];
    const std::string &l1 = lines[2];

    EXPECT_TRUE(LineIsWellFormed(l0)) << l0;
    EXPECT_EQ(GetRaw(l0, "k"), "log");
    EXPECT_EQ(GetRaw(l0, "lvl"), "warn");
    EXPECT_EQ(GetRaw(l0, "msg"), "low memory");
    EXPECT_EQ(GetRaw(l0, "src"), "Heap.cpp:204");

    EXPECT_EQ(GetRaw(l1, "lvl"), "assert");
    EXPECT_EQ(GetRaw(l1, "msg"), "bad index");
    EXPECT_FALSE(HasKey(l1, "src")) << "null src must omit the key: " << l1;
}

// ---------------------------------------------------------------------------
// (10) RB3RecordMark emits mark{tag} + note only when non-null.
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, MarkRowNoteOptional) {
    RB3TraceInit();
    RB3TraceSetFrame(1);
    RB3RecordMark("repro_start", "stutter on entering hub");   // with note
    RB3TraceSetFrame(2);
    RB3RecordMark("checkpoint", nullptr);                       // no note
    RB3TraceShutdown();

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_GE(lines.size(), 3u);
    const std::string &m0 = lines[1];
    const std::string &m1 = lines[2];

    EXPECT_TRUE(LineIsWellFormed(m0)) << m0;
    EXPECT_EQ(GetRaw(m0, "k"), "mark");
    EXPECT_EQ(GetRaw(m0, "tag"), "repro_start");
    EXPECT_EQ(GetRaw(m0, "note"), "stutter on entering hub");

    EXPECT_EQ(GetRaw(m1, "tag"), "checkpoint");
    EXPECT_FALSE(HasKey(m1, "note")) << "null note must omit the key: " << m1;
}

// ---------------------------------------------------------------------------
// (11) RB3TraceSetSongMs(>=0) makes the envelope carry `sm`; setting it back to
//      <0 omits `sm` again. Replaces the hardcoded -1 CurrentSongMs in the core.
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, SongMsEnvelopePresentThenOmitted) {
    RB3TraceInit();
    RB3TraceSetFrame(1);
    // No song yet -> sm omitted.
    RB3RecordMark("before", nullptr);
    // In a song now.
    RB3TraceSetSongMs(12345.5f);
    RB3TraceSetFrame(2);
    RB3RecordMark("during", nullptr);
    // Song ended (menus) -> back to <0 -> omit.
    RB3TraceSetSongMs(-1.0f);
    RB3TraceSetFrame(3);
    RB3RecordMark("after", nullptr);
    RB3TraceShutdown();

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_GE(lines.size(), 4u);
    const std::string &before = lines[1];
    const std::string &during = lines[2];
    const std::string &after  = lines[3];

    EXPECT_FALSE(HasKey(before, "sm")) << "no song yet => no sm: " << before;
    ASSERT_TRUE(HasKey(during, "sm")) << "song-ms set => sm present: " << during;
    EXPECT_NEAR(std::atof(GetRaw(during, "sm").c_str()), 12345.5, 0.1);
    EXPECT_FALSE(HasKey(after, "sm")) << "song ended (<0) => sm omitted: " << after;
    EXPECT_EQ(after.find("\"sm\":-1"), std::string::npos) << "never emit sm:-1";
}

// ---------------------------------------------------------------------------
// (12) A song id with a quote + newline round-trips as ONE valid JSON line
//      (proves the typed song recorder json_escapes its strings too).
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, SongStringsEscaped) {
    RB3TraceInit();
    RB3TraceSetFrame(1);
    const char *nastyId = "song\"x\\y\nz";
    RB3RecordSong("load", nastyId, "drum", "hard", -1.0f, -1.0f);
    RB3TraceShutdown();

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_EQ(lines.size(), 2u) << "embedded newline must not split the event";
    const std::string &s = lines[1];
    EXPECT_TRUE(LineIsWellFormed(s)) << s;
    std::string id = GetRaw(s, "id");
    EXPECT_NE(id.find("\\n"), std::string::npos) << "newline escaped: " << id;
    EXPECT_NE(id.find("\\\""), std::string::npos) << "quote escaped: " << id;
}

// ---------------------------------------------------------------------------
// (M4-a) RB3RecordCheckpoint emits a chk row carrying the fast-equality hash `h`
//        + RAW state fields (scr/focus/sec/beat/score/scores[]/np/pct). In a song
//        the envelope carries sm; in menus (songMs<0) sm is omitted. The hash is
//        DETERMINISTIC for an identical tuple and PERTURBED by a 1-point score
//        change (the integer scoreSum is un-quantized).
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, CheckpointRowAndHash) {
    RB3TraceInit();

    // In-song checkpoint: 2 players, exact scores 1000 + 500.
    int scores[2] = {1000, 500};
    RB3TraceSetFrame(30);
    RB3RecordCheckpoint("game", "trk_focus", /*taskSec*/ 4.250f, /*beat*/ 8.00f,
                        /*songMs*/ 4250.0f, /*scoreSum*/ 1500, scores, 2,
                        /*nPlayers*/ 2, /*pct*/ 42);
    // Identical tuple again -> identical hash (determinism).
    RB3TraceSetFrame(60);
    RB3RecordCheckpoint("game", "trk_focus", 4.250f, 8.00f, 4250.0f, 1500,
                        scores, 2, 2, 42);
    // One point higher score -> hash MUST differ (exact-int score in the hash).
    int scores2[2] = {1001, 500};
    RB3TraceSetFrame(90);
    RB3RecordCheckpoint("game", "trk_focus", 4.250f, 8.00f, 4250.0f, 1501,
                        scores2, 2, 2, 42);
    // Menu checkpoint: no song -> sm omitted, empty scores, np=0.
    RB3TraceSetFrame(120);
    RB3RecordCheckpoint("main_hub", "play_btn", 9.000f, 0.0f, /*songMs*/ -1.0f,
                        0, nullptr, 0, 0, -1);
    RB3TraceShutdown();

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_GE(lines.size(), 5u);
    const std::string &c0 = lines[1];
    const std::string &c1 = lines[2];
    const std::string &c2 = lines[3];
    const std::string &cm = lines[4];

    // Shape: k=chk, hash h, raw fields.
    EXPECT_TRUE(LineIsWellFormed(c0)) << c0;
    EXPECT_EQ(GetRaw(c0, "k"), "chk");
    EXPECT_EQ(GetRaw(c0, "scr"), "game");
    EXPECT_EQ(GetRaw(c0, "focus"), "trk_focus");
    EXPECT_FALSE(GetRaw(c0, "h").empty()) << "chk must carry the hash h: " << c0;
    EXPECT_EQ(GetRaw(c0, "h").size(), 16u) << "h is 16 hex chars (u64)";
    EXPECT_EQ(std::atoll(GetRaw(c0, "score").c_str()), 1500);
    EXPECT_EQ(std::atoi(GetRaw(c0, "np").c_str()), 2);
    EXPECT_EQ(std::atoi(GetRaw(c0, "pct").c_str()), 42);
    ASSERT_TRUE(HasKey(c0, "sm")) << "in-song chk carries sm: " << c0;
    EXPECT_NEAR(std::atof(GetRaw(c0, "sm").c_str()), 4250.0, 0.1);
    EXPECT_NE(c0.find("\"scores\":[1000,500]"), std::string::npos)
        << "raw per-player scores array: " << c0;

    // Determinism: identical tuple -> identical hash.
    EXPECT_EQ(GetRaw(c0, "h"), GetRaw(c1, "h"))
        << "identical state tuple must hash identically";
    // Sensitivity: a 1-point score bump -> different hash.
    EXPECT_NE(GetRaw(c0, "h"), GetRaw(c2, "h"))
        << "a 1-point score change must perturb the hash";

    // Menu chk: sm omitted, np=0, empty scores, pct omitted.
    EXPECT_EQ(GetRaw(cm, "k"), "chk");
    EXPECT_FALSE(HasKey(cm, "sm")) << "menu chk omits sm: " << cm;
    EXPECT_EQ(cm.find("\"sm\":-1"), std::string::npos) << "never emit sm:-1";
    EXPECT_EQ(std::atoi(GetRaw(cm, "np").c_str()), 0);
    EXPECT_NE(cm.find("\"scores\":[]"), std::string::npos)
        << "menu chk has an empty scores array: " << cm;
    EXPECT_FALSE(HasKey(cm, "pct")) << "pct omitted when <0: " << cm;
}

// ---------------------------------------------------------------------------
// (M4-clk-a) RB3RecordClock emits an UN-decimated clk{f,sdt,sm} row EVERY frame
//        (no §4.7 decimation — even off-multiple frames), carrying sdt always and
//        sm only in a song. This is the per-frame clock that frame-locks the
//        fixed-clock replay (vs the decimated fr stream's stale carry-forward).
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, ClockRowsEmittedEveryFrame) {
    RB3TraceInit();
    // Default decimate=30, so frames 7/13 would NOT emit an fr row — but clk must.
    // Frame 7: in a menu (songMs < 0 -> no sm).
    RB3TraceSetFrame(7);
    RB3RecordClock(0.016f, -1.0f);
    // Frame 13: in a song (songMs >= 0 -> sm present).
    RB3TraceSetFrame(13);
    RB3RecordClock(0.017f, 4250.0f);
    RB3TraceShutdown();

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_GE(lines.size(), 3u) << "two clk rows + hdr";
    const std::string &c0 = lines[1];
    const std::string &c1 = lines[2];

    EXPECT_TRUE(LineIsWellFormed(c0)) << c0;
    EXPECT_EQ(GetRaw(c0, "k"), "clk");
    EXPECT_EQ(std::atoi(GetRaw(c0, "f").c_str()), 7) << "clk is un-decimated (frame 7)";
    EXPECT_NEAR(std::atof(GetRaw(c0, "sdt").c_str()), 0.016, 0.001);
    EXPECT_FALSE(HasKey(c0, "sm")) << "menu clk omits sm: " << c0;
    EXPECT_EQ(c0.find("\"sm\":-1"), std::string::npos) << "never emit sm:-1";

    EXPECT_EQ(GetRaw(c1, "k"), "clk");
    EXPECT_EQ(std::atoi(GetRaw(c1, "f").c_str()), 13);
    EXPECT_NEAR(std::atof(GetRaw(c1, "sdt").c_str()), 0.017, 0.001);
    ASSERT_TRUE(HasKey(c1, "sm")) << "in-song clk carries sm: " << c1;
    EXPECT_NEAR(std::atof(GetRaw(c1, "sm").c_str()), 4250.0, 0.1);
}

// ---------------------------------------------------------------------------
// (M4-GAP1-a) RB3TraceSetSeed stamps the boot RNG seed into the hdr `seed`. The
//        hdr is written LAZILY (the seed is set during the App ctor, AFTER
//        RB3TraceInit), so a seed set BEFORE the first event must still land in
//        the hdr. With NO seed set, the hdr omits `seed` entirely.
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, HeaderCarriesSeedWhenSet) {
    RB3TraceInit();
    // Boot path sets the seed before any event (mirrors System.cpp SeedRand site).
    RB3TraceSetSeed(1234567);
    RB3TraceSetFrame(1);
    RB3RecordBootMark("engine_init_done");   // first event -> flushes the hdr
    RB3TraceShutdown();

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_FALSE(lines.empty());
    const std::string &h = lines[0];
    EXPECT_EQ(GetRaw(h, "k"), "hdr");
    ASSERT_TRUE(HasKey(h, "seed")) << "hdr must carry the captured seed: " << h;
    EXPECT_EQ(std::atoi(GetRaw(h, "seed").c_str()), 1234567);
}

TEST_F(SessionTrace, HeaderOmitsSeedWhenUnset) {
    RB3TraceInit();
    RB3TraceSetFrame(1);
    RB3RecordBootMark("engine_init_done");
    RB3TraceShutdown();

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_FALSE(lines.empty());
    EXPECT_FALSE(HasKey(lines[0], "seed")) << "no seed set => hdr omits seed: " << lines[0];
}

// ---------------------------------------------------------------------------
// (M4-GAP1-b) A hdr-only session (no events) is still a valid trace: the lazy
//        hdr is flushed on shutdown so a seed-bearing run with no events is
//        readable. (Regression guard for the deferred-hdr change.)
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, HeaderFlushedOnShutdownWithNoEvents) {
    RB3TraceInit();
    RB3TraceSetSeed(42);
    RB3TraceShutdown();   // no events recorded at all

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_EQ(lines.size(), 1u) << "hdr-only trace must have exactly the hdr line";
    EXPECT_EQ(GetRaw(lines[0], "k"), "hdr");
    EXPECT_EQ(std::atoi(GetRaw(lines[0], "seed").c_str()), 42);
}

// ---------------------------------------------------------------------------
// (M4-GAP2-a) RB3TraceRecordAid emits a one-shot replayable mark{tag:"aid",
//        note:<name>} at the current frame AND folds the aid into hdr.flags.aids.
//        De-duped: re-applying the same aid records the mark once.
// ---------------------------------------------------------------------------
TEST_F(SessionTrace, RunAidMarkAndHeaderFlag) {
    RB3TraceInit();
    RB3TraceSetSeed(7);             // so the hdr is interesting too
    RB3TraceSetFrame(900);
    RB3TraceRecordAid("autohit");   // first event -> flushes hdr (aids summary set)
    RB3TraceSetFrame(905);
    RB3TraceRecordAid("autohit");   // duplicate -> NO second mark
    RB3TraceSetFrame(910);
    RB3TraceRecordAid("nofail");
    RB3TraceShutdown();

    std::vector<std::string> lines = ReadLines(path);
    ASSERT_GE(lines.size(), 3u);

    // The aid marks: exactly TWO (autohit once + nofail once), tag "aid".
    int aidMarks = 0;
    bool sawAutohit = false, sawNofail = false;
    int autohitFrame = -1;
    for (size_t i = 1; i < lines.size(); ++i) {
        if (GetRaw(lines[i], "k") == "mark" && GetRaw(lines[i], "tag") == "aid") {
            aidMarks++;
            std::string note = GetRaw(lines[i], "note");
            if (note == "autohit") { sawAutohit = true; autohitFrame = std::atoi(GetRaw(lines[i], "f").c_str()); }
            if (note == "nofail")  sawNofail = true;
        }
    }
    EXPECT_EQ(aidMarks, 2) << "autohit de-duped -> 2 aid marks total";
    EXPECT_TRUE(sawAutohit);
    EXPECT_TRUE(sawNofail);
    EXPECT_EQ(autohitFrame, 900) << "aid mark stamps the frame it was applied at";

    // hdr.flags.aids summary carries both (the hdr was written when the first aid
    // fired; autohit was already set, nofail comes later so it may or may not be
    // in the summary — autohit MUST be).
    const std::string &h = lines[0];
    std::string flags = GetObject(h, "flags");
    ASSERT_FALSE(flags.empty()) << "hdr must carry flags{}: " << h;
    EXPECT_NE(flags.find("autohit"), std::string::npos)
        << "hdr.flags.aids must list autohit (set before the hdr flushed): " << flags;
}

// ---------------------------------------------------------------------------
// (M4-GAP1-c) The replay side parses the hdr `seed`; RB3ReplaySeed returns it.
//        And the run-aid marks parse into RB3ReplayPendingAids with the recorded
//        frame + the latch (RB3ReplayMarkAidApplied) one-shot semantics.
//        Standalone (own temp file + RB3_REPLAY env), independent of the fixture.
// ---------------------------------------------------------------------------
TEST(SessionReplay, SeedAndAidsFromTrace) {
    std::string p = kAbsTempBase + "rb3_replay_gap_" +
                    std::to_string((long)getpid()) + ".jsonl";
    std::remove(p.c_str());
    {
        std::ofstream f(p.c_str());
        // hdr carries a seed + an aids flag; two aid marks at known frames; one
        // `in` edge so replay also arms (not required for seed/aids, but realistic).
        f << "{\"k\":\"hdr\",\"v\":1,\"sid\":\"abcdef0123456789\",\"platform\":\"native\",\"seed\":-559038737,\"flags\":{\"aids\":[\"autohit\",\"nofail\"]}}\n";
        f << "{\"t\":10.0,\"f\":300,\"cs\":1,\"k\":\"mark\",\"tag\":\"aid\",\"note\":\"nofail\"}\n";
        f << "{\"t\":20.0,\"f\":360,\"cs\":2,\"k\":\"mark\",\"tag\":\"aid\",\"note\":\"autohit\"}\n";
        f << "{\"t\":30.0,\"f\":360,\"cs\":3,\"k\":\"in\",\"pad\":0,\"b\":1,\"dn\":1,\"up\":0}\n";
        f.close();
    }

    RB3ReplayResetForTest();   // clear any latched state from a prior replay test
    setenv("RB3_REPLAY", p.c_str(), 1);
    RB3ReplayInit();

    // GAP 1: the seed parsed back exactly (signed; -559038737 == 0xDEADBEEF).
    int seed = 0;
    ASSERT_TRUE(RB3ReplaySeed(&seed)) << "trace carried a seed -> RB3ReplaySeed true";
    EXPECT_EQ(seed, -559038737);

    // GAP 2: two aid markers parsed; ordered by frame (nofail@300 before autohit@360).
    ASSERT_TRUE(RB3ReplayHasAids());
    const char *due[4] = {nullptr, nullptr, nullptr, nullptr};

    // Before the first aid frame -> nothing due.
    EXPECT_EQ(RB3ReplayPendingAids(299, due, 4), 0);

    // At frame 300 -> nofail due (not autohit yet).
    int n = RB3ReplayPendingAids(300, due, 4);
    ASSERT_EQ(n, 1);
    EXPECT_STREQ(due[0], "nofail");
    // Not latched until we confirm it applied: still due if we re-poll.
    EXPECT_EQ(RB3ReplayPendingAids(300, due, 4), 1) << "unlatched aid stays pending";
    RB3ReplayMarkAidApplied("nofail");
    EXPECT_EQ(RB3ReplayPendingAids(300, due, 4), 0) << "latched aid no longer pending";

    // At frame 360 -> autohit due; latch it.
    n = RB3ReplayPendingAids(360, due, 4);
    ASSERT_EQ(n, 1);
    EXPECT_STREQ(due[0], "autohit");
    RB3ReplayMarkAidApplied("autohit");
    EXPECT_EQ(RB3ReplayPendingAids(100000, due, 4), 0) << "all aids latched";

    unsetenv("RB3_REPLAY");
    std::remove(p.c_str());
}

// ---------------------------------------------------------------------------
// (13) Tier-1 REPLAY carry-forward semantics (rb3_replay.cpp).
//
// Write a tiny `in`-trace fixture with two edges at KNOWN frames, point
// RB3_REPLAY at it, arm replay (RB3ReplayInit), and assert RB3ReplayBitsForFrame
// is the edge-only carry-forward the JoypadPoll hook re-drives:
//   - 0 before the first edge,
//   - the edge's `b` AT and AFTER its frame,
//   - HELD until the next edge supersedes it (and held forever past the last).
// The fixture mirrors the live recorder's `in` line shape exactly
// ("k":"in", "f":<frame>, "b":<bits>), with non-`in` and comment lines mixed in
// to prove the tolerant scanner skips them.
//
// NB: RB3ReplayInit is idempotent (it latches on first call) and is otherwise
// only invoked from JoypadPoll, which these recorder-core tests never reach — so
// this is the single, first, and only caller in the process. It is registered
// AFTER the SessionTrace fixture tests, but it does not depend on the fixture
// (it owns its own temp file + env), so ordering is irrelevant.
// ---------------------------------------------------------------------------
TEST(SessionReplay, BitsForFrameCarryForward) {
    // Build the fixture path next to the other temp traces (absolute, chdir-safe).
    std::string path = kAbsTempBase + "rb3_replay_test_" +
                       std::to_string((long)getpid()) + ".jsonl";
    std::remove(path.c_str());

    // Two edges: bits 0x40 held from frame 10, bits 0x1000 held from frame 20.
    // Interleave a hdr, a non-`in` row, a comment, and a blank to exercise the
    // tolerant line scanner (it must ingest ONLY the two `in` rows).
    {
        std::ofstream f(path.c_str());
        f << "{\"k\":\"hdr\",\"v\":1,\"sid\":\"deadbeefdeadbeef\",\"platform\":\"native\"}\n";
        f << "# a comment line the scanner must skip\n";
        f << "{\"t\":100.0,\"f\":5,\"cs\":1,\"k\":\"nav\",\"from\":\"a\",\"to\":\"b\"}\n";
        f << "{\"t\":200.0,\"f\":10,\"cs\":2,\"k\":\"in\",\"pad\":0,\"b\":64,\"dn\":64,\"up\":0}\n";
        f << "\n";  // blank line
        f << "{\"t\":400.0,\"f\":20,\"cs\":3,\"k\":\"in\",\"pad\":0,\"b\":4096,\"dn\":4096,\"up\":64}\n";
        f.close();
    }

    RB3ReplayResetForTest();   // clear any latched state from a prior replay test
    setenv("RB3_REPLAY", path.c_str(), 1);
    RB3ReplayInit();
    EXPECT_TRUE(RB3ReplayActive()) << "two `in` edges must arm replay";
    EXPECT_EQ(RB3ReplayLastFrame(), 20) << "lastFrame is the last `in` edge's f";

    // Before the first edge -> 0 (default carry-forward base).
    EXPECT_EQ(RB3ReplayBitsForFrame(0), 0u);
    EXPECT_EQ(RB3ReplayBitsForFrame(9), 0u) << "frame just before first edge holds 0";

    // AT and AFTER the first edge -> 0x40, held until the next edge's frame.
    EXPECT_EQ(RB3ReplayBitsForFrame(10), 0x40u) << "edge applies AT its own frame";
    EXPECT_EQ(RB3ReplayBitsForFrame(11), 0x40u) << "held forward";
    EXPECT_EQ(RB3ReplayBitsForFrame(19), 0x40u) << "held right up to the next edge";

    // AT and AFTER the second edge -> 0x1000, held forever past the last edge.
    EXPECT_EQ(RB3ReplayBitsForFrame(20), 0x1000u) << "second edge supersedes at its frame";
    EXPECT_EQ(RB3ReplayBitsForFrame(21), 0x1000u) << "held forward";
    EXPECT_EQ(RB3ReplayBitsForFrame(100000), 0x1000u) << "last edge holds indefinitely";

    unsetenv("RB3_REPLAY");
    std::remove(path.c_str());
}

// ---------------------------------------------------------------------------
// (M4-clk-b) The replay clock prefers the PER-FRAME `clk` stream so
// RB3ReplaySongMsForFrame(N) returns the EXACT recorded sm at frame N — NO stale
// carry-forward between decimated samples. The fixture carries a clk row at EVERY
// frame plus a single (decimated) fr row whose sm would carry forward stale; the
// per-frame clk lookup must return each frame's own value, not the fr value.
// ---------------------------------------------------------------------------
TEST(SessionReplay, ClockPerFrameExactSongMs) {
    std::string path = kAbsTempBase + "rb3_replay_clk_" +
                       std::to_string((long)getpid()) + ".jsonl";
    std::remove(path.c_str());

    // clk at frames 100..104, sm advancing 16 ms/frame (1000,1016,...). A single
    // (decimated) fr row at frame 100 with a STALE sm=1000 — if the lookup used the
    // fr carry-forward it would wrongly return 1000 for frames 101..104. One `in`
    // edge so replay arms (clk drives the clock even with no input, but be realistic).
    {
        std::ofstream f(path.c_str());
        f << "{\"k\":\"hdr\",\"v\":1,\"sid\":\"c1c1c1c1c1c1c1c1\",\"platform\":\"native\"}\n";
        f << "{\"t\":1.0,\"f\":100,\"sm\":1000.0,\"cs\":1,\"k\":\"fr\",\"dt\":16.0,\"lp\":0,\"lpu\":0,\"sdt\":0.016,\"scr\":\"game\",\"ld\":0,\"st\":0,\"pend\":0}\n";
        f << "{\"t\":2.0,\"f\":100,\"sm\":1000.0,\"cs\":2,\"k\":\"clk\",\"sdt\":0.016}\n";
        f << "{\"t\":3.0,\"f\":101,\"sm\":1016.0,\"cs\":3,\"k\":\"clk\",\"sdt\":0.016}\n";
        f << "{\"t\":4.0,\"f\":102,\"sm\":1032.0,\"cs\":4,\"k\":\"clk\",\"sdt\":0.016}\n";
        f << "{\"t\":5.0,\"f\":103,\"sm\":1048.0,\"cs\":5,\"k\":\"clk\",\"sdt\":0.016}\n";
        f << "{\"t\":6.0,\"f\":104,\"sm\":1064.0,\"cs\":6,\"k\":\"clk\",\"sdt\":0.016}\n";
        f << "{\"t\":7.0,\"f\":100,\"cs\":7,\"k\":\"in\",\"pad\":0,\"b\":1,\"dn\":1,\"up\":0}\n";
        f.close();
    }

    RB3ReplayResetForTest();
    setenv("RB3_REPLAY", path.c_str(), 1);
    RB3ReplayInit();
    ASSERT_TRUE(RB3ReplayActive());

    // EXACT per-frame song-ms — each frame returns ITS OWN recorded value, NOT the
    // fr's stale 1000. This is the frame-lock the decimated fr stream could not give.
    EXPECT_NEAR(RB3ReplaySongMsForFrame(100), 1000.0f, 0.01f);
    EXPECT_NEAR(RB3ReplaySongMsForFrame(101), 1016.0f, 0.01f)
        << "per-frame clk must give 1016, NOT the carried-forward fr 1000";
    EXPECT_NEAR(RB3ReplaySongMsForFrame(102), 1032.0f, 0.01f);
    EXPECT_NEAR(RB3ReplaySongMsForFrame(103), 1048.0f, 0.01f);
    EXPECT_NEAR(RB3ReplaySongMsForFrame(104), 1064.0f, 0.01f);
    // sdt likewise comes from the per-frame clk.
    EXPECT_NEAR(RB3ReplayDtForFrame(102), 0.016f, 0.0001f);
    // Past the last clk frame -> holds the final sample (no snap-back to menus).
    EXPECT_NEAR(RB3ReplaySongMsForFrame(200), 1064.0f, 0.01f);
    // Before the first recorded frame -> menus (-1).
    EXPECT_LT(RB3ReplaySongMsForFrame(50), 0.0f);

    unsetenv("RB3_REPLAY");
    std::remove(path.c_str());
}

// ---------------------------------------------------------------------------
// (M4-clk-c) FALLBACK: an OLDER trace with NO clk stream still drives the
// fixed-clock replay from the decimated `fr` rows (carry-forward), so the seam
// keeps working on pre-clk traces.
// ---------------------------------------------------------------------------
TEST(SessionReplay, ClockFallsBackToFrRows) {
    std::string path = kAbsTempBase + "rb3_replay_frfallback_" +
                       std::to_string((long)getpid()) + ".jsonl";
    std::remove(path.c_str());

    // Only fr rows (decimated), NO clk. sm at frame 100 = 1000, next fr at 130 = 1500.
    {
        std::ofstream f(path.c_str());
        f << "{\"k\":\"hdr\",\"v\":1,\"sid\":\"f0f0f0f0f0f0f0f0\",\"platform\":\"native\"}\n";
        f << "{\"t\":1.0,\"f\":100,\"sm\":1000.0,\"cs\":1,\"k\":\"fr\",\"dt\":16.0,\"lp\":0,\"lpu\":0,\"sdt\":0.016,\"scr\":\"game\",\"ld\":0,\"st\":0,\"pend\":0}\n";
        f << "{\"t\":2.0,\"f\":130,\"sm\":1500.0,\"cs\":2,\"k\":\"fr\",\"dt\":16.0,\"lp\":0,\"lpu\":0,\"sdt\":0.016,\"scr\":\"game\",\"ld\":0,\"st\":0,\"pend\":0}\n";
        f << "{\"t\":3.0,\"f\":100,\"cs\":3,\"k\":\"in\",\"pad\":0,\"b\":1,\"dn\":1,\"up\":0}\n";
        f.close();
    }

    RB3ReplayResetForTest();
    setenv("RB3_REPLAY", path.c_str(), 1);
    RB3ReplayInit();
    ASSERT_TRUE(RB3ReplayActive());

    // No clk -> fr carry-forward: frames 100..129 hold 1000, 130+ hold 1500.
    EXPECT_NEAR(RB3ReplaySongMsForFrame(100), 1000.0f, 0.01f);
    EXPECT_NEAR(RB3ReplaySongMsForFrame(115), 1000.0f, 0.01f) << "fr carry-forward";
    EXPECT_NEAR(RB3ReplaySongMsForFrame(130), 1500.0f, 0.01f);
    EXPECT_LT(RB3ReplaySongMsForFrame(50), 0.0f) << "before first fr -> menus";

    unsetenv("RB3_REPLAY");
    std::remove(path.c_str());
}

// ---------------------------------------------------------------------------
// (M4-T3) MILESTONE ANCHORING — faithful repro of the Wave-7 gate trace shape:
// a long pre-roll PLATEAU where songMs is pinned at exactly 0.0 (the countdown,
// SAME absolute frame on both sides), then the AUDIO-START where the clock begins
// advancing at a NON-deterministic absolute frame (gate trace: rec 4698 vs replay
// 4703, a +5-frame async audio-callback phase shift). The in-song replay clock is
// indexed RELATIVE to each side's audio-start so that phase shift cancels (the
// Wave-7 residual drifted in-song to ~-754 ms because the recorded curve was read
// at the phase-shifted ABSOLUTE frame).
//
// Recorded curve: menus < f100, plateau sm==0 at f100..109, audio-start (sm>0) at
// f110 advancing 16 ms/frame (1000,1016,...). recSongStartFrame = 110 (first sm>0).
// The REPLAY's plateau runs 5 frames LONGER: its OWN audio clock (injected via
// RB3ReplaySetLiveSongMsForTest — the real-engine live-audio signal) stays 0
// through replay frame 114 and advances at frame 115 (the +5 phase shift). The
// latch must fire at 115 (the replay's audio-start), NOT 110 (where the recorded
// curve advances at the same absolute frame), so the relative index cancels the
// shift. This is the case the pure-recorded-curve signal CANNOT resolve and is
// exactly why the latch reads the live audio clock.
// ---------------------------------------------------------------------------
TEST(SessionReplay, InSongMilestoneAnchoredToReplayLiveAudioStart) {
    std::string path = kAbsTempBase + "rb3_replay_anchor_" +
                       std::to_string((long)getpid()) + ".jsonl";
    std::remove(path.c_str());

    {
        std::ofstream f(path.c_str());
        f << "{\"k\":\"hdr\",\"v\":1,\"sid\":\"a5a5a5a5a5a5a5a5\",\"platform\":\"native\"}\n";
        // Pre-roll PLATEAU: sm pinned at exactly 0.0 (countdown), frames 100..109.
        for (int fr = 100; fr <= 109; ++fr)
            f << "{\"t\":" << fr << ".0,\"f\":" << fr
              << ",\"sm\":0.0,\"cs\":" << (fr - 99) << ",\"k\":\"clk\",\"sdt\":0.016}\n";
        // AUDIO-START: clock advances at frame 110 (recSongStart), sm=1000+(fr-110)*16.
        for (int fr = 110; fr <= 140; ++fr) {
            int sm = 1000 + (fr - 110) * 16;
            f << "{\"t\":" << (fr + 1000) << ".0,\"f\":" << fr << ",\"sm\":" << sm
              << ".0,\"cs\":" << (fr + 100) << ",\"k\":\"clk\",\"sdt\":0.016}\n";
        }
        f << "{\"t\":9000.0,\"f\":100,\"cs\":900,\"k\":\"in\",\"pad\":0,\"b\":1,\"dn\":1,\"up\":0}\n";
        f.close();
    }

    RB3ReplayResetForTest();
    setenv("RB3_REPLAY", path.c_str(), 1);
    RB3ReplayInit();
    ASSERT_TRUE(RB3ReplayActive());

    // Drive the replay's OWN live audio clock: 0 during ITS plateau (frames
    // 100..114, 5 frames longer than the recording), then advancing from 115. At
    // the audio-start frame 115 the clock has already ticked off 0 (rec[110]=1000's
    // analogue is rec[110]'s 1000 but the LIVE clock just needs to be > 0 to signal
    // the start), so 115 yields a small positive value.
    auto liveAt = [](int fr) -> float {
        if (fr < 115) return 0.0f;          // replay still in its (longer) plateau
        return 16.0f * (fr - 115) + 0.9f;   // replay's own audio advancing (>0 at 115)
    };

    // Before the recorded curve exists (menus) -> -1, no matter the live clock.
    RB3ReplaySetLiveSongMsForTest(liveAt(99));
    EXPECT_LT(RB3ReplaySongMsForFrame(99), 0.0f) << "before plateau -> menus";

    // PLATEAU on the replay side: the recorded curve is 0.0 AND the live clock is 0,
    // so frames 100..114 feed the aligned 0.0 and DO NOT latch — even at 110..114
    // where the RECORDED curve has already gone positive (the rec-side audio-start).
    // The live clock (still 0) correctly keeps the replay in plateau.
    for (int fr = 100; fr <= 114; ++fr) {
        RB3ReplaySetLiveSongMsForTest(liveAt(fr));
        EXPECT_NEAR(RB3ReplaySongMsForFrame(fr), 0.0f, 0.01f)
            << "replay plateau frame " << fr
            << " feeds 0.0 (live clock still 0) — no early latch on the rec curve";
    }

    // AUDIO-START on the replay side at f115: the live clock advances (> 0) -> latch
    // replaySongStart=115. relIndex = recSongStart(110) + (115-115) = 110 ->
    // recorded 1000 (the recorded audio-start value), NOT the absolute-frame value
    // rec[115]=1080 (the Wave-7 +5-frame-ahead drift).
    RB3ReplaySetLiveSongMsForTest(liveAt(115));
    EXPECT_NEAR(RB3ReplaySongMsForFrame(115), 1000.0f, 0.01f)
        << "replay live audio-start latches; reads recorded audio-start 1000, "
           "NOT the +5-frame-ahead absolute rec[115]=1080 (the Wave-7 drift)";
    // Thereafter the recorded curve is indexed RELATIVE to audio-start:
    // replay 116 -> recorded 111 -> 1016, replay 120 -> recorded 115 -> 1080.
    RB3ReplaySetLiveSongMsForTest(liveAt(116));
    EXPECT_NEAR(RB3ReplaySongMsForFrame(116), 1016.0f, 0.01f);
    RB3ReplaySetLiveSongMsForTest(liveAt(120));
    EXPECT_NEAR(RB3ReplaySongMsForFrame(120), 1080.0f, 0.01f)
        << "replay 120 -> recorded 110+(120-115)=115 -> 1080";
    // The +5-frame (80 ms) phase shift removed: anchored replay-115 (1000) + 80 ms
    // == the absolute rec[115] (1080) it replaces, proving the IN-SONG drift cancels.
    EXPECT_NEAR(RB3ReplaySongMsForFrame(115) + 80.0f, 1080.0f, 0.01f)
        << "anchored 1000 + 80 ms phase == absolute rec[115] 1080 (in-song drift gone)";

    // Past the recorded end: relIndex 110+(150-115)=145 > last recorded f140; holds
    // the final recorded sample (1000+(140-110)*16=1480), no snap-back to menus.
    RB3ReplaySetLiveSongMsForTest(liveAt(150));
    EXPECT_NEAR(RB3ReplaySongMsForFrame(150), 1480.0f, 0.01f)
        << "past recorded end holds final sample";

    RB3ReplaySetLiveSongMsForTest(-2.0f);   // restore the real accessor
    unsetenv("RB3_REPLAY");
    std::remove(path.c_str());
}

// ---------------------------------------------------------------------------
// (M4-T3-b) ANCHOR INVARIANT — when the replay audio-start coincides with the
// recorded audio-start (zero phase shift), the anchored lookup is IDENTICAL to the
// absolute-frame value. This is the no-op case the menu/plateau path relies on and
// the reason the existing per-frame-exact test (ClockPerFrameExactSongMs) stays
// green: latching at the recorded audio-start makes relIndex == askFrame.
// ---------------------------------------------------------------------------
TEST(SessionReplay, AnchorZeroPhaseEqualsAbsolute) {
    std::string path = kAbsTempBase + "rb3_replay_anchor0_" +
                       std::to_string((long)getpid()) + ".jsonl";
    std::remove(path.c_str());

    {
        std::ofstream f(path.c_str());
        f << "{\"k\":\"hdr\",\"v\":1,\"sid\":\"b6b6b6b6b6b6b6b6\",\"platform\":\"native\"}\n";
        for (int fr = 200; fr <= 205; ++fr) {  // advancing song clock from frame 200
            int sm = 5000 + (fr - 200) * 16;   // all > 0 -> audio-start at 200
            f << "{\"t\":" << fr << ".0,\"f\":" << fr << ",\"sm\":" << sm
              << ".0,\"cs\":" << (fr - 199) << ",\"k\":\"clk\",\"sdt\":0.016}\n";
        }
        f << "{\"t\":300.0,\"f\":200,\"cs\":99,\"k\":\"in\",\"pad\":0,\"b\":1,\"dn\":1,\"up\":0}\n";
        f.close();
    }

    RB3ReplayResetForTest();
    setenv("RB3_REPLAY", path.c_str(), 1);
    RB3ReplayInit();
    ASSERT_TRUE(RB3ReplayActive());

    // First advancing ask is at the recorded audio-start frame 200 -> latch=200 ->
    // zero phase shift -> relIndex == askFrame -> identical to the absolute curve.
    EXPECT_NEAR(RB3ReplaySongMsForFrame(200), 5000.0f, 0.01f);
    EXPECT_NEAR(RB3ReplaySongMsForFrame(201), 5016.0f, 0.01f);
    EXPECT_NEAR(RB3ReplaySongMsForFrame(205), 5080.0f, 0.01f);

    unsetenv("RB3_REPLAY");
    std::remove(path.c_str());
}
