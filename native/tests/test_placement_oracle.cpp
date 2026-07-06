// W2.1.S1 — placement-gate oracle test (the "right, not just different" gate the
// splash draw-log golden cannot provide).
//
// The committed drawlog golden is splash_screen (no crowd, no drum), so the
// canonical comparator is blind to the SYS-1 placement bug: DrawMesh forces
// obj.world = identity for every skinned draw (Rnd_Wgpu_RB3.cpp:2847-2848), so
// all crowd 3D-char instances co-locate at the origin. This suite stands up the
// gate that CAN see it.
//
//   * Synthetic tests (always run, no GPU / no boot) prove the oracle logic
//     (placement_oracle.h): it FAILS a co-located (all-identity) frame the way
//     the current build draws, PASSES a correctly-spread frame, and reports
//     INCONCLUSIVE (not a false pass) when the capture never reached a crowd
//     scene. These are the committed fail-red demonstrations.
//
//   * RealCaptureSpansBowl is the live gate: when RB3_PLACEMENT_DRAWLOG and
//     RB3_PLACEMENT_PROBE_LOG point at a real gameplay capture
//     (scripts/native/placement-gate-capture.py), it runs the oracle over the
//     live artifacts and EXPECTs PASS. On the UNCHANGED 6221a56 build it goes
//     RED (crowd drawn at identity) — that red is the free proof the gate sees
//     the bug. Under the flag-ON W2.1.S2 build it turns GREEN. It GTEST_SKIPs
//     when the env is unset so the default `rb3-tests` run is unaffected.

// test_helpers.h FIRST (neutralizes glibc st_*time macros before decomp headers;
// see test_draw_log_golden.cpp). It pulls in <gtest/gtest.h>.
#include "test_helpers.h"

#include "placement_oracle.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace placement;
using drawlog::DrawLogFrame;
using drawlog::DrawRec;

namespace {

// A skinned draw at a given world translation (column-major world[12..14]).
DrawRec SkinnedAt(double x, double y, double z) {
    DrawRec d;
    d.skinned = true;
    for (int i = 0; i < 16; ++i) d.world[i] = (i % 5 == 0) ? 1.f : 0.f; // identity basis
    d.world[12] = (float)x; d.world[13] = (float)y; d.world[14] = (float)z;
    return d;
}

// A bowl of N crowd instances spread along +X, offset from origin (like a real
// venue crowd: far from origin, pairwise-distinct).
std::vector<PosedInstance> BowlPosed(int n) {
    std::vector<PosedInstance> v;
    for (int i = 0; i < n; ++i) {
        PosedInstance p;
        p.kind = "crowd"; p.index = i;
        p.x = 20.0 + i * 4.0;   // 20, 24, 28, ...  — spans the bowl, far from origin
        p.y = 3.0;
        p.z = -10.0 + i * 1.5;
        v.push_back(p);
    }
    return v;
}

} // namespace

// ---------------------------------------------------------------------------
// Probe parser: pulls RB3_PLACEMENT_PROBE lines out of a noisy engine log.
// ---------------------------------------------------------------------------
TEST(PlacementOracle, ParsesProbeLog) {
    std::string log =
        "[boot] something\n"
        "RB3_PLACEMENT_PROBE crowd inst=0 x=20.0000 y=3.0000 z=-10.0000\n"
        "some other engine noise line\n"
        "RB3_PLACEMENT_PROBE crowd inst=1 x=24.0000 y=3.0000 z=-8.5000\n"
        "[warn] undefined symbol foo\n";
    auto posed = ParsePlacementProbeText(log);
    ASSERT_EQ(posed.size(), 2u);
    EXPECT_EQ(posed[0].kind, "crowd");
    EXPECT_EQ(posed[0].index, 0);
    EXPECT_NEAR(posed[0].x, 20.0, 1e-6);
    EXPECT_NEAR(posed[1].z, -8.5, 1e-6);
}

// ---------------------------------------------------------------------------
// (fail-red demo) The CURRENT build draws every skinned instance at identity.
// The oracle must FAIL a frame shaped like that, naming all three defect kinds.
// ---------------------------------------------------------------------------
TEST(PlacementOracle, CatchesColocationLikeCurrentBuild) {
    auto posed = BowlPosed(6);                 // faithful spread (from the probe)
    DrawLogFrame frame; frame.valid = true;
    // Every skinned draw at the origin (identity) — exactly what DrawMesh's
    // `else if (skinned) { identity }` arm produces today. Include a couple of
    // band-like skinned draws too (also identity — correctly so).
    for (int i = 0; i < 8; ++i) frame.draws.push_back(SkinnedAt(0, 0, 0));

    OracleResult r = RunPlacementOracle(frame, posed);
    EXPECT_FALSE(r.passed) << r.Describe();
    EXPECT_FALSE(r.inconclusive) << r.Describe();
    EXPECT_TRUE(r.Has(kPosedNotDrawn))  << r.Describe();
    EXPECT_TRUE(r.Has(kDrawnCollapsed)) << r.Describe();
    EXPECT_TRUE(r.Has(kDrawnColocated)) << r.Describe();
}

