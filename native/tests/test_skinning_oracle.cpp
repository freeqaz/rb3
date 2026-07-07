// test_skinning_oracle.cpp — R2 (Wave 17, Lane S) skinning oracle + oracle-VALIDATION harness.
//
// See skinning_oracle.h for the WHY. This TU has four suites:
//
//   Suite C — SkinOracleSynthetic.*   boot/asset/GPU-free, ALWAYS runs. Builds a
//     coherent palette and the three known-BAD signatures IN VITRO (a serial bone
//     chain with FK-composed palettes), and proves each metric's discrimination:
//     M_BlendSpread reads RED on the torn blend while Tier-1(own rest)/Tier-2
//     (shared joint) stay GREEN — the exact Wave-16 spike-fan signature. This is
//     the in-vitro replica the plan (§3.4 Suite C) names, and it makes the whole
//     harness exit assumption-free: it holds even with zero real captures.
//
//   Suite A — OracleValidation.*      the harness firing ValidateMetric on
//     known-GOOD / known-BAD populations, with the registry rule (RegistryComplete)
//     and the proven-blindness pins (Tier1/Tier2 blind to torn, wext not an oracle).
//     Prefers committed fixtures under goldens/r2-skinning/<arm>/; when a fixture set
//     is absent it validates on the SYNTHETIC populations (logged), so the
//     "no metric gates without a demonstrated separation" rule (OPTIONS.md §4.3) is
//     enforceable regardless of capture state. Real fixtures STRENGTHEN, never gate.
//
//   Suite B — VerdictTable.*          reproduces the Wave-15 arm-W/arm-S adjudication
//     numbers (male 0.1°/3.1° PASS, female 28.9° FAIL-pre-fix) from committed
//     arm-w/arm-s fixtures. SKIPs (not fails) when those goldens are not present —
//     they require a live capture (RB3_PALETTE_DUMP). ROADMAP's named Wave-17 exit.
//
//   Suite D — SkinningLiveGate.*      env-pointed (RB3_SKIN_ORACLE_GOOD_DIR /
//     _CAND_DIR), SKIPs when unset — the crowd-oracle pattern. The one-command
//     grader for any future hands / W2.4 BandPatchMesh / R5-reskin claim.
//
#include "test_helpers.h"
#include "skinning_oracle.h"

#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>

using namespace skinoracle;

namespace {

// ---------------------------------------------------------------------------
// Loader implementation (declared in skinning_oracle.h). Strict: rejects a frame
// whose bone/vert counts don't match the declared nb/nv (G1 fail-red).
// ---------------------------------------------------------------------------
bool LoadFrameImpl(const std::string& path, PaletteFrame& out) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    out = PaletteFrame{};
    int declaredNv = -1;
    char line[8192];
    bool ok = true;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        char key[64];
        if (sscanf(line, "%63s", key) != 1) continue;
        if (!std::strcmp(key, "mesh")) { char b[512]; if (sscanf(line, "mesh %511s", b)==1) out.mesh = b; }
        else if (!std::strcmp(key, "owner")) { char b[512]; if (sscanf(line, "owner %511s", b)==1) out.owner = b; }
        else if (!std::strcmp(key, "frame")) sscanf(line, "frame %d", &out.frame);
        else if (!std::strcmp(key, "nb")) sscanf(line, "nb %d", &out.nb);
        else if (!std::strcmp(key, "arm")) { char b[256]; if (sscanf(line, "arm %255s", b)==1) out.armTag = b; }
        else if (!std::strcmp(key, "rebound")) sscanf(line, "rebound %d", &out.rebound);
        else if (!std::strcmp(key, "nv")) sscanf(line, "nv %d", &declaredNv);
        else if (!std::strcmp(key, "bone")) {
            BoneRec br{};
            char nm[256]; int idx, parent;
            // bone <idx> <name> <parent> <off[12]> <world[12]> <palette[16]>
            int c = sscanf(line,
                "bone %d %255s %d "
                "%f %f %f %f %f %f %f %f %f %f %f %f "
                "%f %f %f %f %f %f %f %f %f %f %f %f "
                "%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
                &idx, nm, &parent,
                &br.off[0],&br.off[1],&br.off[2],&br.off[3],&br.off[4],&br.off[5],
                &br.off[6],&br.off[7],&br.off[8],&br.off[9],&br.off[10],&br.off[11],
                &br.world[0],&br.world[1],&br.world[2],&br.world[3],&br.world[4],&br.world[5],
                &br.world[6],&br.world[7],&br.world[8],&br.world[9],&br.world[10],&br.world[11],
                &br.palette[0],&br.palette[1],&br.palette[2],&br.palette[3],
                &br.palette[4],&br.palette[5],&br.palette[6],&br.palette[7],
                &br.palette[8],&br.palette[9],&br.palette[10],&br.palette[11],
                &br.palette[12],&br.palette[13],&br.palette[14],&br.palette[15]);
            if (c != 43) { ok = false; break; }
            br.name = nm; br.parent = parent;
            out.bones.push_back(br);
        }
        else if (!std::strcmp(key, "v")) {
            SkinVert sv{};
            int c = sscanf(line, "v %f %f %f %d %d %d %d %f %f %f %f",
                &sv.pos[0],&sv.pos[1],&sv.pos[2],
                &sv.idx[0],&sv.idx[1],&sv.idx[2],&sv.idx[3],
                &sv.w[0],&sv.w[1],&sv.w[2],&sv.w[3]);
            if (c != 11) { ok = false; break; }
            out.verts.push_back(sv);
        }
    }
    fclose(f);
    if (!ok) return false;
    // strict: declared counts must match parsed counts
    if (out.nb != (int)out.bones.size()) return false;
    if (declaredNv >= 0 && declaredNv != (int)out.verts.size()) return false;
    if (out.bones.empty()) return false;
    return true;
}

