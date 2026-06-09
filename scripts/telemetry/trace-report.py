#!/usr/bin/env python3
"""
trace-report.py — render a markdown report from a session-telemetry .jsonl.

This is the D7 report generator for the Session Telemetry pipeline
(docs/native/SESSION_TELEMETRY_DESIGN.md §9). It is a PURE READER: it never
launches the engine. It reads a `.jsonl` session (the unified multi-kind stream;
SQLite fetch-back is a later add) and emits a markdown report covering:

  - Boot timeline      : per-phase durations + total (from `boot` events).
  - Frame health       : FPS, frame-time p50/p95/p99, a text histogram, and a
                         long-frame list ATTRIBUTED to lp / lpu / draw(=dt-lp-lpu).
                         DECIMATION-AWARE: `fr` rows are sampled (§4.7), so FPS is
                         derived from the captured dt sum / span, NOT naively as
                         (#rows / wall-span). The report states whether the stream
                         looks fully sampled or decimated.
  - Input              : timeline + APM + per-screen dwell. `b`/`dn`/`up` bitmasks
                         are decoded via a bit->name dict GENERATED from Joypad.h
                         (the kPad_* JoypadButton enum), not blind-hardcoded.
  - Session summary    : songs (song events), audio underruns (au), warns/asserts
                         (log), and sid/build/asset_version (hdr).

It extends the proven renderer in scripts/native/frame_profiler.py (pct/histogram/
worst-N/per-screen/spike-attribution) rather than forking it — same math, applied
to the richer session stream. Unknown keys are tolerated and a hdr.v ahead of the
known version is parsed best-effort and flagged `version_ahead` (§4.1).

Usage:
  python3 scripts/telemetry/trace-report.py SESSION.jsonl
  python3 scripts/telemetry/trace-report.py --jsonl SESSION.jsonl [--worst N]
  gen-synth-trace.py | python3 scripts/telemetry/trace-report.py -
"""
import argparse, json, os, re, sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
JOYPAD_H = os.path.join(REPO, "src", "system", "os", "Joypad.h")

KNOWN_HDR_V = 1


# --------------------------------------------------------------------------- #
# JoypadButton bit->name table — GENERATED from Joypad.h (§ OQ17). We parse the
# `enum JoypadButton { kPad_* = N, ... }` block so the report's button names track
# the C++ enum and never silently drift. Analog axes (sticks/triggers/sensors)
# are reported separately via the `ax` object, not these digital bits.
# --------------------------------------------------------------------------- #
def load_button_names(joypad_h=JOYPAD_H):
    """Return {bit_index: short_name} for the digital JoypadButton bits
    (kPad_* < kPad_NumButtons), parsed from Joypad.h. Falls back to a built-in
    table if the header can't be read (the report still runs off a raw trace)."""
    names = {}
    try:
        with open(joypad_h, errors="replace") as fh:
            src = fh.read()
    except OSError:
        return _builtin_button_names()

    # Isolate the `enum JoypadButton { ... }` body so we don't pick up the Xbox
    # alias values (kPad_Xbox_*) that re-use the same bit indices.
    m = re.search(r"enum\s+JoypadButton\s*\{(.*?)\}", src, re.S)
    body = m.group(1) if m else src
    num = None
    for em in re.finditer(r"kPad_(\w+)\s*=\s*(\d+)", body):
        label, val = em.group(1), int(em.group(2))
        if label == "NumButtons":
            num = val
            continue
        if label.startswith("Xbox_"):
            continue            # alias indices — skip, keep the canonical names
        # First name wins for a given bit (canonical kPad_* are listed first).
        names.setdefault(val, label)
    if num is not None:
        names = {b: n for b, n in names.items() if b < num}
    return names or _builtin_button_names()


def _builtin_button_names():
    return {
        0: "L2", 1: "R2", 2: "L1", 3: "R1", 4: "Tri", 5: "Circle", 6: "X",
        7: "Square", 8: "Select", 9: "L3", 10: "R3", 11: "Start", 12: "DUp",
        13: "DRight", 14: "DDown", 15: "DLeft", 16: "LStickUp",
        17: "LStickRight", 18: "LStickDown", 19: "LStickLeft", 20: "RStickUp",
        21: "RStickRight", 22: "RStickDown", 23: "RStickLeft",
    }


