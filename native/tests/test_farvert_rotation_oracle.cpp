// ============================================================================
// test_farvert_rotation_oracle.cpp — W2.8.BL-A2 FAR-VERTEX ROTATION-BASIS ORACLE
//
// WHY THIS EXISTS (execution/W2.8/STATUS.md — the finger-shard diagnosis).
// The band hand/finger meshes are loaded, bound, and drawn every frame, yet the
// fingers explode into thin radiating sheets ("missing hands"). The mechanism is
// the documented C8 rotation-basis divergence: the per-member ANIMATED skeleton
// bone's rotation BASIS differs from the STATIC magnet the outfit's authored
// inverse-bind offsets were baked against, so a vertex at bone-local radius R
// with a rotation-basis error theta flings by ~R*sin(theta) (2*R*sin(theta/2)
// exactly — the chord). Long-thin extremity geometry (fingertips) shards while
// the compact torso survives.
//
// THE GATE HOLE THIS CLOSES (STATUS.md "Gate-blindness finding").
// W2.2's shipped oracle (test_hands_bind_oracle.cpp) and the engine's
// REBIND_DRAW_SKINPOS probe both measure the bone-ORIGIN translation delta. At
// R=0 the R*sin(theta) smear is identically zero, so those metrics PASS while
// the fingers explode. The whole-mesh AABB ratio is likewise ~1.3x (most verts
// are compact; only a few fingertips fling) and reads clean. Any future "hands
// fixed" claim needs a FAR-VERTEX rotation metric, not an origin metric. This
// file is that metric.
//
// WHAT THE ORACLE MEASURES.
//   metric = max over hand/finger FAR verts of
//              | RefSkinVertex(v, asDrawnInvBind) - RefSkinVertex(v, coherentInvBind) |
//   under an ANIMATED pose (the bone WorldXfm is a real posed frame, not bind —
//   at bind pose R*sin(0)=0 and the metric is structurally GREEN, the Wave-3
//   DC3-suite trap the WAVE7_REVIEW A6 warns about). `asDrawnInvBind` corrupts
//   the coherent invBind rotation basis by the rotation-basis angle today's
//   rebind path leaves in place (the magnet-vs-per-member divergence). The
//   reference skinner is DUPLICATED from engine tests/test_skin_golden.cpp:164
//   (chosen over an engine test-header export — no engine edit this stage) and
//   adapted to the RB3 `RndMesh::SkinVertex` (Mesh.cpp:1367-1410) semantics.
//
// RB3 vs DC3 FAITHFULNESS DEVIATION (recorded).  The engine RefSkinVertex reads
// DC3's plain-float `Vector4 boneWeights` via `(&v.boneWeights.x)[i]`. RB3's
// `RndMesh::Vert::boneWeights` is a `Vector4_16_01` (the u16 "hate format",
// Mesh.h:80): the zero-check is on the raw u16 and the weight is read via
// `.FloatAt(i)`. This oracle is parameterized over an explicit bone palette (so
// it runs without constructing an RndMesh) and its weights are already floats,
// but it preserves the THREE faithful RB3 invariants:
//   (a) `boneIndices[i] < NumBones()` SKIP (out-of-range ignored, not clamped)
//   (b) NO weight normalization (tf60 is the raw weighted sum of skin matrices)
//   (c) skinMat = invBind * boneWorld   (invBind on the LEFT, row-vector Milo)
//
// EXIT CRITERIA (WAVE7_KICKOFF W2.8.BL-A2 / task B.S1).
//   * RED on today's build: `FarVertShardIsDetected` and the env fail-red
//     (`RB3_FARVERT_PERTURB=<rad>`) show the far-vertex metric flings past
//     tolerance under the documented ~94deg (1.645 rad) basis mismatch; magnitude
//     recorded in STATUS.md.
//   * Synthetic-perturbation control: `MetricScalesWithRSinTheta` sweeps the
//     basis break angle and asserts the metric == 2*R*sin(theta/2) (the chord;
//     ~R*sin(theta) for small theta), monotone on (0, pi], zero at theta=0.
//   * Blindness contrast: `OriginAndWholeMeshMetricsAreBlind` shows that under
//     the same break the bone-ORIGIN skinpos delta and the whole-mesh AABB ratio
//     stay clean while the far-vertex metric is large — the exact reason BL-A2
//     had to exist.
//
// This is a pure math/compose unit test (always buildable, GPU/Dolphin-
// independent): it reuses the production engine free functions (Multiply,
// ScaleAddEq, Invert) linked in via _RB3_NATIVE_SRCS. A best-effort real-band-
// path arm (`RealPathFixture`) runs the SAME metric on a captured live gameplay
// pose dumped through the real rb3 band load path when that fixture is present.
// ============================================================================

