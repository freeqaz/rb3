#pragma once
//
// W2.3.S1 — crowd bone-source "negative-control" oracle.
//
// SYS-1 bone-source half: DrawMesh builds the GPU bone palette from
// `owner = mesh->GeomOwner()` (owner->BoneTransAt / BoneOffsetAt), never from the
// drawn instance's OWN bones. When a crowd/extras skinned mesh's owner carries an
// offset-poisoned bind (the female-outfit / shared-servo-skeleton class), the
// composed skin flings vertices thousands of units — the SKIN_CLAMP backstop drops
// those bones to identity (`sFallbackBones++`), so the crowd renders bunched /
// T-posed instead of shattered. The draw-time RebindCrowdCharBonesToOwnSkeleton
// (Crowd.cpp) papers over this by rebaking the OWNER's offsets so the composed skin
// stays near bind; disable it (RB3_NO_CROWD_REBIND=1) and the shard-drop returns.
//
// This oracle is the negative control the splash golden cannot provide. It consumes
// the `[CROWD_BONE_PROBE]` lines emitted by the engine's RB3_CROWD_BONE_PROBE
// diagnostic (Rnd_Wgpu_RB3.cpp, W2.3.S1) under TWO conditions and asserts:
//
//   * shard-drop NOT elevated — the number of crowd/extras meshes whose OWNER-bone
//     palette would exceed the 12u SKIN_CLAMP (i.e. would shard) in the candidate
//     capture is not greater (beyond a small slack) than in the rebind-ON baseline.
//     On the current build with RB3_NO_CROWD_REBIND=1 (rebind OFF) that count
//     SPIKES vs the rebind-ON baseline → the gate goes RED. That red is the S1
//     fail-red: the exact condition W2.3-flag-ON must later make GREEN without the
//     rebind.
//
// It also derives the S1 DECISION (SHARED / SELF+POISON / MIXED) from the RAW
// (rebind-OFF) candidate seam by comparing, per poisoned-owner mesh, whether the
// DRAWN mesh's OWN bones would pass the SKIN_CLAMP (own-bones-fix → SHARED) or also
// exceed it (own-bones-poison → SELF+POISON, "read own bones" is a no-op).
//
// Header-only, pure text parse (no GPU / no engine boot / no drawlog dependency),
// mirroring placement_oracle.h. The gtest drives it over synthetic captures
// (logic + fail-red) and, when RB3_CROWD_BONE_BASELINE / RB3_CROWD_BONE_CANDIDATE
// point at real captures from scripts/native/crowd-bone-gate-capture.py, over the
// live artifacts.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace crowdbone {

// One parsed [CROWD_BONE_PROBE] line (one drawn instance of a crowd/extras mesh).
struct ProbeRec {
    std::string tag;          // "crowdextra" | "otherskin"
    std::string mesh;
    int         inst        = 0;
    bool        ownerEqMesh = false;
    int         ownerBones  = 0;
    int         meshBones   = 0;
    int         diffInstance = 0;   // # bones where owner->BoneTransAt != mesh->BoneTransAt
    double      worstOwnerExtent = -1.0;  // owner-bone mesh-local skin extent (u)
    double      worstOwnExtent   = -1.0;  // own-bone   mesh-local skin extent (u)
    std::string verdict;      // OWNER-CLEAN | OWN-BONES-FIX | OWN-BONES-POISON
};

