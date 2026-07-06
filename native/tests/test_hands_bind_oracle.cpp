// W2.2.S1b — Numeric bind-pose identity oracle for the per-member hands/fingers
// rebind (`BandCharacter::RebindHeadHandsAtRest`, src/system/bandobj/
// BandCharacter.cpp:1425-1428).
//
// WHY THIS EXISTS (the SYS-7 hole, execution/ARCHITECTURE_REVIEW.md:123-130,150).
// Both BandPatchMesh rewrites shipped and were reverted because they were
// "Wii-correct by eyeball" with NO numeric invariant: a wrong-basis bind looks
// plausible in a wide frame and only shards on close inspection. This test is
// the falsifiable invariant that code never had.
//
// THE INVARIANT (WAVE3_REVIEW.md:73-79). By definition of invBind, the rebake
//   offset' = meshBindWorld · inverse(perMemberBoneBindWorld)      (:1425-1428)
// must, when composed back with the bone at its captured BIND pose, reproduce
// the mesh's world transform exactly:
//   skinMatrix(bind) = offset' ∘ restWorld  ==  meshWorld
// and therefore, in mesh space, offset' ∘ restWorld ∘ inverse(meshWorld) == I.
// Equivalently: a mesh-local vertex skinned by that bone at the bind pose lands
// at exactly its authored (decoded) world position. If the captured bind basis
// is wrong, far-from-bone verts smear by R·sin(theta) — the exact failure
// signature the render-polish 2026-06-11 rest-rebake experiment produced
// (200-460u glove/nail extents; Rnd_Wgpu_RB3.cpp:3714-3725). We exercise that
// with a long-thin finger vertex at radius R so a wrong basis is amplified.
//
// COMPOSITION CONVENTION (verified against the engine, READ-ONLY):
//   * Row-vector point transform: `Multiply(Vector3 v, Transform t, out)` =>
//     out = v·t.m + t.v  (math/Mtx.h:416, local->world).
//   * `Multiply(Transform a, Transform b, res)` composes "apply a THEN b":
//     apply(res,p) = apply(b, apply(a,p))  (math/Rot.cpp:732-740, row-vector).
//   * `Invert(Transform,Transform)` (math/Mtx.h:697).
// These are the SAME free functions the shipped rebake calls, linked in via
// _RB3_NATIVE_SRCS — the oracle re-uses the production math, it does not
// re-derive it.
//
// FAIL-RED. `HANDS_BIND_ORACLE_PERTURB=<radians>` rotates the captured bind
// pose used for the BAKE while skinning still happens at the true rest pose,
// modelling a wrong-basis capture. The identity residual and skinned-vertex
// error then exceed eps and the invariant tests turn RED. `PerturbationIsDetected`
// is a self-contained permanent guard that a fixed perturbation IS caught (so
// eps can never be loosened into blindness).
//
// This is a pure math/compose unit test (always buildable, Dolphin-independent).
// The best-effort real-path arm (`RealPathFixture`) asserts the same invariant
// on real transforms dumped in-game by S1a's probe when that fixture is present.

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
// Matrix/translation residual on the identity invariant. Compose is a handful
// of single-precision products over translations <= a few hundred u, so true
// error is ~1e-4; 1e-3 leaves headroom without going blind.
constexpr float kMatEps = 1.0e-3f;
// Skinned-vertex vs authored-vertex world-space error, in game units.
constexpr float kVertEps = 5.0e-2f;

// Perturbation used by the self-contained sensitivity guard. Chosen so the
// finger-tip smear (R*sin) is orders of magnitude above kVertEps but small
// enough to be a realistic "slightly wrong bind basis".
constexpr float kGuardTheta = 0.02f;

float PerturbTheta() {
    const char *e = getenv("HANDS_BIND_ORACLE_PERTURB");
    return e ? (float)atof(e) : 0.0f;
}