def decode_mask(mask, names):
    """List of button short-names set in `mask` (bit i -> names[i])."""
    out = []
    if not mask:
        return out
    for bit in range(32):
        if mask & (1 << bit):
            out.append(names.get(bit, "bit%d" % bit))
    return out


# --------------------------------------------------------------------------- #
# Parse — tolerate '#' comments, blank lines, unknown keys, bad lines.
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


def bucket_by_kind(rows):
    by = {}
    hdr = None
    for r in rows:
        k = r.get("k")
        if k == "hdr":
            hdr = r
            continue
        by.setdefault(k, []).append(r)
    return hdr, by


# --------------------------------------------------------------------------- #
# Small stats helpers (mirror frame_profiler.pct).
# --------------------------------------------------------------------------- #
def pct(sorted_vals, q):
    if not sorted_vals:
        return 0.0
    i = min(len(sorted_vals) - 1, int(round((q / 100.0) * (len(sorted_vals) - 1))))
    return sorted_vals[i]


def fmt_ms(x):
    return "%.1f" % x


def fmt_dur(ms):
    if ms is None:
        return "?"
    if ms < 1000:
        return "%.0f ms" % ms
    return "%.2f s" % (ms / 1000.0)


# --------------------------------------------------------------------------- #
# Report sections — each returns a list[str] of markdown lines.
# --------------------------------------------------------------------------- #
def section_header(hdr, all_rows):
    L = ["# Session Telemetry Report", ""]
    if not hdr:
        L += ["_(no `hdr` line found — reporting off the raw event stream)_", ""]
        return L
    v = hdr.get("v")
    if isinstance(v, int) and v > KNOWN_HDR_V:
        L.append("> **version_ahead**: trace `hdr.v=%d` > known %d — parsed "
                 "best-effort." % (v, KNOWN_HDR_V))
        L.append("")
    build = hdr.get("build") or {}
    vp = hdr.get("viewport") or []
    L += [
        "| field | value |",
        "|---|---|",
        "| sid | `%s` |" % hdr.get("sid", "?"),
        "| platform | %s |" % hdr.get("platform", "?"),
        "| started | %s |" % hdr.get("started", "?"),
        "| schema v | %s |" % v,
        "| build.git | `%s` |" % build.get("git", "?"),
        "| build.wasm_sha | `%s` |" % build.get("wasm_sha", "?"),
        "| asset_version | %s |" % hdr.get("asset_version", "?"),
        "| ua | %s |" % hdr.get("ua", "?"),
        "| viewport | %s |" % ("x".join(str(x) for x in vp) if vp else "?"),
        "| flags | %s |" % json.dumps(hdr.get("flags", {})),
        "| events | %d |" % len(all_rows),
    ]
    L.append("")
    return L


def section_boot(boot_rows):
    L = ["## Boot timeline", ""]
    if not boot_rows:
        L += ["_(no `boot` events)_", ""]
        return L
    ph = [(r.get("ph", "?"), r.get("t")) for r in boot_rows
          if r.get("t") is not None]
    ph.sort(key=lambda x: x[1])
    L += ["| phase | at (ms) | Δ from prev | bar |",
          "|---|---:|---:|---|"]
    prev = None
    durs = []
    for name, tms in ph:
        d = (tms - prev) if prev is not None else 0.0
        durs.append((name, d))
        prev = tms
    maxd = max((d for _, d in durs), default=1.0) or 1.0
    prev = None
    for (name, tms), (_, d) in zip(ph, durs):
        bar = "#" * int(round(40 * d / maxd)) if d else ""
        dtxt = "+%.0f" % d if prev is not None else "—"
        L.append("| %s | %.0f | %s | %s |" % (name, tms, dtxt, bar))
        prev = tms
    if ph:
        total = ph[-1][1] - ph[0][1]
        L += ["", "**Total boot**: %s (`%s` → `%s`)."
              % (fmt_dur(total), ph[0][0], ph[-1][0]), ""]
    return L


