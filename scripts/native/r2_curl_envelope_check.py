#!/usr/bin/env python3
"""
r2_curl_envelope_check.py — R2 (Wave-18 Lane N) middle/ring curl-envelope
regression net over the committed D4 FLOOR table.

WHAT IT ASSERTS (and, deliberately, what it does NOT). Per the D4 CORRECTION
block (D4_findings.md:133, R5 VERDICT §8): the surviving 14-41deg middle/ring
FLOOR is **clip curl-interval coverage vs a driverless settled pose**, NOT the
vert-encoded inter-bone basis mechanism. So this check asserts the MEASURED
ENVELOPE only:

  * anchor  FLOOR (forearm->hand) is ~0                 (frame-matchable, shared)
  * thumb   FLOOR is exonerated, small                  (frame-matchable)
  * middle/ring inner-segment FLOOR SURVIVES frame-matching (stays well above the
    thumb/anchor floors) — no vignette frame closes it

The FLOOR = magdiff_min_over_sweep = the smallest convention-invariant |Δmag|
achievable by ANY frame of the shared clip. It is the regression net R5's chosen
fix must keep from regressing; it makes NO mechanism claim (the banned "confirming
the mechanism" phrase, VERDICT §8.1, is not used).

Reads the committed native measurement (real D4 sweep), NOT a synthetic
construction — this is why it is not a Suite-C in-vitro test (A9).

Usage:  scripts/native/r2_curl_envelope_check.py [--json PATH] [--emit-summary PATH]
Exit 0 = envelope holds; 1 = a band is violated (regression).
"""
import argparse, json, os, sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
D4 = os.path.join(REPO, "docs", "native", "engine-arch-review-2026-07-05",
                  "execution", "R1-DOLPHIN", "evidence", "D4_delta_table.json")

# The pinned envelope (the SAME values the rb3-tests CurlEnvelope gtest pins).
# Derived from the committed D4 headline + per-pair FLOOR rows; bands are wide
# enough to be a regression net, tight enough to catch a real collapse.
PIN = {
    "anchor_floor_mean_max": 2.0,        # anchor FLOOR mean must stay ~0
    "thumb_floor_mean_max": 10.0,        # thumb FLOOR exonerated (small)
    "midring_floor_mean_band": (8.0, 20.0),   # committed headline 13.406
    "midring_floor_max_band": (30.0, 48.0),   # committed headline max 41.215
    # ordering: anchor FLOOR < thumb FLOOR < middle/ring FLOOR (means)
}

def classify(pair):
    p = pair.lower()
    if "middlefinger" in p or "ringfinger" in p:
        # inner segments only (02->03, 01->02, hand->01 base) — all middle/ring
        return "midring"
    if "thumb" in p:
        return "thumb"
    if p.endswith("-hand") or "forearm->" in p:
        return "anchor"
    return "other"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", default=D4)
    ap.add_argument("--emit-summary", default=None)
    args = ap.parse_args()

    d = json.load(open(args.json))
    groups = {"anchor": [], "thumb": [], "midring": []}
    for m in d["members"]:
        for row in m["rows"]:
            g = classify(row["pair"])
            if g in groups and row.get("status") == "ok":
                groups[g].append(float(row["magdiff_min_over_sweep"]))

    def mean(xs): return sum(xs) / len(xs) if xs else -1.0
    anchor_mean = mean(groups["anchor"])
    thumb_mean = mean(groups["thumb"])
    midring_mean = mean(groups["midring"])
    midring_max = max(groups["midring"]) if groups["midring"] else -1.0

    hd = d["headline"]
    summary = {
        "anchor_floor_mean": round(anchor_mean, 4),
        "thumb_floor_mean": round(thumb_mean, 4),
        "midring_floor_mean": round(midring_mean, 4),
        "midring_floor_max": round(midring_max, 4),
        "committed_headline": {
            "anchor_magdiff_at_matched_mean": hd.get("anchor_magdiff_at_matched_mean"),
            "thumb_magdiff_FLOOR_mean": hd.get("thumb_magdiff_FLOOR_mean"),
            "middlering_magdiff_FLOOR_mean": hd.get("middlering_magdiff_FLOOR_mean"),
            "middlering_magdiff_FLOOR_max": hd.get("middlering_magdiff_FLOOR_max"),
        },
        "framing": "measured envelope only (D4 CORRECTION / VERDICT §8) — NOT a mechanism claim",
    }
    print(json.dumps(summary, indent=2))

    fails = []
    if anchor_mean > PIN["anchor_floor_mean_max"]:
        fails.append(f"anchor FLOOR mean {anchor_mean:.3f} > {PIN['anchor_floor_mean_max']}")
    if thumb_mean > PIN["thumb_floor_mean_max"]:
        fails.append(f"thumb FLOOR mean {thumb_mean:.3f} > {PIN['thumb_floor_mean_max']}")
    lo, hi = PIN["midring_floor_mean_band"]
    if not (lo <= midring_mean <= hi):
        fails.append(f"middle/ring FLOOR mean {midring_mean:.3f} outside [{lo},{hi}]")
    lo, hi = PIN["midring_floor_max_band"]
    if not (lo <= midring_max <= hi):
        fails.append(f"middle/ring FLOOR max {midring_max:.3f} outside [{lo},{hi}]")
    # ordering: middle/ring FLOOR SURVIVES (stays above thumb + anchor)
    if not (midring_mean > thumb_mean > anchor_mean):
        fails.append(f"ordering broken: anchor {anchor_mean:.3f} < thumb {thumb_mean:.3f} "
                     f"< midring {midring_mean:.3f} FAILED")
    # cross-check the recompute against the committed headline (drift guard)
    if abs(midring_mean - hd["middlering_magdiff_FLOOR_mean"]) > 1.0:
        fails.append(f"recomputed midring FLOOR mean {midring_mean:.3f} drifted from committed "
                     f"headline {hd['middlering_magdiff_FLOOR_mean']}")

    if args.emit_summary:
        summary["result"] = "FAIL" if fails else "PASS"
        summary["violations"] = fails
        json.dump(summary, open(args.emit_summary, "w"), indent=2)

    if fails:
        print("\nENVELOPE VIOLATIONS:", file=sys.stderr)
        for f in fails:
            print("  - " + f, file=sys.stderr)
        return 1
    print("\nOK: middle/ring curl FLOOR survives frame-matching; thumb+anchor exonerated "
          "(measured envelope holds).")
    return 0

if __name__ == "__main__":
    sys.exit(main())