#include "test_helpers.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "math/Vec.h"
#include "math/Mtx.h"
#include "math/Rot.h"

namespace {

// --- tolerances -------------------------------------------------------------
// Coherent (theta=0) far-vertex residual: a handful of single-precision
// products over positions of order tens of units => true error ~1e-4; 1e-2
// leaves headroom without going blind to a multi-unit fling.
constexpr float kCoherentEps = 1.0e-2f;
// Bone-origin skinpos residual tolerance (the W2.2/REBIND_DRAW_SKINPOS analog).
// A near-origin (R~0) vertex must stay clean under ANY basis break.
constexpr float kOriginEps = 1.0e-1f;
// A far vertex must fling past this to count as a detected shard. The band hand
// meshes fling tens of units; 20u is comfortably above compose noise and below
// the real magnitude.
constexpr float kShardThreshold = 20.0f;

// The documented today's-build rotation-basis divergence. bone_R-upperArm
// worldRot magnet (0.73,-0.07,-0.68) vs per-member (-0.73,0.09,-0.68):
//   cos(theta) = dot = -0.077  =>  theta ~= 94.4deg = 1.645 rad
// (CHAR_SKINNING_DEFORM_INVESTIGATION.md:104-156). This is the basis error the
// rebind (SetBone calcOffset=false) leaves in the invBind of a rebound bone.
constexpr float kDocumentedShardTheta = 1.645f;
// Fixed positive-control break (matches engine test_skin_golden.cpp breakBasis,
// ~90deg — representative of the documented magnitude).
constexpr float kControlTheta = 1.5707963f; // pi/2

// --- host-libm rotation builders (trig-table-independent; the engine Sine LUT
// is never inited in this standalone gtest — see test_hands_bind_oracle.cpp) --
Hmx::Matrix3 RotX(float a) {
    float c = std::cos(a), s = std::sin(a);
    return Hmx::Matrix3(1, 0, 0, 0, c, s, 0, -s, c);
}
Hmx::Matrix3 RotY(float a) {
    float c = std::cos(a), s = std::sin(a);
    return Hmx::Matrix3(c, 0, -s, 0, 1, 0, s, 0, c);
}
Hmx::Matrix3 RotZ(float a) {
    float c = std::cos(a), s = std::sin(a);
    return Hmx::Matrix3(c, -s, 0, s, c, 0, 0, 0, 1);
}
Transform MakeXfm(float rx, float ry, float rz, const Vector3 &t) {
    Hmx::Matrix3 tmp, rot;
    Multiply(RotX(rx), RotY(ry), tmp);
    Multiply(tmp, RotZ(rz), rot);
    Transform out;
    out.Set(rot, t);
    return out;
}

Vector3 ApplyPoint(const Transform &t, const Vector3 &p) {
    Vector3 out;
    Multiply(p, t, out); // row-vector local->world (Mtx.h:416)
    return out;
}
float Dist(const Vector3 &a, const Vector3 &b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) +
                     (a.z - b.z) * (a.z - b.z));
}

// A skinnable vertex: authored mesh-LOCAL position + 4-bone blend.
struct SkinVert {
    Vector3 pos;
    float weights[4];
    int indices[4];
};