// ---------------------------------------------------------------------------
// Synthetic in-vitro fixtures — a serial bone chain with FK-composed palettes.
// Conventions match the engine dump (col-major mat4 palette, row-major 3x3+v off/world).
// ---------------------------------------------------------------------------
constexpr float kL = 10.0f;    // bone length / joint spacing along +X
constexpr float kR = 15.0f;    // blend-vert perpendicular radius
constexpr float kD2R = 0.01745329252f;

enum ArmKind { GOOD, CEILING, TORN, FROZEN };

// col-major mat4 helpers (P[col*4+row]).
void M4Identity(float* m) { for (int i=0;i<16;i++) m[i] = (i%5==0)?1.f:0.f; }
void M4Translate(float tx,float ty,float tz,float* m){ M4Identity(m); m[12]=tx; m[13]=ty; m[14]=tz; }
void M4RotZ(float a,float* m){ float c=cosf(a),s=sinf(a); M4Identity(m);
    m[0]=c; m[1]=s; m[4]=-s; m[5]=c; }
// C = A*B (col-major), C_col_j = A * B_col_j.
void M4Mul(const float* A,const float* B,float* C){
    for(int j=0;j<4;j++) for(int i=0;i<4;i++){ float s=0; for(int k=0;k<4;k++) s+=A[k*4+i]*B[j*4+k]; C[j*4+i]=s; } }

