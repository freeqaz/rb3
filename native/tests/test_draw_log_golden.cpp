// W0.3.S2 — draw-log golden test (the per-draw state regression net).
//
// This is the regression NET itself: it proves the W0.3 comparator
// (drawlog_compare.h) can go GREEN on an eps-equivalent candidate and, more
// importantly, can go RED on the two historical per-draw regression classes we
// must catch mechanically:
//
//   * CatchesCoLocation      — duplicate one crowd instance's world xfm onto
//                              another (the crowd/drum collapse) -> world-eps
//                              FAIL naming draw 1 / world.
//   * CatchesBindGroupCollapse — share one obj bind-group id across two
//                              instances (the a0f98ad uniform-collapse class)
//                              -> sharing-pattern FAIL naming the obj stream
//                              and the pair (0,1).
//
// Plus MatchesGoldenWithinEps (tolerance is real, not zero) and
// CatchesDroppedDraw (a missing draw is itself a regression). These four
// require NEITHER a GPU NOR an engine boot — they run the pure comparator over
// the committed synthetic golden (goldens/drawlog/synthetic_scene.json).
//
// PopulatesFromRealDrawMesh is GPU-gated (EnsureGpu + GTEST_SKIP). Real-draw
// population of the ring is independently proven by S1's headless boot capture
// (877-891 draws dumped from a real menu frame) and S3's /api/drawlog live
// capture; an in-process gBandRnd.DrawMesh drive needs full camera + material
// + active-pass state that the unit fixture does not stand up, so this case
// documents that and SKIPs rather than risk a crash on the headless host.

// test_helpers.h FIRST: it pulls in <gtest/gtest.h> then neutralizes glibc's
// st_atime/st_mtime/st_ctime macros before any decomp header (os/File.h uses
// those as struct member names). Including raw <gtest/gtest.h> here instead
// would let the macros corrupt File.h pulled transitively by Rnd_Wgpu_RB3.h.
#include "test_helpers.h"

#include "drawlog_compare.h"

#include "platform/Rnd_Wgpu_RB3.h"        // gBandRnd, BandRnd::InitGpu
#include "platform/RB3DrawLogDebug.h"     // RB3DebugSetDrawLogEnabled / RB3DebugGetDrawLog

#include <string>
#include <cstdlib>

using namespace drawlog;

namespace {

// Resolve goldens/drawlog/<name> relative to THIS source file. CMake compiles
// tests with absolute source paths, so __FILE__ is absolute; strip to the
// directory and append the fixture path. An env override (RB3_DRAWLOG_GOLDEN_DIR)
// wins if set, for out-of-tree runs.
std::string GoldenPath(const char* name) {
    if (const char* env = getenv("RB3_DRAWLOG_GOLDEN_DIR")) {
        return std::string(env) + "/" + name;
    }
    std::string file = __FILE__;
    size_t slash = file.find_last_of("/\\");
    std::string dir = (slash == std::string::npos) ? "." : file.substr(0, slash);
    return dir + "/goldens/drawlog/" + name;
}

DrawLogFrame LoadGolden() {
    return LoadDrawLogFile(GoldenPath("synthetic_scene.json").c_str());
}

} // namespace

// ---------------------------------------------------------------------------
// The committed golden parses to the expected 4-draw synthetic scene.
// ---------------------------------------------------------------------------
TEST(DrawLogGolden, GoldenParses) {
    DrawLogFrame g = LoadGolden();
    ASSERT_TRUE(g.valid) << "golden parse error: " << g.error
                         << " (path " << GoldenPath("synthetic_scene.json") << ")";
    ASSERT_EQ(g.draws.size(), 4u);
    EXPECT_EQ(g.count, 4);
    // Two crowd-like instances (draws 0,1): distinct world translations, distinct obj ids.
    EXPECT_FLOAT_EQ(g.draws[0].world[12],  3.5f);
    EXPECT_FLOAT_EQ(g.draws[1].world[12], -3.5f);
    EXPECT_NE(g.draws[0].obj, g.draws[1].obj);
    // Scene bind group is shared across the frame (all 0); obj ids all distinct.
    EXPECT_EQ(g.draws[0].scene, g.draws[3].scene);
    EXPECT_EQ(g.draws[0].pipelineHash, g.draws[1].pipelineHash);
    EXPECT_TRUE(g.draws[0].skinned);
    EXPECT_FALSE(g.draws[2].skinned);
}