def _decimation_note(fr_rows):
    """Detect whether `fr` rows look decimated (sampled) vs full per-frame.
    Heuristic: compare captured row count to the frame-index span. A near-1:1
    ratio => full; a small fraction => decimated."""
    if not fr_rows:
        return None, "no frame rows"
    fs = [r.get("f") for r in fr_rows if isinstance(r.get("f"), int)]
    if len(fs) < 2:
        return None, "too few frames"
    span = max(fs) - min(fs) + 1
    captured = len(fr_rows)
    ratio = captured / span if span else 1.0
    if ratio >= 0.9:
        return False, ("full (≈1 row/frame: %d rows over %d-frame span)"
                       % (captured, span))
    return True, ("decimated/sampled (%d rows over a %d-frame span ≈ %.1f%% of "
                  "frames captured; FPS derived from captured dt, not row count)"
                  % (captured, span, 100.0 * ratio))


def section_frames(fr_rows):
    L = ["## Frame health", ""]
    if not fr_rows:
        L += ["_(no `fr` events)_", ""]
        return L

    decimated, note = _decimation_note(fr_rows)
    L += ["_Sampling: %s_" % note, ""]

    dts = sorted(r["dt"] for r in fr_rows if isinstance(r.get("dt"), (int, float)))
    n = len(dts)
    sumdt = sum(dts)
    # DECIMATION-AWARE FPS: average frame is sumdt/n ms; fps = 1000 / mean_dt.
    # This is correct whether rows are full or sampled (each captured row is a
    # real frame's dt). We do NOT use rows/wall-span, which a sampled stream
    # would deflate.
    mean = sumdt / n if n else 0.0
    fps = (1000.0 / mean) if mean > 0 else 0.0
    L += [
        "- **FPS (from sampled frame dt):** %.1f  (mean frame %.2f ms)" % (fps, mean),
        "- **frame ms:** p50=%.2f  p95=%.2f  p99=%.2f  max=%.2f"
        % (pct(dts, 50), pct(dts, 95), pct(dts, 99), dts[-1] if dts else 0.0),
        "",
        "### Histogram (frame dt ms)",
        "```",
    ]
    buckets = [(0, 8), (8, 16), (16, 33), (33, 50), (50, 100),
               (100, 250), (250, 500), (500, 1e12)]
    for lo, hi in buckets:
        c = sum(1 for d in dts if lo <= d < hi)
        hilabel = "inf" if hi > 1e11 else str(int(hi))
        bar = "#" * min(60, int(60 * c / n)) if n else ""
        L.append("  [%4d-%4s) ms : %6d  %s" % (lo, hilabel, c, bar))
    L.append("```")
    L.append("")

    # Long-frame list ATTRIBUTED to lp / lpu / draw(=dt-lp-lpu).
    long_thresh = max(20.0, pct(dts, 95))
    longs = sorted((r for r in fr_rows
                    if isinstance(r.get("dt"), (int, float)) and r["dt"] >= long_thresh),
                   key=lambda r: -r["dt"])
    L += ["### Long frames (dt ≥ %.0f ms) — attributed to lp / lpu / draw"
          % long_thresh, ""]
    if not longs:
        L += ["_(none above threshold)_", ""]
    else:
        L += ["| frame | dt | lp(bg) | lpu(sync) | draw=dt-lp-lpu | scr | ld | st | pend |",
              "|---:|---:|---:|---:|---:|---|---:|---:|---:|"]
        for r in longs[:20]:
            lp = float(r.get("lp", 0) or 0)
            lpu = float(r.get("lpu", 0) or 0)
            draw = r["dt"] - lp - lpu
            L.append("| %d | %.1f | %.1f | %.1f | %.1f | %s | %d | %d | %d |"
                     % (r.get("f", -1), r["dt"], lp, lpu, max(0.0, draw),
                        r.get("scr", "?"), int(r.get("ld", 0) or 0),
                        int(r.get("st", 0) or 0), int(r.get("pend", 0) or 0)))
        # aggregate attribution over the long tail
        sumlong = sum(r["dt"] for r in longs)
        sumlp = sum(float(r.get("lp", 0) or 0) for r in longs)
        sumlpu = sum(float(r.get("lpu", 0) or 0) for r in longs)
        with_asset = sum(1 for r in longs
                         if r.get("ld") or r.get("st") or float(r.get("lp", 0) or 0) > 1
                         or float(r.get("lpu", 0) or 0) > 1 or r.get("pend"))
        L += ["",
              "**Long-tail attribution** (%d long frames, %.0f ms total): "
              "%.0f ms LoadMgr.Poll (bg) · %.0f ms PollUntil* (sync drain) · "
              "%.0f ms draw/poll/gpu. %d/%d carry an asset signal (ld/st/pend/"
              "loader-ms)."
              % (len(longs), sumlong, sumlp, sumlpu,
                 sumlong - sumlp - sumlpu, with_asset, len(longs)),
              ""]

    # Per-screen percentiles (keyed on scr, like frame_profiler :163-171).
    by_screen = {}
    for r in fr_rows:
        if isinstance(r.get("dt"), (int, float)):
            by_screen.setdefault(r.get("scr", "?"), []).append(r["dt"])
    if by_screen:
        L += ["### Per-screen frame ms", "",
              "| screen | n | p50 | p95 | p99 | max |",
              "|---|---:|---:|---:|---:|---:|"]
        for scr, vals in sorted(by_screen.items(), key=lambda kv: -max(kv[1])):
            vs = sorted(vals)
            L.append("| %s | %d | %.1f | %.1f | %.1f | %.1f |"
                     % (scr, len(vs), pct(vs, 50), pct(vs, 95),
                        pct(vs, 99), vs[-1]))
        L.append("")
    return L


