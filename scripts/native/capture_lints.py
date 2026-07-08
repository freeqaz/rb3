#!/usr/bin/env python3
"""capture_lints.py — Wave-19 Lane I (W-ISO): shared capture-discipline lints.

Extracts the WAVE19 review's F2/F7/F10 capture disciplines (OPTIONS.md §6.5) as pure
functions with no side effects and no boot, so every grading harness (white_regrade.py
here; wash v2 in Lane F) enforces the same provenance rules from one place.

Lints:
  F2  refuse_refinish_for_grade  — a --refinish (rebuilt-from-disk) run may NOT emit a
      graded verdict. --refinish is a crash-recovery finisher; a verdict's provenance
      must be live boots.
  F7  partition_black_frames     — split luma-0 frames out of the hi_frac population,
      the luma sibling of white_regrade's existing mid_sat==NaN all-black rule. Returns
      the excluded set so the caller commits it (never silently drop).
  F10 safe_json_dump             — json.dump(..., allow_nan=False) plus a pre-pass that
      raises CaptureLintError naming the offending key path on any bare NaN/Inf, so an
      invalid dump fails loud instead of emitting non-standard JSON.
      attempt_disclosure         — surface the overshoot-discard survivorship (capture
      loops to n*4 attempts; §6.5 "attempt/retry counts disclosed").

F7 THRESHOLD DERIVATION (PLAN_REVIEW AM-1, BINDING):
  T = 0.05. Derived from the committed NEARBLACK exemplar row wr_n10_OFF_01
  (mean_luma = 0.0, the 12.43->13.81 hi_frac restatement precedent, WAVE18_CLOSEOUT).
  NOT from wr_n10_ON02_WHITE_zerochroma.png (measured mean_luma 0.78 — a bright
  zero-CHROMA WHITE frame; a threshold above it would exclude 19/20 committed frames).
  Lowest LEGITIMATE committed frame = wr_n10_OFF_02 at mean_luma 0.4798, so T=0.05 sits
  safely in (0.0, 0.1]: it excludes the luma-0 boot ONLY.

Normalization: mean_luma is expected NORMALIZED to [0,1] (white_regrade rows carry it
that way, wr_n10.json). Callers passing 0-255 luma must scale first.
"""
import json
import math

# F7 luma threshold — see module docstring (PLAN_REVIEW AM-1). Bounds documented:
#   exemplar NEARBLACK luma = 0.0 ; lowest legitimate committed frame = 0.4798.
BLACK_LUMA_THRESH = 0.05


class CaptureLintError(Exception):
    """A capture-discipline lint fired: the run/data violates a §6.5 provenance rule."""


# ---------------------------------------------------------------------------
# F2 — refuse a refinish (disk-rebuilt) run from emitting a graded verdict
# ---------------------------------------------------------------------------
def refuse_refinish_for_grade(is_refinish, is_graded):
    """F2: raise if a --refinish run is used to emit a graded verdict.

    --refinish rebuilds rows from on-disk PNGs/logs (crash-recovery finisher, no
    re-boot); a *verdict's* provenance must be live boots. A --refinish inspection that
    emits NO verdict (is_graded=False) is allowed, as is a live graded run.
    """
    if is_refinish and is_graded:
        raise CaptureLintError(
            "F2: a --refinish run (rows rebuilt from disk, not live boots) may not emit "
            "a graded verdict. --refinish is a crash-recovery finisher for non-verdict "
            "inspection only; re-capture live boots for a verdict."
        )


# ---------------------------------------------------------------------------
# F7 — partition luma-0 (black) frames out of the hi_frac population
# ---------------------------------------------------------------------------
def partition_black_frames(rows, luma_key="mean_luma", thresh=BLACK_LUMA_THRESH):
    """F7: split rows whose luma <= thresh (near-black) out of the population.

    Returns (kept, excluded). The luma sibling of white_regrade's mid_sat==NaN
    all-black exclusion (white_regrade.py:218-220). The caller commits `excluded`
    (disclosure) and computes hi_frac/mean stats over `kept` only. A row missing the
    luma key, or carrying a NaN luma, is treated as excluded (it cannot be a valid
    bright frame) and surfaced for disclosure rather than silently averaged in.
    """
    kept, excluded = [], []
    for r in rows:
        v = r.get(luma_key, None)
        if v is None or (isinstance(v, float) and math.isnan(v)) or v <= thresh:
            excluded.append(r)
        else:
            kept.append(r)
    return kept, excluded


# ---------------------------------------------------------------------------
# attempt disclosure — surface overshoot-discard survivorship
# ---------------------------------------------------------------------------
def attempt_disclosure(attempts, captured, discarded_reasons):
    """Surface the capture survivorship: how many boot attempts were made, how many
    yielded a graded frame, and why the rest were discarded. §6.5 "attempt/retry counts
    disclosed" — capture_arm loops to n*4 attempts and drops overshoots/engagement
    failures; without this the discard is invisible."""
    reasons = list(discarded_reasons or [])
    return {
        "attempts": int(attempts),
        "captured": int(captured),
        "discarded": int(attempts) - int(captured),
        "discarded_reasons": reasons,
    }