// ---------------------------------------------------------------------------
// A golden compared to itself passes (self-consistency of the comparator).
// ---------------------------------------------------------------------------
TEST(DrawLogGolden, IdenticalPasses) {
    DrawLogFrame g = LoadGolden();
    ASSERT_TRUE(g.valid) << g.error;
    CompareResult r = CompareDrawLogs(g, g);
    EXPECT_TRUE(r.passed) << Describe(r);
    EXPECT_TRUE(r.failures.empty());
}

// ---------------------------------------------------------------------------
// Tolerance is REAL: an eps-jittered candidate (translations < transEps, basis
// < rotEps) still PASSES. Proves the net does not false-positive on float noise.
// ---------------------------------------------------------------------------
TEST(DrawLogGolden, MatchesGoldenWithinEps) {
    DrawLogFrame g = LoadGolden();
    ASSERT_TRUE(g.valid) << g.error;
    DrawLogFrame cand = g;   // deep copy of the parsed structs

    Tolerances tol;   // rotEps=1e-4, transEps=1e-2
    for (auto& d : cand.draws) {
        // Translation jitter strictly under transEps.
        d.world[12] += 0.004f;
        d.world[13] -= 0.003f;
        d.world[14] += 0.002f;
        // Basis jitter strictly under rotEps.
        d.world[0]  += 5e-5f;
        d.world[5]  -= 4e-5f;
        d.world[10] += 3e-5f;
    }

    CompareResult r = CompareDrawLogs(g, cand, tol);
    EXPECT_TRUE(r.passed) << "eps-jittered candidate should pass:\n" << Describe(r);
}

// ---------------------------------------------------------------------------
// A jitter just OVER transEps must FAIL — confirms the eps bound is a real edge,
// not "anything passes". (Guards against an accidentally-huge tolerance.)
// ---------------------------------------------------------------------------
TEST(DrawLogGolden, RejectsOverEpsTranslation) {
    DrawLogFrame g = LoadGolden();
    ASSERT_TRUE(g.valid) << g.error;
    DrawLogFrame cand = g;
    cand.draws[2].world[13] += 0.05f;   // > transEps (1e-2)

    CompareResult r = CompareDrawLogs(g, cand);
    EXPECT_FALSE(r.passed);
    EXPECT_TRUE(r.Has("world", 2)) << Describe(r);
}

// ---------------------------------------------------------------------------
// FAIL-RED #1: CO-LOCATION. Duplicate draw[0]'s world xfm onto draw[1] (the
// crowd/drum collapse: instance B lands on instance A). The comparator must
// report passed=false with a world failure at draw 1.
// ---------------------------------------------------------------------------
TEST(DrawLogGolden, CatchesCoLocation) {
    DrawLogFrame g = LoadGolden();
    ASSERT_TRUE(g.valid) << g.error;
    DrawLogFrame cand = g;

    // Co-locate: instance B's world becomes identical to instance A's.
    for (int e = 0; e < 16; ++e) cand.draws[1].world[e] = g.draws[0].world[e];

    CompareResult r = CompareDrawLogs(g, cand);
    EXPECT_FALSE(r.passed) << "co-located candidate must fail";
    EXPECT_TRUE(r.Has("world", 1))
        << "failure must name draw 1 / world:\n" << Describe(r);
    // The offending element is the translation X (index 12: 3.5 vs golden -3.5).
    bool namedTx = false;
    for (const auto& f : r.failures)
        if (f.field == "world" && f.index == 1 &&
            f.golden.find("world[12]") != std::string::npos)
            namedTx = true;
    EXPECT_TRUE(namedTx) << "should name world[12] (translation X):\n" << Describe(r);
}

// ---------------------------------------------------------------------------
// FAIL-RED #2: BIND-GROUP COLLAPSE (the a0f98ad class). Share one obj bind-group
// id across draws 0 and 1 (two instances collapse onto one per-object uniform).
// The comparator must report passed=false with an obj-stream failure naming the
// pair (0,1).
// ---------------------------------------------------------------------------
TEST(DrawLogGolden, CatchesBindGroupCollapse) {
    DrawLogFrame g = LoadGolden();
    ASSERT_TRUE(g.valid) << g.error;
    DrawLogFrame cand = g;

    // Collapse: draw 1 now shares draw 0's obj bind group. Golden had them
    // distinct (obj 0 vs 1); candidate makes them equal -> sharing diverges.
    cand.draws[1].obj = g.draws[0].obj;

    CompareResult r = CompareDrawLogs(g, cand);
    EXPECT_FALSE(r.passed) << "bind-group-collapsed candidate must fail";
    EXPECT_TRUE(r.HasPair("obj", 0, 1))
        << "failure must name the obj stream and the pair (0,1):\n" << Describe(r);
    // World is untouched, so there must be NO world failure — this proves the
    // sharing-pattern rule fires independently of the xfm rule.
    EXPECT_FALSE(r.Has("world")) << "collapse should not trip a world failure:\n"
                                 << Describe(r);
}