// ---------------------------------------------------------------------------
// The oracle PASSES a frame whose skinned draws carry the posed spXfm positions
// (what the fixed placement contract produces). Proves it is not a tautological
// "always red" gate.
// ---------------------------------------------------------------------------
TEST(PlacementOracle, PassesOnCorrectSpread) {
    auto posed = BowlPosed(6);
    DrawLogFrame frame; frame.valid = true;
    // Each crowd instance drawn at its spXfm.v. Plus band members at identity
    // (their WorldXfm is identity by design — must NOT trip the gate).
    for (const auto& p : posed) frame.draws.push_back(SkinnedAt(p.x, p.y, p.z));
    frame.draws.push_back(SkinnedAt(0, 0, 0));   // band member
    frame.draws.push_back(SkinnedAt(0, 0, 0));   // band member

    OracleResult r = RunPlacementOracle(frame, posed);
    EXPECT_TRUE(r.passed) << r.Describe();
    EXPECT_GE(r.matchedCount, 2);
    EXPECT_GE(r.drawnClusters, 2);
}

// ---------------------------------------------------------------------------
// Tolerance is real: a small per-instance jitter (< matchEps) still PASSES, so
// sub-mesh local offsets / float noise do not false-fail the fixed build.
// ---------------------------------------------------------------------------
TEST(PlacementOracle, ToleratesSubEpsJitter) {
    auto posed = BowlPosed(6);
    DrawLogFrame frame; frame.valid = true;
    for (const auto& p : posed)
        frame.draws.push_back(SkinnedAt(p.x + 0.3, p.y - 0.2, p.z + 0.25));  // < matchEps=1.0
    OracleResult r = RunPlacementOracle(frame, posed);
    EXPECT_TRUE(r.passed) << r.Describe();
}

// ---------------------------------------------------------------------------
// Not a false pass: an empty / tiny probe (capture never reached a crowd scene)
// is INCONCLUSIVE, not GREEN.
// ---------------------------------------------------------------------------
TEST(PlacementOracle, InconclusiveWhenNoCrowd) {
    std::vector<PosedInstance> none;
    DrawLogFrame frame; frame.valid = true;
    frame.draws.push_back(SkinnedAt(0, 0, 0));
    OracleResult r = RunPlacementOracle(frame, none);
    EXPECT_FALSE(r.passed);
    EXPECT_TRUE(r.inconclusive) << r.Describe();
}

// ---------------------------------------------------------------------------
// A collapsed-but-nonzero pathology (all instances share ONE non-origin world —
// the historical co-location class) still FAILS on span + distinctness.
// ---------------------------------------------------------------------------
TEST(PlacementOracle, CatchesSharedNonOriginWorld) {
    auto posed = BowlPosed(6);
    DrawLogFrame frame; frame.valid = true;
    for (int i = 0; i < 6; ++i) frame.draws.push_back(SkinnedAt(24, 3, -8));  // all one spot
    OracleResult r = RunPlacementOracle(frame, posed);
    EXPECT_FALSE(r.passed) << r.Describe();
    EXPECT_TRUE(r.Has(kDrawnCollapsed) || r.Has(kDrawnColocated)) << r.Describe();
}

// ---------------------------------------------------------------------------
// LIVE gate. RB3_PLACEMENT_DRAWLOG (JSON draw-log) + RB3_PLACEMENT_PROBE_LOG
// (engine log with RB3_PLACEMENT_PROBE lines) point at a real capture from
// scripts/native/placement-gate-capture.py. Runs the oracle over the live
// artifacts and EXPECTs PASS — RED on the current build (the free fail-red),
// GREEN under the flag-ON W2.1.S2 build. SKIP when unset.
// ---------------------------------------------------------------------------
TEST(PlacementOracle, RealCaptureSpansBowl) {
    const char* drawlogPath = getenv("RB3_PLACEMENT_DRAWLOG");
    const char* probePath   = getenv("RB3_PLACEMENT_PROBE_LOG");
    if (!drawlogPath || !probePath) {
        GTEST_SKIP() << "set RB3_PLACEMENT_DRAWLOG + RB3_PLACEMENT_PROBE_LOG "
                        "(see scripts/native/placement-gate-capture.py) to run the "
                        "live placement gate";
    }
    DrawLogFrame frame = drawlog::LoadDrawLogFile(drawlogPath);
    ASSERT_TRUE(frame.valid) << "draw-log parse error: " << frame.error
                             << " (" << drawlogPath << ")";
    auto posed = ParsePlacementProbeFile(probePath);

    OracleResult r = RunPlacementOracle(frame, posed);
    if (r.inconclusive) {
        GTEST_SKIP() << "capture inconclusive (did not reach a spread crowd frame):\n"
                     << r.Describe();
    }
    EXPECT_TRUE(r.passed)
        << "PLACEMENT GATE RED — crowd instances are not drawn at their faithful "
           "spXfm positions:\n" << r.Describe();
}
