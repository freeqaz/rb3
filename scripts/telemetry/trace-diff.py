#!/usr/bin/env python3
"""
trace-diff.py — compare two session-telemetry .jsonl traces and report DIVERGENCE.

This is the replay-verification comparator for the Session Telemetry pipeline
(docs/native/SESSION_TELEMETRY_DESIGN.md §8 + the "M4 (Tier-2)" section). Given
two traces — A (the original RECORDING) and B (a REPLAY of A) — it answers: did
the replay reproduce the SAME run? Two modes:

NAV mode (Tier-1, §8) — the legacy default. A faithful frame-index replay must
reproduce, ordered:
  - the SAME nav path  : (from -> to, wb) screen transitions (EXACT).
  - the SAME inputs    : (f, b, dn, up) input edges (bit-exact; ax informational).
  - the SAME song flow : (ev, id, track, diff) lifecycle (informational note).
We IGNORE sid / t / cs, which legitimately differ between two independent runs.

CHK mode (Tier-2, "M4") — milestone-anchored, tiered tolerance. The replay clock
is milestone-relative (to defeat async-load frame-skew), so we align by MILESTONE
SEGMENTS (nav transitions + song lifecycle), NOT absolute frame, and apply the
M4 tiered tolerance:
  - nav sequence            : EXACT (hard fail on reorder / miss / extra).
  - final score             : EXACT integer (hard fail on any delta).
  - song lifecycle (ev/id)  : EXACT order (hard fail).
  - screen-ready            : within +-K frames (K=8 default) per milestone.
  - clocks / positions      : chk.h fast equality; on hash mismatch, a per-field
                              epsilon fallback classifies benign x86-vs-x86 drift
                              (sec/beat/sm within eps) vs a REAL divergence
                              (score/np/scr/focus differ, or eps exceeded).
  - first true divergence   : the first segment with a HARD-fail OR >= M (default
                              3) consecutive chk epsilon-breaches (a sustained
                              breach, not a one-frame blip).

Exit code is the gate: 0 on match, non-zero on the first hard-fail or sustained
eps breach. MODE: --mode {auto,nav,chk} (default auto — chk when BOTH traces carry
chk rows, else nav).

Forward-compat parsing (mirrors trace-report.py / gen-synth-trace.py): skip
'#'-comment and blank lines, tolerate unknown keys, and a `hdr.v` ahead of the
known version parses best-effort (flagged `version_ahead`). Unknown event KINDS
are also tolerated — e.g. the M4 per-frame `clk` clock sample (the un-decimated
{f,sdt,sm} that frame-locks the fixed-clock replay) is bucketed but NOT gated by
either mode: nav mode compares nav/in/song, chk mode segments on nav/song +
chk, so clk rows are informational (they appear in the per-side event counts).

Usage:
  python3 scripts/telemetry/trace-diff.py A.jsonl B.jsonl
  python3 scripts/telemetry/trace-diff.py --a REC.jsonl --b REPLAY.jsonl [--json]
  python3 scripts/telemetry/trace-diff.py --mode chk REC.jsonl REPLAY.jsonl
  python3 scripts/telemetry/trace-diff.py --selftest    # built-in chk self-test
  gen-synth-trace.py --seed 1 >/tmp/a; gen-synth-trace.py --seed 1 >/tmp/b
  python3 scripts/telemetry/trace-diff.py /tmp/a /tmp/b   # -> MATCH, exit 0
"""
import argparse
import json
import os
import sys

KNOWN_HDR_V = 1

# Tiered-tolerance defaults (M4). K = screen-ready frame window; M = consecutive
# chk eps-breaches that constitute a SUSTAINED (gating) divergence; the per-field
# eps absorb benign x86-vs-x86 float drift (post-quantization on the C++ side).
DEFAULT_K_FRAMES = 8          # screen-ready alignment window (+- frames)
DEFAULT_M_SUSTAINED = 3       # consecutive chk eps-breaches => hard divergence
EPS_SEC = 0.005               # task seconds (5 ms; recorder quantizes to 1 ms)
EPS_BEAT = 0.02               # beats (recorder quantizes to 0.01)
EPS_SM = 2.0                  # song ms (recorder quantizes to 1 ms)


# --------------------------------------------------------------------------- #
# Parse — tolerate '#' comments, blank lines, unknown keys, bad lines. Mirrors
# trace-report.parse_jsonl so the two readers stay byte-for-byte compatible.
# --------------------------------------------------------------------------- #
def parse_jsonl(path):
    rows = []
    fh = sys.stdin if path == "-" else open(path, errors="replace")
    try:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            try:
                rows.append(json.loads(line))
            except Exception:
                pass
    finally:
        if fh is not sys.stdin:
            fh.close()
    return rows


