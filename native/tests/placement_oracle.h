#pragma once
//
// W2.1.S1 — skinned-placement "right, not just different" oracle.
//
// The committed draw-log golden (splash_screen) has NO crowd and NO drum kit, so
// the canonical comparator CANNOT see the SYS-1 placement bug at all: every
// skinned draw in the game (band, hair, CROWD 3D chars, skinned UI) is forced to
// obj.world = identity by DrawMesh (Rnd_Wgpu_RB3.cpp:2847-2848), so all crowd
// instances co-locate at the origin. This header is the placement gate the
// splash golden cannot provide.
//
// It correlates two capture artifacts:
//
//   * the PROBE log — one line per crowd instance emitted by an
//     HX_NATIVE + RB3_PLACEMENT_PROBE-gated fprintf at Crowd.cpp's
//     `curChar->SetWorldXfm(spXfm)` site (WorldCrowd::Draw3DChars). `spXfm.v` is
//     the FAITHFUL per-instance bowl placement the decomp computed CPU-side — the
//     "right" reference. Format:
//         RB3_PLACEMENT_PROBE crowd inst=<u> x=<f> y=<f> z=<f>
//     (a `kind` field — "crowd" today, "drum" reserved for the S2 prop probe — is
//     parsed so the format extends without touching the oracle).
//
//   * the DRAW-LOG (drawlog_compare.h DrawLogFrame) — what the renderer actually
//     submitted. Each skinned draw carries obj.world[16] (column-major), whose
//     translation is world[12..14].
//
// The contract W2.1 adopts (PLAN.md "Design"): under the fixed placement contract
// a crowd instance's drawn obj.world translation == the spXfm.v it was posed with.
// So the oracle asserts, over the skinned draws:
//
//   (A) reference sanity — the probe captured a real, spread crowd frame:
//       >= 2 posed instances, pairwise-distinct, posed bbox spanning the bowl
//       (max-axis extent > minPosedSpan). Otherwise INCONCLUSIVE (the capture
//       never reached a populated crowd scene) — neither pass nor the bug.
//   (B) coverage — at least `minMatched` non-origin posed positions have a drawn
//       skinned translation within matchEps. On the unfixed build every skinned
//       translation is 0 -> 0 matches -> FAIL (kPosedNotDrawn).
//   (C) span — the drawn skinned translations span a bbox whose max-axis extent
//       is >= spanFrac * the posed extent (the crowd is spread, not collapsed).
//       On the unfixed build the drawn extent is ~0 -> FAIL (kDrawnCollapsed).
//   (D) distinctness — the drawn skinned translations form >= 2 clusters at
//       matchEps (instances are pairwise-distinct, not one origin blob). On the
//       unfixed build there is one origin cluster -> FAIL (kDrawnColocated).
//
// (B)+(C)+(D) are build-independent: they go RED on the current all-identity
// build and GREEN once obj.world carries spXfm, without false-failing under
// frustum culling of some instances (coverage needs only `minMatched`, and the
// span/distinctness checks read the drawn set, not every posed instance).
//
// Header-only; reuses drawlog_compare.h's parser. No GPU, no engine boot: the
// gtest drives it over synthetic frames (proving the logic + fail-red) and, when
// RB3_PLACEMENT_DRAWLOG / RB3_PLACEMENT_PROBE_LOG point at a real capture from
// scripts/native/placement-gate-capture.py, over the live artifacts.

#include "drawlog_compare.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace placement {

// One posed crowd (or, reserved, drum) instance parsed from the probe log.
struct PosedInstance {
    std::string kind;   // "crowd" | "drum"
    int    index = -1;
    double x = 0, y = 0, z = 0;
};

struct OracleOptions {
    double matchEps    = 1.0;   // world-units: drawn translation ~= posed spXfm.v
    double originRadius = 5.0;  // posed instances within this of origin are excluded
                                // from the coverage check (identity would spuriously
                                // "match" them).
    double minPosedSpan = 5.0;  // posed bbox max-axis extent must exceed this to judge
    double spanFrac     = 0.5;  // drawn extent must be >= spanFrac * posed extent
    int    minMatched   = 2;    // >= this many far-posed positions must be drawn
    // Drum-kind (S2): a bone-attached kit prop sits offset in front of the
    // drummer's waypoint, so the drawn kit translation is near — not exactly at —
    // the band-member reference. Generous enough to admit that offset; the RED
    // discriminator does not depend on it (flag-OFF has ZERO non-origin skinned
    // draws, so any reasonable value goes RED).
    double drumEps      = 12.0;
};