// Build a Transform from an XYZ euler (radians) + translation.
//
// NOTE: rotation matrices are built with the HOST libm (std::sin/std::cos), NOT
// the engine's `Matrix3::RotateAboutX/Y/Z` — those call the engine `Sine`, which
// reads a lookup table (`gBigSinTable`) filled by `TrigTableInit()`, and that
// init never runs in this standalone gtest process (Sine would return 0 →
// degenerate/singular test inputs). The composition + inversion under test
// (`Multiply(Transform,...)`, `Invert(Transform,...)`) are the production engine
// functions and are trig-init-independent; only the INPUT rotations are host-built.
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

// Max abs element-wise difference between two Transforms (9 matrix + 3 trans).
float MaxDiff(const Transform &a, const Transform &b) {
    float m = 0.0f;
    for (int i = 0; i < 3; i++) {
        m = std::max(m, std::fabs(a.m[i].x - b.m[i].x));
        m = std::max(m, std::fabs(a.m[i].y - b.m[i].y));
        m = std::max(m, std::fabs(a.m[i].z - b.m[i].z));
    }
    m = std::max(m, std::fabs(a.v.x - b.v.x));
    m = std::max(m, std::fabs(a.v.y - b.v.y));
    m = std::max(m, std::fabs(a.v.z - b.v.z));
    return m;
}

Transform Identity() {
    Transform t;
    t.Reset();
    return t;
}

// Rotate a bind pose about Y by theta (models a wrong-basis capture). "apply
// perturb-rotation THEN the true rest" == rotating the rest orientation.
Transform PerturbRest(const Transform &rest, float theta) {
    if (theta == 0.0f) return rest;
    Transform pert = MakeXfm(0.0f, theta, 0.0f, Vector3(0, 0, 0));
    Transform out;
    Multiply(pert, rest, out);
    return out;
}

// The production rebake, using the exact engine free functions:
//   offset' = meshWorld * inverse(bakeRest)     (BandCharacter.cpp:1425-1428)
Transform BakeOffset(const Transform &meshWorld, const Transform &bakeRest) {
    Transform invRest, offset;
    Invert(bakeRest, invRest);
    Multiply(meshWorld, invRest, offset);
    return offset;
}

// Skinning-matrix build, matching the engine palette compose
//   skinMatrix = offset' ∘ boneWorld    (Rnd_Wgpu_RB3.cpp: Multiply(BoneOffsetAt,WorldXfm))
Transform SkinMatrix(const Transform &offset, const Transform &boneWorld) {
    Transform sm;
    Multiply(offset, boneWorld, sm);
    return sm;
}

Vector3 ApplyPoint(const Transform &t, const Vector3 &p) {
    Vector3 out;
    Multiply(p, t, out); // row-vector local->world (Mtx.h:416)
    return out;
}

float VertErr(const Vector3 &a, const Vector3 &b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) +
                     (a.z - b.z) * (a.z - b.z));
}

// A representative rebound hand mesh + finger bone. restWorld is offset far from
// origin with a real rotation (a hand at the end of an arm), and we probe a
// vertex a long thin distance out (a fingertip) so any basis error is amplified
// by that radius — the R*sin(theta) smear the invariant must catch.
struct HandCase {
    Transform meshWorld;
    Transform restWorld; // = perMemberBoneBindWorld at capture (true bind)
    std::vector<Vector3> verts; // authored mesh-LOCAL vertices
};

HandCase MakeHandCase() {
    HandCase c;
    // Mesh authored at the hand, rotated, ~120u out along an arm.
    c.meshWorld = MakeXfm(0.30f, -0.55f, 0.20f, Vector3(42.0f, 118.0f, -30.0f));
    // The per-member finger bone's world at bind: near the mesh but distinct
    // basis/translation (this is what the real capture yields).
    c.restWorld = MakeXfm(-0.15f, 0.40f, 0.10f, Vector3(48.0f, 121.0f, -33.0f));
    // Mesh-local verts incl. a long-thin fingertip at radius ~200u.
    c.verts.push_back(Vector3(0.0f, 0.0f, 0.0f));      // bone root
    c.verts.push_back(Vector3(2.0f, 1.0f, -1.5f));     // knuckle
    c.verts.push_back(Vector3(200.0f, 3.0f, 0.0f));    // fingertip (long thin)
    c.verts.push_back(Vector3(-8.0f, 140.0f, 6.0f));   // splayed thumb
    return c;
}

} // namespace