def split_rows(rows):
    """Return (hdr, by_kind) where by_kind maps k -> [rows in file order]."""
    hdr = None
    by = {}
    for r in rows:
        if not isinstance(r, dict):
            continue
        k = r.get("k")
        if k == "hdr":
            if hdr is None:
                hdr = r
            continue
        by.setdefault(k, []).append(r)
    return hdr, by


def hdr_version_ahead(hdr):
    if not hdr:
        return False
    v = hdr.get("v")
    return isinstance(v, int) and v > KNOWN_HDR_V


# --------------------------------------------------------------------------- #
# Projections — reduce each event to ONLY the run-invariant fields we compare.
# Anything not in the projection (sid/t/cs, focus, ax) is deliberately ignored
# for the pass/fail decision; some are surfaced in the human-readable report.
# --------------------------------------------------------------------------- #
def nav_key(r):
    """Ordered nav identity: the transition itself. wb (wentBack) is part of the
    transition (a back-nav A<-B is not the same edge as a forward A->B)."""
    return (r.get("from"), r.get("to"), bool(r.get("wb", False)))


def nav_str(r):
    arrow = "%s -> %s" % (r.get("from", "?"), r.get("to", "?"))
    if r.get("wb"):
        arrow += " (back)"
    return arrow


def in_key(r):
    """Run-invariant input identity: frame index + held bitmask + edge masks.
    IGNORES sid/t/cs (legitimately differ) and ax (analog; reported, not gated).
    Missing dn/up default to 0 (an edge row always carries b)."""
    return (
        r.get("f"),
        int(r.get("b", 0) or 0),
        int(r.get("dn", 0) or 0),
        int(r.get("up", 0) or 0),
    )


def in_str(r):
    return "f=%s b=0x%06x dn=0x%06x up=0x%06x" % (
        r.get("f"),
        int(r.get("b", 0) or 0),
        int(r.get("dn", 0) or 0),
        int(r.get("up", 0) or 0),
    )


def song_key(r):
    return (r.get("ev"), r.get("id"), r.get("track"), r.get("diff"))


def song_str(r):
    s = "%s id=%s track=%s diff=%s" % (
        r.get("ev", "?"), r.get("id", "?"),
        r.get("track", "?"), r.get("diff", "?"),
    )
    extra = []
    if r.get("score") is not None:
        extra.append("score=%s" % r.get("score"))
    if r.get("pct") is not None:
        extra.append("pct=%s" % r.get("pct"))
    if extra:
        s += " (" + ", ".join(extra) + ")"
    return s


# --------------------------------------------------------------------------- #
# Sequence comparison — ordered, report first divergence index + both sides.
# --------------------------------------------------------------------------- #
def compare_seq(a_rows, b_rows, keyfn, strfn):
    """Compare two ordered event lists by keyfn(row). Returns a dict:
       { match, count_a, count_b, first_index, a_at, b_at, reason }.
    first_index is the 0-based index of the first divergence (-1 if matched)."""
    a_keys = [keyfn(r) for r in a_rows]
    b_keys = [keyfn(r) for r in b_rows]
    n = min(len(a_keys), len(b_keys))
    for i in range(n):
        if a_keys[i] != b_keys[i]:
            return {
                "match": False,
                "count_a": len(a_rows),
                "count_b": len(b_rows),
                "first_index": i,
                "a_at": strfn(a_rows[i]),
                "b_at": strfn(b_rows[i]),
                "reason": "transition differs at index %d" % i,
            }
    if len(a_keys) != len(b_keys):
        # Common prefix matched; one side has extra trailing events.
        longer, idx = ("A", len(b_keys)) if len(a_keys) > len(b_keys) else ("B", len(a_keys))
        a_at = strfn(a_rows[idx]) if idx < len(a_rows) else "(none)"
        b_at = strfn(b_rows[idx]) if idx < len(b_rows) else "(none)"
        return {
            "match": False,
            "count_a": len(a_rows),
            "count_b": len(b_rows),
            "first_index": idx,
            "a_at": a_at,
            "b_at": b_at,
            "reason": "length differs (%d vs %d); %s has %d extra after a matching prefix"
            % (len(a_keys), len(b_keys), longer, abs(len(a_keys) - len(b_keys))),
        }
    return {
        "match": True,
        "count_a": len(a_rows),
        "count_b": len(b_rows),
        "first_index": -1,
        "a_at": None,
        "b_at": None,
        "reason": "exact (%d transitions)" % len(a_rows),
    }