enum FailureKind {
    kPosedNotDrawn,   // (B) far-posed positions have no matching drawn translation
    kDrawnCollapsed,  // (C) drawn skinned translations do not span the bowl
    kDrawnColocated,  // (D) drawn skinned translations form < 2 distinct clusters
    kDrumAtOrigin,    // (S2) no non-origin skinned draw near any band waypoint
    kDrumRefAtOrigin, // (S2) INCONCLUSIVE: no non-origin drum reference in the probe
};

struct OracleFailure {
    FailureKind kind;
    std::string detail;
};

struct OracleResult {
    bool passed = false;
    bool inconclusive = false;   // capture did not reach a spread crowd frame
    std::vector<OracleFailure> failures;

    // metrics (for logging / STATUS evidence)
    int    posedCount = 0;
    int    farPosedCount = 0;    // posed with radius > originRadius
    int    matchedCount = 0;
    int    skinnedDrawCount = 0;
    int    drawnClusters = 0;
    double posedExtent = 0;
    double drawnExtent = 0;

    bool Has(FailureKind k) const {
        for (const auto& f : failures) if (f.kind == k) return true;
        return false;
    }
    std::string Describe() const {
        std::string s;
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "posed=%d far=%d matched=%d skinnedDraws=%d clusters=%d "
                 "posedExtent=%.3f drawnExtent=%.3f %s\n",
                 posedCount, farPosedCount, matchedCount, skinnedDrawCount,
                 drawnClusters, posedExtent, drawnExtent,
                 inconclusive ? "[INCONCLUSIVE]" : (passed ? "[PASS]" : "[FAIL]"));
        s += buf;
        for (const auto& f : failures) { s += "  - "; s += f.detail; s += "\n"; }
        return s;
    }
};

// ---------------------------------------------------------------------------
// Parse the probe log. Scans every line for the RB3_PLACEMENT_PROBE prefix
// (other engine-log noise is ignored), so it works on a raw stdout/stderr dump.
// ---------------------------------------------------------------------------
inline std::vector<PosedInstance> ParsePlacementProbeText(const std::string& text) {
    std::vector<PosedInstance> out;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = (nl == std::string::npos) ? text.size() : nl + 1;
        size_t p = line.find("RB3_PLACEMENT_PROBE");
        if (p == std::string::npos) continue;
        // RB3_PLACEMENT_PROBE <kind> inst=<i> x=<f> y=<f> z=<f>
        char kind[32] = {0};
        PosedInstance pi;
        // The kind token sits between the tag and "inst=".
        const char* c = line.c_str() + p + strlen("RB3_PLACEMENT_PROBE");
        if (sscanf(c, " %31s inst=%d x=%lf y=%lf z=%lf",
                   kind, &pi.index, &pi.x, &pi.y, &pi.z) == 5) {
            pi.kind = kind;
            out.push_back(pi);
        }
    }
    return out;
}

inline std::vector<PosedInstance> ParsePlacementProbeFile(const char* path) {
    std::vector<PosedInstance> out;
    FILE* f = fopen(path, "rb");
    if (!f) return out;
    std::string s;
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    fclose(f);
    return ParsePlacementProbeText(s);
}

// ---------------------------------------------------------------------------
// The oracle.
// ---------------------------------------------------------------------------
inline double Radius(double x, double y, double z) { return std::sqrt(x*x + y*y + z*z); }

