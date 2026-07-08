// skinning_oracle.h — R2 (Wave 17, Lane S) skinning oracle metrics + the
// oracle-VALIDATION harness.
//
// WHY THIS EXISTS. Through Waves 9-16 every NUMERIC skinning gate (Tier-1 rest
// coherence, Tier-2 joint attach, wext ratio) read GREEN while the screen showed
// torn fingers (the Wave-16 RB3_HANDS_AUTHORED_REPOINT "spike fan"). The lesson
// (OPTIONS.md §4.3, REPORT.md:276): *no metric may gate a skinning wave without a
// demonstrated known-good / known-bad separation.* This header is that rule as
// code — every gate-eligible metric must pass `ValidateMetric` on real fixtures
// AND appear in `GateRegistry()` with a green OracleValidation test, or the live
// gate (Suite D) refuses to grade with it.
//
// SCOPE (stated as a limit, per plan §1 non-goals): this suite proves a palette is
// internally *incoherent* (a shard) but NOT that a coherent palette is
// *Wii-faithful*. Faithfulness is R1's job (Dolphin ground truth). The
// M_InterBoneRelPoseWorld metric below is the native side of R1's cross-instrument
// comparand (INDEX.md M-3: D_side = inv(W_parent)*W_child).
//
// SELF-CONTAINED. All math is plain float (col-major mat4 for the uploaded palette,
// row-major 3x3 + translation for the bone offset/world transforms — the exact
// conventions the engine dump probe RB3_PALETTE_DUMP writes, Rnd_Wgpu_RB3.cpp
// ~:4989). No engine/Hmx dependency, no TrigTableInit, no boot — a PaletteFrame is
// parsed from a committed text file (or built in-memory for the synthetic suite).
//
// COMPOSITION CONVENTIONS (verified against the probe source, READ-ONLY):
//   * Uploaded palette P = bones.bones[b], col-major mat4: transform of a local
//     point v is  out[i] = P[0*4+i]*v.x + P[1*4+i]*v.y + P[2*4+i]*v.z + P[3*4+i]
//     (Rnd_Wgpu_RB3.cpp haPal :4798-4801 == the shader blend).
//   * Bone offset `off` / world `W`: row-vector Transform, dumped as 9 matrix
//     floats (rows m.x,m.y,m.z) then 3 translation floats. Compose "apply off then
//     world": R = off.m * W.m (standard row-major 3x3 product) — mirrors the engine
//     Multiply(off, restW, sk) at :4818.
//
#ifndef RB3_TESTS_SKINNING_ORACLE_H
#define RB3_TESTS_SKINNING_ORACLE_H

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