// ============================================================================
// Invariant 1 — offset' composed back with the bind pose reproduces meshWorld,
// i.e. offset' ∘ restWorld ∘ inverse(meshWorld) == identity in mesh space.
// Honors HANDS_BIND_ORACLE_PERTURB (fail-red).
// ============================================================================
TEST(HandsBindOracle, ComposeIdentityAtBindPose) {
    HandCase c = MakeHandCase();
    float theta = PerturbTheta();
    Transform bakeRest = PerturbRest(c.restWorld, theta);

    Transform offset = BakeOffset(c.meshWorld, bakeRest);
    // Skin at the TRUE bind pose (capture may be perturbed; the bone is not).
    Transform skin = SkinMatrix(offset, c.restWorld);

    // (a) skinMatrix(bind) == meshWorld
    EXPECT_LT(MaxDiff(skin, c.meshWorld), kMatEps)
        << "offset'∘restWorld must reproduce meshWorld at the bind pose "
           "(perturb=" << theta << ")";

    // (b) in mesh space it is identity: skin ∘ inverse(meshWorld) == I
    Transform invMesh, meshSpace;
    Invert(c.meshWorld, invMesh);
    Multiply(skin, invMesh, meshSpace);
    EXPECT_LT(MaxDiff(meshSpace, Identity()), kMatEps)
        << "offset'∘restWorld∘inverse(meshWorld) must be identity "
           "(perturb=" << theta << ")";
}

// ============================================================================
// Invariant 2 — a mesh-local vertex skinned by the bone at the bind pose lands
// at exactly its authored world position (incl. the long-thin fingertip).
// Honors HANDS_BIND_ORACLE_PERTURB (fail-red).
// ============================================================================
TEST(HandsBindOracle, SkinnedVertsMatchAuthored) {
    HandCase c = MakeHandCase();
    float theta = PerturbTheta();
    Transform bakeRest = PerturbRest(c.restWorld, theta);

    Transform offset = BakeOffset(c.meshWorld, bakeRest);
    Transform skin = SkinMatrix(offset, c.restWorld);

    for (size_t i = 0; i < c.verts.size(); i++) {
        Vector3 authored = ApplyPoint(c.meshWorld, c.verts[i]); // decoded world
        Vector3 skinned = ApplyPoint(skin, c.verts[i]);         // bind-pose skin
        EXPECT_LT(VertErr(skinned, authored), kVertEps)
            << "vert[" << i << "]=(" << c.verts[i].x << "," << c.verts[i].y
            << "," << c.verts[i].z << ") skinned to authored mismatch "
               "(perturb=" << theta << ")";
    }
}

// ============================================================================
// Sensitivity guard (self-contained, always GREEN) — proves a fixed wrong-basis
// bind IS caught, so kMatEps/kVertEps can never be loosened into the blindness
// that shipped both BandPatchMesh reverts. This is the permanent regression
// guard the fail-red demo proves once by hand.
// ============================================================================
TEST(HandsBindOracle, PerturbationIsDetected) {
    HandCase c = MakeHandCase();

    Transform bakeRest = PerturbRest(c.restWorld, kGuardTheta);
    Transform offset = BakeOffset(c.meshWorld, bakeRest);
    Transform skin = SkinMatrix(offset, c.restWorld);

    // Identity residual must exceed eps under a wrong basis.
    EXPECT_GT(MaxDiff(skin, c.meshWorld), kMatEps)
        << "a " << kGuardTheta << "rad bind-basis error must be detectable";

    // The long-thin fingertip must smear well past kVertEps (R*sin dominates).
    Vector3 authored = ApplyPoint(c.meshWorld, c.verts[2]); // R~200 fingertip
    Vector3 skinned = ApplyPoint(skin, c.verts[2]);
    float err = VertErr(skinned, authored);
    EXPECT_GT(err, kVertEps)
        << "fingertip smear under wrong basis must exceed vert eps";
    // Sanity: the smear scales with radius — the R*sin(theta) failure mode.
    EXPECT_GT(err, 1.0f) << "R~200u * sin(" << kGuardTheta
                         << ") should be several units of smear";
}