// ============================================================================
// RefSkinVertex — DUPLICATED from engine tests/test_skin_golden.cpp:164, adapted
// to RB3 RndMesh::SkinVertex (Mesh.cpp:1367-1410) and parameterized over an
// explicit (boneWorld, invBind) palette so it runs without an RndMesh.
//   `breakTheta` models the bone rotation-BASIS error the rebind leaves in place
//   (magnet-vs-per-member): the bone's frame is off by a rotation about its own
//   local Z. Faithfully this is a rotation inserted in BONE-LOCAL space between
//   invBind and boneWorld — skinMat = invBind * RotZ(theta) * boneWorld — so a
//   vertex at bone-local radius R flings by exactly 2*R*sin(theta/2) about the
//   BONE origin (not the mesh origin). A vertex AT the bone origin does not move,
//   which is precisely why the bone-origin/REBIND_DRAW_SKINPOS metric is blind.
//   (The engine test's left-multiply breakBasis rotates about the mesh origin —
//   fine for a golden fail-red, but it does NOT scale with bone radius, so it is
//   the wrong break for a far-vertex rotation oracle. This is the corrected form.)
// Faithful invariants preserved: (a) index<nbones SKIP, (b) NO normalization,
// (c) skinMat = invBind * boneWorld.
// ============================================================================
Vector3 RefSkinVertex(const SkinVert &v,
                      const std::vector<Transform> &boneWorld,
                      const std::vector<Transform> &invBind,
                      float breakTheta) {
    const int nb = (int)boneWorld.size();
    Transform tf60;
    tf60.Zero();
    bool any = false;
    for (int i = 0; i < 4; i++) {
        int bi = v.indices[i];
        if (bi < nb) {                 // (a) SKIP out-of-range (not clamp)
            float w = v.weights[i];
            if (w != 0.0f) {           // RB3 zero-check (raw u16 nonzero <=> w!=0)
                Transform skinMat;
                if (breakTheta != 0.0f) {
                    // Bone-local basis error: invBind * RotZ(theta) * boneWorld.
                    Transform rot;
                    rot.Set(RotZ(breakTheta), Vector3(0, 0, 0));
                    Transform tmp;
                    Multiply(invBind[bi], rot, tmp);       // -> bone-local, rotate
                    Multiply(tmp, boneWorld[bi], skinMat); // -> world
                } else {
                    Multiply(invBind[bi], boneWorld[bi], skinMat); // (c)
                }
                ScaleAddEq(tf60, skinMat, w);         // (b) raw weighted sum
                any = true;
            }
        }
    }
    Vector3 ret(0, 0, 0);
    if (any) Multiply(v.pos, tf60, ret); // row-vector pos * tf60
    return ret;
}

// The coherent invBind bake: offset = meshWorld * inverse(boneBindWorld), so
// that at the bind pose skinMat = offset * boneBindWorld reproduces meshWorld
// (BandCharacter.cpp:1425-1428 / hands_bind_oracle BakeOffset). This is what a
// CORRECT rebake produces; the shard is this same bake read back against a bone
// posed with a divergent rotation basis.
Transform BakeInvBind(const Transform &meshWorld, const Transform &boneBindWorld) {
    Transform invBind, offset;
    Invert(boneBindWorld, invBind);
    Multiply(meshWorld, invBind, offset);
    return offset;
}

// ---------------------------------------------------------------------------
// A representative posed hand: one finger bone, a mesh authored at the hand
// (~120u out along an arm, rotated), a range of mesh-local vert radii from the
// bone origin (near knuckle -> long-thin fingertip), and an ANIMATED bone pose.
// Single-bone-dominant verts (weight 1 on the finger bone) so the coherent skin
// is exactly rigid and any residual is purely the basis break => the R*sin(theta)
// signal is clean.
// ---------------------------------------------------------------------------
struct HandScene {
    Transform meshWorld;      // authored mesh world at bind
    Transform boneBindWorld;  // finger bone world at bind (== rest)
    Transform boneAnimWorld;  // finger bone world at the ANIMATED pose
    std::vector<SkinVert> verts;
    std::vector<float> localRadius; // perpendicular-to-break-axis bone-local R
};