inline OracleResult RunPlacementOracle(const drawlog::DrawLogFrame& frame,
                                       const std::vector<PosedInstance>& posedAll,
                                       const OracleOptions& opt = OracleOptions()) {
    OracleResult r;
    char buf[256];

    // crowd-kind posed instances only (drum reserved for S2).
    std::vector<PosedInstance> posed;
    for (const auto& p : posedAll)
        if (p.kind == "crowd") posed.push_back(p);
    r.posedCount = (int)posed.size();

    // (A) reference sanity.
    if (posed.size() < 2) {
        r.inconclusive = true;
        r.failures.push_back({kPosedNotDrawn,
            "INCONCLUSIVE: < 2 crowd instances in the probe log (capture did not "
            "reach a populated crowd frame)"});
        return r;
    }
    double pminx=1e30, pminy=1e30, pminz=1e30, pmaxx=-1e30, pmaxy=-1e30, pmaxz=-1e30;
    for (const auto& p : posed) {
        pminx = std::min(pminx, p.x); pmaxx = std::max(pmaxx, p.x);
        pminy = std::min(pminy, p.y); pmaxy = std::max(pmaxy, p.y);
        pminz = std::min(pminz, p.z); pmaxz = std::max(pmaxz, p.z);
    }
    r.posedExtent = std::max(pmaxx-pminx, std::max(pmaxy-pminy, pmaxz-pminz));
    if (r.posedExtent <= opt.minPosedSpan) {
        r.inconclusive = true;
        snprintf(buf, sizeof(buf),
                 "INCONCLUSIVE: posed crowd bbox extent %.3f <= minPosedSpan %.3f "
                 "(crowd not spread in this capture)", r.posedExtent, opt.minPosedSpan);
        r.failures.push_back({kDrawnCollapsed, buf});
        return r;
    }

    // Drawn skinned translations.
    std::vector<std::array<double,3>> drawn;
    for (const auto& d : frame.draws) {
        if (!d.skinned) continue;
        drawn.push_back({(double)d.world[12], (double)d.world[13], (double)d.world[14]});
    }
    r.skinnedDrawCount = (int)drawn.size();
    if (drawn.empty()) {
        r.inconclusive = true;
        r.failures.push_back({kDrawnColocated,
            "INCONCLUSIVE: no skinned draws in the draw-log frame"});
        return r;
    }

    // (B) coverage: far-posed positions with a matching drawn translation.
    int farPosed = 0, matched = 0;
    for (const auto& p : posed) {
        if (Radius(p.x, p.y, p.z) <= opt.originRadius) continue;
        farPosed++;
        bool hit = false;
        for (const auto& t : drawn) {
            double dx = t[0]-p.x, dy = t[1]-p.y, dz = t[2]-p.z;
            if (std::sqrt(dx*dx+dy*dy+dz*dz) <= opt.matchEps) { hit = true; break; }
        }
        if (hit) matched++;
    }
    r.farPosedCount = farPosed;
    r.matchedCount = matched;

    // (C) drawn span.
    double dminx=1e30, dminy=1e30, dminz=1e30, dmaxx=-1e30, dmaxy=-1e30, dmaxz=-1e30;
    for (const auto& t : drawn) {
        dminx = std::min(dminx, t[0]); dmaxx = std::max(dmaxx, t[0]);
        dminy = std::min(dminy, t[1]); dmaxy = std::max(dmaxy, t[1]);
        dminz = std::min(dminz, t[2]); dmaxz = std::max(dmaxz, t[2]);
    }
    r.drawnExtent = std::max(dmaxx-dminx, std::max(dmaxy-dminy, dmaxz-dminz));

    // (D) distinct clusters at matchEps (greedy).
    std::vector<std::array<double,3>> centers;
    for (const auto& t : drawn) {
        bool merged = false;
        for (const auto& c : centers) {
            double dx=t[0]-c[0], dy=t[1]-c[1], dz=t[2]-c[2];
            if (std::sqrt(dx*dx+dy*dy+dz*dz) <= opt.matchEps) { merged = true; break; }
        }
        if (!merged) centers.push_back(t);
    }
    r.drawnClusters = (int)centers.size();

    // Verdict.
    if (farPosed > 0 && matched < opt.minMatched) {
        snprintf(buf, sizeof(buf),
                 "posed-not-drawn: only %d/%d far crowd positions have a drawn "
                 "skinned obj.world within %.3f (need >= %d) — instances collapsed "
                 "to identity", matched, farPosed, opt.matchEps, opt.minMatched);
        r.failures.push_back({kPosedNotDrawn, buf});
    }
    if (r.drawnExtent < opt.spanFrac * r.posedExtent) {
        snprintf(buf, sizeof(buf),
                 "drawn-collapsed: drawn skinned bbox extent %.3f < %.2f * posed "
                 "extent %.3f (%.3f) — crowd does not span the bowl",
                 r.drawnExtent, opt.spanFrac, r.posedExtent, opt.spanFrac*r.posedExtent);
        r.failures.push_back({kDrawnCollapsed, buf});
    }
    if (r.drawnClusters < 2) {
        snprintf(buf, sizeof(buf),
                 "drawn-colocated: drawn skinned translations form only %d cluster(s) "
                 "at eps %.3f — instances co-located", r.drawnClusters, opt.matchEps);
        r.failures.push_back({kDrawnColocated, buf});
    }
    r.passed = r.failures.empty();
    return r;
}

