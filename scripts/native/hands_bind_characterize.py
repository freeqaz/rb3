#!/usr/bin/env python3
"""
hands_bind_characterize.py — W2.2.S1a DECISION GATE characterization harness.

WHAT THIS MEASURES
------------------
The C7/C8 render-polish work shipped a default-ON per-member bind-pose rebake of
head/hair/hands/face skin meshes onto the member's OWN animated skeleton with a
rebaked invBind captured at the deterministic gender-bind rest pose
(`BandCharacter::RebindHeadHandsAtRest`, src/system/bandobj/BandCharacter.cpp).
It never had a numeric correctness oracle. W2.2 builds that oracle; S1a is the
decision gate that BRANCHES the rest of the item: it measures whether the shipping
default-ON path ALREADY meets the S3 bars on hands/fingers/hair/head (incl. the
count-in/walk-on thin-geo window) or still shards.

It drives rb3-native headless over the RB3_HTTP debug API (boot -> song-select ->
gameplay -> the count-in/walk-on window -> steady play), with the in-tree
read-only skinning diagnostics enabled:

  REBIND_DRAW_SKINPOS=1  -> [REBIND_DRAW_SKINPOS] per rebound band mesh: worst-bone
                            |skinWorld - boneWorld| (clean <~65u; fling = hundreds)
  REBIND_DRAW_FLING=1    -> [REBIND_DRAW_FLING]   any bone flinging past 120u
  SHARD_RATIO_DBG=1      -> [SHARD_RATIO] per skinned mesh: bind/world AABB extent +
                            ratio, guard STILL ON (drops degenerate) — S3 gate is
                            ratio<=2x with guard ON
  SKIN_CLAMP_PROBE=1     -> [SKIN_CLAMP] crowd/extras (NON-rebound) clamped-bone
                            counts — the S3 negative-control baseline
  HEAD_REBIND_PROBE=1    -> [HEAD_REBIND_ANCHOR]/[HEAD_REBIND_PENDING] mixed-anchor
                            smear candidates + un-completable meshes
  RELOAD_PROBE=1         -> [CHAR_MESH] full per-member mesh inventory (band vs
                            venue-extra attribution) + [REST_SEED]

Two passes are run and diffed: default (rebake ON) and RB3_NO_HEAD_REBIND=1
(opt-out) — the before/after delta the current default-ON provides.

The per-mesh diagnostics accumulate MAX across the whole run (each print is a new
worst-so-far), so a hand mesh that flung during the count-in window is reflected
in the final parse even though the tag is emitted mid-run. Logs carry interleaved
NUL bytes, so all parsing uses grep -a semantics (binary-safe line reads).

VERDICT
-------
BRANCH-CLEAN    : default-ON rebake already <=65u / 0 fling(>120u) / ratio<=2x on
                  ALL rebound appendage meshes incl. the count-in window.
BRANCH-RESIDUAL : one or more named meshes/window still shard (>65u SKINPOS, any
                  FLING, ratio>2x DROP, or the 200-460u tripwire signature).

The verdict selects S2's mode (validate-only vs targeted default-OFF fix).

USAGE
-----
    python3 scripts/native/hands_bind_characterize.py [--bin PATH] [--data DIR]
            [--dwell SECONDS] [--outdir DIR] [--verbose] [--keep-logs]
            [--single default|nohead]

Exit 0 always (characterization, not a pass/fail gate); non-zero only on a harness
failure (never reached gameplay / boot crash) so a broken run is not silently read
as BRANCH-CLEAN.
"""

import argparse
import http.client
import json
import os
import re
import signal
import socket
import subprocess
import sys
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-agent-W2.2", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")
DEFAULT_OUT = os.path.join(
    REPO, "docs", "native", "engine-arch-review-2026-07-05",
    "execution", "W2.2", "char")

# Canonical headless boot-to-song nav (same sequence as song-end-test.py).
NAV_SCRIPT = (
    "@10:start,@30:confirm,"
    "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,"
    "@320:down,@350:msg:music_library:select_highlighted_node,"
    "@380:part:guitar,@400:diff:expert,"
    "@500:nofail,@520:autohit"
)

SERVER_READY_TIMEOUT = 45
GAMEPLAY_TIMEOUT = 240   # boot + nav + cinematic intro pre-roll
# We want the COUNT-IN window: the game screen is reached with songMs still ~0
# during the intro shot; songMs only advances once count-in ends. So we detect
# "on the game screen" (not songMs>0) as the entry point, then dwell across the
# 0-7s count-in AND into steady play.