def timing_note(a_in, b_in):
    """`t` may legitimately differ between runs (§8). Quantify it as a note —
    never a failure. Reports the max |Δt| over the in events matched by index."""
    deltas = []
    n = min(len(a_in), len(b_in))
    for i in range(n):
        ta, tb = a_in[i].get("t"), b_in[i].get("t")
        if isinstance(ta, (int, float)) and isinstance(tb, (int, float)):
            deltas.append(abs(ta - tb))
    if not deltas:
        return None
    return {"n": len(deltas), "max_abs": max(deltas),
            "mean_abs": sum(deltas) / len(deltas)}


# --------------------------------------------------------------------------- #
# Summary per side (frames captured, last screen, counts per kind).
# --------------------------------------------------------------------------- #
def side_summary(hdr, by):
    fr = by.get("fr", [])
    nav = by.get("nav", [])
    frames = [r.get("f") for r in fr if isinstance(r.get("f"), int)]
    # last screen: prefer the most recent fr.scr, fall back to the last nav.to.
    last_scr = None
    for r in reversed(fr):
        if r.get("scr"):
            last_scr = r.get("scr")
            break
    if last_scr is None and nav:
        last_scr = nav[-1].get("to")
    counts = {k: len(v) for k, v in sorted(by.items())}
    return {
        "sid": (hdr or {}).get("sid"),
        "platform": (hdr or {}).get("platform"),
        "hdr_v": (hdr or {}).get("v"),
        "version_ahead": hdr_version_ahead(hdr),
        "frames_captured": len(fr),
        "max_frame_index": max(frames) if frames else None,
        "last_screen": last_scr,
        "counts": counts,
    }


# --------------------------------------------------------------------------- #
# Report rendering.
# --------------------------------------------------------------------------- #
def render(result, sumA, sumB):
    L = []
    L.append("# trace-diff — A (recorded) vs B (replay)")
    L.append("")
    L.append("## Summary per side")
    L.append("")
    L.append("| field | A (recorded) | B (replay) |")
    L.append("|---|---|---|")
    L.append("| sid | %s | %s |" % (sumA["sid"], sumB["sid"]))
    L.append("| platform | %s | %s |" % (sumA["platform"], sumB["platform"]))
    L.append("| hdr.v | %s%s | %s%s |" % (
        sumA["hdr_v"], " (version_ahead)" if sumA["version_ahead"] else "",
        sumB["hdr_v"], " (version_ahead)" if sumB["version_ahead"] else ""))
    L.append("| frames captured | %s | %s |"
             % (sumA["frames_captured"], sumB["frames_captured"]))
    L.append("| last frame index | %s | %s |"
             % (sumA["max_frame_index"], sumB["max_frame_index"]))
    L.append("| last screen | %s | %s |"
             % (sumA["last_screen"], sumB["last_screen"]))
    allkinds = sorted(set(sumA["counts"]) | set(sumB["counts"]))
    L.append("| event counts | %s | %s |" % (
        ", ".join("%s=%d" % (k, sumA["counts"].get(k, 0)) for k in allkinds) or "—",
        ", ".join("%s=%d" % (k, sumB["counts"].get(k, 0)) for k in allkinds) or "—"))
    L.append("")

    def block(title, res, gating):
        out = ["## %s%s" % (title, " (GATING)" if gating else "")]
        out.append("")
        if res is None:
            out += ["_(both sides empty — nothing to compare)_", ""]
            return out
        if res["match"]:
            out += ["**MATCH** — %s." % res["reason"], ""]
        else:
            out += [
                "**DIVERGENCE** — %s." % res["reason"],
                "",
                "- first divergence index: **%d**" % res["first_index"],
                "- A: `%s`" % res["a_at"],
                "- B: `%s`" % res["b_at"],
                "",
            ]
        return out

    L += block("nav sequence", result["nav"], gating=True)
    L += block("`in` events", result["in"], gating=True)
    L += block("song events", result["song"], gating=False)

    tn = result.get("timing")
    L += ["## Timing (informational — `t` may differ, never fails)", ""]
    if tn:
        L += ["- `in` rows compared: %d; |Δt| max=%.1f ms, mean=%.1f ms "
              "(differs by run; not a failure per §8)."
              % (tn["n"], tn["max_abs"], tn["mean_abs"]), ""]
    else:
        L += ["_(no overlapping `in` timing to compare)_", ""]

    nav_ok = result["nav"] is None or result["nav"]["match"]
    in_ok = result["in"] is None or result["in"]["match"]
    verdict = "MATCH" if (nav_ok and in_ok) else "DIVERGENCE"
    L += ["## Verdict", "",
          "**%s** — nav %s, in %s (gating). song %s (informational)." % (
              verdict,
              "match" if nav_ok else "DIVERGED",
              "match" if in_ok else "DIVERGED",
              "match" if (result["song"] is None or result["song"]["match"])
              else "diverged"),
          ""]
    return "\n".join(L)