// Author verts by their BONE-LOCAL offset from the bone origin, then map to the
// authored mesh-local `pos`. Because the break rotates about bone-local Z, the
// fling scales with the offset's radius PERPENDICULAR to Z — we store that as
// `localRadius` so it is exactly the R in 2*R*sin(theta/2). Verts far from the
// bone (fingertips) get large R; a vert at the bone origin gets R~0 (invisible
// to the origin metric).
HandScene MakeHandScene() {
    HandScene s;
    // Mesh + finger bone: distinct rotated frames ~120u out along the arm.
    s.meshWorld = MakeXfm(0.30f, -0.55f, 0.20f, Vector3(42.0f, 118.0f, -30.0f));
    s.boneBindWorld = MakeXfm(-0.15f, 0.40f, 0.10f, Vector3(48.0f, 121.0f, -33.0f));
    // ANIMATED pose: the per-member finger bone swung through a large arc
    // (~40deg about X + ~25deg about Y) away from bind. Skinning is measured
    // HERE, not at bind (bind pose is structurally green — the A6 trap).
    Hmx::Matrix3 anim;
    Multiply(RotX(0.70f), RotY(0.45f), anim);
    Hmx::Matrix3 animRot;
    Multiply(s.boneBindWorld.m, anim, animRot);
    s.boneAnimWorld.Set(animRot, s.boneBindWorld.v);

    // invBind (coherent bake) and its inverse (bone-local -> mesh-local).
    Transform invBind = BakeInvBind(s.meshWorld, s.boneBindWorld);
    Transform boneLocalToMesh;
    Invert(invBind, boneLocalToMesh);

    // Bone-local offsets. Pure-XY entries (z=0) have r_perp == |offset| exactly;
    // the R=40u fingertip is the R*sin(theta) control vertex. A couple carry a
    // Z component (adds mesh depth for the AABB blindness contrast) — their
    // r_perp is the XY magnitude, which is what flings.
    const Vector3 boneLocal[] = {
        Vector3(0.02f, 0.0f, 0.0f),  // bone origin (R~0) — origin-metric blind spot
        Vector3(1.5f, 0.5f, 0.0f),   // knuckle
        Vector3(0.0f, 3.0f, 0.0f),   // knuckle
        Vector3(4.0f, 0.0f, 5.0f),   // palm, has depth  (r_perp=4)
        Vector3(8.0f, 6.0f, -3.0f),  // mid-finger        (r_perp=10)
        Vector3(40.0f, 0.0f, 0.0f),  // splayed fingertip (r_perp=40, pure XY)
    };
    const int n = (int)(sizeof(boneLocal) / sizeof(boneLocal[0]));
    for (int i = 0; i < n; i++) {
        SkinVert v;
        v.pos = ApplyPoint(boneLocalToMesh, boneLocal[i]);
        v.weights[0] = 1.0f; v.weights[1] = v.weights[2] = v.weights[3] = 0.0f;
        v.indices[0] = 0; v.indices[1] = v.indices[2] = v.indices[3] = 1; // idx1 skipped
        s.verts.push_back(v);
        float rPerp = std::sqrt(boneLocal[i].x * boneLocal[i].x +
                                boneLocal[i].y * boneLocal[i].y);
        s.localRadius.push_back(rPerp);
    }
    return s;
}

// Skin one scene at the ANIMATED pose with a given basis break; return world
// positions per vertex.
std::vector<Vector3> SkinSceneAnimated(const HandScene &s, float breakTheta) {
    std::vector<Transform> boneWorld(1, s.boneAnimWorld);
    std::vector<Transform> invBind(1, BakeInvBind(s.meshWorld, s.boneBindWorld));
    std::vector<Vector3> out;
    out.reserve(s.verts.size());
    for (const SkinVert &v : s.verts)
        out.push_back(RefSkinVertex(v, boneWorld, invBind, breakTheta));
    return out;
}