// Parse every "[CROWD_BONE_PROBE] ..." line out of a noisy engine log.
inline std::vector<ProbeRec> ParseCrowdBoneProbeText(const std::string& text) {
    std::vector<ProbeRec> out;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = (nl == std::string::npos) ? text.size() : nl + 1;
        size_t p = line.find("[CROWD_BONE_PROBE]");
        if (p == std::string::npos) continue;
        const char* c = line.c_str();
        ProbeRec r;
        char tag[32] = {0}, mesh[256] = {0}, verdict[32] = {0};
        int ownerEq = 0, ob = 0, mb = 0, di = 0, no = 0, nn = 0, wob = 0, wnb = 0;
        double woe = -1, wne = -1;
        // tag=%s mesh='%s' inst=%d owner=%p mesh=%p ownerEqMesh=%d ownerBones=%d
        // meshBones=%d diffInstance=%d nullOwner=%d nullOwn=%d
        // worstOwnerExtent=%fu(b%d) worstOwnExtent=%fu(b%d) SKIN_CLAMP=12u -> %s
        const char* tp = strstr(c, "tag=");
        const char* mp = strstr(c, "mesh='");
        if (!tp || !mp) continue;
        sscanf(tp, "tag=%31s", tag);
        // mesh name is single-quoted (may contain spaces): copy between the quotes.
        const char* q0 = mp + 6;
        const char* q1 = strchr(q0, '\'');
        if (!q1) continue;
        size_t mlen = (size_t)(q1 - q0);
        if (mlen >= sizeof(mesh)) mlen = sizeof(mesh) - 1;
        memcpy(mesh, q0, mlen); mesh[mlen] = 0;
        const char* rest = q1;
        auto grabI = [&](const char* key, int* dst) {
            const char* k = strstr(rest, key); if (k) sscanf(k + strlen(key), "%d", dst);
        };
        auto grabExt = [&](const char* key, double* dst, int* bdst) {
            const char* k = strstr(rest, key);
            if (k) sscanf(k + strlen(key), "%lfu(b%d)", dst, bdst);
        };
        grabI("inst=", &r.inst);
        grabI("ownerEqMesh=", &ownerEq);
        grabI("ownerBones=", &ob);
        grabI("meshBones=", &mb);
        grabI("diffInstance=", &di);
        grabI("nullOwner=", &no);
        grabI("nullOwn=", &nn);
        grabExt("worstOwnerExtent=", &woe, &wob);
        grabExt("worstOwnExtent=", &wne, &wnb);
        const char* ap = strstr(rest, "-> ");
        if (ap) sscanf(ap + 3, "%31s", verdict);
        r.tag = tag; r.mesh = mesh; r.ownerEqMesh = ownerEq != 0;
        r.ownerBones = ob; r.meshBones = mb; r.diffInstance = di;
        r.worstOwnerExtent = woe; r.worstOwnExtent = wne; r.verdict = verdict;
        (void)no; (void)nn;
        out.push_back(r);
    }
    return out;
}

inline std::vector<ProbeRec> ParseCrowdBoneProbeFile(const char* path) {
    std::vector<ProbeRec> out;
    FILE* f = fopen(path, "rb");
    if (!f) return out;
    std::string s; char buf[8192]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    fclose(f);
    return ParseCrowdBoneProbeText(s);
}

struct Options {
    double clampU     = 12.0;  // the SKIN_CLAMP shard threshold (mesh-local extent)
    int    shardSlack = 1;     // candidate may exceed baseline by this many meshes
                               // (boot-random band-char generation jitter, per W2.1)
    bool   crowdOnly  = true;  // restrict to crowd/extras meshes (drop other skin)
    // The faithful shard-drop gate operates on the SKIN_CLAMP EVENT count (the
    // engine's per-bone fallback-to-identity, = how badly the crowd shards). The
    // candidate is "elevated" when it exceeds baseline*elevationFactor + eventSlack.
    // MEASURED on 6852caa: baseline (rebind ON) ~1.6k, candidate (rebind OFF) ~14.5k
    // (8.8x) — factor 2.0 + slack 500 cleanly separates them while absorbing the
    // boot-random band-char generation jitter W2.1 documented.
    double elevationFactor = 2.0;
    int    eventSlack      = 500;
};

// Per-mesh reduction of the probe records (worst-instance extents, OR'd shared sig).
struct MeshAgg {
    std::string mesh;
    bool   anyShared = false;  // any instance with diffInstance>0 || !ownerEqMesh
    double worstOwner = -1.0;
    double worstOwn   = -1.0;
    int    instances  = 0;
};

inline std::vector<MeshAgg> AggregatePerMesh(const std::vector<ProbeRec>& recs,
                                             const Options& opt) {
    std::map<std::string, MeshAgg> m;
    for (const auto& r : recs) {
        if (opt.crowdOnly && r.tag != "crowdextra") continue;
        MeshAgg& a = m[r.mesh];
        a.mesh = r.mesh;
        a.instances++;
        if (r.diffInstance > 0 || !r.ownerEqMesh) a.anyShared = true;
        a.worstOwner = std::max(a.worstOwner, r.worstOwnerExtent);
        a.worstOwn   = std::max(a.worstOwn,   r.worstOwnExtent);
    }
    std::vector<MeshAgg> out;
    for (auto& kv : m) out.push_back(kv.second);
    return out;
}