# ---------------------------------------------------------------------------
# F10 — strict JSON dump; fail loud on bare NaN/Inf
# ---------------------------------------------------------------------------
def _find_nonfinite(obj, path="$"):
    """Yield (key_path, value) for every bare NaN/Inf float anywhere in obj."""
    if isinstance(obj, float):
        if not math.isfinite(obj):
            yield path, obj
    elif isinstance(obj, dict):
        for k, v in obj.items():
            yield from _find_nonfinite(v, f"{path}.{k}")
    elif isinstance(obj, (list, tuple)):
        for i, v in enumerate(obj):
            yield from _find_nonfinite(v, f"{path}[{i}]")


def safe_json_dump(obj, path, indent=2):
    """F10: dump obj to `path` with allow_nan=False, after a pre-pass that raises
    CaptureLintError naming the FIRST offending key path on any bare NaN/Inf. Prevents
    emitting non-standard JSON (bare NaN, which a strict parser rejects — the exact
    defect seen in the legacy wr_n10.json)."""
    bad = list(_find_nonfinite(obj))
    if bad:
        kp, val = bad[0]
        raise CaptureLintError(
            f"F10: refusing to write non-finite value {val!r} at key path {kp} "
            f"(+{len(bad) - 1} more) to {path}. Sanitize (drop/None the field) first."
        )
    with open(path, "w") as f:
        json.dump(obj, f, indent=indent, allow_nan=False)


# ---------------------------------------------------------------------------
# --selftest — the lane's own fail-red for the lint module (§4 lint 3)
# ---------------------------------------------------------------------------
def _selftest():
    import os
    import tempfile

    passes = 0
    total = 4

    # 1. safe_json_dump: NaN RAISES (bad), finite writes (good).
    tmp = tempfile.mkdtemp(prefix="caplint_selftest_")
    p = os.path.join(tmp, "t.json")
    bad_raised = False
    try:
        safe_json_dump({"x": float("nan")}, p)
    except CaptureLintError as e:
        bad_raised = True
        print(f"  [1] safe_json_dump BAD  -> RAISED: {e}")
    good_ok = False
    try:
        safe_json_dump({"x": 1.0, "nested": [1.0, {"y": 2.5}]}, p)
        good_ok = os.path.exists(p) and json.load(open(p))["x"] == 1.0
        print(f"  [1] safe_json_dump GOOD -> wrote {p} round-trips={good_ok}")
    except Exception as e:
        print(f"  [1] safe_json_dump GOOD -> UNEXPECTED RAISE: {e}")
    if bad_raised and good_ok:
        passes += 1
        print("  [1] PASS (NaN raises, finite writes)")
    else:
        print("  [1] FAIL")

    # 2. refuse_refinish_for_grade: (refinish, graded) RAISES; other combos pass.
    r_raised = False
    try:
        refuse_refinish_for_grade(True, True)
    except CaptureLintError as e:
        r_raised = True
        print(f"  [2] refuse(refinish,graded) -> RAISED: {e}")
    other_ok = True
    for a, b in [(True, False), (False, True), (False, False)]:
        try:
            refuse_refinish_for_grade(a, b)
        except CaptureLintError:
            other_ok = False
            print(f"  [2] refuse({a},{b}) -> UNEXPECTED RAISE")
    if r_raised and other_ok:
        passes += 1
        print("  [2] PASS (refinish+graded raises, others pass)")
    else:
        print("  [2] FAIL")

    # 3. partition_black_frames: excludes exactly the luma-0 frame.
    rows = [{"mean_luma": 0.0}, {"mean_luma": 0.5}]
    kept, excl = partition_black_frames(rows)
    p3 = len(kept) == 1 and kept[0]["mean_luma"] == 0.5 and len(excl) == 1 \
        and excl[0]["mean_luma"] == 0.0
    print(f"  [3] partition_black_frames({rows}) -> kept={kept} excluded={excl}")
    # also exercise the shipped threshold boundary: 0.4798 legit stays, 0.05 boundary
    kept2, excl2 = partition_black_frames(
        [{"mean_luma": 0.4798}, {"mean_luma": 0.05}, {"mean_luma": float("nan")}])
    p3b = len(kept2) == 1 and kept2[0]["mean_luma"] == 0.4798 and len(excl2) == 2
    print(f"  [3] boundary (0.4798 legit / 0.05 at-T / NaN) -> kept={kept2} "
          f"excluded_n={len(excl2)}")
    if p3 and p3b:
        passes += 1
        print(f"  [3] PASS (T={BLACK_LUMA_THRESH}: luma<=T & NaN excluded, 0.4798 kept)")
    else:
        print("  [3] FAIL")

    # 4. attempt_disclosure: overshoot survivorship math.
    d = attempt_disclosure(12, 10, ["overshoot", "no-engage"])
    p4 = d == {"attempts": 12, "captured": 10, "discarded": 2,
               "discarded_reasons": ["overshoot", "no-engage"]}
    print(f"  [4] attempt_disclosure(12,10,[...]) -> {d}")
    if p4:
        passes += 1
        print("  [4] PASS")
    else:
        print("  [4] FAIL")

    print(f"\nSELFTEST {passes}/{total} PASS")
    return passes == total


if __name__ == "__main__":
    import sys

    if "--selftest" in sys.argv:
        sys.exit(0 if _selftest() else 1)
    print(__doc__)
    print("Run with --selftest to exercise all four lints on known-GOOD/known-BAD inputs.")