// Far-vertex metric: max |asDrawn - coherent| over verts with radius >= rMin.
float FarVertMetric(const HandScene &s, float breakTheta, float rMin,
                    int *worstIdx = nullptr) {
    std::vector<Vector3> ref = SkinSceneAnimated(s, 0.0f);
    std::vector<Vector3> drawn = SkinSceneAnimated(s, breakTheta);
    float m = 0.0f;
    int wi = -1;
    for (size_t i = 0; i < s.verts.size(); i++) {
        if (s.localRadius[i] < rMin) continue;
        float d = Dist(drawn[i], ref[i]);
        if (d > m) { m = d; wi = (int)i; }
    }
    if (worstIdx) *worstIdx = wi;
    return m;
}

// Bone-origin skinpos delta (the W2.2 blind metric): |asDrawn - coherent| for
// the near-origin (R~0) vertex.
float OriginMetric(const HandScene &s, float breakTheta) {
    std::vector<Vector3> ref = SkinSceneAnimated(s, 0.0f);
    std::vector<Vector3> drawn = SkinSceneAnimated(s, breakTheta);
    return Dist(drawn[0], ref[0]); // vert 0 = smallest radius
}

// Whole-mesh AABB extent ratio drawn/coherent (the other blind metric).
float WholeMeshAabbRatio(const HandScene &s, float breakTheta) {
    std::vector<Vector3> ref = SkinSceneAnimated(s, 0.0f);
    std::vector<Vector3> drawn = SkinSceneAnimated(s, breakTheta);
    auto ext = [](const std::vector<Vector3> &p) {
        Vector3 lo = p[0], hi = p[0];
        for (const Vector3 &v : p) {
            lo.x = std::min(lo.x, v.x); lo.y = std::min(lo.y, v.y); lo.z = std::min(lo.z, v.z);
            hi.x = std::max(hi.x, v.x); hi.y = std::max(hi.y, v.y); hi.z = std::max(hi.z, v.z);
        }
        return std::sqrt((hi.x-lo.x)*(hi.x-lo.x) + (hi.y-lo.y)*(hi.y-lo.y) + (hi.z-lo.z)*(hi.z-lo.z));
    };
    float e0 = ext(ref), e1 = ext(drawn);
    return e0 > 1e-4f ? e1 / e0 : 1.0f;
}

float PerturbTheta() {
    const char *e = getenv("RB3_FARVERT_PERTURB");
    return e ? (float)atof(e) : 0.0f;
}

} // namespace

// ============================================================================
// FAIL-RED / coherence — honors RB3_FARVERT_PERTURB. With no env the far-vertex
// metric is GREEN (coherent skin under the animated pose reproduces the
// reference). Setting RB3_FARVERT_PERTURB=<rad> corrupts the invBind basis and
// the metric flings RED — the human/CI-runnable fail-red for today's shard
// (e.g. RB3_FARVERT_PERTURB=1.645 = the documented ~94deg divergence).
// ============================================================================
TEST(FarVertRotationOracle, CoherentUnderAnimation_FailsRedOnPerturb) {
    HandScene s = MakeHandScene();
    float theta = PerturbTheta();
    int worst = -1;
    float m = FarVertMetric(s, theta, /*rMin*/4.0f, &worst);
    EXPECT_LT(m, kCoherentEps)
        << "far-vertex metric under animated pose must be ~0 for a coherent "
           "invBind (perturb=" << theta << " rad); worst far vert idx=" << worst
        << " fling=" << m << "u  [set RB3_FARVERT_PERTURB=1.645 to see the "
           "today's-build shard fail-red]";
}