// ============================================================================
// Best-effort real-path arm — assert the same invariant on REAL transforms
// dumped in-game by W2.2.S1a's probe. Fixture format (whitespace-separated
// floats, '#'-comments ok), one record per rebound bone:
//   <12 meshWorld: m.x.x m.x.y m.x.z  m.y.x m.y.y m.y.z  m.z.x m.z.y m.z.z  v.x v.y v.z>
//   <12 restWorld (perMemberBoneBindWorld) same layout>
//   <int nverts> then nverts*(x y z) mesh-local authored verts
// Absent fixture => SKIP (documents the path S1a should dump to). Present =>
// the numeric invariant is checked on real captured data.
// ============================================================================
namespace {
bool ReadFloats(FILE *f, float *out, int n) {
    for (int i = 0; i < n; i++)
        if (fscanf(f, " %f", &out[i]) != 1) return false;
    return true;
}
Transform ReadXfm(FILE *f, bool &ok) {
    float b[12];
    ok = ReadFloats(f, b, 12);
    Transform t;
    t.m.x.Set(b[0], b[1], b[2]);
    t.m.y.Set(b[3], b[4], b[5]);
    t.m.z.Set(b[6], b[7], b[8]);
    t.v.Set(b[9], b[10], b[11]);
    return t;
}
} // namespace

TEST(HandsBindOracle, RealPathFixture) {
    const char *env = getenv("HANDS_BIND_ORACLE_FIXTURE");
    std::string path = env
        ? env
        : std::string("native/tests/goldens/w2.2-hands/bind_fixture.txt");
    FILE *f = fopen(path.c_str(), "r");
    if (!f) {
        GTEST_SKIP() << "no real-path fixture at '" << path
                     << "' (S1a in-game probe not dumped yet) — math/compose "
                        "arm covers the invariant";
        return;
    }
    // Skip comment lines by reading char-by-char is fiddly; fscanf(" %f") skips
    // whitespace but not '#'. Strip comments into a scratch buffer first.
    std::string clean;
    {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            char *hash = strchr(line, '#');
            if (hash) *hash = '\n', hash[1] = '\0';
            clean += line;
        }
        fclose(f);
    }
    FILE *m = fmemopen((void *)clean.data(), clean.size(), "r");
    ASSERT_TRUE(m != nullptr);

    int records = 0;
    bool ok = true;
    while (true) {
        Transform meshWorld = ReadXfm(m, ok);
        if (!ok) break; // clean EOF between records
        Transform restWorld = ReadXfm(m, ok);
        ASSERT_TRUE(ok) << "fixture record " << records
                        << " missing restWorld block";
        int nv = 0;
        ASSERT_EQ(fscanf(m, " %d", &nv), 1)
            << "fixture record " << records << " missing vert count";

        Transform offset = BakeOffset(meshWorld, restWorld);
        Transform skin = SkinMatrix(offset, restWorld);
        EXPECT_LT(MaxDiff(skin, meshWorld), kMatEps)
            << "real bind record " << records << ": compose≠meshWorld";

        for (int i = 0; i < nv; i++) {
            float v[3];
            ASSERT_TRUE(ReadFloats(m, v, 3))
                << "record " << records << " vert " << i;
            Vector3 vv(v[0], v[1], v[2]);
            Vector3 authored = ApplyPoint(meshWorld, vv);
            Vector3 skinned = ApplyPoint(skin, vv);
            EXPECT_LT(VertErr(skinned, authored), kVertEps)
                << "real bind record " << records << " vert " << i;
        }
        records++;
    }
    fclose(m);
    EXPECT_GT(records, 0) << "fixture present but held no records";
}
