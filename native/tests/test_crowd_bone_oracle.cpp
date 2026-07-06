// W2.3.S1 — crowd bone-source negative-control oracle test.
//
// The splash draw-log golden has no crowd, so it is blind to the SYS-1 bone-source
// half: the crowd bone palette is composed from the GeomOwner's bones, and its
// correctness today depends on the draw-time RebindCrowdCharBonesToOwnSkeleton
// (Crowd.cpp) rebaking those owner offsets. This suite stands up the gate that sees
// the shard-drop return when that rebind is disabled.
//
//   * Synthetic tests (always run, no GPU / no boot) prove the oracle logic
//     (crowd_bone_oracle.h): it PASSES when candidate shard-drop is not elevated vs
//     the rebind-ON baseline, FAILS (fail-red) when it spikes, classifies the S1
//     DECISION (SHARED / SELF+POISON / MIXED) from the raw seam, and parses a real
//     [CROWD_BONE_PROBE] engine-log line. These are the committed fail-red demos.
//
//   * RealCaptureNotElevated is the live gate: when RB3_CROWD_BONE_BASELINE (rebind
//     ON) and RB3_CROWD_BONE_CANDIDATE (rebind OFF) point at real captures from
//     scripts/native/crowd-bone-gate-capture.py, it runs the oracle over the live
//     artifacts and EXPECTs PASS. On the UNCHANGED 6852caa build the candidate is
//     RB3_NO_CROWD_REBIND=1 → shard-drop spikes → RED (the free fail-red). Under a
//     future W2.3-flag-ON build the candidate is flag-ON+rebind-OFF and must turn
//     GREEN. It GTEST_SKIPs when the env is unset.

#include "test_helpers.h"   // FIRST (glibc st_*time macro neutralization + gtest)

#include "crowd_bone_oracle.h"

#include <string>
#include <vector>

using namespace crowdbone;

namespace {

// Build a probe record for one crowd-mesh instance.
ProbeRec Rec(const std::string& mesh, int inst, bool ownerEqMesh, int diffInstance,
             double worstOwner, double worstOwn, const char* tag = "crowdextra") {
    ProbeRec r;
    r.tag = tag; r.mesh = mesh; r.inst = inst;
    r.ownerEqMesh = ownerEqMesh; r.diffInstance = diffInstance;
    r.ownerBones = 24; r.meshBones = 24;
    r.worstOwnerExtent = worstOwner; r.worstOwnExtent = worstOwn;
    r.verdict = (worstOwner <= 12.0) ? "OWNER-CLEAN"
              : (worstOwn   <= 12.0) ? "OWN-BONES-FIX" : "OWN-BONES-POISON";
    return r;
}

} // namespace

// ---------------------------------------------------------------------------
// Parser: pull a real [CROWD_BONE_PROBE] line out of a noisy engine log.
// ---------------------------------------------------------------------------
TEST(CrowdBoneOracle, ParsesProbeLine) {
    std::string log =
        "[boot] noise\n"
        "[CROWD_BONE_PROBE] tag=crowdextra mesh='char_crowd_body.mesh' inst=0 "
        "owner=0x55a1 mesh=0x55a1 ownerEqMesh=1 ownerBones=24 meshBones=24 "
        "diffInstance=0 nullOwner=0 nullOwn=0 worstOwnerExtent=41.3u(b7) "
        "worstOwnExtent=41.3u(b7) SKIN_CLAMP=12u -> OWN-BONES-POISON\n"
        "[warn] undefined symbol foo\n";
    auto recs = ParseCrowdBoneProbeText(log);
    ASSERT_EQ(recs.size(), 1u);
    EXPECT_EQ(recs[0].tag, "crowdextra");
    EXPECT_EQ(recs[0].mesh, "char_crowd_body.mesh");
    EXPECT_TRUE(recs[0].ownerEqMesh);
    EXPECT_EQ(recs[0].diffInstance, 0);
    EXPECT_NEAR(recs[0].worstOwnerExtent, 41.3, 1e-3);
    EXPECT_NEAR(recs[0].worstOwnExtent, 41.3, 1e-3);
    EXPECT_EQ(recs[0].verdict, "OWN-BONES-POISON");
}