// ============================================================================
// Positive control (always GREEN) — a fixed ~90deg basis break flings the
// long-thin fingertip past kShardThreshold, so kShardThreshold/kCoherentEps can
// never be loosened into the origin-blindness that shipped both BandPatchMesh
// reverts. This is the RED-capability lock.
// ============================================================================
TEST(FarVertRotationOracle, FarVertShardIsDetected) {
    HandScene s = MakeHandScene();
    int worst = -1;
    float m = FarVertMetric(s, kControlTheta, /*rMin*/4.0f, &worst);
    EXPECT_GT(m, kShardThreshold)
        << "a " << kControlTheta << " rad (~90deg) invBind basis error must "
           "fling a far finger vertex past " << kShardThreshold
        << "u (got " << m << "u, worst idx=" << worst << ")";

    // And at the DOCUMENTED today's-build angle the fling is even larger — the
    // magnitude recorded in STATUS.md.
    float mDoc = FarVertMetric(s, kDocumentedShardTheta, 4.0f, nullptr);
    EXPECT_GT(mDoc, kShardThreshold)
        << "documented ~94deg divergence fling=" << mDoc << "u";
    RecordProperty("shard_fling_90deg_u", (int)(m + 0.5f));
    RecordProperty("shard_fling_documented_u", (int)(mDoc + 0.5f));
}

// ============================================================================
// BLINDNESS CONTRAST (always GREEN) — under the SAME basis break, the bone-
// ORIGIN skinpos delta stays clean (< kOriginEps) and the whole-mesh AABB ratio
// stays modest, while the far-vertex metric is large. This is the exact reason
// BL-A2 had to exist: W2.2's origin oracle and the whole-mesh ratio are both
// PROVABLY blind to the R*sin(theta) finger shard.
// ============================================================================
TEST(FarVertRotationOracle, OriginAndWholeMeshMetricsAreBlind) {
    HandScene s = MakeHandScene();
    float origin = OriginMetric(s, kControlTheta);
    float ratio = WholeMeshAabbRatio(s, kControlTheta);
    float far = FarVertMetric(s, kControlTheta, 4.0f, nullptr);

    EXPECT_LT(origin, kOriginEps)
        << "bone-origin (R~0) skinpos delta must stay clean under the shard "
           "(the W2.2/REBIND_DRAW_SKINPOS blind spot); got " << origin << "u";
    EXPECT_LT(ratio, 2.0f)
        << "whole-mesh AABB ratio must read ~clean under the shard (the other "
           "blind metric — diagnosis measured ~1.3x); got " << ratio;
    EXPECT_GT(far, kShardThreshold)
        << "...while the FAR-vertex metric is loud: " << far << "u";
    // The whole point, as one assertion: far >> origin.
    EXPECT_GT(far, origin * 50.0f)
        << "far-vertex fling (" << far << "u) must dwarf the origin delta ("
        << origin << "u) — origin metrics cannot see this shard";
}