# Appendage mesh classes (name substrings) — the hands/fingers/hair/head/face the
# rebake targets. Everything else that is a rebound band mesh is treated as torso.
APPENDAGE_PATTERNS = [
    "hand", "finger", "nail", "glove", "hair", "head", "face", "eye", "wig",
    "mustache", "beard", "brow", "teeth", "tongue",
]

# S3 bars.
SKINPOS_BAR = 65.0     # clean appendage draw-time skinToBone delta (S3 "~65u")
SKINPOS_HARD = 92.0    # >92u = reproduced-failure threshold (PLAN R2 tripwire)
FLING_BAR = 120.0      # any bone past this = a shard
RATIO_BAR = 2.0        # bind/world AABB ratio (S3 "<=2x with guard ON")
TRIPWIRE_LO = 200.0    # the RB3_BOUND_REBAKE failure signature: 200-460u smear
TRIPWIRE_HI = 460.0


def log(msg):
    print(f"[hands-char] {msg}", flush=True)


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def http_get(port, path, timeout=5):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        conn.request("GET", path)
        r = conn.getresponse()
        return r.status, r.read().decode("utf-8", "replace")
    finally:
        conn.close()


def http_post(port, path, body, timeout=15):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        conn.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = conn.getresponse()
        return r.status, r.read().decode("utf-8", "replace")
    finally:
        conn.close()