PaletteFrame BuildSynthetic(int nb, ArmKind kind, int seed) {
    PaletteFrame f;
    f.mesh = "hands_naked";
    f.owner = (nb==40) ? "player1" : "player0";
    f.armTag = (kind==GOOD?"good":kind==CEILING?"bad-ceiling":kind==TORN?"bad-torn":"frozen");
    f.frame = 100 + seed;
    f.nb = nb;
    f.rebound = 1;

    float jitter = (seed % 5) * 0.4f;             // tiny per-frame variation (deg)
    float globalRot = (kind==FROZEN) ? 0.0f : (10.0f + jitter) * kD2R;  // whole-hand articulation

    // per-joint own angles (radians)
    std::vector<float> ownAng(nb, (5.0f + jitter*0.2f) * kD2R);
    ownAng[0] = globalRot;                        // root carries the global pose
    if (kind == TORN) {
        for (int b = 1; b < nb; b++)
            if (b % 2 == 1) ownAng[b] = (40.0f + jitter) * kD2R;   // spike-fan: many bones over-rotated
    }

    // FK: G_b = G_parent * (translate(J_b - J_parent) * RotZ(ownAng[b])); P_b = G_b * translate(-J_b)
    std::vector<std::vector<float>> G(nb, std::vector<float>(16));
    for (int b = 0; b < nb; b++) {
        float Jb = b * kL, Jp = (b>0)?(b-1)*kL:0.f;
        float local[16], rz[16], tr[16];
        M4Translate(Jb - Jp, 0, 0, tr);
        M4RotZ(ownAng[b], rz);
        M4Mul(tr, rz, local);
        if (b == 0) { for (int i=0;i<16;i++) G[b][i]=local[i]; }
        else        { M4Mul(G[b-1].data(), local, G[b].data()); }
    }

    // per-bone off/world/palette
    for (int b = 0; b < nb; b++) {
        BoneRec br{};
        char nm[32]; snprintf(nm,sizeof(nm),"bone%02d", b);
        br.name = nm;
        br.parent = (b>0) ? b-1 : -1;
        float Jb = b * kL;
        // world = REST world (bind): identity rotation, translation J_b. off checked against this.
        float wr[9] = {1,0,0, 0,1,0, 0,0,1};
        std::memcpy(br.world, wr, sizeof(wr));
        br.world[9]=Jb; br.world[10]=0; br.world[11]=0;
        // off: coherent bake => off.m = I; CEILING rotates off basis (Tier-1 red, joint kept). v = -J_b.
        float om[9] = {1,0,0, 0,1,0, 0,0,1};
        if (kind == CEILING) {
            float a = 87.0f * kD2R, c=cosf(a), s=sinf(a);
            float rot[9] = { c,-s,0, s,c,0, 0,0,1 };
            std::memcpy(om, rot, sizeof(om));
        }
        std::memcpy(br.off, om, sizeof(om));
        br.off[9]=-Jb; br.off[10]=0; br.off[11]=0;
        // palette = posed FK: P_b = G_b * translate(-J_b)
        float tNeg[16]; M4Translate(-Jb, 0, 0, tNeg);
        M4Mul(G[b].data(), tNeg, br.palette);
        f.bones.push_back(br);
    }

    // verts: per non-root bone, blend verts near the joint (weight split with parent) + single-bone verts.
    for (int b = 1; b < nb; b++) {
        float Jb = b * kL;
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            SkinVert bv{};
            bv.pos[0]=Jb; bv.pos[1]=sgn*kR; bv.pos[2]=0;
            bv.idx[0]=b; bv.idx[1]=b-1; bv.idx[2]=0; bv.idx[3]=0;
            bv.w[0]=0.5f; bv.w[1]=0.5f; bv.w[2]=0; bv.w[3]=0;
            f.verts.push_back(bv);
        }
        SkinVert sv{};       // single-bone vert mid-bone
        sv.pos[0]=Jb - kL*0.5f; sv.pos[1]=0; sv.pos[2]=0;
        sv.idx[0]=b; sv.idx[1]=0; sv.idx[2]=0; sv.idx[3]=0;
        sv.w[0]=1.0f; sv.w[1]=0; sv.w[2]=0; sv.w[3]=0;
        f.verts.push_back(sv);
    }
    return f;
}

std::vector<PaletteFrame> SynthPop(int nb, ArmKind kind, int n=6) {
    std::vector<PaletteFrame> v;
    for (int i=0;i<n;i++) v.push_back(BuildSynthetic(nb, kind, i));
    return v;
}

// ---------------------------------------------------------------------------
// Fixture directory discovery (committed goldens; absent => synthetic fallback).
// ---------------------------------------------------------------------------
std::string GoldenRoot() {
    // ctest runs from the repo root (as the farvert oracle golden path assumes).
    const char* env = getenv("RB3_SKIN_ORACLE_GOLDENS");
    if (env && *env) return env;
    return std::string("native/tests/goldens/r2-skinning");
}
std::vector<PaletteFrame> LoadArmFixtures(const std::string& arm) {
    std::vector<PaletteFrame> out;
    std::string dir = GoldenRoot() + "/" + arm;
    DIR* d = opendir(dir.c_str());
    if (!d) return out;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (std::strncmp(e->d_name, "palette_", 8) != 0) continue;
        PaletteFrame f;
        if (LoadFrameImpl(dir + "/" + e->d_name, f)) out.push_back(f);
    }
    closedir(d);
    return out;
}