def section_input(in_rows, nav_rows, names):
    L = ["## Input", ""]
    if not in_rows:
        L += ["_(no `in` events)_", ""]
    else:
        # APM = down edges / span-minutes.
        ts = [r["t"] for r in in_rows if isinstance(r.get("t"), (int, float))]
        span_ms = (max(ts) - min(ts)) if len(ts) >= 2 else 0.0
        span_min = span_ms / 60000.0 if span_ms else 0.0
        # popcount(dn) per row = number of buttons newly pressed this edge.
        down_edges = 0
        per_btn = {}
        axis_seen = {}
        for r in in_rows:
            dn = int(r.get("dn", 0) or 0)
            for b in decode_mask(dn, names):
                per_btn[b] = per_btn.get(b, 0) + 1
                down_edges += 1
            ax = r.get("ax") or {}
            for axk, axv in ax.items():
                a = axis_seen.setdefault(axk, {"n": 0, "max": 0})
                a["n"] += 1
                a["max"] = max(a["max"], abs(axv))
        apm = (down_edges / span_min) if span_min > 0 else 0.0
        L += [
            "- **edges:** %d input rows, %d button-down edges" % (len(in_rows), down_edges),
            "- **APM:** %.1f actions/min (over %s)" % (apm, fmt_dur(span_ms)),
            "",
            "### Per-button presses (decoded from Joypad.h)", "",
            "| button | down edges |", "|---|---:|",
        ]
        for b, c in sorted(per_btn.items(), key=lambda kv: -kv[1]):
            L.append("| %s | %d |" % (b, c))
        if not per_btn:
            L.append("| _(none)_ | 0 |")
        L.append("")
        if axis_seen:
            L += ["### Analog axes (sparse ax{}; values ×1000)", "",
                  "| axis | rows | peak(|v|×1000) |", "|---|---:|---:|"]
            axnames = {"wh": "whammy (wh)", "ti": "tilt (ti)"}
            for axk, a in sorted(axis_seen.items()):
                L.append("| %s | %d | %d |"
                         % (axnames.get(axk, axk), a["n"], a["max"]))
            L.append("")

    # Per-screen dwell from nav transitions: dwell(to) = next-nav.t - this-nav.t.
    L += ["### Per-screen dwell (from nav transitions)", ""]
    navs = sorted((r for r in (nav_rows or [])
                   if isinstance(r.get("t"), (int, float))),
                  key=lambda r: r["t"])
    if not navs:
        L += ["_(no `nav` events)_", ""]
    else:
        L += ["| screen | entered (ms) | dwell | back? | focus |",
              "|---|---:|---:|:---:|---|"]
        for i, r in enumerate(navs):
            to = r.get("to", "?")
            ent = r["t"]
            nxt = navs[i + 1]["t"] if i + 1 < len(navs) else None
            dwell = (nxt - ent) if nxt is not None else None
            L.append("| %s | %.0f | %s | %s | %s |"
                     % (to, ent, fmt_dur(dwell) if dwell is not None else "(open)",
                        "↩" if r.get("wb") else "", r.get("focus", "") or ""))
        L.append("")
    return L