# =========================================================================== #
# CHK MODE (Tier-2, M4) — milestone-anchored, tiered-tolerance comparator.
# =========================================================================== #

# Milestone kinds: a nav transition or a song lifecycle event opens a new segment.
# (Inputs/frames/logs are NOT milestones; chk events live INSIDE a segment.)
MILESTONE_KINDS = ("nav", "song")


def milestone_key(r):
    """Run-invariant identity of a milestone (the segment boundary). For nav it is
    the (from,to,wb) transition; for song the (ev,id,track,diff) lifecycle. This
    is the EXACT-match gate — milestones must align 1:1 in order."""
    k = r.get("k")
    if k == "nav":
        return ("nav",) + nav_key(r)
    if k == "song":
        return ("song",) + song_key(r)
    return (k,)


def milestone_str(r):
    k = r.get("k")
    if k == "nav":
        return "nav: " + nav_str(r)
    if k == "song":
        return "song: " + song_str(r)
    return "%s" % k


def chk_int(r, key, default=0):
    v = r.get(key, default)
    try:
        return int(v)
    except Exception:
        return default


def chk_float(r, key):
    v = r.get(key)
    if isinstance(v, (int, float)):
        return float(v)
    return None


def build_segments(rows):
    """Split the time-ordered event stream into milestone SEGMENTS.

    A segment = {boundary: <the opening milestone row or None for the pre-first
    'prologue' segment>, chks: [chk rows until the next milestone]}. chk rows that
    precede the first milestone form segment 0 (boundary=None)."""
    segments = [{"boundary": None, "key": ("__start__",), "chks": []}]
    for r in rows:
        if not isinstance(r, dict):
            continue
        k = r.get("k")
        if k in MILESTONE_KINDS:
            segments.append({"boundary": r, "key": milestone_key(r), "chks": []})
        elif k == "chk":
            segments[-1]["chks"].append(r)
    return segments


def settled_chk(seg):
    """The 'settled' checkpoint of a segment = its LAST chk row (the state once the
    milestone has fully resolved). None if the segment carried no chk."""
    return seg["chks"][-1] if seg["chks"] else None


def compare_settled(ca, cb, k_frames):
    """Tiered comparison of two settled chk rows (recorded vs replay) within an
    aligned segment. Returns (hard_fail, eps_breach, notes[]).
      - hard_fail  : a TIER-1 violation (score/np/scr/focus differ, or screen not
                     ready within +-K frames) — gates immediately.
      - eps_breach : a clock/position drifted past eps but no hard field differs —
                     gates only when SUSTAINED (>= M consecutive)."""
    notes = []
    if ca is None or cb is None:
        # A segment with a chk on only one side is itself a structural divergence
        # of the checkpoint stream (one run reached a state the other did not).
        if ca is None and cb is None:
            return (False, False, notes)
        notes.append("chk present on only one side (%s has it)"
                     % ("A" if ca is not None else "B"))
        return (True, False, notes)

    hard = False
    # --- Tier-1 hard fields: exact ---
    if ca.get("scr") != cb.get("scr"):
        notes.append("scr: A=%r B=%r" % (ca.get("scr"), cb.get("scr")))
        hard = True
    if ca.get("focus") != cb.get("focus"):
        notes.append("focus: A=%r B=%r" % (ca.get("focus"), cb.get("focus")))
        hard = True
    sa, sb = chk_int(ca, "score"), chk_int(cb, "score")
    if sa != sb:
        notes.append("score(exact): A=%d B=%d (delta %d)" % (sa, sb, sb - sa))
        hard = True
    na, nb = chk_int(ca, "np"), chk_int(cb, "np")
    if na != nb:
        notes.append("nPlayers: A=%d B=%d" % (na, nb))
        hard = True
    # --- screen-ready within +-K frames (milestone frame skew, async-load) ---
    fa, fb = ca.get("f"), cb.get("f")
    if isinstance(fa, int) and isinstance(fb, int):
        df = abs(fa - fb)
        if df > k_frames:
            notes.append("screen-ready frame skew %d > K=%d (A.f=%d B.f=%d)"
                         % (df, k_frames, fa, fb))
            hard = True

    # --- clocks/positions: chk.h fast equality, eps fallback on mismatch ---
    eps_breach = False
    if ca.get("h") is not None and ca.get("h") == cb.get("h"):
        # Hash equal => the whole quantized tuple matched; no clock check needed.
        return (hard, False, notes)
    # Hash differs (or absent) — classify the float fields by eps.
    for key, eps, label in (("sec", EPS_SEC, "taskSec"),
                            ("beat", EPS_BEAT, "beat"),
                            ("sm", EPS_SM, "songMs")):
        va, vb = chk_float(ca, key), chk_float(cb, key)
        if va is None and vb is None:
            continue
        if (va is None) != (vb is None):
            # sm present on one side only = menu/song boundary mismatch (a real
            # divergence, but only the sm field — treat as an eps breach unless a
            # hard field already failed).
            notes.append("%s present on one side only (A=%r B=%r)"
                         % (label, va, vb))
            eps_breach = True
            continue
        if abs(va - vb) > eps:
            notes.append("%s eps-breach: A=%.3f B=%.3f (|delta|=%.3f > %.3f)"
                         % (label, va, vb, abs(va - vb), eps))
            eps_breach = True
    if not eps_breach and not hard:
        # Hash differed but every classified field is within eps — benign drift in
        # an UNHASHED field (or quantization boundary). Note it, do not gate.
        notes.append("chk.h differs but all fields within eps (benign drift)")
    return (hard, eps_breach, notes)