namespace skinoracle {

// ---------------------------------------------------------------------------
// Data model — one dumped (or synthetic) draw's palette + skinned verts.
// ---------------------------------------------------------------------------
struct BoneRec {
    std::string name;
    int parent = -1;      // palette-resolved parent index (-1 = chain root)
    float off[12];        // row-major 3x3 (9) + translation (3)
    float world[12];      // bone WorldXfm at this frame, same layout
    float palette[16];    // uploaded palette matrix, col-major mat4
};
struct SkinVert {
    float pos[3];
    int idx[4];
    float w[4];
};
struct PaletteFrame {
    std::string mesh, owner, armTag;
    int frame = -1, nb = 0, rebound = 0;
    // Engine-emitted Tier-1 rest-coherence field (R2 Wave-18 Lane N; NEW header
    // key `tier1`). Value = max_b angle(off_b * FIRST-SEEN cached rest world, I),
    // the RB3_HANDS_ATTACH_PROBE :4820-4841 xcheck quantity — pose-STABLE because
    // it composes the constant bind offset against a rest world cached at first-
    // seen pointer identity, NOT off_b * the dump's current `world` field (which
    // the offline M_Tier1RestCoherence reads ~180deg at a gameplay frame). Absent
    // (-1) on legacy goldens captured before the field existed.
    double tier1Worst = -1.0;
    int tier1Count5 = -1, tier1Recap = -1, tier1WorstBone = -1, tier1Cold = -1;
    std::vector<BoneRec> bones;
    std::vector<SkinVert> verts;
    bool IsFemale() const { return nb == 40; }   // nb: 38=male, 40=female (analysis-side gender key)
    bool HasEngineTier1() const { return tier1Worst >= 0.0; }
};

// ---------------------------------------------------------------------------
// Small float linear algebra (col-major mat4 palette + row-major 3x3 transform).
// ---------------------------------------------------------------------------
inline void PaletteXform(const float* P, const float* v, float* out) {
    out[0] = P[0]*v[0] + P[4]*v[1] + P[8]*v[2] + P[12];
    out[1] = P[1]*v[0] + P[5]*v[1] + P[9]*v[2] + P[13];
    out[2] = P[2]*v[0] + P[6]*v[1] + P[10]*v[2] + P[14];
}
// 3x3 rotation part of a col-major palette: R[r][c] = P[c*4 + r].
inline void PaletteRot(const float* P, float R[9]) {
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            R[r*3 + c] = P[c*4 + r];
}
// standard row-major 3x3 multiply: C = A*B.
inline void Mat3Mul(const float* A, const float* B, float* C) {
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            float s = 0.f;
            for (int k = 0; k < 3; k++) s += A[i*3+k]*B[k*3+j];
            C[i*3+j] = s;
        }
}
inline void Mat3Transpose(const float* A, float* T) {
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            T[i*3+j] = A[j*3+i];
}
// rotation angle (deg) of a ~orthonormal 3x3 relative to identity.
inline double Mat3AngleDeg(const float* R) {
    double tr = R[0] + R[4] + R[8];
    double c = (tr - 1.0) * 0.5;
    if (c > 1.0) c = 1.0; if (c < -1.0) c = -1.0;
    return std::acos(c) * 57.2957795131;
}
// angle(A * inv(B)) in deg, both ~orthonormal (inv == transpose).
inline double RelAngleDeg(const float* A, const float* B) {
    float Bt[9]; Mat3Transpose(B, Bt);
    float R[9]; Mat3Mul(A, Bt, R);
    return Mat3AngleDeg(R);
}

// ---------------------------------------------------------------------------
// Loader — strict; rejects short reads so a truncated fixture fails G1.
// ---------------------------------------------------------------------------
bool LoadPaletteFrame(const std::string& path, PaletteFrame& out);

// ---------------------------------------------------------------------------
// Metrics.  Each returns a scalar in game units or degrees; higher == worse
// (more incoherent) for every gate metric.  Detail structs available for
// gtest messages.
// ---------------------------------------------------------------------------

// M_Tier1RestCoherence — max_b angle(off_b * world_b, I), degrees. Offline analog
// of the engine Tier-1 xcheck (:4818). Pose-dependent by design.
struct Tier1Detail { double worstDeg; int worstBone; int count5; };
inline Tier1Detail Tier1Detail_(const PaletteFrame& f) {
    Tier1Detail d{0.0, -1, 0};
    for (int b = 0; b < (int)f.bones.size(); b++) {
        const BoneRec& br = f.bones[b];
        float sk[9]; Mat3Mul(br.off, br.world, sk);   // off.m * world.m (row-vector compose)
        double a = Mat3AngleDeg(sk);
        if (a > 5.0) d.count5++;
        if (a > d.worstDeg) { d.worstDeg = a; d.worstBone = b; }
    }
    return d;
}
inline double M_Tier1RestCoherence(const PaletteFrame& f) { return Tier1Detail_(f).worstDeg; }