// ---------------------------------------------------------------------------
// Parser: pull a real [SKIN_CLAMP] line out of a noisy engine log; only
// crowd/extras meshes count toward the shard-drop metric.
// ---------------------------------------------------------------------------
TEST(CrowdBoneOracle, ParsesSkinClampAndFiltersCrowd) {
    std::string log =
        "[SKIN_CLAMP] mesh='female_extras_skin01.mesh' bone='bone_L-mid.mesh' meshLocal=375.7u (clamped to bind)\n"
        "[SKIN_CLAMP] mesh='male_crowd_body01.mesh' bone='bone_x.mesh' meshLocal=42.0u (clamped to bind)\n"
        "[SKIN_CLAMP] mesh='trackjacket.mesh' bone='bone_y.mesh' meshLocal=20.0u (clamped to bind)\n";
    Capture c = ParseCapture(log);
    EXPECT_EQ(c.skinClampEvents, 2);  // trackjacket (band) is NOT crowd/extras
    EXPECT_EQ(c.skinClampMeshes, 2);
}

// ---------------------------------------------------------------------------
// GREEN: rebind-ON baseline and a candidate with a comparable SKIN_CLAMP
// shard-drop count (within baseline*factor + slack) → not elevated → PASS.
// ---------------------------------------------------------------------------
TEST(CrowdBoneOracle, PassesWhenNotElevated) {
    Capture baseline;  baseline.skinClampEvents = 1642;
    Capture candidate; candidate.skinClampEvents = 1700;
    candidate.probes = { Rec("male_crowd_body01.mesh", 0, true, 0, 42.0, 42.0) };
    candidate.stats  = ComputeShardStats(candidate.probes, Options());
    OracleResult r = RunCrowdBoneOracle(baseline, candidate);
    EXPECT_TRUE(r.passed) << r.Describe();
    EXPECT_FALSE(r.inconclusive) << r.Describe();
}

// ---------------------------------------------------------------------------
// FAIL-RED: the current-build fail-red. The rebind-OFF candidate's crowd bones
// fling past the 12u clamp so the SKIN_CLAMP event count spikes ~8.8x vs the
// rebind-ON baseline → the gate goes RED. (MEASURED: baseline 1642 → candidate
// 14500 on 6852caa.)
// ---------------------------------------------------------------------------
TEST(CrowdBoneOracle, FailsRedWhenShardDropSpikes) {
    Capture baseline;  baseline.skinClampEvents = 1642;   // rebind ON
    Capture candidate; candidate.skinClampEvents = 14500; // rebind OFF
    candidate.probes = {
        Rec("male_crowd_body01.mesh",  0, true, 0, 42.7, 42.7),
        Rec("female_extra_body01.mesh", 0, true, 0, 380.5, 380.5),
    };
    candidate.stats = ComputeShardStats(candidate.probes, Options());
    OracleResult r = RunCrowdBoneOracle(baseline, candidate);
    EXPECT_FALSE(r.passed) << r.Describe();
    EXPECT_FALSE(r.inconclusive) << r.Describe();
}

// ---------------------------------------------------------------------------
// DECISION = SELF+POISON: crowd meshes are self-owned (owner==mesh, diffInstance
// 0) AND the drawn mesh's OWN bones exceed the clamp too → reading own bones is a
// no-op → W2.3's bone-source thesis is refuted for crowd. (S2 STOPS + escalates.)
// ---------------------------------------------------------------------------
TEST(CrowdBoneOracle, ClassifiesSelfPoison) {
    std::vector<ProbeRec> candidate = {  // raw seam, rebind OFF
        Rec("char_crowd_body.mesh", 0, /*ownerEqMesh*/true, /*diff*/0, 41.3, 41.3),
        Rec("char_extra_body.mesh", 0, true, 0, 55.7, 55.7),
    };
    ShardStats raw = ComputeShardStats(candidate, Options());
    EXPECT_EQ(raw.ownPoisonMeshes, 2);
    EXPECT_EQ(raw.ownFixMeshes, 0);
    EXPECT_EQ(DecideFrom(raw), kSelfPoison);
}

// ---------------------------------------------------------------------------
// DECISION = SHARED: crowd meshes share a GeomOwner (diffInstance>0 / owner!=mesh)
// AND the drawn mesh's OWN bones would pass the clamp while the owner's do not →
// reading own bones IS the fix → S2 proceeds with the bone-source de-alias.
// ---------------------------------------------------------------------------
TEST(CrowdBoneOracle, ClassifiesShared) {
    std::vector<ProbeRec> candidate = {
        Rec("char_crowd_body.mesh", 0, /*ownerEqMesh*/false, /*diff*/12, /*owner*/41.3, /*own*/2.0),
        Rec("char_extra_body.mesh", 0, false, 12, 55.7, 3.1),
    };
    ShardStats raw = ComputeShardStats(candidate, Options());
    EXPECT_EQ(raw.ownFixMeshes, 2);
    EXPECT_EQ(raw.ownPoisonMeshes, 0);
    EXPECT_EQ(raw.sharedMeshes, 2);
    EXPECT_EQ(DecideFrom(raw), kShared);
}