void PrintSep(const char* tag, const SeparationReport& r) {
    fprintf(stderr, "[oracle-validate] %-28s verdict=%-9s good[%.3f..%.3f] bad[%.3f..%.3f] margin=%.2fx (nG=%d nB=%d)\n",
            tag, r.VerdictName(), r.goodMin, r.goodMax, r.badMin, r.badMax, r.marginRatio, r.nGood, r.nBad);
}

// Validate a metric on a good/bad population pair, splitting by gender (nb) so
// no aggregate can close a cell (OPTIONS.md §4.2).
SeparationReport ValidateGenderSplit(MetricFn m, ArmKind good, ArmKind bad, int nb) {
    return ValidateMetricRaw(m, SynthPop(nb, good), SynthPop(nb, bad));
}

} // namespace

// ===========================================================================
// Suite C — SkinOracleSynthetic.* (always runs; in-vitro).
// ===========================================================================

TEST(SkinOracleSynthetic, CoherentPaletteAllMetricsLow) {
    for (int nb : {38, 40}) {
        PaletteFrame f = BuildSynthetic(nb, GOOD, 0);
        EXPECT_LT(M_Tier1RestCoherence(f), 1.0)  << "coherent Tier-1 nb=" << nb;
        EXPECT_LT(M_Tier2JointAttach(f),   0.5)  << "coherent Tier-2 nb=" << nb;
        auto bs = BlendSpreadDetail_(f);
        EXPECT_GT(bs.nBlend, 0) << "no blend verts constructed";
        EXPECT_LT(bs.p95, 3.0) << "coherent BlendSpread nb=" << nb;
    }
}

TEST(SkinOracleSynthetic, TornBlendIsSeenByBlendSpreadButNotByTiers) {
    // The Wave-16 signature, in vitro: torn palette => BlendSpread RED while
    // Tier-1 (single-bone own rest) and Tier-2 (shared joint) stay GREEN.
    for (int nb : {38, 40}) {
        PaletteFrame good = BuildSynthetic(nb, GOOD, 0);
        PaletteFrame torn = BuildSynthetic(nb, TORN, 0);
        // Tiers: torn is INDISTINGUISHABLE from good (this is the blindness).
        EXPECT_NEAR(M_Tier1RestCoherence(torn), M_Tier1RestCoherence(good), 1.0)
            << "Tier-1 must be blind to the torn blend (nb=" << nb << ")";
        EXPECT_NEAR(M_Tier2JointAttach(torn), M_Tier2JointAttach(good), 1.0)
            << "Tier-2 must be blind to the torn blend (nb=" << nb << ")";
        // BlendSpread: torn is much larger than good.
        double gS = M_BlendSpread(good), tS = M_BlendSpread(torn);
        EXPECT_GT(tS, gS * 3.0) << "BlendSpread must SEE the torn blend (nb=" << nb
                                << ") good=" << gS << " torn=" << tS;
    }
}

TEST(SkinOracleSynthetic, CeilingHandIsSeenByTier1) {
    for (int nb : {38, 40}) {
        PaletteFrame ceil = BuildSynthetic(nb, CEILING, 0);
        EXPECT_GT(M_Tier1RestCoherence(ceil), 60.0) << "Tier-1 must catch the ceiling (rotated-off) hand";
        // ceiling keeps joints attached => Tier-2 stays green
        EXPECT_LT(M_Tier2JointAttach(ceil), 1.0) << "ceiling Tier-2 should still attach";
    }
}

// Permanent self-contained guard: a fixed perturbation IS detected, so no eps can
// be loosened into blindness (mirrors the hands-bind-oracle PerturbationIsDetected).
TEST(SkinOracleSynthetic, PerturbationIsDetected) {
    PaletteFrame good = BuildSynthetic(38, GOOD, 0);
    PaletteFrame torn = BuildSynthetic(38, TORN, 0);
    ASSERT_GT(M_BlendSpread(torn), 5.0)
        << "the in-vitro torn palette must exceed the tear threshold, else the metric is dead";
    // Env fail-red: SKIN_ORACLE_PERTURB rotates one good bone's palette and must trip BlendSpread.
    const char* pe = getenv("SKIN_ORACLE_PERTURB");
    if (pe && atof(pe) != 0.0) {
        float a = (float)atof(pe);
        // rotate bone 5's palette about Z in place
        float rz[16]; M4RotZ(a, rz);
        float np[16]; M4Mul(rz, good.bones[5].palette, np);
        for (int i=0;i<16;i++) good.bones[5].palette[i]=np[i];
        EXPECT_GT(M_BlendSpread(good), 3.0) << "SKIN_ORACLE_PERTURB must turn a good frame RED";
    }
}