// M_Tier2JointAttach — offline replica of the engine Tier-2 exact-joint metric
// (:4826-4848). max over (b,parent p) of || j_b*P[p] - j_b*P[b] ||, j_b = -off_b.v.
struct Tier2Detail { double worstU; int worstBone, worstParent; int pairs; };
inline Tier2Detail Tier2Detail_(const PaletteFrame& f) {
    Tier2Detail d{0.0, -1, -1, 0};
    for (int b = 0; b < (int)f.bones.size(); b++) {
        int p = f.bones[b].parent;
        if (p < 0 || p >= (int)f.bones.size()) continue;
        d.pairs++;
        float j[3] = { -f.bones[b].off[9], -f.bones[b].off[10], -f.bones[b].off[11] };
        float pc[3], cc[3];
        PaletteXform(f.bones[p].palette, j, pc);
        PaletteXform(f.bones[b].palette, j, cc);
        double att = std::sqrt((double)(pc[0]-cc[0])*(pc[0]-cc[0])
                             + (double)(pc[1]-cc[1])*(pc[1]-cc[1])
                             + (double)(pc[2]-cc[2])*(pc[2]-cc[2]));
        if (att > d.worstU) { d.worstU = att; d.worstBone = b; d.worstParent = p; }
    }
    return d;
}
inline double M_Tier2JointAttach(const PaletteFrame& f) { return Tier2Detail_(f).worstU; }

// M_BlendSpread (NEW — the tear metric, the plan's centerpiece). For every vertex
// with >=2 active weights (w_i,w_j >= 0.05): spread(v) = max_{i,j} || v*P[idx_i] -
// v*P[idx_j] ||. Mesh scalar = p95 over such verts (max reported as detail). A
// coherent palette bounds this by joint-local articulation; a torn blend drives
// adjacent composed matrices apart at wrist radius => large spread. INVISIBLE to
// Tier-1 (single-bone) and Tier-2 (shared-joint, origin-anchored).
struct BlendSpreadDetail { double p95; double maxU; int nBlend; };
inline BlendSpreadDetail BlendSpreadDetail_(const PaletteFrame& f, float wmin = 0.05f) {
    std::vector<double> spreads;
    double mx = 0.0;
    int nb = (int)f.bones.size();
    for (const SkinVert& v : f.verts) {
        // collect active bone slots
        int act[4]; int na = 0;
        for (int k = 0; k < 4; k++)
            if (v.w[k] >= wmin && v.idx[k] >= 0 && v.idx[k] < nb) act[na++] = v.idx[k];
        if (na < 2) continue;
        float pos[3] = { v.pos[0], v.pos[1], v.pos[2] };
        // transformed positions under each active bone's palette
        float tp[4][3];
        for (int a = 0; a < na; a++) PaletteXform(f.bones[act[a]].palette, pos, tp[a]);
        double s = 0.0;
        for (int a = 0; a < na; a++)
            for (int c = a+1; c < na; c++) {
                double d = std::sqrt((double)(tp[a][0]-tp[c][0])*(tp[a][0]-tp[c][0])
                                   + (double)(tp[a][1]-tp[c][1])*(tp[a][1]-tp[c][1])
                                   + (double)(tp[a][2]-tp[c][2])*(tp[a][2]-tp[c][2]));
                if (d > s) s = d;
            }
        spreads.push_back(s);
        if (s > mx) mx = s;
    }
    BlendSpreadDetail out{0.0, mx, (int)spreads.size()};
    if (!spreads.empty()) {
        std::sort(spreads.begin(), spreads.end());
        size_t i95 = (size_t)(0.95 * (spreads.size() - 1));
        out.p95 = spreads[i95];
    }
    return out;
}
inline double M_BlendSpread(const PaletteFrame& f) { return BlendSpreadDetail_(f).p95; }

// M_InterBoneRelPose (PALETTE-composed; diagnostic, non-gating). max over adjacent
// (b,parent p) of angle(rot(P_b) * inv(rot(P_p))), degrees. INTERNAL metric only —
// per INDEX.md M-3 this is NOT the R1 cross-instrument comparand (that is the
// bone-WORLD variant below); labeled palette-composed to keep them distinct.
inline double M_InterBoneRelPose_Palette(const PaletteFrame& f) {
    double mx = 0.0;
    for (int b = 0; b < (int)f.bones.size(); b++) {
        int p = f.bones[b].parent;
        if (p < 0 || p >= (int)f.bones.size()) continue;
        float Rb[9], Rp[9];
        PaletteRot(f.bones[b].palette, Rb);
        PaletteRot(f.bones[p].palette, Rp);
        double a = RelAngleDeg(Rb, Rp);
        if (a > mx) mx = a;
    }
    return mx;
}