// ============================================================================
// SYNTHETIC-PERTURBATION CONTROL (always GREEN) — rotate the bone/invBind basis
// by a known angle and confirm the far-vertex metric scales as the exact chord
// 2*R*sin(theta/2) (~R*sin(theta) for small theta), is monotone increasing on
// (0, pi], and is exactly 0 at theta=0. This is the task's required control that
// the metric is a genuine rotation-sensitive far-vertex statistic.
// ============================================================================
TEST(FarVertRotationOracle, MetricScalesWithRSinTheta) {
    HandScene s = MakeHandScene();
    // The dominant far vert is the R=40u fingertip (last authored). Its
    // predicted fling for a pure-basis break is the bone-local chord scaled by
    // the (orthonormal) bone/mesh rotations, i.e. 2*R*sin(theta/2).
    const float R = s.localRadius.back();

    // theta = 0 => exactly coherent.
    EXPECT_LT(FarVertMetric(s, 0.0f, 4.0f, nullptr), kCoherentEps)
        << "theta=0 must be exactly coherent (R*sin(0)=0)";

    const float thetas[] = {0.1f, 0.3f, 0.6f, 1.0f, kControlTheta, 2.5f, 3.14159f};
    float prev = 0.0f;
    for (float th : thetas) {
        float m = FarVertMetric(s, th, /*rMin*/ R - 0.5f, nullptr); // isolate R=40 vert
        float predicted = 2.0f * R * std::sin(th * 0.5f);
        // 8% relative tolerance absorbs the compose FP noise + the small
        // non-fingertip verts that also clear rMin.
        EXPECT_NEAR(m, predicted, 0.08f * predicted + 0.05f)
            << "theta=" << th << " far metric=" << m
            << "u vs 2*R*sin(theta/2)=" << predicted << "u (R=" << R << ")";
        if (th <= 3.14159f) {
            EXPECT_GE(m, prev - 1e-3f) << "metric must be monotone up to pi (theta=" << th << ")";
            prev = m;
        }
    }
    // Small-angle sanity: 2*R*sin(theta/2) ~= R*theta ~= R*sin(theta).
    float mSmall = FarVertMetric(s, 0.05f, R - 0.5f, nullptr);
    EXPECT_NEAR(mSmall, R * 0.05f, 0.05f * R * 0.05f + 0.02f)
        << "small-angle metric ~= R*theta (R*sin(theta))";
}

// ============================================================================
// Best-effort REAL-BAND-PATH arm — run the SAME far-vertex metric on a captured
// live gameplay pose dumped through the real rb3 band load path. This is the
// A6-sanctioned "captured live pose through the real band path" hook the BL-A1
// fix (S2) drives with a real capture. Fixture format (whitespace-separated
// floats, '#'-comments ok), one block per hand/finger far vertex:
//   asDrawnX asDrawnY asDrawnZ  refX refY refZ  radiusR
// where asDrawn = the world position the engine skinned this frame (magnet-basis
// invBind * animated bone) and ref = the coherent reference world position
// (correct rebake, or a golden captured from a known-good build). radiusR is the
// vert's bone-local radius, used only for reporting. The metric is max|asDrawn -
// ref| over the block: on a real today's-build capture the hand verts diverge
// (RED); a correct BL-A1 fix drives it GREEN. Absent fixture => SKIP.
// ============================================================================
TEST(FarVertRotationOracle, RealPathFixture) {
    const char *env = getenv("RB3_FARVERT_FIXTURE");
    std::string path = env ? env
                           : std::string("native/tests/goldens/w2.8-farvert/"
                                         "live_pose.txt");
    FILE *f = fopen(path.c_str(), "r");
    if (!f) {
        GTEST_SKIP()
            << "no live-pose fixture at '" << path
            << "' (BL-A1 S2 captures it through the real band path) — the "
               "math/compose tiers cover the metric + R*sin(theta) control";
        return;
    }
    std::string clean;
    {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            char *hash = strchr(line, '#');
            if (hash) { *hash = '\n'; hash[1] = '\0'; }
            clean += line;
        }
        fclose(f);
    }
    FILE *m = fmemopen((void *)clean.data(), clean.size(), "r");
    ASSERT_TRUE(m != nullptr);
    float worst = 0.0f;
    int n = 0;
    float ax, ay, az, rx, ry, rz, R;
    while (fscanf(m, " %f %f %f %f %f %f %f", &ax, &ay, &az, &rx, &ry, &rz, &R) == 7) {
        float d = Dist(Vector3(ax, ay, az), Vector3(rx, ry, rz));
        if (d > worst) worst = d;
        n++;
    }
    fclose(m);
    ASSERT_GT(n, 0) << "fixture present but held no far-vertex records";
    // On a today's-build capture this EXPECT is the fail-red; a correct BL-A1
    // fix turns it GREEN. Recorded either way.
    RecordProperty("real_far_metric_u", (int)(worst + 0.5f));
    EXPECT_LT(worst, kShardThreshold)
        << "real captured hand far-vertex fling=" << worst << "u over " << n
        << " verts (>=" << kShardThreshold << "u == the shard is present)";
}