// G3(b): the discrimination lives in the BLEND, not an artifact — force every vert
// single-bone (weight 1,0,0,0) and BlendSpread must go to ~0 even on the torn palette.
TEST(SkinOracleSynthetic, BlendSpreadDiscriminationIsInTheBlend) {
    PaletteFrame torn = BuildSynthetic(38, TORN, 0);
    for (SkinVert& v : torn.verts) { v.w[0]=1.f; v.w[1]=v.w[2]=v.w[3]=0.f; }
    EXPECT_LT(M_BlendSpread(torn), 0.001)
        << "with single-bone weights the torn palette has NO blend spread (proves the metric reads the blend)";
}

// ===========================================================================
// Suite A — OracleValidation.* (harness + registry + blindness pins).
// Uses committed fixtures when present; synthetic populations otherwise.
// ===========================================================================

// Returns good/bad populations for a metric-validation cell, real if committed.
static bool UseRealFixtures() {
    return !LoadArmFixtures("good-body").empty() && !LoadArmFixtures("bad-torn").empty();
}

TEST(OracleValidation, BlendSpreadSeparatesTornBlend) {
    // THE permanent Wave-16 red test. Had this existed, HANDS-FIX's numeric gates
    // could not all have read GREEN.
    SeparationReport r;
    if (UseRealFixtures()) {
        auto good = LoadArmFixtures("good-body"); auto bad = LoadArmFixtures("bad-torn");
        r = ValidateMetricRaw(&M_BlendSpread, good, bad);
    } else {
        r = ValidateGenderSplit(&M_BlendSpread, GOOD, TORN, 38);
    }
    PrintSep("BlendSpread good-vs-torn", r);
    EXPECT_EQ(r.verdict, SeparationReport::VALID)
        << "M_BlendSpread must VALID-separate the torn blend from known-good";
}

TEST(OracleValidation, Tier1SeparatesCeilingHand) {
    SeparationReport r = UseRealFixtures() && !LoadArmFixtures("bad-ceiling").empty()
        ? ValidateMetricRaw(&M_Tier1RestCoherence, LoadArmFixtures("good-body"), LoadArmFixtures("bad-ceiling"))
        : ValidateGenderSplit(&M_Tier1RestCoherence, GOOD, CEILING, 38);
    PrintSep("Tier1 good-vs-ceiling", r);
    EXPECT_EQ(r.verdict, SeparationReport::VALID)
        << "M_Tier1 must VALID-separate the rotated-off ceiling hand";
}

TEST(OracleValidation, Tier2SeparatesCeilingHand) {
    // Tier-2 is NOT expected to catch the ceiling in the general case, but the
    // synthetic ceiling keeps joints attached => this documents Tier-2 as BLIND to
    // the ceiling (a recorded blindness, not a gate).
    SeparationReport r = ValidateGenderSplit(&M_Tier2JointAttach, GOOD, CEILING, 38);
    PrintSep("Tier2 good-vs-ceiling", r);
    EXPECT_NE(r.verdict, SeparationReport::INVERTED);
}

TEST(OracleValidation, Tier1IsBlindToTornBlend) {
    // Pins the Wave-16 lesson: Tier-1 CANNOT tell torn from good.
    SeparationReport r = ValidateGenderSplit(&M_Tier1RestCoherence, GOOD, TORN, 38);
    PrintSep("Tier1 good-vs-torn (EXPECT BLIND)", r);
    EXPECT_EQ(r.verdict, SeparationReport::BLIND)
        << "if Tier-1 ever separates the torn blend, the pinned Wave-16 fact changed — investigate";
}

TEST(OracleValidation, Tier2IsBlindToTornBlend) {
    SeparationReport r = ValidateGenderSplit(&M_Tier2JointAttach, GOOD, TORN, 38);
    PrintSep("Tier2 good-vs-torn (EXPECT BLIND)", r);
    EXPECT_EQ(r.verdict, SeparationReport::BLIND)
        << "Tier-2 is origin-anchored/joint-attaching => blind to far-vert tear";
}