// M_InterBoneRelPoseWorld (bone-WORLD variant — INDEX.md M-3 comparand basis).
// D_side = inv(W_parent) * W_child (rotation part); returns max rotation angle of
// D over adjacent pairs. This is the NATIVE-side quantity R1's Dolphin probe will
// diff against the Wii side via delta = angle(D_wii * inv(D_native)). Reported here
// as a scalar characterization of the native inter-bone relative pose.
inline double M_InterBoneRelPoseWorld(const PaletteFrame& f) {
    double mx = 0.0;
    for (int b = 0; b < (int)f.bones.size(); b++) {
        int p = f.bones[b].parent;
        if (p < 0 || p >= (int)f.bones.size()) continue;
        // rotation parts of the row-major world transforms
        const float* Wc = f.bones[b].world;
        const float* Wp = f.bones[p].world;
        float WpInv[9]; Mat3Transpose(Wp, WpInv);   // ~orthonormal => inverse == transpose
        float D[9]; Mat3Mul(WpInv, Wc, D);
        double a = Mat3AngleDeg(D);
        if (a > mx) mx = a;
    }
    return mx;
}

// M_WorldExtent — wext recomputed from skinned verts (blend each vert, AABB
// diagonal). Included SPECIFICALLY to be validated as NOT-an-oracle (VERDICT §6.4):
// legit posed extents overlap shard extents, so this must read BLIND.
inline double M_WorldExtent(const PaletteFrame& f) {
    float mn[3] = { 1e30f, 1e30f, 1e30f }, mx[3] = { -1e30f, -1e30f, -1e30f };
    int nb = (int)f.bones.size();
    for (const SkinVert& v : f.verts) {
        float pos[3] = { v.pos[0], v.pos[1], v.pos[2] };
        float s[3] = { 0, 0, 0 };
        for (int k = 0; k < 4; k++) {
            int bi = v.idx[k]; if (bi < 0 || bi >= nb) continue;
            float t[3]; PaletteXform(f.bones[bi].palette, pos, t);
            s[0] += v.w[k]*t[0]; s[1] += v.w[k]*t[1]; s[2] += v.w[k]*t[2];
        }
        for (int c = 0; c < 3; c++) { if (s[c] < mn[c]) mn[c] = s[c]; if (s[c] > mx[c]) mx[c] = s[c]; }
    }
    return std::sqrt((double)(mx[0]-mn[0])*(mx[0]-mn[0])
                   + (double)(mx[1]-mn[1])*(mx[1]-mn[1])
                   + (double)(mx[2]-mn[2])*(mx[2]-mn[2]));
}

// ---------------------------------------------------------------------------
// Oracle-validation harness.
// ---------------------------------------------------------------------------
typedef double (*MetricFn)(const PaletteFrame&);

struct SeparationReport {
    double goodMin, goodMed, goodMax, badMin, badMed, badMax;
    double marginRatio;   // badMin / goodMax  (>1 => bad reads worse; the separation)
    enum Verdict { VALID, MARGINAL, BLIND, INVERTED } verdict;
    int nGood, nBad;
    const char* VerdictName() const {
        switch (verdict) { case VALID: return "VALID"; case MARGINAL: return "MARGINAL";
                           case BLIND: return "BLIND"; default: return "INVERTED"; }
    }
};

inline void MinMedMax(std::vector<double> v, double& mn, double& md, double& mx) {
    std::sort(v.begin(), v.end());
    mn = v.front(); mx = v.back();
    md = v[v.size()/2];
}