def section_summary(by):
    L = ["## Session summary", ""]
    # Songs
    songs = by.get("song", [])
    starts = [r for r in songs if r.get("ev") == "start"]
    ends = [r for r in songs if r.get("ev") == "end"]
    L += ["### Songs", ""]
    if not songs:
        L += ["_(no `song` events)_", ""]
    else:
        L += ["| id | track | diff | score | pct | result |",
              "|---|---|---|---:|---:|---|"]
        # pair ends with their id; report each end row (the scoreable event)
        for r in ends:
            pctv = r.get("pct")
            L.append("| %s | %s | %s | %s | %s | end |"
                     % (r.get("id", "?"), r.get("track", "?"),
                        r.get("diff", "?"),
                        r.get("score", "—"),
                        ("%.0f%%" % (pctv * 100)) if isinstance(pctv, (int, float)) else "—"))
        # songs that started but never ended
        ended_ids = {r.get("id") for r in ends}
        for r in starts:
            if r.get("id") not in ended_ids:
                L.append("| %s | %s | %s | — | — | started (no end) |"
                         % (r.get("id", "?"), r.get("track", "?"), r.get("diff", "?")))
        L.append("")
        L.append("**%d started · %d completed.**" % (len(starts), len(ends)))
        L.append("")

    # Audio underruns
    au = by.get("au", [])
    under = sum(int(r.get("under", 0) or 0) for r in au)
    auframes = sum(int(r.get("frames", 0) or 0) for r in au)
    L += ["### Audio", "",
          "- **underrun events:** %d (%d underrun-frames) across %d `au` rows"
          % (under, auframes, len(au)), ""]

    # Logs (warns/asserts)
    logs = by.get("log", [])
    by_lvl = {}
    for r in logs:
        by_lvl[r.get("lvl", "?")] = by_lvl.get(r.get("lvl", "?"), 0) + 1
    L += ["### Logs", ""]
    if not logs:
        L += ["_(no `log` events)_", ""]
    else:
        L.append("Counts: " + ", ".join("`%s`=%d" % (k, v)
                 for k, v in sorted(by_lvl.items())))
        L.append("")
        # surface assert/warn lines (most actionable)
        notable = [r for r in logs if r.get("lvl") in ("assert", "warn", "error")][:15]
        if notable:
            L += ["| lvl | f | msg | src |", "|---|---:|---|---|"]
            for r in notable:
                msg = (r.get("msg", "") or "").replace("|", "\\|")
                L.append("| %s | %s | %s | %s |"
                         % (r.get("lvl", "?"), r.get("f", "—"), msg,
                            r.get("src", "") or ""))
            L.append("")

    # Markers
    marks = by.get("mark", [])
    if marks:
        L += ["### Markers", "", "| f | tag | note |", "|---:|---|---|"]
        for r in marks:
            L.append("| %s | %s | %s |" % (r.get("f", "—"), r.get("tag", "?"),
                                           r.get("note", "") or ""))
        L.append("")
    return L


def render(rows, worst_n):
    hdr, by = bucket_by_kind(rows)
    names = load_button_names()
    out = []
    out += section_header(hdr, rows)
    out += section_boot(by.get("boot", []))
    out += section_frames(by.get("fr", []))
    out += section_input(by.get("in", []), by.get("nav", []), names)
    out += section_summary(by)
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("session", nargs="?", default=None,
                    help="path to a session .jsonl (or '-' for stdin)")
    ap.add_argument("--jsonl", dest="jsonl", default=None,
                    help="path to a session .jsonl (alias for the positional arg)")
    ap.add_argument("--worst", type=int, default=20,
                    help="max long frames to list (default 20)")
    args = ap.parse_args()

    path = args.jsonl or args.session
    if not path:
        ap.error("provide a session .jsonl path (positional or --jsonl), or '-' for stdin")
    if path != "-" and not os.path.exists(path):
        print("ERROR: no such file: %s" % path, file=sys.stderr)
        return 2

    rows = parse_jsonl(path)
    if not rows:
        print("ERROR: no parseable JSONL rows in %s" % path, file=sys.stderr)
        return 2
    sys.stdout.write(render(rows, args.worst))
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