def compare_chk(rows_a, rows_b, k_frames, m_sustained):
    """Milestone-anchored chk comparison. Returns a result dict with the verdict +
    the first divergence (segment index, milestone, field notes)."""
    segs_a = build_segments(rows_a)
    segs_b = build_segments(rows_b)

    keys_a = [s["key"] for s in segs_a]
    keys_b = [s["key"] for s in segs_b]

    # --- Milestone alignment is the EXACT gate (nav/song sequence). ---
    n = min(len(keys_a), len(keys_b))
    for i in range(n):
        if keys_a[i] != keys_b[i]:
            return {
                "match": False, "kind": "milestone",
                "first_segment": i,
                "a_at": milestone_str(segs_a[i]["boundary"]) if segs_a[i]["boundary"] else "(start)",
                "b_at": milestone_str(segs_b[i]["boundary"]) if segs_b[i]["boundary"] else "(start)",
                "notes": ["milestone sequence diverges at segment %d" % i],
                "n_segments_a": len(segs_a), "n_segments_b": len(segs_b),
                "checkpoints_a": sum(len(s["chks"]) for s in segs_a),
                "checkpoints_b": sum(len(s["chks"]) for s in segs_b),
            }
    if len(keys_a) != len(keys_b):
        idx = n
        longer = "A" if len(keys_a) > len(keys_b) else "B"
        which = segs_a if longer == "A" else segs_b
        b = which[idx]["boundary"] if idx < len(which) else None
        return {
            "match": False, "kind": "milestone",
            "first_segment": idx,
            "a_at": milestone_str(segs_a[idx]["boundary"]) if (idx < len(segs_a) and segs_a[idx]["boundary"]) else "(none)",
            "b_at": milestone_str(segs_b[idx]["boundary"]) if (idx < len(segs_b) and segs_b[idx]["boundary"]) else "(none)",
            "notes": ["milestone count differs (%d vs %d); %s has %d extra after a matching prefix"
                      % (len(keys_a), len(keys_b), longer, abs(len(keys_a) - len(keys_b)))],
            "n_segments_a": len(segs_a), "n_segments_b": len(segs_b),
            "checkpoints_a": sum(len(s["chks"]) for s in segs_a),
            "checkpoints_b": sum(len(s["chks"]) for s in segs_b),
        }

    # --- Within each aligned segment, compare the settled chk + the in-segment
    #     chk run for a SUSTAINED eps breach (>= M consecutive). ---
    consecutive_eps = 0
    for i in range(len(segs_a)):
        sa, sb = segs_a[i], segs_b[i]
        # Settled-state (tier-1 hard fields + screen-ready) check.
        hard, eps_breach, notes = compare_settled(
            settled_chk(sa), settled_chk(sb), k_frames)
        if hard:
            return {
                "match": False, "kind": "settled",
                "first_segment": i,
                "a_at": milestone_str(sa["boundary"]) if sa["boundary"] else "(start)",
                "b_at": milestone_str(sb["boundary"]) if sb["boundary"] else "(start)",
                "notes": notes,
                "n_segments_a": len(segs_a), "n_segments_b": len(segs_b),
                "checkpoints_a": sum(len(s["chks"]) for s in segs_a),
                "checkpoints_b": sum(len(s["chks"]) for s in segs_b),
            }
        # Sustained eps-breach detection over the in-segment chk run (pairwise by
        # index; the runs are decimated the same way on both sides).
        m = min(len(sa["chks"]), len(sb["chks"]))
        for j in range(m):
            _h, eb, _n = compare_settled(sa["chks"][j], sb["chks"][j], k_frames)
            if eb:
                consecutive_eps += 1
                if consecutive_eps >= m_sustained:
                    return {
                        "match": False, "kind": "sustained_eps",
                        "first_segment": i,
                        "a_at": milestone_str(sa["boundary"]) if sa["boundary"] else "(start)",
                        "b_at": milestone_str(sb["boundary"]) if sb["boundary"] else "(start)",
                        "notes": ["%d consecutive chk eps-breaches (>= M=%d) at segment %d, chk %d"
                                  % (consecutive_eps, m_sustained, i, j)] + _n,
                        "n_segments_a": len(segs_a), "n_segments_b": len(segs_b),
                        "checkpoints_a": sum(len(s["chks"]) for s in segs_a),
                        "checkpoints_b": sum(len(s["chks"]) for s in segs_b),
                    }
            else:
                consecutive_eps = 0

    return {
        "match": True, "kind": "chk",
        "first_segment": -1, "a_at": None, "b_at": None,
        "notes": ["%d milestones aligned exactly; settled state + clocks within tolerance"
                  % len(segs_a)],
        "n_segments_a": len(segs_a), "n_segments_b": len(segs_b),
        "checkpoints_a": sum(len(s["chks"]) for s in segs_a),
        "checkpoints_b": sum(len(s["chks"]) for s in segs_b),
    }