// Population hygiene: unless allowMixed, a population may not mix nb (gender) or
// mesh (OPTIONS.md §4.2 as an ASSERT, not a norm). Returns false + fills `err`.
inline bool PopulationHomogeneous(const std::vector<PaletteFrame>& p, std::string& err) {
    if (p.empty()) { err = "empty population"; return false; }
    int nb = p[0].nb; const std::string& mesh = p[0].mesh;
    for (const PaletteFrame& f : p) {
        if (f.nb != nb) { err = "mixed nb (gender) in population: " + std::to_string(nb) + " vs " + std::to_string(f.nb); return false; }
        if (f.mesh != mesh) { err = "mixed mesh in population: '" + mesh + "' vs '" + f.mesh + "'"; return false; }
    }
    return true;
}

// marginRatio threshold for VALID (zero-overlap AND >= this).
constexpr double kValidMargin = 3.0;

inline SeparationReport ValidateMetricRaw(MetricFn m,
                                          const std::vector<PaletteFrame>& good,
                                          const std::vector<PaletteFrame>& bad) {
    SeparationReport r{};
    r.nGood = (int)good.size(); r.nBad = (int)bad.size();
    std::vector<double> g, b;
    for (const PaletteFrame& f : good) g.push_back(m(f));
    for (const PaletteFrame& f : bad)  b.push_back(m(f));
    MinMedMax(g, r.goodMin, r.goodMed, r.goodMax);
    MinMedMax(b, r.badMin, r.badMed, r.badMax);
    r.marginRatio = (r.goodMax > 1e-9) ? (r.badMin / r.goodMax) : (r.badMin > 1e-9 ? 1e9 : 1.0);
    if (r.badMax < r.goodMin)                                  r.verdict = SeparationReport::INVERTED;
    else if (r.badMin > r.goodMax && r.marginRatio >= kValidMargin) r.verdict = SeparationReport::VALID;
    else if (r.badMin > r.goodMax)                            r.verdict = SeparationReport::MARGINAL;
    else                                                       r.verdict = SeparationReport::BLIND;
    return r;
}

// ---------------------------------------------------------------------------
// Gate registry (the "no unvalidated oracles as gates" lint §4.3 as code). A
// metric may gate the live suite (Suite D) ONLY if it is here AND its named
// OracleValidation test is green. RegistryComplete pins this.
// ---------------------------------------------------------------------------
struct GateEntry { const char* metric; const char* validationTest; bool gateEligible; };
inline const std::vector<GateEntry>& GateRegistry() {
    static const std::vector<GateEntry> reg = {
        // metric                       validating OracleValidation.* test              gate-eligible
        { "M_BlendSpread",              "OracleValidation.BlendSpreadSeparatesTornBlend", true  },
        { "M_Tier1RestCoherence",       "OracleValidation.Tier1SeparatesCeilingHand",     true  },
        { "M_Tier2JointAttach",         "OracleValidation.Tier2SeparatesCeilingHand",     true  },
        // NON-gating: proven blind / diagnostic — listed for completeness, never a gate.
        { "M_WorldExtent",              "OracleValidation.WextIsNotAnOracle",             false },
        { "M_InterBoneRelPose_Palette", "OracleValidation.InterBonePaletteDiagnostic",    false },
        { "M_InterBoneRelPoseWorld",    "OracleValidation.InterBoneWorldComparand",       false },
    };
    return reg;
}
inline MetricFn MetricByName(const char* name) {
    if (!std::strcmp(name, "M_BlendSpread"))              return &M_BlendSpread;
    if (!std::strcmp(name, "M_Tier1RestCoherence"))       return &M_Tier1RestCoherence;
    if (!std::strcmp(name, "M_Tier2JointAttach"))         return &M_Tier2JointAttach;
    if (!std::strcmp(name, "M_WorldExtent"))              return &M_WorldExtent;
    if (!std::strcmp(name, "M_InterBoneRelPose_Palette")) return &M_InterBoneRelPose_Palette;
    if (!std::strcmp(name, "M_InterBoneRelPoseWorld"))    return &M_InterBoneRelPoseWorld;
    return nullptr;
}

} // namespace skinoracle

#endif // RB3_TESTS_SKINNING_ORACLE_H