// ---------------------------------------------------------------------------
// Drum-kind oracle (W2.1-flip.S2). Distinct from the crowd spread test: a single
// band/kit placement is not a spread bowl. It correlates the "drum" probe
// references (band-member waypoint worlds emitted by BandConfiguration::
// SyncPlayMode after Teleport — the faithful placement the drum kit and other
// bone-attached instrument props hang off) with the drawn skinned set:
//
//   (a) reference non-origin — >= 1 drum reference with Radius > originRadius.
//       Else INCONCLUSIVE (kDrumRefAtOrigin): this venue placed the band at the
//       origin (or SyncPlayMode never resolved a member) — neither pass nor bug.
//   (b) drawn consistency — >= 1 SKINNED draw whose obj.world translation is
//       non-origin (Radius > originRadius) AND within drumEps of a drum
//       reference. On the current default-OFF build every skinned obj.world is
//       forced to identity (DrawMesh, Rnd_Wgpu_RB3.cpp) -> zero non-origin
//       skinned draws -> RED (kDrumAtOrigin). Flag-ON the bone-attached kit prop
//       (or the drummer body, if the prop is not itself flagged skinned in the
//       drawlog) carries meshWorld near the drummer -> GREEN.
//
// The RED/GREEN discriminator is build-independent and does NOT depend on tight
// eps tuning: the flag-OFF build has ZERO non-origin skinned draws, so any
// reasonable drumEps goes RED; drumEps only needs to be generous enough to admit
// the offset kit prop flag-ON.
// ---------------------------------------------------------------------------
inline OracleResult RunDrumOracle(const drawlog::DrawLogFrame& frame,
                                  const std::vector<PosedInstance>& posedAll,
                                  const OracleOptions& opt = OracleOptions()) {
    OracleResult r;
    char buf[256];

    // drum-kind posed instances only (crowd handled by RunPlacementOracle).
    std::vector<PosedInstance> posed;
    for (const auto& p : posedAll)
        if (p.kind == "drum") posed.push_back(p);
    r.posedCount = (int)posed.size();

    // (a) reference non-origin.
    if (posed.empty()) {
        r.inconclusive = true;
        r.failures.push_back({kDrumRefAtOrigin,
            "INCONCLUSIVE: no drum references in the probe log (SyncPlayMode did "
            "not resolve a band member during the capture)"});
        return r;
    }
    std::vector<PosedInstance> farRefs;
    for (const auto& p : posed)
        if (Radius(p.x, p.y, p.z) > opt.originRadius) farRefs.push_back(p);
    r.farPosedCount = (int)farRefs.size();
    if (farRefs.empty()) {
        r.inconclusive = true;
        snprintf(buf, sizeof(buf),
                 "INCONCLUSIVE: all %d drum reference(s) within originRadius %.3f "
                 "of origin (this venue placed the band at origin) — neither pass "
                 "nor bug", r.posedCount, opt.originRadius);
        r.failures.push_back({kDrumRefAtOrigin, buf});
        return r;
    }

    // Drawn skinned translations.
    std::vector<std::array<double,3>> drawn;
    for (const auto& d : frame.draws) {
        if (!d.skinned) continue;
        drawn.push_back({(double)d.world[12], (double)d.world[13], (double)d.world[14]});
    }
    r.skinnedDrawCount = (int)drawn.size();
    if (drawn.empty()) {
        r.inconclusive = true;
        r.failures.push_back({kDrumAtOrigin,
            "INCONCLUSIVE: no skinned draws in the draw-log frame"});
        return r;
    }

    // (b) drawn consistency: >= 1 non-origin skinned draw within drumEps of a far
    // drum reference. On the flag-OFF build every skinned obj.world is identity,
    // so the Radius filter drops them all -> matched == 0 -> RED.
    int matched = 0;
    double bestDist = 1e30;
    for (const auto& t : drawn) {
        if (Radius(t[0], t[1], t[2]) <= opt.originRadius) continue;  // identity/origin draw
        for (const auto& p : farRefs) {
            double dx=t[0]-p.x, dy=t[1]-p.y, dz=t[2]-p.z;
            double dist = std::sqrt(dx*dx+dy*dy+dz*dz);
            if (dist < bestDist) bestDist = dist;
            if (dist <= opt.drumEps) { matched++; break; }
        }
    }
    r.matchedCount = matched;

    if (matched < 1) {
        snprintf(buf, sizeof(buf),
                 "drum-at-origin: no non-origin skinned draw within drumEps %.3f "
                 "of any band waypoint (nearest %.3f) — the kit/band collapsed to "
                 "identity (obj.world forced to origin)", opt.drumEps,
                 bestDist == 1e30 ? -1.0 : bestDist);
        r.failures.push_back({kDrumAtOrigin, buf});
    }
    r.passed = r.failures.empty();
    return r;
}

} // namespace placement