def render_chk(result, sumA, sumB):
    L = []
    L.append("# trace-diff (CHK / milestone mode) — A (recorded) vs B (replay)")
    L.append("")
    L.append("## Summary per side")
    L.append("")
    L.append("| field | A (recorded) | B (replay) |")
    L.append("|---|---|---|")
    L.append("| sid | %s | %s |" % (sumA["sid"], sumB["sid"]))
    L.append("| platform | %s | %s |" % (sumA["platform"], sumB["platform"]))
    L.append("| milestones | %s | %s |"
             % (result["n_segments_a"] - 1, result["n_segments_b"] - 1))
    L.append("| checkpoints | %s | %s |"
             % (result["checkpoints_a"], result["checkpoints_b"]))
    L.append("| last screen | %s | %s |" % (sumA["last_screen"], sumB["last_screen"]))
    L.append("")
    if result["match"]:
        L += ["## Verdict", "", "**MATCH** — %s." % result["notes"][0], ""]
    else:
        kind = {"milestone": "MILESTONE SEQUENCE", "settled": "SETTLED STATE",
                "sustained_eps": "SUSTAINED CLOCK DRIFT"}.get(result["kind"], result["kind"])
        L += ["## Verdict", "",
              "**DIVERGENCE (%s)** — first at segment **%d**." % (kind, result["first_segment"]),
              "",
              "- A milestone: `%s`" % result["a_at"],
              "- B milestone: `%s`" % result["b_at"],
              ""]
        for nt in result["notes"]:
            L.append("- %s" % nt)
        L.append("")
    return "\n".join(L)


def diff_traces_chk(rows_a, rows_b, k_frames, m_sustained):
    hdrA, byA = split_rows(rows_a)
    hdrB, byB = split_rows(rows_b)
    res = compare_chk(rows_a, rows_b, k_frames, m_sustained)
    sumA = side_summary(hdrA, byA)
    sumB = side_summary(hdrB, byB)
    res["gate_pass"] = bool(res["match"])
    return res, sumA, sumB


def has_chk(rows):
    for r in rows:
        if isinstance(r, dict) and r.get("k") == "chk":
            return True
    return False


def diff_traces(rows_a, rows_b):
    hdrA, byA = split_rows(rows_a)
    hdrB, byB = split_rows(rows_b)

    a_nav, b_nav = byA.get("nav", []), byB.get("nav", [])
    a_in, b_in = byA.get("in", []), byB.get("in", [])
    a_song, b_song = byA.get("song", []), byB.get("song", [])

    nav_res = compare_seq(a_nav, b_nav, nav_key, nav_str) if (a_nav or b_nav) else None
    in_res = compare_seq(a_in, b_in, in_key, in_str) if (a_in or b_in) else None
    song_res = compare_seq(a_song, b_song, song_key, song_str) if (a_song or b_song) else None

    result = {
        "nav": nav_res,
        "in": in_res,
        "song": song_res,
        "timing": timing_note(a_in, b_in),
    }
    sumA = side_summary(hdrA, byA)
    sumB = side_summary(hdrB, byB)
    nav_ok = nav_res is None or nav_res["match"]
    in_ok = in_res is None or in_res["match"]
    result["gate_pass"] = bool(nav_ok and in_ok)
    return result, sumA, sumB