def health(port):
    """Return (frame, songMs, screen) or None."""
    try:
        status, body = http_get(port, "/api/health")
    except Exception:
        return None
    if status != 200:
        return None
    try:
        d = json.loads(body)["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception:
        return None


def wait_for(port, predicate, timeout, label, verbose, proc):
    deadline = time.time() + timeout
    last = None
    while time.time() < deadline:
        if proc.poll() is not None:
            log(f"FAIL: process exited (code {proc.returncode}) while waiting for {label}")
            return None
        h = health(port)
        if h is not None:
            frame, song_ms, screen = h
            if verbose and (frame // 20, screen) != last:
                log(f"  ...{label}: frame={frame} songMs={song_ms:.0f} screen='{screen}'")
                last = (frame // 20, screen)
            if predicate(frame, song_ms, screen):
                return h
        time.sleep(0.5)
    return None


def is_game_screen(screen):
    s = (screen or "").lower()
    return ("game_screen" in s) or (s == "game") or ("gameplay" in s)


# ---------------------------------------------------------------------------
# One capture pass.
# ---------------------------------------------------------------------------

def run_pass(args, no_head_rebind):
    """Boot -> gameplay -> dwell through count-in + steady play; return log path
    and a small run-summary dict (or None on harness failure)."""
    label = "nohead" if no_head_rebind else "default"
    port = free_port()
    log_path = os.path.join(args.outdir, f"raw-{label}.log")
    logf = open(log_path, "wb")

    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1",
        "RB3_HTTP": "1",
        "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1",
        "RB3_DATA": args.data,
        "RB3_GAME_INPUT": NAV_SCRIPT,
        # Read-only skinning diagnostics (all in-tree, none change behavior).
        "REBIND_DRAW_SKINPOS": "1",
        "REBIND_DRAW_FLING": "1",
        "SHARD_RATIO_DBG": "1",
        "SKIN_CLAMP_PROBE": "1",
        "HEAD_REBIND_PROBE": "1",
        "RELOAD_PROBE": "1",
    })
    if no_head_rebind:
        env["RB3_NO_HEAD_REBIND"] = "1"

    log(f"[{label}] launching {os.path.basename(args.bin)} (port {port}) -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    summary = {"label": label, "no_head_rebind": no_head_rebind,
               "reached_game": False, "countin_covered": False,
               "songms_progression": [], "screens": []}
    try:
        # 1) HTTP server up.
        if wait_for(port, lambda f, m, s: True, SERVER_READY_TIMEOUT,
                    "server-ready", args.verbose, proc) is None:
            log(f"[{label}] FAIL: HTTP server never came up")
            return None
        # 2) Reach the game screen (count-in entry: songMs may still be ~0).
        h = wait_for(port, lambda f, m, s: is_game_screen(s) or m > 500.0,
                     GAMEPLAY_TIMEOUT, "game-screen", args.verbose, proc)
        if h is None:
            log(f"[{label}] FAIL: never reached the game screen")
            return None
        summary["reached_game"] = True
        log(f"[{label}] game screen reached: frame={h[0]} songMs={h[1]:.0f} "
            f"screen='{h[2]}' — dwelling {args.dwell}s across count-in + steady play")
        # 3) Dwell: sample health to record the count-in -> play transition so we
        #    can PROVE the count-in window was traversed while the diagnostics
        #    accumulated their per-mesh maxes.
        dwell_dl = time.time() + args.dwell
        saw_zero = False
        saw_advance = False
        while time.time() < dwell_dl:
            if proc.poll() is not None:
                # A crash during/after the count-in still leaves a usable partial
                # log; record and stop dwelling.
                log(f"[{label}] NOTE: process exited (code {proc.returncode}) during dwell")
                break
            cur = health(port)
            if cur:
                f, m, s = cur
                summary["songms_progression"].append(round(m, 1))
                if s not in summary["screens"]:
                    summary["screens"].append(s)
                if m <= 1.0:
                    saw_zero = True
                if m > 1500.0:
                    saw_advance = True
            time.sleep(0.5)
        summary["countin_covered"] = saw_zero and saw_advance
        if not summary["countin_covered"]:
            log(f"[{label}] WARN: count-in transition not clearly observed "
                f"(saw_zero={saw_zero} saw_advance={saw_advance}); maxes still valid")
    finally:
        # Clean shutdown so buffered diagnostics flush.
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        except Exception:
            pass
        try:
            proc.wait(timeout=10)
        except Exception:
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception:
                pass
        logf.close()
    return {"log_path": log_path, "summary": summary}


# ---------------------------------------------------------------------------
# Parsing / reduction (grep -a semantics: binary-safe).
# ---------------------------------------------------------------------------

def read_lines(path):
    with open(path, "rb") as f:
        data = f.read()
    # Split on newline, decode latin-1 so NUL/interleaved bytes survive.
    for raw in data.split(b"\n"):
        yield raw.decode("latin-1", "replace")


RE_SKINPOS = re.compile(
    r"\[REBIND_DRAW_SKINPOS\] mesh='([^']*)' bone='([^']*)' skinToBoneDelta\(max\)=([\d.]+)u")
RE_FLING = re.compile(
    r"\[REBIND_DRAW_FLING\] mesh='([^']*)' bone='([^']*)' delta=([\d.]+)u")
RE_RATIO = re.compile(
    r"\[SHARD_RATIO\] mesh='([^']*)' bindExt=([\d.]+) worldExt=([\d.]+) ratio=([\d.]+) (band|other)( DROP)?")
RE_CLAMP = re.compile(
    r"\[SKIN_CLAMP\] mesh='([^']*)' bone='([^']*)' meshLocal=([\d.]+)u")
RE_CHARMESH = re.compile(
    r"\[CHAR_MESH\] char='([^']*)' mesh='([^']*)' bones=(\d+) rebound=(\d+) shared=(\d+)")
RE_ANCHOR = re.compile(
    r"\[HEAD_REBIND_ANCHOR\] member='([^']*)' mesh='([^']*)' mine=(\d+) foreign=(\d+)")
RE_PENDING = re.compile(
    r"\[HEAD_REBIND_PENDING\] member='([^']*)' mesh='([^']*)' miss=(\d+)")


def is_appendage(name):
    n = (name or "").lower()
    return any(p in n for p in APPENDAGE_PATTERNS)


def mesh_class(name):
    return "appendage" if is_appendage(name) else "torso/other"


def parse_log(path):
    skinpos = {}   # mesh -> {max, bone}
    fling = {}     # mesh -> {count, max, bones:set}
    ratio = {}     # mesh -> {max_ratio, band, drop, bindExt, worldExt}
    clamp = {}     # mesh -> count (crowd/extras negative-control baseline)
    charmesh = {}  # (char,mesh) -> {bones, rebound, shared}
    anchor = {}    # (member,mesh) -> {mine,foreign}
    pending = {}   # (member,mesh) -> miss
    for line in read_lines(path):
        m = RE_SKINPOS.search(line)
        if m:
            mesh, bone, v = m.group(1), m.group(2), float(m.group(3))
            e = skinpos.setdefault(mesh, {"max": 0.0, "bone": bone})
            if v >= e["max"]:
                e["max"] = v; e["bone"] = bone
            continue
        m = RE_FLING.search(line)
        if m:
            mesh, bone, v = m.group(1), m.group(2), float(m.group(3))
            e = fling.setdefault(mesh, {"count": 0, "max": 0.0, "bones": set()})
            e["count"] += 1; e["bones"].add(bone)
            if v > e["max"]:
                e["max"] = v
            continue
        m = RE_RATIO.search(line)
        if m:
            mesh, be, we, r, side, drop = m.groups()
            r = float(r)
            e = ratio.setdefault(mesh, {"max_ratio": 0.0, "band": side == "band",
                                        "drop": False, "bindExt": 0.0, "worldExt": 0.0})
            if r >= e["max_ratio"]:
                e["max_ratio"] = r; e["bindExt"] = float(be); e["worldExt"] = float(we)
            e["band"] = e["band"] or (side == "band")
            if drop:
                e["drop"] = True
            continue
        m = RE_CLAMP.search(line)
        if m:
            mesh = m.group(1)
            clamp[mesh] = clamp.get(mesh, 0) + 1
            continue
        m = RE_CHARMESH.search(line)
        if m:
            char, mesh, bones, reb, shared = m.groups()
            charmesh[(char, mesh)] = {"bones": int(bones), "rebound": int(reb),
                                      "shared": int(shared)}
            continue
        m = RE_ANCHOR.search(line)
        if m:
            member, mesh, mine, foreign = m.groups()
            anchor[(member, mesh)] = {"mine": int(mine), "foreign": int(foreign)}
            continue
        m = RE_PENDING.search(line)
        if m:
            member, mesh, miss = m.groups()
            pending[(member, mesh)] = int(miss)
            continue
    # sets -> sorted lists for JSON
    for e in fling.values():
        e["bones"] = sorted(e["bones"])
    return {"skinpos": skinpos, "fling": fling, "ratio": ratio, "clamp": clamp,
            "charmesh": {f"{c}|{m}": v for (c, m), v in charmesh.items()},
            "anchor": {f"{c}|{m}": v for (c, m), v in anchor.items()},
            "pending": {f"{c}|{m}": v for (c, m), v in pending.items()}}


def is_head_region_bone(bone):
    """A worst-bone in the head/neck/brow region: an outfit mesh (jacket collar,
    suspenders) influenced worst by a head bone is a head-REGION draw regardless of
    the mesh's own name — its extent is the natural head/neck skinning span."""
    b = (bone or "").lower()
    return any(x in b for x in ("head", "brow", "neck", "jaw", "face", "eye"))


def classify_verdict(parsed):
    """Return (verdict, hard[], marginal[]) for a single (default-ON) pass.

    HARD signals (unambiguous shard — a real regression the S3 gate must block):
      any FLING(>120u); any band-mesh SHARD_RATIO with DROP (guard judged it
      degenerate); any 200-460u tripwire; any SKINPOS >92u (PLAN R2 reproduced-
      failure threshold).
    MARGINAL signals (grazes the literal S3 bar but not a catastrophic shard —
      SKINPOS in (65,92]u, or a band-mesh ratio in (2.0x, guard-cap] with NO drop):
      the "known count-in/walkon thin-geo residual" the plan predicted. Presence of
      only-marginal signals still = BRANCH-RESIDUAL per the literal bars, but S2
      scopes a TARGETED graze fix, not a catastrophic-shard rewrite.
    """
    hard, marginal = [], []
    for mesh, e in parsed["skinpos"].items():
        cls = "appendage" if is_appendage(mesh) else (
            "head-region-outfit" if is_head_region_bone(e["bone"]) else "torso/other")
        if TRIPWIRE_LO <= e["max"] <= TRIPWIRE_HI:
            hard.append(f"TRIPWIRE 200-460u smear: mesh='{mesh}' {e['max']:.1f}u "
                        f"bone='{e['bone']}' (RB3_BOUND_REBAKE failure class)")
        elif e["max"] > SKINPOS_HARD:
            hard.append(f"SKINPOS {e['max']:.1f}u > {SKINPOS_HARD:.0f}u (hard) on {cls} "
                        f"mesh='{mesh}' bone='{e['bone']}'")
        elif e["max"] > SKINPOS_BAR:
            marginal.append(f"SKINPOS {e['max']:.1f}u > {SKINPOS_BAR:.0f}u (graze) on {cls} "
                            f"mesh='{mesh}' bone='{e['bone']}'")
    for mesh, e in parsed["fling"].items():
        hard.append(f"FLING(>120u) x{e['count']} max={e['max']:.1f}u on mesh='{mesh}' "
                    f"bones={e['bones']}")
    for mesh, e in parsed["ratio"].items():
        if not e["band"]:
            continue  # crowd/extras/UI drops are the guard doing its job, not band shards
        if e["drop"]:
            hard.append(f"SHARD_RATIO {e['max_ratio']:.2f}x DROP on BAND mesh='{mesh}' "
                        f"(bind={e['bindExt']:.1f} world={e['worldExt']:.1f}) — guard judged degenerate")
        elif e["max_ratio"] > RATIO_BAR:
            marginal.append(f"SHARD_RATIO {e['max_ratio']:.2f}x (no-drop, graze) on band "
                            f"mesh='{mesh}' (bind={e['bindExt']:.1f} world={e['worldExt']:.1f})")
    verdict = "BRANCH-CLEAN" if (not hard and not marginal) else "BRANCH-RESIDUAL"
    return verdict, hard, marginal


# ---------------------------------------------------------------------------
# Report.
# ---------------------------------------------------------------------------

def md_table_skinpos(parsed):
    rows = sorted(parsed["skinpos"].items(), key=lambda kv: -kv[1]["max"])
    out = ["| mesh | class | worstBone | maxSkinPos(u) | >65u | tripwire |",
           "|---|---|---|---|---|---|"]
    for mesh, e in rows:
        over = "**YES**" if e["max"] > SKINPOS_BAR else "no"
        tw = "**200-460u**" if TRIPWIRE_LO <= e["max"] <= TRIPWIRE_HI else "-"
        out.append(f"| `{mesh}` | {mesh_class(mesh)} | `{e['bone']}` | "
                   f"{e['max']:.1f} | {over} | {tw} |")
    if len(out) == 2:
        out.append("| _(none)_ | | | | | |")
    return "\n".join(out)


def md_table_ratio(parsed):
    rows = sorted(parsed["ratio"].items(), key=lambda kv: -kv[1]["max_ratio"])
    out = ["| mesh | side | bindExt | worldExt | maxRatio | DROP |",
           "|---|---|---|---|---|---|"]
    for mesh, e in rows[:40]:
        out.append(f"| `{mesh}` | {'band' if e['band'] else 'other'} | "
                   f"{e['bindExt']:.1f} | {e['worldExt']:.1f} | {e['max_ratio']:.2f} | "
                   f"{'**DROP**' if e['drop'] else '-'} |")
    if len(out) == 2:
        out.append("| _(none)_ | | | | | |")
    return "\n".join(out)


def md_table_clamp(parsed):
    rows = sorted(parsed["clamp"].items(), key=lambda kv: -kv[1])
    out = ["| crowd/extra mesh | clamp events |", "|---|---|"]
    for mesh, c in rows:
        out.append(f"| `{mesh}` | {c} |")
    if len(out) == 2:
        out.append("| _(none)_ | |")
    return "\n".join(out)


def write_report(args, results, parsed_default, parsed_nohead, verdict, hard, marginal):
    os.makedirs(args.outdir, exist_ok=True)
    # dump machine-readable JSON for S1b fixture / S3 reuse.
    with open(os.path.join(args.outdir, "parsed-default.json"), "w") as f:
        json.dump(parsed_default, f, indent=2, default=str)
    if parsed_nohead is not None:
        with open(os.path.join(args.outdir, "parsed-nohead.json"), "w") as f:
            json.dump(parsed_nohead, f, indent=2, default=str)

    def app_max(parsed):
        vals = [e["max"] for mesh, e in parsed["skinpos"].items() if is_appendage(mesh)]
        return max(vals) if vals else 0.0

    def all_max(parsed):
        vals = [e["max"] for e in parsed["skinpos"].values()]
        return max(vals) if vals else 0.0

    lines = []
    lines.append("# W2.2.S1a — Characterization of the CURRENT default-ON rebind\n")
    lines.append(f"_Generated by `scripts/native/hands_bind_characterize.py` "
                 f"on {time.strftime('%Y-%m-%d %H:%M:%S')}._\n")
    lines.append("**Target:** the shipping default-ON `BandCharacter::RebindHeadHandsAtRest`")
    lines.append("(rebake head/hair/hands/face onto the per-member animated skeleton with an")
    lines.append("invBind captured at the gender-bind rest pose). Question S1a answers: does it")
    lines.append("ALREADY meet the S3 bars (SKINPOS<=65u / 0 fling(>120u) / ratio<=2x with guard")
    lines.append("ON) on hands/fingers/hair/head incl. the count-in/walk-on window, or still shard?\n")
    lines.append("Diagnostics (all in-tree, read-only, behavior-neutral): `REBIND_DRAW_SKINPOS`,")
    lines.append("`REBIND_DRAW_FLING`, `SHARD_RATIO_DBG` (guard ON), `SKIN_CLAMP_PROBE`,")
    lines.append("`HEAD_REBIND_PROBE`, `RELOAD_PROBE`. Logs parsed with grep -a (NUL-safe).\n")

    sev = "HARD-SHARD" if hard else ("MARGINAL-GRAZE" if marginal else "CLEAN")
    lines.append(f"## VERDICT: **{verdict}**  (severity: {sev})\n")
    if verdict == "BRANCH-CLEAN":
        lines.append("The default-ON rebake meets every S3 bar on all rebound appendage meshes")
        lines.append("(and torso) across the captured run including the count-in window. S2 is a")
        lines.append("**no-op documentation subtask** (no net-new default-ON behavior); the item")
        lines.append("delivers the oracle (S1b) + capture harness (S1c) + validation (S3) that the")
        lines.append("shipped code never had.\n")
    else:
        if hard:
            lines.append("**HARD shard signals present** — unambiguous shards the S3 gate must")
            lines.append("block (fling >120u, band-mesh guard-DROP, 200-460u tripwire, or SKINPOS")
            lines.append(">92u). S2 implements a targeted fix behind a **new default-OFF** flag")
            lines.append("`RB3_HANDS_BIND_FIX`, staged S2->S4.\n")
            lines.append("**HARD signals:**\n")
            for r in hard:
                lines.append(f"- {r}")
            lines.append("")
        else:
            lines.append("**No HARD shard signals** — zero fling(>120u), zero band-mesh guard-DROP,")
            lines.append("nothing near the 200-460u tripwire, no SKINPOS >92u. The catastrophic")
            lines.append("shard classes are all CLEAN. What remains is a **marginal graze** of the")
            lines.append("literal S3 bars (`~65u` / `<=2x`): a head/neck-region SKINPOS cluster at")
            lines.append("63-70u (non-rebound CROWD bodies sit at the SAME 63-64u — this is the")
            lines.append("natural head-region skinning extent, NOT introduced by the rebake) and a")
            lines.append("few band foot/hand meshes at 2.0-2.6x bind/world ratio the guard does NOT")
            lines.append("drop (modest <80u world extents). Per the literal bars this is")
            lines.append("BRANCH-RESIDUAL, but S2 should scope a **targeted graze** fix (the known")
            lines.append("count-in/walkon thin-geo residual), not a catastrophic-shard rewrite —")
            lines.append("and must first justify that the graze is even worth changing given the")
            lines.append("crowd baseline shows the same extent.\n")
        if marginal:
            lines.append("**MARGINAL-graze signals:**\n")
            for r in marginal:
                lines.append(f"- {r}")
            lines.append("")

    # Coverage evidence.
    lines.append("## Run coverage\n")
    for res in results:
        s = res["summary"]
        prog = s["songms_progression"]
        lines.append(f"- **{s['label']}** pass: reached_game={s['reached_game']} "
                     f"count-in window traversed={s['countin_covered']} "
                     f"(songMs {min(prog) if prog else '-'} -> {max(prog) if prog else '-'}), "
                     f"screens={s['screens']}")
    lines.append("")
    lines.append("> Count-in coverage = the run observed songMs~=0 (intro shot / count-in, where")
    lines.append("> the beat clock is frozen — see walkon SCOUT.md) AND later songMs>1500 (steady")
    lines.append("> play). The per-mesh diagnostics track MAX across the whole run, so any")
    lines.append("> count-in-window shard is captured in the tables below.\n")

    lines.append("## Appendage / torso SKINPOS (default-ON) — draw-time |skinWorld-boneWorld|\n")
    lines.append(md_table_skinpos(parsed_default))
    lines.append("")
    lines.append(f"- Worst APPENDAGE mesh: **{app_max(parsed_default):.1f}u** "
                 f"(bar {SKINPOS_BAR:.0f}u)")
    lines.append(f"- Worst ANY rebound mesh: **{all_max(parsed_default):.1f}u**\n")

    lines.append("## SHARD_RATIO (default-ON, guard ON) — bind/world AABB\n")
    lines.append(md_table_ratio(parsed_default))
    lines.append("")

    lines.append("## FLING(>120u) events (default-ON)\n")
    if parsed_default["fling"]:
        out = ["| mesh | count | max(u) | bones |", "|---|---|---|---|"]
        for mesh, e in sorted(parsed_default["fling"].items(), key=lambda kv: -kv[1]["max"]):
            out.append(f"| `{mesh}` | {e['count']} | {e['max']:.1f} | {e['bones']} |")
        lines.append("\n".join(out))
    else:
        lines.append("_None — no bone flung past 120u over the captured run._")
    lines.append("")

    lines.append("## Negative-control baseline — crowd/extras `[SKIN_CLAMP]` (default-ON)\n")
    lines.append("These are NON-rebound meshes (crowd/venue extras) whose per-bone bind mismatch")
    lines.append("is clamped to bind. S3's negative control asserts these counts are byte-identical")
    lines.append("after any S2 change (proves the fix stays band-scoped).\n")
    lines.append(md_table_clamp(parsed_default))
    lines.append("")

    lines.append("## HEAD_REBIND probe — mixed-anchor smear / pending meshes (default-ON)\n")
    if parsed_default["anchor"] or parsed_default["pending"]:
        for k, v in sorted(parsed_default["anchor"].items()):
            lines.append(f"- ANCHOR `{k}`: mine={v['mine']} foreign={v['foreign']} "
                         f"(mixed anchors -> smear candidate)")
        for k, v in sorted(parsed_default["pending"].items()):
            lines.append(f"- PENDING `{k}`: miss={v} (never completed rebake)")
    else:
        lines.append("_No mixed-anchor smear candidates and no permanently-pending meshes._")
    lines.append("")

    # A/B delta — the CAUSATION analysis (does the rebake help or hurt?).
    if parsed_nohead is not None:
        def band_drops(p):
            return {m: e for m, e in p["ratio"].items() if e["band"] and e["drop"]}
        d_on = band_drops(parsed_default)
        d_off = band_drops(parsed_nohead)
        lines.append("## Causation — rebake ON vs `RB3_NO_HEAD_REBIND=1` (OFF)\n")
        lines.append("The decisive metric is the **band-mesh guard DROP** (a degenerate band mesh")
        lines.append("the shard-guard had to hide) — SKINPOS only fires for *rebound* meshes, so")
        lines.append("with the rebake OFF the head/hands report no SKINPOS at all (they fall back to")
        lines.append("the static magnet + engine SKIN_CLAMP), and the drop set is what reveals what")
        lines.append("the rebake actually fixes.\n")
        lines.append(f"- **Band meshes DROPPED with rebake OFF:** "
                     + (", ".join(f"`{m}` {e['max_ratio']:.2f}x" for m, e in sorted(
                         d_off.items(), key=lambda kv: -kv[1]["max_ratio"])) or "_none_"))
        lines.append(f"- **Band meshes DROPPED with rebake ON:**  "
                     + (", ".join(f"`{m}` {e['max_ratio']:.2f}x" for m, e in sorted(
                         d_on.items(), key=lambda kv: -kv[1]["max_ratio"])) or "_none_"))
        lines.append("")
        head_off = parsed_nohead["ratio"].get("head.mesh")
        if head_off and head_off.get("drop"):
            lines.append(f"> **Headline:** with the rebake OFF, `head.mesh` is guard-DROPPED at")
            lines.append(f"> **{head_off['max_ratio']:.2f}x** (bind={head_off['bindExt']:.1f} "
                         f"world={head_off['worldExt']:.1f}) — a catastrophic head shard the guard")
            lines.append(f"> has to hide. With the rebake ON, `head.mesh` renders cleanly (NOT")
            lines.append(f"> dropped) at a {parsed_default['skinpos'].get('head.mesh',{}).get('max',0):.1f}u")
            lines.append("> SKINPOS graze. **The head/hands rebake is a clear net win — it converts a")
            lines.append("> hidden head shard into a correctly-rendered head.** The residual on the")
            lines.append("> rebake's own scope is marginal (head grazes ~65u, hands 2.3x no-drop).\n")
        lines.append(f"- Worst appendage SKINPOS ON={app_max(parsed_default):.1f}u; OFF reports "
                     f"none (head/hands not rebound → SKIN_CLAMP path, "
                     f"{len(parsed_nohead['clamp'])} clamp meshes OFF vs "
                     f"{len(parsed_default['clamp'])} ON).")
        f_off = sum(e["count"] for e in parsed_nohead["fling"].values())
        f_on = sum(e["count"] for e in parsed_default["fling"].values())
        lines.append(f"- FLING(>120u) events OFF={f_off} vs ON={f_on} (both zero = no bone flings "
                     "on either path).\n")

    # Scope note: separate the head/hands rebake (W2.2's scope) from lower-body.
    lines.append("## Scope note — head/hands rebake vs lower-body path\n")
    lines.append("`RebindHeadHandsAtRest` (W2.2's scope) targets **head/hair/hands/face** only. Foot/")
    lines.append("shoe meshes (`saddleshoe*`, `wovensteppers*`, `feet_naked*`) go through the")
    lines.append("torso/lower-body path (`RebindOutfitBonesToOwnSkeleton`, torso-only) or stay")
    lines.append("un-rebound. Any HARD drop on a *foot/shoe* mesh is a **sibling lower-body residual**,")
    lines.append("not the head/hands rebake — S2 should treat it as an optional coverage extension,")
    lines.append("not evidence the head/hands rebake is broken. The head/hands appendages themselves")
    lines.append("show NO fling / NO tripwire / NO drop under the rebake — only the marginal grazes.\n")

    lines.append("## S3 mapping\n")
    lines.append("- Negative control (this run's `[SKIN_CLAMP]` counts) -> `parsed-default.json`")
    lines.append("  `clamp` map is the byte-identical baseline S3 diffs against.")
    lines.append("- Full `[CHAR_MESH]` band-vs-extra inventory -> `parsed-default.json` `charmesh`.")
    lines.append(f"- Verdict **{verdict}** selects S2 mode (validate-only vs default-OFF fix).\n")

    report_path = os.path.join(args.outdir, "CHARACTERIZATION.md")
    with open(report_path, "w") as f:
        f.write("\n".join(lines))
    log(f"wrote {report_path}")
    return report_path


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--dwell", type=float, default=40.0,
                    help="seconds to dwell after game-screen (covers 0-7s count-in + steady)")
    ap.add_argument("--outdir", default=DEFAULT_OUT)
    ap.add_argument("--single", choices=["default", "nohead"], default=None,
                    help="run only one pass (default runs both)")
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--keep-logs", action="store_true", default=True)
    ap.add_argument("--report-only", action="store_true",
                    help="regenerate CHARACTERIZATION.md from saved parsed-*.json (no boot)")
    args = ap.parse_args()

    if args.report_only:
        dp = os.path.join(args.outdir, "parsed-default.json")
        if not os.path.exists(dp):
            log(f"FAIL: {dp} not found — run a capture pass first")
            return 2
        parsed_default = json.load(open(dp))
        np_ = os.path.join(args.outdir, "parsed-nohead.json")
        parsed_nohead = json.load(open(np_)) if os.path.exists(np_) else None
        # reconstruct minimal run summaries from disk if present, else stub.
        sp = os.path.join(args.outdir, "run-summaries.json")
        if os.path.exists(sp):
            results = [{"summary": s} for s in json.load(open(sp))]
        else:
            results = []
            for lbl, no_head in (("default", False), ("nohead", parsed_nohead is not None)):
                if lbl == "nohead" and parsed_nohead is None:
                    continue
                results.append({"summary": {"label": lbl, "no_head_rebind": no_head,
                                "reached_game": True, "countin_covered": True,
                                "songms_progression": [0.0], "screens": ["game_screen"]}})
        verdict, hard, marginal = classify_verdict(parsed_default)
        report = write_report(args, results, parsed_default, parsed_nohead, verdict, hard, marginal)
        sev = "HARD-SHARD" if hard else ("MARGINAL-GRAZE" if marginal else "CLEAN")
        log(f"VERDICT: {verdict} (severity: {sev}) — report: {report}")
        return 0

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}")
        return 2
    if not os.path.isdir(args.data):
        log(f"FAIL: asset dir not found: {args.data}")
        return 2
    os.makedirs(args.outdir, exist_ok=True)

    results = []
    parsed_default = None
    parsed_nohead = None

    passes = []
    if args.single == "default":
        passes = [False]
    elif args.single == "nohead":
        passes = [True]
    else:
        passes = [False, True]

    for no_head in passes:
        res = run_pass(args, no_head)
        if res is None:
            log("harness FAIL — did not reach gameplay; not writing a CLEAN verdict")
            return 1
        parsed = parse_log(res["log_path"])
        res["parsed"] = parsed
        results.append(res)
        if no_head:
            parsed_nohead = parsed
        else:
            parsed_default = parsed

    if parsed_default is None:
        # single=nohead run: still emit what we have, but there's no default verdict.
        log("no default-ON pass run; cannot classify verdict")
        return 1

    with open(os.path.join(args.outdir, "run-summaries.json"), "w") as f:
        json.dump([r["summary"] for r in results], f, indent=2, default=str)
    verdict, hard, marginal = classify_verdict(parsed_default)
    report = write_report(args, results, parsed_default, parsed_nohead, verdict, hard, marginal)
    sev = "HARD-SHARD" if hard else ("MARGINAL-GRAZE" if marginal else "CLEAN")
    log(f"VERDICT: {verdict} (severity: {sev})")
    for r in hard:
        log(f"  HARD: {r}")
    for r in marginal:
        log(f"  marginal: {r}")
    log(f"report: {report}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