// ---------------------------------------------------------------------------
// DECISION = MIXED: some poisoned-owner meshes are own-bone-fixable, others are
// own-bone-poisoned → S2 lands the de-alias for the shared subset and documents
// the residual offset dependence.
// ---------------------------------------------------------------------------
TEST(CrowdBoneOracle, ClassifiesMixed) {
    std::vector<ProbeRec> candidate = {
        Rec("char_crowd_body.mesh", 0, false, 12, 41.3, 2.0),   // own-fix
        Rec("char_extra_body.mesh", 0, true,  0,  55.7, 55.7),  // own-poison
    };
    ShardStats raw = ComputeShardStats(candidate, Options());
    EXPECT_EQ(raw.ownFixMeshes, 1);
    EXPECT_EQ(raw.ownPoisonMeshes, 1);
    EXPECT_EQ(DecideFrom(raw), kMixed);
}

// ---------------------------------------------------------------------------
// Not a false pass: an empty candidate capture (gameplay never reached a crowd
// frame) is INCONCLUSIVE, not GREEN.
// ---------------------------------------------------------------------------
TEST(CrowdBoneOracle, InconclusiveWhenNoCrowd) {
    Capture baseline, candidate;   // both empty
    OracleResult r = RunCrowdBoneOracle(baseline, candidate);
    EXPECT_FALSE(r.passed);
    EXPECT_TRUE(r.inconclusive) << r.Describe();
}

// ---------------------------------------------------------------------------
// Multi-instance reduction: a mesh whose FIRST instance looks clean but a later
// instance flings past the clamp is counted as a shard-drop (worst-instance
// reduction), and a shared signal on ANY instance marks the mesh shared.
// ---------------------------------------------------------------------------
TEST(CrowdBoneOracle, ReducesWorstInstancePerMesh) {
    std::vector<ProbeRec> candidate = {
        Rec("char_crowd_body.mesh", 0, false, 0,  2.0, 2.0),    // inst 0 clean, owner==mesh here
        Rec("char_crowd_body.mesh", 1, false, 14, 48.0, 2.0),   // inst 1 owner-poisoned + shared
    };
    ShardStats raw = ComputeShardStats(candidate, Options());
    EXPECT_EQ(raw.meshes, 1);
    EXPECT_EQ(raw.ownerShardMeshes, 1);   // worst instance flags it
    EXPECT_EQ(raw.sharedMeshes, 1);       // shared signal OR'd across instances
    EXPECT_EQ(raw.ownFixMeshes, 1);       // own bones clean → SHARED-fixable
}

// ---------------------------------------------------------------------------
// LIVE gate. RB3_CROWD_BONE_BASELINE (rebind-ON capture) + RB3_CROWD_BONE_CANDIDATE
// (rebind-OFF capture) are engine logs with [CROWD_BONE_PROBE] lines from
// scripts/native/crowd-bone-gate-capture.py. Runs the oracle over the live
// artifacts and EXPECTs PASS — RED on the current build (candidate = rebind OFF,
// the free fail-red), GREEN under a W2.3-flag-ON build. SKIP when unset.
// ---------------------------------------------------------------------------
TEST(CrowdBoneOracle, RealCaptureNotElevated) {
    const char* basePath = getenv("RB3_CROWD_BONE_BASELINE");
    const char* candPath = getenv("RB3_CROWD_BONE_CANDIDATE");
    if (!basePath || !candPath) {
        GTEST_SKIP() << "set RB3_CROWD_BONE_BASELINE + RB3_CROWD_BONE_CANDIDATE "
                        "(see scripts/native/crowd-bone-gate-capture.py) to run the "
                        "live crowd bone-source gate";
    }
    Capture baseline  = ParseCaptureFile(basePath);
    Capture candidate = ParseCaptureFile(candPath);
    OracleResult r = RunCrowdBoneOracle(baseline, candidate);
    if (r.inconclusive) {
        GTEST_SKIP() << "capture inconclusive (no crowd frame):\n" << r.Describe();
    }
    fprintf(stderr, "[CrowdBoneOracle live] %s", r.Describe().c_str());
    EXPECT_TRUE(r.passed)
        << "CROWD BONE GATE RED — the crowd shard-drop is elevated with the rebind "
           "OFF (the current-build fail-red):\n" << r.Describe();
}