# =========================================================================== #
# Built-in self-test (chk mode) — two synthetic traces:
#   (1) identical          -> MATCH (gate_pass True)
#   (2) perturbed score    -> DIVERGENCE (settled-state hard fail)
#   (3) frame skew <= K     -> MATCH (screen-ready tolerance)
#   (4) frame skew  > K     -> DIVERGENCE (screen-ready hard fail)
#   (5) tiny clock drift    -> MATCH (within eps; one blip, not sustained)
# =========================================================================== #
def _synth_chk_trace(sid, *, score_at_end=1500, end_frame=300, sec_drift=0.0):
    """Build a minimal but schema-faithful chk-bearing trace: hdr, a nav into the
    game, periodic in-song chk rows, the final-score chk, and the song-end
    milestone. `score_at_end` / `end_frame` / `sec_drift` let the self-test
    perturb exactly one dimension."""
    rows = [{"k": "hdr", "v": 1, "sid": sid, "cs": 0, "platform": "native",
             "started": "2026-06-09T00:00:00Z", "build": {"git": "selftest"},
             "flags": {}}]
    cs = 1

    def add(d):
        nonlocal cs
        d["cs"] = cs
        cs += 1
        rows.append(d)

    # Milestone: nav menu -> game.
    add({"t": 100.0, "f": 10, "k": "nav", "from": "main_hub", "to": "game",
         "focus": "", "wb": False})
    add({"t": 101.0, "f": 10, "k": "chk", "h": "1111111111111111", "scr": "game",
         "focus": "", "sec": 0.0 + sec_drift, "beat": 0.0, "score": 0,
         "scores": [0, 0], "np": 2, "pct": 0})
    # Periodic in-song chk run (score ramps deterministically).
    f = 60
    sm = 1000.0
    sc = 0
    while f < end_frame:
        sc += 100
        add({"t": float(f * 16), "f": f, "sm": sm, "k": "chk",
             "h": "%016x" % (0x2000 + f), "scr": "game", "focus": "",
             "sec": (sm / 1000.0) + sec_drift, "beat": sm / 500.0, "score": sc,
             "scores": [sc, 0], "np": 2, "pct": min(99, f // 3)})
        f += 30
        sm += 480.0
    # Final-score chk (the EXACT-match tier) at the song end.
    add({"t": float(end_frame * 16), "f": end_frame, "sm": sm, "k": "chk",
         "h": "f00df00df00df00d" if score_at_end == 1500 else "dead0000dead0000",
         "scr": "game", "focus": "", "sec": (sm / 1000.0) + sec_drift,
         "beat": sm / 500.0, "score": score_at_end, "scores": [score_at_end, 0],
         "np": 2, "pct": 100})
    # Milestone: song end.
    add({"t": float(end_frame * 16 + 1), "f": end_frame, "sm": sm, "k": "song",
         "ev": "end", "id": "song_abc", "track": "guitar", "diff": "expert",
         "score": float(score_at_end), "pct": 1.0})
    return rows


def selftest():
    import io
    fails = []

    def run(name, rows_a, rows_b, expect_pass, k=DEFAULT_K_FRAMES, m=DEFAULT_M_SUSTAINED):
        res, _sa, _sb = diff_traces_chk(rows_a, rows_b, k, m)
        ok = (res["gate_pass"] == expect_pass)
        verdict = "MATCH" if res["gate_pass"] else "DIVERGENCE(%s)" % res.get("kind")
        print("  [%s] %-26s -> %s (expected %s)%s"
              % ("PASS" if ok else "FAIL", name, verdict,
                 "MATCH" if expect_pass else "DIVERGENCE",
                 "" if ok else "  <-- notes: " + "; ".join(res.get("notes", []))))
        if not ok:
            fails.append(name)

    print("trace-diff chk-mode self-test:")
    a = _synth_chk_trace("aaaaaaaaaaaaaaaa")
    # (1) identical (different sid only) -> MATCH.
    b_same = _synth_chk_trace("bbbbbbbbbbbbbbbb")
    run("identical", a, b_same, True)
    # (2) perturbed FINAL SCORE -> hard DIVERGENCE.
    b_score = _synth_chk_trace("cccccccccccccccc", score_at_end=1501)
    run("perturbed_score", a, b_score, False)
    # (3) screen-ready frame skew within K (8) -> MATCH.
    b_skew_ok = _synth_chk_trace("dddddddddddddddd", end_frame=304)  # 300 vs 304 = 4 <= 8
    run("frame_skew_within_K", a, b_skew_ok, True)
    # (4) screen-ready frame skew beyond K -> DIVERGENCE.
    b_skew_bad = _synth_chk_trace("eeeeeeeeeeeeeeee", end_frame=330)  # 30 > 8
    run("frame_skew_beyond_K", a, b_skew_bad, False)
    # (5) tiny clock drift within eps -> MATCH (hash differs but eps absorbs it).
    b_drift = _synth_chk_trace("ffffffffffffffff", sec_drift=0.002)  # 2 ms < EPS_SEC*1000
    run("clock_drift_within_eps", a, b_drift, True)

    # NAV-mode smoke: the same chk traces should pass NAV mode too (nav+song
    # milestones align, no `in` rows) — proves chk additions didn't break Tier-1.
    nav_res, _na, _nb = diff_traces(a, b_same)
    nav_ok = nav_res["gate_pass"]
    print("  [%s] %-26s -> %s (expected MATCH)"
          % ("PASS" if nav_ok else "FAIL", "nav_mode_backcompat",
             "MATCH" if nav_ok else "DIVERGENCE"))
    if not nav_ok:
        fails.append("nav_mode_backcompat")

    if fails:
        print("SELFTEST FAILED: %d/%d cases: %s" % (len(fails), 6, ", ".join(fails)))
        return 1
    print("SELFTEST OK: 6/6 cases.")
    return 0


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("a", nargs="?", default=None,
                    help="trace A — the original RECORDING (.jsonl, or '-' for stdin)")
    ap.add_argument("b", nargs="?", default=None,
                    help="trace B — the REPLAY to verify against A (.jsonl)")
    ap.add_argument("--a", dest="a_opt", default=None,
                    help="trace A (alias for positional)")
    ap.add_argument("--b", dest="b_opt", default=None,
                    help="trace B (alias for positional)")
    ap.add_argument("--json", action="store_true",
                    help="emit a machine-readable JSON result instead of markdown")
    ap.add_argument("--mode", choices=["auto", "nav", "chk"], default="auto",
                    help="comparison mode: nav (Tier-1 nav+in), chk (Tier-2 "
                         "milestone/tolerance), or auto (chk when BOTH traces "
                         "carry chk rows, else nav). Default: auto.")
    ap.add_argument("--k-frames", type=int, default=DEFAULT_K_FRAMES,
                    help="chk mode: screen-ready alignment window in frames "
                         "(default %d)" % DEFAULT_K_FRAMES)
    ap.add_argument("--m-sustained", type=int, default=DEFAULT_M_SUSTAINED,
                    help="chk mode: consecutive chk eps-breaches that constitute "
                         "a SUSTAINED (gating) divergence (default %d)"
                         % DEFAULT_M_SUSTAINED)
    ap.add_argument("--selftest", action="store_true",
                    help="run the built-in chk-mode self-test (two synthetic "
                         "traces: identical => match exit 0; perturbed score => "
                         "divergence exit 1) and exit")
    args = ap.parse_args()

    if args.selftest:
        return selftest()

    pa = args.a_opt or args.a
    pb = args.b_opt or args.b
    if not pa or not pb:
        ap.error("provide two traces: A (recorded) and B (replay)")
    for label, p in (("A", pa), ("B", pb)):
        if p != "-" and not os.path.exists(p):
            print("ERROR: no such file (%s): %s" % (label, p), file=sys.stderr)
            return 2
    if pa == "-" and pb == "-":
        ap.error("only one of A/B may read from stdin")

    rows_a = parse_jsonl(pa)
    rows_b = parse_jsonl(pb)
    if not rows_a:
        print("ERROR: no parseable JSONL rows in A: %s" % pa, file=sys.stderr)
        return 2
    if not rows_b:
        print("ERROR: no parseable JSONL rows in B: %s" % pb, file=sys.stderr)
        return 2

    # Resolve the mode: auto => chk when BOTH traces carry chk rows, else nav.
    mode = args.mode
    if mode == "auto":
        mode = "chk" if (has_chk(rows_a) and has_chk(rows_b)) else "nav"

    if mode == "chk":
        result, sumA, sumB = diff_traces_chk(
            rows_a, rows_b, args.k_frames, args.m_sustained)
        if args.json:
            blob = {"mode": "chk", "gate_pass": result["gate_pass"],
                    "result": result, "summary_a": sumA, "summary_b": sumB}
            sys.stdout.write(json.dumps(blob, indent=2, sort_keys=False) + "\n")
        else:
            sys.stdout.write(render_chk(result, sumA, sumB) + "\n")
        return 0 if result["gate_pass"] else 1

    # NAV mode (Tier-1).
    result, sumA, sumB = diff_traces(rows_a, rows_b)
    if args.json:
        blob = {
            "mode": "nav",
            "gate_pass": result["gate_pass"],
            "nav": result["nav"],
            "in": result["in"],
            "song": result["song"],
            "timing": result["timing"],
            "summary_a": sumA,
            "summary_b": sumB,
        }
        sys.stdout.write(json.dumps(blob, indent=2, sort_keys=False))
        sys.stdout.write("\n")
    else:
        sys.stdout.write(render(result, sumA, sumB))
        sys.stdout.write("\n")

    # Gate: 0 if nav + in match, 1 if diverged.
    return 0 if result["gate_pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
