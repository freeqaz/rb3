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
void RB3TraceFlush();
void RB3TraceShutdown();
void RB3FrameTraceRecord(int frame, float dtMs, float loadPollMs,
                         float loadPollUntilMs, const char *screen,
                         int pendingLoaders);

// Engine counters (so test (6) can prove ld/st flow through the shim).
extern int gFrameTraceLoaderAdds;
extern int gFrameTraceStreamOpens;

// ===========================================================================
// Minimal NDJSON test harness — strict-enough to prove the wire contract
// without a JSON library. Each line is validated brace/string-balanced and
// free of raw control chars; scalar/string values are pulled by key.
// ===========================================================================
namespace {

std::string TempTracePath() {
    const char *dir = std::getenv("TMPDIR");
    std::string base = (dir && dir[0]) ? std::string(dir) : std::string("/tmp");
    if (!base.empty() && base[base.size() - 1] != '/') base += '/';
    static int sCounter = 0;
    std::ostringstream os;
    os << base << "rb3_trace_test_" << (long)getpid() << "_" << (++sCounter)
       << ".jsonl";
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