TEST(OracleValidation, WextIsNotAnOracle) {
    // legit posed extent overlaps shard extent => wext must NOT be VALID (VERDICT §6.4).
    SeparationReport r = ValidateGenderSplit(&M_WorldExtent, GOOD, TORN, 38);
    PrintSep("Wext good-vs-torn (EXPECT non-VALID)", r);
    EXPECT_NE(r.verdict, SeparationReport::VALID)
        << "world-extent must be proven NOT an oracle";
}

TEST(OracleValidation, GenderSplitEnforced) {
    // A mixed-nb population must ASSERT-fail the hygiene check (fail-red of the rule).
    std::vector<PaletteFrame> mixed;
    mixed.push_back(BuildSynthetic(38, GOOD, 0));
    mixed.push_back(BuildSynthetic(40, GOOD, 1));   // different nb (gender)
    std::string err;
    EXPECT_FALSE(PopulationHomogeneous(mixed, err)) << "mixed-nb population must be rejected";
    EXPECT_NE(err.find("mixed nb"), std::string::npos) << "err=" << err;
}

TEST(OracleValidation, RegistryComplete) {
    // Every gate-eligible metric must resolve by name and name a validation test.
    for (const GateEntry& e : GateRegistry()) {
        EXPECT_NE(MetricByName(e.metric), nullptr) << "metric not resolvable: " << e.metric;
        EXPECT_TRUE(e.validationTest && *e.validationTest) << "no validation test for: " << e.metric;
        if (e.gateEligible)
            EXPECT_EQ(std::strncmp(e.validationTest, "OracleValidation.", 17), 0)
                << "gate metric " << e.metric << " must point at an OracleValidation.* test";
    }
    // At least one gate-eligible metric exists.
    int nGate = 0; for (const GateEntry& e : GateRegistry()) if (e.gateEligible) nGate++;
    EXPECT_GE(nGate, 1);
}

// Diagnostic (non-gating) metric smoke: the palette-composed and bone-WORLD
// inter-bone variants both run and are distinct quantities (M-3).
TEST(OracleValidation, InterBonePaletteDiagnostic) {
    PaletteFrame torn = BuildSynthetic(38, TORN, 0);
    EXPECT_GT(M_InterBoneRelPose_Palette(torn), 0.0);
}
TEST(OracleValidation, InterBoneWorldComparand) {
    // bone-WORLD variant (INDEX.md M-3 comparand basis). On the synthetic REST
    // worlds (identity rotation) inter-bone D is ~0; the metric is exercised for
    // provenance and is the native side of R1's angle(D_wii·inv(D_native)) diff.
    PaletteFrame good = BuildSynthetic(38, GOOD, 0);
    EXPECT_GE(M_InterBoneRelPoseWorld(good), 0.0);
}

// ===========================================================================
// Suite B — VerdictTable.* (Wave-15 arm-w/arm-s numbers; needs committed fixtures).
// ===========================================================================

TEST(VerdictTable, ArmWArmSReproducedFromFixtures) {
    auto armw = LoadArmFixtures("arm-w");
    auto arms = LoadArmFixtures("arm-s");
    if (armw.empty() || arms.empty())
        GTEST_SKIP() << "arm-w/arm-s fixtures not committed (need a live RB3_PALETTE_DUMP capture); "
                        "run scripts/native/skinning-fixture-capture.py then re-run. "
                        "Synthetic verdict structure is covered by SkinOracleSynthetic.*";
    // Split by gender; reproduce the adjudication modes within tolerance.
    auto modeFor = [](const std::vector<PaletteFrame>& pop, bool female)->double {
        double worst = 0; int n = 0;
        for (const auto& f : pop) if (f.IsFemale()==female) { worst += M_Tier1RestCoherence(f); n++; }
        return n ? worst/n : -1;
    };
    double wMale = modeFor(armw, false), wFem = modeFor(armw, true);
    double sMale = modeFor(arms, false), sFem = modeFor(arms, true);
    fprintf(stderr, "[verdict-table] arm-w male=%.1f fem=%.1f | arm-s male=%.1f fem=%.1f\n",
            wMale, wFem, sMale, sFem);
    if (wMale >= 0) EXPECT_NEAR(wMale, 0.1, 1.5) << "arm-w male mode";
    if (sMale >= 0) EXPECT_NEAR(sMale, 3.1, 1.5) << "arm-s male mode";
    if (wFem >= 0)  EXPECT_NEAR(wFem, 28.9, 3.0) << "arm-w female FAIL-pre-fix";
    if (sFem >= 0)  EXPECT_NEAR(sFem, 28.9, 3.0) << "arm-s female FAIL-pre-fix";
}

