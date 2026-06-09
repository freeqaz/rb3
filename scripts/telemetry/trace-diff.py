#!/usr/bin/env python3
"""
trace-diff.py — compare two session-telemetry .jsonl traces and report DIVERGENCE.

This is the Tier-1 replay-verification comparator for the Session Telemetry
pipeline (docs/native/SESSION_TELEMETRY_DESIGN.md §8). Given two traces —
A (the original RECORDING) and B (a REPLAY of A) — it answers the Tier-1
acceptance question: did the replay reproduce the SAME run?

Per §8 ("Tier-1 verification"), a faithful frame-index replay must reproduce:
  - the SAME nav path  : ordered (from -> to) screen transitions.
  - the SAME inputs     : ordered (f, b, dn, up) input edges. We compare ONLY the
                          replay-relevant fields — frame index `f`, full held
                          bitmask `b`, and the down/up edge masks — and IGNORE
                          sid / t (timing) / cs (client_seq), which LEGITIMATELY
                          differ between two independent runs (a replay mints its
                          own session id, wall clock, and per-line sequence).
  - the SAME song flow  : ordered (ev, id, track, diff) song-lifecycle events,
                          plus a note on score/pct of the matched `end` rows.

Exit code is the gate: 0 when nav AND in match (within tolerance), 1 when either
diverges. Tolerance (from the task contract):
  - nav  : EXACT (from/to/wb must match in order).
  - in   : bit-exact on (f, b, dn, up); ax is reported but does NOT fail.
  - song : compared + reported, but (like timing) does not by itself flip the
           exit code — nav+in are the gate. score/pct deltas are noted.
  - t (timing) MAY differ between runs — noted, never a failure.

Forward-compat parsing (mirrors trace-report.py / gen-synth-trace.py): skip
'#'-comment and blank lines, tolerate unknown keys, and a `hdr.v` ahead of the
known version parses best-effort (flagged `version_ahead`).

Usage:
  python3 scripts/telemetry/trace-diff.py A.jsonl B.jsonl
  python3 scripts/telemetry/trace-diff.py --a REC.jsonl --b REPLAY.jsonl [--json]
  gen-synth-trace.py --seed 1 >/tmp/a; gen-synth-trace.py --seed 1 >/tmp/b
  python3 scripts/telemetry/trace-diff.py /tmp/a /tmp/b   # -> MATCH, exit 0
"""
import argparse
import json
import os
import sys

KNOWN_HDR_V = 1


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
    args = ap.parse_args()

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

    result, sumA, sumB = diff_traces(rows_a, rows_b)

    if args.json:
        blob = {
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