// Aggregate shard/decision statistics over a single capture.
struct ShardStats {
    int meshes           = 0;  // distinct crowd meshes seen
    int instances        = 0;
    int ownerShardMeshes = 0;  // worstOwner > clamp  (the "shard-drop" signal)
    int ownShardMeshes   = 0;  // worstOwn   > clamp
    int sharedMeshes     = 0;  // any instance owner!=mesh / diffInstance>0
    int selfMeshes       = 0;  // owner==mesh AND diffInstance==0 across instances
    // Among poisoned-owner meshes (the population W2.3 must fix):
    int ownFixMeshes     = 0;  // own bones would pass the clamp  → SHARED-fixable
    int ownPoisonMeshes  = 0;  // own bones also exceed the clamp → SELF+POISON
};

inline ShardStats ComputeShardStats(const std::vector<ProbeRec>& recs, const Options& opt) {
    ShardStats s;
    auto aggs = AggregatePerMesh(recs, opt);
    s.meshes = (int)aggs.size();
    for (const auto& a : aggs) {
        s.instances += a.instances;
        bool ownerShard = a.worstOwner > opt.clampU;
        bool ownShard   = a.worstOwn   > opt.clampU;
        if (ownerShard) s.ownerShardMeshes++;
        if (ownShard)   s.ownShardMeshes++;
        if (a.anyShared) s.sharedMeshes++; else s.selfMeshes++;
        if (ownerShard) { // the poisoned-owner population that would shard w/o rebind
            if (!ownShard) s.ownFixMeshes++;   // own bones clean → reading them fixes it
            else           s.ownPoisonMeshes++; // own bones poisoned too → no-op
        }
    }
    return s;
}

enum Decision { kNoData, kShared, kSelfPoison, kMixed };

inline const char* DecisionName(Decision d) {
    switch (d) {
        case kShared:     return "SHARED";
        case kSelfPoison: return "SELF+POISON";
        case kMixed:      return "MIXED";
        default:          return "NO-DATA";
    }
}

// Decide from the RAW (rebind-OFF candidate) seam. Only poisoned-owner meshes are
// informative (a clean-owner mesh cannot exercise "does reading own bones help").
inline Decision DecideFrom(const ShardStats& raw) {
    if (raw.ownerShardMeshes == 0) return kNoData;      // owner already clean — rebind was ON?
    if (raw.ownFixMeshes > 0 && raw.ownPoisonMeshes == 0) return kShared;
    if (raw.ownPoisonMeshes > 0 && raw.ownFixMeshes == 0) return kSelfPoison;
    return kMixed;
}

// ---------------------------------------------------------------------------
// SKIN_CLAMP shard-drop signal — the FAITHFUL fail-red metric.
//
// The engine's crowd SKIN_CLAMP backstop drops a bone to identity
// (sFallbackBones++) whenever its composed skin flings past 12u in the mesh's own
// frame; SKIN_CLAMP_PROBE emits one "[SKIN_CLAMP] mesh='..' bone='..' meshLocal=..u"
// per clamped bone. The COUNT of those events on crowd/extras meshes IS how badly
// the crowd shards: with RebindCrowdCharBonesToOwnSkeleton ON the owner offsets are
// rebaked at rest so few bones fling; with it OFF every offset-poisoned bone flings
// and clamps. Unlike the CROWD_BONE_PROBE owner-extent (which is confounded by the
// probe's `!mNativeBonesRebound` skip and — for these self-owned meshes — reads the
// same poisoned bones in both states), this event count separates the two states
// cleanly. MEASURED on 6852caa: baseline (rebind ON) ~1.6k, candidate (rebind OFF)
// ~14.5k. This is the signal the gate asserts on.
// ---------------------------------------------------------------------------
inline bool IsCrowdExtraName(const std::string& n) {
    return n.find("crowd") != std::string::npos || n.find("extra") != std::string::npos;
}