// ===========================================================================
// Suite D — SkinningLiveGate.* (env-pointed one-command grader).
// ===========================================================================

TEST(SkinningLiveGate, CandidateWithinGoodEnvelope) {
    const char* gd = getenv("RB3_SKIN_ORACLE_GOOD_DIR");
    const char* cd = getenv("RB3_SKIN_ORACLE_CAND_DIR");
    if (!gd || !cd) GTEST_SKIP() << "set RB3_SKIN_ORACLE_GOOD_DIR and RB3_SKIN_ORACLE_CAND_DIR to fresh capture dirs";
    std::vector<PaletteFrame> good, cand;
    { DIR* d=opendir(gd); if(d){ struct dirent* e; while((e=readdir(d))){ if(std::strncmp(e->d_name,"palette_",8)) continue; PaletteFrame f; if(LoadFrameImpl(std::string(gd)+"/"+e->d_name,f)) good.push_back(f);} closedir(d);} }
    { DIR* d=opendir(cd); if(d){ struct dirent* e; while((e=readdir(d))){ if(std::strncmp(e->d_name,"palette_",8)) continue; PaletteFrame f; if(LoadFrameImpl(std::string(cd)+"/"+e->d_name,f)) cand.push_back(f);} closedir(d);} }
    ASSERT_FALSE(good.empty()) << "no palette_*.txt in GOOD dir";
    ASSERT_FALSE(cand.empty()) << "no palette_*.txt in CAND dir";
    // Every gate-eligible registry metric: candidate must not exceed 3x the good p95.
    for (const GateEntry& e : GateRegistry()) {
        if (!e.gateEligible) continue;
        MetricFn m = MetricByName(e.metric);
        double goodMax = 0, candMax = 0;
        for (const auto& f : good) goodMax = std::max(goodMax, m(f));
        for (const auto& f : cand) candMax = std::max(candMax, m(f));
        fprintf(stderr, "[live-gate] %s good<=%.3f cand<=%.3f\n", e.metric, goodMax, candMax);
        EXPECT_LT(candMax, goodMax * 3.0 + 1.0) << "candidate exceeds good envelope on " << e.metric;
    }
}

// Loader strictness (G1): a truncated fixture must be rejected.
TEST(SkinOracleLoader, RejectsTruncatedFixture) {
    // write a valid tiny frame, then a truncated copy, and confirm strict rejection.
    std::string dir = std::string(getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
    std::string good = dir + "/skin_ok.txt";
    std::string bad  = dir + "/skin_trunc.txt";
    FILE* f = fopen(good.c_str(), "w");
    ASSERT_TRUE(f);
    fprintf(f, "# R2 PaletteFrame v1\nmesh test\nowner o\nframe 1\nnb 1\narm t\nrebound 0\n");
    fprintf(f, "bone 0 root -1 1 0 0 0 1 0 0 0 1 0 0 0  1 0 0 0 1 0 0 0 1 0 0 0  1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1\n");
    fprintf(f, "nv 1\nv 0 0 0 0 0 0 0 1 0 0 0\n");
    fclose(f);
    PaletteFrame pf;
    EXPECT_TRUE(LoadFrameImpl(good, pf)) << "valid frame must load";
    // truncated: declare nb 2 but give 1 bone
    f = fopen(bad.c_str(), "w");
    ASSERT_TRUE(f);
    fprintf(f, "mesh test\nnb 2\narm t\n");
    fprintf(f, "bone 0 root -1 1 0 0 0 1 0 0 0 1 0 0 0  1 0 0 0 1 0 0 0 1 0 0 0  1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1\n");
    fclose(f);
    EXPECT_FALSE(LoadFrameImpl(bad, pf)) << "nb/bone-count mismatch must be rejected";
}