// ---------------------------------------------------------------------------
// A dropped draw (candidate one shorter) is itself a regression -> count FAIL.
// ---------------------------------------------------------------------------
TEST(DrawLogGolden, CatchesDroppedDraw) {
    DrawLogFrame g = LoadGolden();
    ASSERT_TRUE(g.valid) << g.error;
    DrawLogFrame cand = g;
    cand.draws.pop_back();   // drop draw 3

    CompareResult r = CompareDrawLogs(g, cand);
    EXPECT_FALSE(r.passed) << "dropped-draw candidate must fail";
    EXPECT_TRUE(r.Has("count")) << Describe(r);
}

// ---------------------------------------------------------------------------
// An EXACT scalar-field change (pipeline hash) is caught and named.
// ---------------------------------------------------------------------------
TEST(DrawLogGolden, CatchesPipelineChange) {
    DrawLogFrame g = LoadGolden();
    ASSERT_TRUE(g.valid) << g.error;
    DrawLogFrame cand = g;
    cand.draws[2].pipelineHash ^= 0x1ull;   // any bit flip

    CompareResult r = CompareDrawLogs(g, cand);
    EXPECT_FALSE(r.passed);
    EXPECT_TRUE(r.Has("pipe", 2)) << Describe(r);
}

// ---------------------------------------------------------------------------
// The JSON round-trips through the parser: re-comparing the parsed golden to a
// freshly re-parsed copy of the same bytes passes (parser determinism).
// ---------------------------------------------------------------------------
TEST(DrawLogGolden, ParserRoundTrip) {
    std::string bytes;
    ASSERT_TRUE(ReadFile(GoldenPath("synthetic_scene.json").c_str(), bytes));
    DrawLogFrame a = ParseDrawLog(bytes);
    DrawLogFrame b = ParseDrawLog(bytes);
    ASSERT_TRUE(a.valid && b.valid) << a.error << " / " << b.error;
    CompareResult r = CompareDrawLogs(a, b);
    EXPECT_TRUE(r.passed) << Describe(r);
}

// ===========================================================================
// GPU-gated: drive real draws through gBandRnd and read the ring back.
// ===========================================================================
namespace {
bool EnsureGpu() {
    static int sState = -1; // -1 untried, 0 failed, 1 ready
    if (sState >= 0) return sState == 1;
    bool ok = gBandRnd.InitGpu(/*width=*/64, /*height=*/64, /*headless=*/true);
    sState = ok ? 1 : 0;
    return ok;
}
} // namespace

TEST(DrawLogGolden, PopulatesFromRealDrawMesh) {
    if (!EnsureGpu())
        GTEST_SKIP() << "headless GPU device unavailable on this host";

    // Prove the debug accessor surface is wired: forcing recording on flips
    // RB3DebugDrawLogEnabled() true without any env var, and the ring is
    // readable (empty until a frame draws). This exercises the S1 accessors the
    // net depends on.
    RB3DebugSetDrawLogEnabled(true);
    EXPECT_TRUE(RB3DebugDrawLogEnabled());
    const std::vector<RB3DrawRecord>& log = RB3DebugGetDrawLog();
    (void)log;   // readable handle; contents depend on prior draws
    RB3DebugSetDrawLogEnabled(false);

    // A full in-process gBandRnd.BeginFrame(cam)/DrawMesh(a)/DrawMesh(b)/EndFrame()
    // drive needs a valid RndCam (RndCam::sCurrent), per-mesh material + geometry
    // that survives unpacking, and an active render pass — none of which the unit
    // fixture stands up. Attempting it risks a segfault on the headless host, so
    // real-draw population is proven independently by S1's boot capture (877-891
    // draws dumped from a live menu frame) and S3's /api/drawlog live capture.
    GTEST_SKIP() << "real-draw population proven by S1 boot capture + S3 live "
                    "capture; in-process DrawMesh needs full camera/material/pass "
                    "state not stood up by the unit fixture";
}