// A single capture (one boot condition): the CROWD_BONE_PROBE records (→ decision)
// plus the SKIN_CLAMP shard-drop event count (→ gate).
struct Capture {
    int skinClampEvents = 0;             // [SKIN_CLAMP] lines on crowd/extras meshes
    int skinClampMeshes = 0;             // distinct crowd/extras meshes clamped
    std::vector<ProbeRec> probes;        // [CROWD_BONE_PROBE] records
    ShardStats stats;                    // derived from probes (for the decision)
};

inline Capture ParseCapture(const std::string& text, const Options& opt = Options()) {
    Capture c;
    c.probes = ParseCrowdBoneProbeText(text);
    c.stats  = ComputeShardStats(c.probes, opt);
    std::map<std::string,int> clampMeshes;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = (nl == std::string::npos) ? text.size() : nl + 1;
        if (line.find("[SKIN_CLAMP]") == std::string::npos) continue;
        const char* mp = strstr(line.c_str(), "mesh='");
        if (!mp) continue;
        const char* q0 = mp + 6;
        const char* q1 = strchr(q0, '\'');
        if (!q1) continue;
        std::string mesh(q0, (size_t)(q1 - q0));
        if (opt.crowdOnly && !IsCrowdExtraName(mesh)) continue;
        c.skinClampEvents++;
        clampMeshes[mesh]++;
    }
    c.skinClampMeshes = (int)clampMeshes.size();
    return c;
}

inline Capture ParseCaptureFile(const char* path, const Options& opt = Options()) {
    FILE* f = fopen(path, "rb");
    if (!f) return Capture();
    std::string s; char buf[8192]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    fclose(f);
    return ParseCapture(s, opt);
}

struct OracleResult {
    bool passed       = false;
    bool inconclusive = false;
    Capture  baseline;    // rebind-ON
    Capture  candidate;   // rebind-OFF (the raw seam)
    Decision decision = kNoData;
    std::vector<std::string> failures;

    std::string Describe() const {
        char buf[640];
        snprintf(buf, sizeof(buf),
            "baseline{clampEvents=%d clampMeshes=%d} candidate{clampEvents=%d "
            "clampMeshes=%d probeMeshes=%d ownerShard=%d ownShard=%d shared=%d "
            "self=%d ownFix=%d ownPoison=%d} decision=%s %s\n",
            baseline.skinClampEvents, baseline.skinClampMeshes,
            candidate.skinClampEvents, candidate.skinClampMeshes,
            candidate.stats.meshes, candidate.stats.ownerShardMeshes,
            candidate.stats.ownShardMeshes, candidate.stats.sharedMeshes,
            candidate.stats.selfMeshes, candidate.stats.ownFixMeshes,
            candidate.stats.ownPoisonMeshes, DecisionName(decision),
            inconclusive ? "[INCONCLUSIVE]" : (passed ? "[PASS]" : "[FAIL]"));
        std::string s = buf;
        for (const auto& f : failures) { s += "  - "; s += f; s += "\n"; }
        return s;
    }
};

// The negative-control gate: the candidate crowd SKIN_CLAMP shard-drop must NOT be
// elevated vs the rebind-ON baseline. On the current build the candidate is
// RB3_NO_CROWD_REBIND=1 → the count spikes ~8.8x → RED (the S1 fail-red).
inline OracleResult RunCrowdBoneOracle(const Capture& baseline, const Capture& candidate,
                                       const Options& opt = Options()) {
    OracleResult r;
    r.baseline  = baseline;
    r.candidate = candidate;
    r.decision  = DecideFrom(candidate.stats);

    if (candidate.probes.empty() && candidate.skinClampEvents == 0) {
        r.inconclusive = true;
        r.failures.push_back("INCONCLUSIVE: no crowd probe/clamp records in the "
                             "candidate capture (gameplay did not reach a crowd frame)");
        return r;
    }
    char buf[320];
    double thresh = baseline.skinClampEvents * opt.elevationFactor + opt.eventSlack;
    if ((double)candidate.skinClampEvents > thresh) {
        snprintf(buf, sizeof(buf),
            "shard-drop elevated: candidate SKIN_CLAMP events=%d > baseline=%d * "
            "%.1f + slack %d (=%.0f) — crowd sharding/co-bunching returned",
            candidate.skinClampEvents, baseline.skinClampEvents, opt.elevationFactor,
            opt.eventSlack, thresh);
        r.failures.push_back(buf);
    }
    r.passed = r.failures.empty();
    return r;
}

} // namespace crowdbone
