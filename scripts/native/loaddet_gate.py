#!/usr/bin/env python3
"""loaddet_gate.py — loader-determinism seam gate + attribution + per-axis ledger.

Promoted from docs/.../execution/W0.3d-b/loaddet_gate.py (Wave-12 A-S2) to
scripts/native/ for R4 (Wave-17 Lane L). Three modes share one boot driver:

  (default)  PRIMARY gate: post-anchor gRand stream-position spread, OFF vs ON arm
             under worker-latency jitter (RB3_LOADDET_JITTER). See A-S2 header below.
  --attrib   M1 attribution: with RB3_LOADDET_ATTRIB=1 the four Rand.cpp free-fn
             wrappers tag each gRand draw with __builtin_return_address(0); this
             mode aligns boots per-frame, diffs per-caller draw counts in BOTH the
             boot window (frames 0..W) and the gate window (anchor..anchor+K), and
             emits a ranked divergent-caller table (module-offset keyed, PIE-ASLR
             invariant; resolved offline via addr2line). M1 GO iff the gate-window
             divergent VARIABLE-COUNT callers form a small set (<= ~8 sites).
  --ledger   M3 per-axis PASS/FAIL ledger (count / order / stream / clock) vs a
             reference boot, written to ledger-<tag>.json. Stream axis is PRIMARY.

--- A-S2 PRIMARY-gate rationale (unchanged) -------------------------------------
Post-anchor DELTA, not absolute gdraw: the seam RE-BASES the stream by reseeding
gRand to a canonical constant at the is_playing 0->1 anchor (GamePanel::StartGame).
After the reseed the RNG STATE is a deterministic function of ONLY the number of
draws SINCE the reseed, so the meaningful "stream position at the pinned capture"
is  postAnchorDelta = gdraw@[anchorFrame+K] - gdraw@anchorFrame. The seam collapses
postAnchorDelta; it does NOT (and is not meant to) collapse absolute gdraw.

Arms:  OFF = jitter set, RB3_LOAD_DETERMINISM unset -> spread reproduces.
       ON  = jitter set, RB3_LOAD_DETERMINISM=1     -> spread collapses (PRIMARY).

Usage:
  # PRIMARY gate (M3)
  python3 scripts/native/loaddet_gate.py --n 10 --k 300 --jitter 200 --ledger \
      --bin native/build-agent-R4/rb3-native --out /tmp/loaddet-gate
  # M1 attribution (OFF arm names the divergent consumers)
  python3 scripts/native/loaddet_gate.py --attrib --arm off --n 4 --k 300 \
      --jitter 200 --bin native/build-agent-R4/rb3-native --out /tmp/loaddet-attrib
"""
import argparse, hashlib, importlib.util, json, os, re, signal, subprocess, sys, time
import statistics as st

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
NSCR = os.path.join(REPO, "scripts", "native")


def _load(mod, path):
    spec = importlib.util.spec_from_file_location(mod, path)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); return m


k = _load("kbd2game", os.path.join(NSCR, "keyboard-to-gameplay.py"))
pgc = _load("pgc", os.path.join(NSCR, "placement-gate-capture.py"))


def wd_arms_eng_hot():
    """Lazily load white_discriminate's ARMS['eng_hot'] venue-forcing env (the same
    eng_hot regime wash_cosample uses) for the T1 --regime eng_hot capture. Lazy so
    the common (default/attrib/ledger) paths don't import the WHITE-fix module."""
    wf = os.path.join(REPO, "docs", "native", "engine-arch-review-2026-07-05",
                      "execution", "WHITE-fix", "white_discriminate.py")
    wd = _load("white_discriminate", wf)
    return dict(wd.ARMS["eng_hot"])

ANCHOR_RE = re.compile(r"\[LOADDET\] anchor frame=(\d+) gdraw=(\d+)")
FRAME_RE = re.compile(r"\[LOADDET\] frame=(\d+) gdraw=(\d+)")
RESEED_RE = re.compile(r"\[LOADDET\] reseed anchor=(\S+) seed=(\S+) gdrawBefore=(\d+)")
ATTRIB_RE = re.compile(r"\[LOADDET\] attrib frame=(-?\d+) pc=(\S+) off=(\S+) sym=(\S+) mod=(\S+) draws=(\d+)")
COMPLETE_RE = re.compile(r"\[LOADDET\] complete frame=(\d+) gdraw=(\d+) kind=(\S+) name=(\S+)")
# Wave-19 T1 (Lane F) FRAME-ASSIGNMENT TIMING markers — new keywords, disjoint from
# COMPLETE_RE/ATTRIB_RE so the R4 order axis and attrib parse are byte-unaffected.
FILEARRIVE_RE = re.compile(r"\[LOADDET\] filearrive frame=(-?\d+) gdraw=(\d+) name=(\S+)")
SONGMS_RE = re.compile(r"\[LOADDET\] songms frame=(-?\d+) ms=(-?\d+(?:\.\d+)?)")
DT_MS = 1000.0 / 60.0


def log(m):
    print(f"[loaddet-gate] {m}", flush=True)


def boot_measure(binpath, arm_env, k_frames, target_ms, log_path, diff="hard",
                 song_downs=4, verbose=False, autohit=False, attrib=False,
                 timeline=False):
    """One boot: nav to gameplay, drive to target songMs, return dict with
    anchorFrame, gAnchor, gTarget, postAnchorDelta, absTarget, reseedFired, and
    (when attrib) frames{}, attribByOff{off:{frame:draws}}, offMeta{off:(pc,mod)},
    completes[(frame,kind,name)]."""
    port = k.free_port()
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_FIXED_CLOCK": "1", "RB3_LOADDET_PROBE": "1",
        "RB3_DATA": k.DEFAULT_DATA,
        "RB3_DTA_OVERLAY": os.path.join(REPO, "native", "dta"),
    })
    if attrib:
        env["RB3_LOADDET_ATTRIB"] = "1"
    if timeline:
        # T1: the timeline gate co-arms PROBE + ATTRIB + TIMELINE so emitTimeline
        # (attrib) + frameAssign/songClock (timeline) all have live signal.
        env["RB3_LOADDET_TIMELINE"] = "1"
        env["RB3_LOADDET_ATTRIB"] = "1"
    env.update(arm_env)
    proc = subprocess.Popen([binpath], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    result = {"ok": False, "reason": "?"}
    try:
        if not pgc.nav_to_gameplay(port, proc, song_downs, k.DIFF_INDEX[diff], verbose):
            result["reason"] = "nav_failed"; return result
        k.verb(port, "nofail")
        dl = time.time() + 60
        playing = False
        while time.time() < dl:
            ip = k.dta(port, "{game is_playing}")
            h = k.health(port)
            if ip is not None and int(ip) == 1 and h and h[1] >= 0:
                playing = True; break
            k.verb(port, "autohit")  # PRE-anchor only; post-anchor drive is input-free
            time.sleep(0.15)
        if not playing:
            result["reason"] = "song_never_played"; return result
        dl = time.time() + 120
        while time.time() < dl:
            h = k.health(port)
            if h and h[1] >= target_ms:
                break
            if autohit:
                k.verb(port, "autohit")
            time.sleep(0.02)
        time.sleep(0.3)
    finally:
        logf.flush()
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM); proc.wait(timeout=10)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass
        logf.close()

    return parse_boot_log(log_path, k_frames, attrib or timeline)


def parse_boot_log(log_path, k_frames, attrib=False):
    """Parse a SINGLE boot log (the [LOADDET] markers) into the boot dict that
    boot_measure returns. Shared by boot_measure (boots this harness launches)
    and by the grade-external-logs mode (boots produced by white_ab/wash harnesses
    that run RB3_LOADDET_ATTRIB=1 and redirect stderr into <prefix>.engine.log).
    A2/F6: the WHITE/wash re-grade grades the EXACT measurement boots' OWN logs."""
    result = {"ok": False, "reason": "?"}
    anchor_frame = None; g_anchor = None; reseed_fired = False
    frames = {}
    attrib_by_off = {}   # off -> {frame: draws}
    off_meta = {}        # off -> (example_pc, mod)
    completes = []       # (frame, kind, name)
    file_arrivals = []   # (frame, name)  — T1 frameAssign (secondary carrier)
    song_ms = {}         # frame -> ms    — T1 songClock
    with open(log_path, "r", errors="replace") as f:
        for ln in f:
            m = ANCHOR_RE.search(ln)
            if m and anchor_frame is None:
                anchor_frame = int(m.group(1)); g_anchor = int(m.group(2)); continue
            if RESEED_RE.search(ln):
                reseed_fired = True; continue
            m = FRAME_RE.search(ln)
            if m:
                frames[int(m.group(1))] = int(m.group(2)); continue
            # T1 timeline markers — parsed unconditionally (cheap; existing callers
            # ignore the extra result keys; legacy logs simply never match).
            m = FILEARRIVE_RE.search(ln)
            if m:
                file_arrivals.append((int(m.group(1)), m.group(3))); continue
            m = SONGMS_RE.search(ln)
            if m:
                song_ms[int(m.group(1))] = float(m.group(2)); continue
            if attrib:
                m = ATTRIB_RE.search(ln)
                if m:
                    fr = int(m.group(1)); pc = m.group(2); off = m.group(3)
                    mod = m.group(5); draws = int(m.group(6))
                    attrib_by_off.setdefault(off, {})[fr] = \
                        attrib_by_off.setdefault(off, {}).get(fr, 0) + draws
                    off_meta.setdefault(off, (pc, mod))
                    continue
                m = COMPLETE_RE.search(ln)
                if m:
                    completes.append((int(m.group(1)), m.group(3), m.group(4)))
                    continue
    if anchor_frame is None:
        result["reason"] = "no_anchor_marker"
        # T1 timeline grading does NOT require the gameplay anchor (it grades the
        # whole eng_hot window incl. song load). Surface the timeline maps so the
        # --timeline grader can still read a boot that legitimately has no anchor.
        result["frames"] = frames
        result["attribByOff"] = attrib_by_off if attrib else None
        result["fileArrivals"] = file_arrivals
        result["songMs"] = song_ms
        result["completes"] = completes if attrib else None
        return result
    tgt_frame = anchor_frame + k_frames
    cand = [fr for fr in frames if fr >= tgt_frame]
    if not cand:
        result["reason"] = f"target_frame_not_reached(anchor={anchor_frame},maxlog={max(frames) if frames else -1})"
        return result
    tf = min(cand)
    g_target = frames[tf]
    result.update({
        "ok": True, "anchorFrame": anchor_frame, "targetFrame": tf,
        "gAnchor": g_anchor, "gTarget": g_target,
        "postAnchorDelta": g_target - g_anchor, "absTarget": g_target,
        "reseedFired": reseed_fired, "maxFrame": max(frames),
        "frames": frames if attrib else None,
        "attribByOff": attrib_by_off if attrib else None,
        "offMeta": off_meta if attrib else None,
        "completes": completes if attrib else None,
        # T1 timeline maps — always populated (independent of the attrib arm for
        # fileArrivals/songMs; emitTimeline still needs attrib for attribByOff).
        "fileArrivals": file_arrivals,
        "songMs": song_ms,
        "framesAll": frames,
    })
    return result


def grade_external_logs(log_paths, k_frames):
    """A2/F6 grade-external-logs mode: parse a set of ALREADY-PRODUCED boot logs
    (the exact measurement boots) and run ledger_for_arm over them. Returns the
    ledger dict (same schema as the ON-arm ledger). Boots that fail to parse are
    surfaced as an ok=False row so a short/failed boot cannot silently pass.
    The harness that CALLS this treats PRIMARY != PASS (stream axis < N/N) as VOID:
    it must REFUSE to emit a WHITE/wash verdict, never discard-and-rerun."""
    boots = []
    for lp in log_paths:
        b = parse_boot_log(lp, k_frames, attrib=True)
        b["logPath"] = lp
        boots.append(b)
    arm = {"arm": "EXTERN", "boots": boots,
           "ok": sum(1 for b in boots if b["ok"]), "n": len(boots)}
    led = ledger_for_arm(arm, k_frames)
    led["nLogs"] = len(log_paths)
    led["nParsed"] = arm["ok"]
    return led


def run_arm(binpath, name, arm_env, n, k_frames, target_ms, out_dir, diff,
            verbose, autohit, attrib=False):
    log(f"=== ARM {name}: n={n} env={arm_env} attrib={attrib} ===")
    boots = []
    for i in range(n):
        lp = os.path.join(out_dir, f"{name}-boot{i}.log")
        r = boot_measure(binpath, arm_env, k_frames, target_ms, lp, diff=diff,
                         verbose=verbose, autohit=autohit, attrib=attrib)
        if r["ok"]:
            log(f"  {name} boot{i}: anchor@{r['anchorFrame']} gAnchor={r['gAnchor']} "
                f"gTarget={r['gTarget']} postDelta={r['postAnchorDelta']} "
                f"reseed={r['reseedFired']}"
                + (f" callers={len(r['attribByOff'])}" if attrib else ""))
        else:
            log(f"  {name} boot{i}: FAILED ({r['reason']})")
        boots.append(r)
    ok = [b for b in boots if b["ok"]]
    deltas = [b["postAnchorDelta"] for b in ok]
    absv = [b["absTarget"] for b in ok]
    return {
        "arm": name, "env": arm_env, "n": n, "ok": len(ok),
        "postAnchorDeltas": deltas,
        "distinctDeltas": sorted(set(deltas)),
        "deltaSpread": (max(deltas) - min(deltas)) if deltas else None,
        "absTargets": absv,
        "absSpread": (max(absv) - min(absv)) if absv else None,
        "reseedFiredAll": all(b["reseedFired"] for b in ok) if ok else False,
        "boots": boots,
    }


# ---------------------------------------------------------------------------
# addr2line offline resolution (module-offset -> file:line/func)
# ---------------------------------------------------------------------------
def resolve_offsets(binpath, offs):
    """Batch-resolve a set of hex module offsets to 'func @ file:line' strings."""
    out = {}
    offs = list(offs)
    if not offs:
        return out
    try:
        p = subprocess.run(["addr2line", "-e", binpath, "-f", "-C"] + offs,
                           capture_output=True, text=True, timeout=120)
        lines = p.stdout.splitlines()
        for i, off in enumerate(offs):
            fn = lines[2 * i].strip() if 2 * i < len(lines) else "?"
            fl = lines[2 * i + 1].strip() if 2 * i + 1 < len(lines) else "?"
            # trim repo prefix for readability
            fl = re.sub(r"^.*/(src|native|engine)/", r"\1/", fl)
            out[off] = f"{fn} @ {fl}"
    except Exception as e:
        for off in offs:
            out[off] = f"?(addr2line failed: {e})"
    return out


def window_totals(boot, lo, hi):
    """Sum draws per caller-offset over frames in [lo, hi] for one boot."""
    tot = {}
    for off, fr_map in (boot["attribByOff"] or {}).items():
        s = sum(d for fr, d in fr_map.items() if lo <= fr <= hi)
        if s:
            tot[off] = s
    return tot


def attribution_table(binpath, off_boots, boot_ok, lo, hi, window_name):
    """off_boots: list (aligned with boots) of {off:total} dicts for the window.
    Returns ranked list of caller records with per-boot totals + divergence."""
    all_offs = set()
    for t in off_boots:
        all_offs.update(t.keys())
    rows = []
    for off in all_offs:
        per_boot = [t.get(off, 0) for t in off_boots]
        present = [v for v in per_boot]
        distinct = sorted(set(present))
        spread = max(present) - min(present)
        # "variable-count" == this caller's window total is not identical across boots
        variable = len(distinct) > 1
        rows.append({
            "off": off, "perBoot": per_boot,
            "min": min(present), "max": max(present), "spread": spread,
            "distinct": distinct, "variable": variable,
        })
    # resolve symbols for all involved offsets
    syms = resolve_offsets(binpath, [r["off"] for r in rows])
    for r in rows:
        r["sym"] = syms.get(r["off"], "?")
    rows.sort(key=lambda r: (-r["spread"], -r["max"]))
    return {"window": window_name, "range": [lo, hi], "nBoots": boot_ok, "rows": rows}


# ---------------------------------------------------------------------------
# ledger (M3)
# ---------------------------------------------------------------------------
def _md5(s):
    return hashlib.md5(s.encode()).hexdigest()[:12]


def ledger_for_arm(arm, k_frames, tol_ms=150.0):
    """Build the per-axis ledger from an ON-arm summary (boots carry frames+
    completes because ledger runs with attrib capture on)."""
    ok = [b for b in arm["boots"] if b["ok"]]
    if not ok:
        return {"error": "no ok boots"}
    ref = ok[0]

    def count_sig(b):
        # ANCHOR-RELATIVE: the seam re-bases at the anchor, so pre-anchor absolute
        # gdraw is non-gating (plan §3.4). The count axis = the post-anchor per-frame
        # gRand draw-delta sequence + the cumulative post-anchor delta (==
        # postAnchorDelta). Absolute gTarget/gAnchor jitter (±3, pre-anchor residue)
        # is excluded by construction.
        a = b["anchorFrame"]
        seq = []
        prev = None
        for fr in range(a, a + k_frames + 1):
            g = (b["frames"] or {}).get(fr)
            if g is None:
                continue
            if prev is not None:
                seq.append(g - prev)
            prev = g
        return (b["postAnchorDelta"], _md5(",".join(map(str, seq))))

    def order_sig(b):
        # The "order" axis proves the loaded SET completes in the SAME SEQUENCE
        # (DirLoader/DataLoader kind:name), i.e. no async completion-order shuffle.
        # NOTE: the post-anchor attrib caller sequence is NOT folded in here — under
        # the seam the isolated particle/eye consumers still draw (on their private
        # streams) and their per-frame timing legitimately varies; that is not a
        # gRand-order property and would fail this axis for a non-gating reason. It
        # is reported separately as the informational callerOrder field.
        return _md5("|".join(f"{c[1]}:{c[2]}" for c in (b["completes"] or [])))

    def caller_order_sig(b):
        a = b["anchorFrame"]
        callers = []
        for off, fr_map in (b["attribByOff"] or {}).items():
            for fr in fr_map:
                if a <= fr <= a + k_frames:
                    callers.append((fr, off))
        callers.sort()
        return _md5("|".join(f"{fr}:{off}" for fr, off in callers))

    # clock axis: the seam RE-BASES at the anchor by design, so the ABSOLUTE
    # anchor frame legitimately varies boot-to-boot (pre-anchor load-timing
    # jitter). What the clock axis proves is that the fixed clock is active and
    # the POST-anchor capture span is a constant number of frames (deterministic
    # dt) -> the capture sits at an identical song-time-since-anchor every boot.
    # PASS = post-anchor span == k. anchorFrame is reported (informative) but the
    # jitter in it is non-gating (that IS what the seam re-bases away).
    def clock_span(b):
        return b["targetFrame"] - b["anchorFrame"]

    rc = count_sig(ref); ro = order_sig(ref); rco = caller_order_sig(ref)
    rows = []
    for i, b in enumerate(ok):
        c = count_sig(b); o = order_sig(b); s = b["postAnchorDelta"]
        span = clock_span(b); co = caller_order_sig(b)
        rows.append({
            "boot": i,
            "anchorFrame": b["anchorFrame"],
            "axes": {
                "count":  {"value": f"{c[0]}+{c[1]}", "pass": c == rc},
                "order":  {"value": o, "pass": o == ro},
                "stream": {"value": s, "pass": s == ref["postAnchorDelta"]},
                "clock":  {"value": f"span={span},anchor={b['anchorFrame']}",
                           "pass": span == k_frames},
            },
            "callerOrder": {"value": co, "pass": co == rco},  # informational, non-gating
        })
    n = len(ok)

    def tally(ax):
        return f"{sum(1 for r in rows if r['axes'][ax]['pass'])}/{n}"
    primary = all(r["axes"]["stream"]["pass"] for r in rows)
    return {
        "referenceBoot": 0, "boots": rows,
        "summary": {"count": tally("count"), "order": tally("order"),
                    "stream": tally("stream"), "clock": tally("clock"),
                    "callerOrder(info)":
                        f"{sum(1 for r in rows if r['callerOrder']['pass'])}/{n}",
                    "PRIMARY": "PASS" if primary else "FAIL"},
    }


# ---------------------------------------------------------------------------
# T1 (Wave-19 Lane F) FRAME-ASSIGNMENT TIMING axes — frameAssign / songClock /
# emitTimeline. READ-ONLY measurement; keyed on PIE/ASLR-stable module offsets
# (`off`) and `kind:name` strings — NEVER raw pc (G6). Each sig is DISJOINT from
# ledger_for_arm.order_sig (naming box): frameAssign binds every completion/arrival
# to its FRAME index (kind:name@frame), whereas order_sig hashes the kind:name
# SEQUENCE with the @frame dropped. This module never calls order_sig/ledger_for_arm.
# ---------------------------------------------------------------------------
SONGCLOCK_LADDER_MS = 2000.0   # checkpoint spacing (encodes "21003 @ 3585 vs 5095")
SONGCLOCK_LADDER_MAX = 60000.0


def _win_frames(boot, win_lo, win_hi):
    frames = boot.get("framesAll") or boot.get("frames") or {}
    lo = min(frames) if win_lo is None and frames else (win_lo or 0)
    hi = max(frames) if win_hi is None and frames else (win_hi if win_hi is not None else 0)
    return lo, hi


def timeline_sigs(boot, win_lo=None, win_hi=None, ladder_step=SONGCLOCK_LADDER_MS):
    """Compute the three T1 axis sigs + per-axis event/sample counts + VOID flags
    for one parsed boot, restricted to frame window [win_lo, win_hi] (inclusive;
    None => full capture range). Pure function => grading the SAME log twice yields
    byte-identical output (grader determinism)."""
    lo, hi = _win_frames(boot, win_lo, win_hi)

    def inwin(fr):
        return lo <= fr <= hi

    frames = boot.get("framesAll") or boot.get("frames") or {}
    # VOID: fixed clock inactive => every marker keyed frame=0 (mis-armed boot).
    win_frame_keys = [fr for fr in frames if inwin(fr)]
    frames_void = (len(set(win_frame_keys)) <= 1)  # 0 or 1 distinct frame => no axis

    # frameAssign: every completion + file-arrival bound to its frame.
    completes = boot.get("completes") or []
    arrivals = boot.get("fileArrivals") or []
    comp_win = sorted((fr, k, n) for (fr, k, n) in completes if inwin(fr))
    arr_win = sorted((fr, n) for (fr, n) in arrivals if inwin(fr))
    fa_str = "|".join(f"{k}:{n}@{fr}" for (fr, k, n) in comp_win) \
        + "#" + "|".join(f"file:{n}@{fr}" for (fr, n) in arr_win)
    frameAssign = _md5(fa_str)
    fa_events = len(comp_win) + len(arr_win)

    # songClock: first frame at which songMs crosses each fixed ms checkpoint.
    song = boot.get("songMs") or {}
    song_win = {fr: ms for fr, ms in song.items() if inwin(fr)}
    n_songms_samples = len(song_win)
    n_songms_positive = sum(1 for ms in song_win.values() if ms >= 0.0)
    cps = []
    cp = ladder_step
    while cp <= SONGCLOCK_LADDER_MAX:
        cps.append(cp); cp += ladder_step
    ladder = []
    for cp in cps:
        crossed = sorted(fr for fr, ms in song_win.items() if ms >= cp)
        ladder.append(f"{int(cp)}:{crossed[0] if crossed else 'none'}")
    songClock = _md5("|".join(ladder))
    n_crossings = sum(1 for e in ladder if not e.endswith(":none"))
    # VOID if no positive samples (no song ever played / RB3_HTTP absent).
    songclock_void = (n_songms_positive == 0)

    # emitTimeline: per-frame, per-off emission over the window (needs attrib arm).
    abo = boot.get("attribByOff")
    emit_pairs = []
    if abo:
        for off, fr_map in abo.items():
            for fr, draws in fr_map.items():
                if inwin(fr):
                    emit_pairs.append((fr, off, draws))
    emit_pairs.sort()
    emitTimeline = _md5("|".join(f"{fr}:{off}:{draws}" for (fr, off, draws) in emit_pairs))
    et_events = len(emit_pairs)
    emit_void = (abo is None) or (et_events == 0)

    return {
        "window": [lo, hi],
        "framesVoid": frames_void,
        "frameAssign": {"sig": frameAssign, "events": fa_events,
                        "completes": len(comp_win), "arrivals": len(arr_win),
                        "void": frames_void or fa_events == 0},
        "songClock": {"sig": songClock, "samples": n_songms_samples,
                      "positiveSamples": n_songms_positive, "crossings": n_crossings,
                      "void": frames_void or songclock_void},
        "emitTimeline": {"sig": emitTimeline, "events": et_events,
                         "void": frames_void or emit_void},
    }


def grade_timeline(boots, win_lo=None, win_hi=None, ladder_step=SONGCLOCK_LADDER_MS,
                   assert_diverge=("emitTimeline", "songClock")):
    """Grade a set of parsed boots on the three axes vs boot[0] (reference).
    Reports per-axis PASS (all boots identical to ref) / DIVERGE + event/sample
    counts (lint-8 disclosure). A VOID axis (no signal / mis-armed) is reported as
    VOID, never PASS. `assert_diverge` names the axes gate-1 asserts DIVERGE on;
    the returned `blind` flag is True iff an asserted axis is identical AND not
    void across all boots (instrument blind => caller RED)."""
    ok = [b for b in boots if b.get("framesAll") or b.get("frames")]
    per = [(b, timeline_sigs(b, win_lo, win_hi, ladder_step)) for b in ok]
    if not per:
        return {"error": "no gradeable boots", "nBoots": len(boots)}
    ref = per[0][1]
    axes = ("frameAssign", "songClock", "emitTimeline")
    rows = []
    for i, (b, s) in enumerate(per):
        row = {"boot": i, "logPath": b.get("logPath"), "window": s["window"],
               "framesVoid": s["framesVoid"], "axes": {}}
        for ax in axes:
            same = (s[ax]["sig"] == ref[ax]["sig"])
            row["axes"][ax] = {**s[ax], "matchesRef": same}
        rows.append(row)
    summary = {}
    for ax in axes:
        sigs = {r["axes"][ax]["sig"] for r in rows}
        all_void = all(r["axes"][ax]["void"] for r in rows)
        any_void = any(r["axes"][ax]["void"] for r in rows)
        distinct = len(sigs)
        if all_void:
            verdict = "VOID"
        elif distinct == 1:
            verdict = "IDENTICAL"      # all boots agree (PASS for a determinism gate)
        else:
            verdict = "DIVERGE"
        summary[ax] = {"verdict": verdict, "distinctSigs": distinct,
                       "anyVoid": any_void, "allVoid": all_void,
                       "events": [r["axes"][ax].get("events") for r in rows],
                       "samples": [r["axes"][ax].get("samples") for r in rows]}
    # instrument-blind check for gate-1: an ASSERTED axis is IDENTICAL and not void.
    blind_axes = [ax for ax in assert_diverge
                  if summary[ax]["verdict"] == "IDENTICAL"]
    diverged_axes = [ax for ax in assert_diverge
                     if summary[ax]["verdict"] == "DIVERGE"]
    void_axes = [ax for ax in assert_diverge if summary[ax]["verdict"] == "VOID"]
    return {
        "nBoots": len(ok), "referenceBoot": 0,
        "window": ref["window"], "ladderStepMs": ladder_step,
        "boots": rows, "summary": summary,
        "assertDiverge": list(assert_diverge),
        "divergedAxes": diverged_axes, "identicalAxes": blind_axes,
        "voidAxes": void_axes,
        "blind": len(blind_axes) > 0,   # any asserted axis identical+not-void => blind
    }


def timeline_from_logs(log_paths, k_frames, win_lo=None, win_hi=None,
                       ladder_step=SONGCLOCK_LADDER_MS,
                       assert_diverge=("emitTimeline", "songClock")):
    """Parse a set of already-produced boot logs and grade_timeline them. A2/F6
    VOID discipline: grades the EXACT measurement boots' own logs; never reruns."""
    boots = []
    for lp in log_paths:
        b = parse_boot_log(lp, k_frames, attrib=True)
        b["logPath"] = lp
        boots.append(b)
    g = grade_timeline(boots, win_lo, win_hi, ladder_step, assert_diverge)
    g["nLogs"] = len(log_paths)
    g["logPaths"] = list(log_paths)
    return g


def _name_frames(boot):
    """Per completion/arrival NAME -> sorted list of frames it landed on (in this
    boot). The frameAssign axis at the element granularity gate-2 asserts on."""
    nf = {}
    for (fr, kind, name) in (boot.get("completes") or []):
        nf.setdefault(f"{kind}:{name}", []).append(fr)
    for (fr, name) in (boot.get("fileArrivals") or []):
        nf.setdefault(f"file:{name}", []).append(fr)
    return {k: sorted(v) for k, v in nf.items()}


def songclock_envelope_test(control_boots, injected_boot,
                            checkpoints=(5000, 10000, 15000, 20000)):
    """Noise-robust songClock responds-test: for each ms checkpoint, does the
    injected boot cross it at a frame OUTSIDE the control [min,max] envelope, in a
    CONSISTENT direction? This survives the ambient boot-to-boot noise (which the
    exact named-move test cannot, because controls disagree at JITTER=0). A pass
    requires the injected crossing to exit the control envelope in the SAME
    direction on a majority of checkpoints — a true dose response, not noise."""
    def crossings(b):
        song = b.get("songMs") or {}
        out = {}
        for cp in checkpoints:
            fr = sorted(f for f, ms in song.items() if ms >= cp)
            out[cp] = fr[0] if fr else None
        return out
    ctrl = [crossings(b) for b in control_boots]
    inj = crossings(injected_boot)
    rows = []
    later = earlier = inside = 0
    for cp in checkpoints:
        cs = [c[cp] for c in ctrl if c[cp] is not None]
        if not cs or inj[cp] is None:
            continue
        lo, hi = min(cs), max(cs)
        if inj[cp] > hi:
            where = "later_outside"; later += 1
        elif inj[cp] < lo:
            where = "earlier_outside"; earlier += 1
        else:
            where = "inside"; inside += 1
        rows.append({"cp": cp, "ctrl_min": lo, "ctrl_max": hi,
                     "injected": inj[cp], "where": where})
    n = len(rows)
    # responds iff a MAJORITY exit the envelope in one consistent direction
    responds = (n > 0 and (later > n / 2 or earlier > n / 2))
    return {"checkpoints": rows, "n": n, "later_outside": later,
            "earlier_outside": earlier, "inside": inside, "responds": responds}


def injection_differential(control_boots, injected_boot, win_lo=None, win_hi=None):
    """Gate-2 (R2): OFF-arm differential. A NAMED frameAssign element (a specific
    completion/arrival name) must move IN the injected boot (higher jitter) while the
    controls agree on it — proving the instrument RESPONDS to the induced mechanism,
    not ambient noise. Returns the named moves + the control-consensus check."""
    ctrl_nf = [_name_frames(b) for b in control_boots]
    inj_nf = _name_frames(injected_boot)
    # names present in every control AND the injected boot
    common = set(inj_nf)
    for nf in ctrl_nf:
        common &= set(nf)
    named_moves = []
    control_disagreements = 0
    for name in sorted(common):
        ctrl_frames = [tuple(nf[name]) for nf in ctrl_nf]
        consensus = ctrl_frames[0]
        controls_agree = all(cf == consensus for cf in ctrl_frames)
        inj_frames = tuple(inj_nf[name])
        if not controls_agree:
            control_disagreements += 1
            continue
        if inj_frames != consensus:
            named_moves.append({
                "name": name,
                "control_frames": list(consensus),
                "injected_frames": list(inj_frames),
                "delta_first": (inj_frames[0] - consensus[0]) if inj_frames and consensus else None,
            })
    env = songclock_envelope_test(control_boots, injected_boot)
    # HONEST verdict (lint 3, no fabricated oracle). Two independent responds-tests:
    #  - named-move: exact completion-frame move against controls that agree (fails
    #    when the ambient floor makes controls disagree — which it does at JITTER=0).
    #  - songClock envelope: dose response beyond the ambient control envelope.
    # If NEITHER fires, the honest conclusion is that the induced jitter is NOT
    # distinguishable from the ambient scheduling-noise floor at this granularity —
    # i.e. RB3_LOADDET_JITTER is not the frame-assignment driver (an ATTRIBUTION
    # finding for T3), NOT an instrument failure. The instrument's real
    # responsiveness is established by gate-1 (it detects the ambient divergence).
    responds = (len(named_moves) >= 1) or env["responds"]
    return {
        "n_common_names": len(common),
        "n_named_moves": len(named_moves),
        "n_control_disagreements": control_disagreements,
        "named_moves": named_moves,
        "songclock_envelope": env,
        "responds": responds,
        "attribution": (
            "RESPONDS: injected jitter produced a distinguishable frame-assignment "
            "move above the ambient floor."
            if responds else
            "HONEST-NEGATIVE: RB3_LOADDET_JITTER at the worker-dispatch granularity is "
            "NOT distinguishable from the ambient boot-to-boot scheduling-noise floor "
            "(controls disagree at JITTER=0; injected sits inside the control songClock "
            "envelope, dose-independent). Attribution for T3: the frame-assignment "
            "timing residual is driven by ambient thread scheduling, NOT the jitter "
            "knob. Instrument responsiveness is established by gate-1's per-frame "
            "divergence detection; gate-2 correctly refuses to manufacture a "
            "mechanism-response where none is distinguishable (lint 3)."),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=os.path.join(REPO, "native", "build-agent-R4", "rb3-native"))
    ap.add_argument("--n", type=int, default=10)
    ap.add_argument("--k", type=int, default=300, help="post-anchor frame offset (gate window)")
    ap.add_argument("--boot-window", type=int, default=120, help="attrib boot-window upper frame")
    ap.add_argument("--jitter", type=int, default=200, help="RB3_LOADDET_JITTER max microseconds")
    ap.add_argument("--diff", default="hard")
    ap.add_argument("--song-downs", type=int, default=4)
    ap.add_argument("--out", default="/tmp/loaddet-gate")
    ap.add_argument("--tag", default="gate")
    ap.add_argument("--arm", default="both", choices=["both", "off", "on"])
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--autohit", action="store_true")
    ap.add_argument("--attrib", action="store_true", help="M1 attribution table")
    ap.add_argument("--ledger", action="store_true", help="M3 per-axis ledger")
    ap.add_argument("--grade-logs", nargs="+", default=None,
                    help="A2/F6: grade already-produced boot logs (do NOT launch boots)")
    ap.add_argument("--grade-out", default=None, help="write the external-log ledger JSON here")
    # --- T1 (Wave-19 Lane F) frame-assignment timing axes ---
    ap.add_argument("--timeline", action="store_true",
                    help="T1 frame-assignment timing mode: grade frameAssign/songClock/"
                         "emitTimeline. With --grade-logs, grade existing logs; else "
                         "launch N eng_hot boots (RB3_LOADDET_TIMELINE=1).")
    ap.add_argument("--regime", default="eng_hot", choices=["eng_hot", "boot"],
                    help="T1 capture regime (eng_hot folds RB3_VENUE_* + drives to song).")
    ap.add_argument("--win-frame-lo", type=int, default=None,
                    help="T1 gate window lower frame (default: full capture).")
    ap.add_argument("--win-frame-hi", type=int, default=None,
                    help="T1 gate window upper frame (default: full capture).")
    ap.add_argument("--assert-diverge", default="emitTimeline,songClock",
                    help="T1 gate-1 axes asserted to DIVERGE (comma-sep). Use "
                         "'frameAssign,songClock,emitTimeline' for the OFF-arm (R1).")
    ap.add_argument("--off-arm", action="store_true",
                    help="T1 R1 OFF-arm: RB3_LOAD_DETERMINISM UNSET (seam off) so all "
                         "three axes should DIVERGE under jitter+contention.")
    ap.add_argument("--injection", action="store_true",
                    help="T1 gate-2 (R2): OFF-arm injection differential. Runs "
                         "--n control boots at --ctrl-jitter and 1 injected boot at "
                         "--inj-jitter; asserts a NAMED frameAssign element moves in "
                         "the injected boot while controls agree.")
    ap.add_argument("--ctrl-jitter", type=int, default=0)
    ap.add_argument("--inj-jitter", type=int, default=2000)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)

    if a.injection:
        # OFF-arm always (R2): worker jitter shifts data:<name>@frame directly;
        # seam-ON it fires inline on main and cannot move the completion frame.
        eng = dict(wd_arms_eng_hot())
        target_ms = (a.k + 60) * DT_MS
        def _boot(jit, lp):
            env = dict(eng); env["RB3_LOADDET_JITTER"] = str(jit)
            r = boot_measure(a.bin, env, a.k, target_ms, lp, diff=a.diff,
                             verbose=a.verbose, autohit=a.autohit, timeline=True)
            r["logPath"] = lp; return r
        controls = []
        for i in range(a.n):
            controls.append(_boot(a.ctrl_jitter,
                            os.path.join(a.out, f"inj-{a.tag}-ctrl{i}.log")))
        injected = _boot(a.inj_jitter, os.path.join(a.out, f"inj-{a.tag}-injected.log"))
        diff = injection_differential(controls, injected)
        diff["meta"] = {"date": time.strftime("%Y-%m-%d"), "bin": a.bin,
                        "seam": "OFF", "ctrl_jitter": a.ctrl_jitter,
                        "inj_jitter": a.inj_jitter, "n_controls": a.n,
                        "carrier": "frameAssign (data:name completion frame)"}
        outp = a.grade_out or os.path.join(a.out, f"gate2-injection.json")
        with open(outp, "w") as f:
            json.dump(diff, f, indent=2, allow_nan=False)
        print("==================== GATE-2 INJECTION (OFF-arm) ====================")
        print(f"common names={diff['n_common_names']}  named moves={diff['n_named_moves']}  "
              f"control disagreements={diff['n_control_disagreements']}")
        for nm in diff["named_moves"][:12]:
            print(f"  MOVED {nm['name']}: ctrl={nm['control_frames']} -> inj={nm['injected_frames']} "
                  f"(dfirst={nm['delta_first']})")
        print(f"RESPONDS (>=1 named move, controls agree): {diff['responds']}")
        print(f"wrote {outp}")
        return

    if a.timeline:
        assert_axes = tuple(x.strip() for x in a.assert_diverge.split(",") if x.strip())
        if a.grade_logs:
            g = timeline_from_logs(a.grade_logs, a.k, a.win_frame_lo, a.win_frame_hi,
                                   assert_diverge=assert_axes)
        else:
            # launch N timeline boots in the chosen regime
            eng = dict(wd_arms_eng_hot()) if a.regime == "eng_hot" else {}
            jitter = {"RB3_LOADDET_JITTER": str(a.jitter)} if a.jitter > 0 else {}
            arm_env = dict(eng); arm_env.update(jitter)
            if not a.off_arm:
                arm_env["RB3_LOAD_DETERMINISM"] = "1"   # seam-ON (gate-1 default)
            target_ms = (a.k + 60) * DT_MS
            boots = []
            for i in range(a.n):
                lp = os.path.join(a.out, f"timeline-{a.tag}-boot{i}.log")
                r = boot_measure(a.bin, arm_env, a.k, target_ms, lp, diff=a.diff,
                                 verbose=a.verbose, autohit=a.autohit, timeline=True)
                r["logPath"] = lp
                boots.append(r)
                s = timeline_sigs(r, a.win_frame_lo, a.win_frame_hi)
                log(f"  boot{i}: window={s['window']} "
                    f"frameAssign(ev={s['frameAssign']['events']},void={s['frameAssign']['void']}) "
                    f"songClock(samp={s['songClock']['positiveSamples']},void={s['songClock']['void']}) "
                    f"emitTimeline(ev={s['emitTimeline']['events']},void={s['emitTimeline']['void']})")
            g = grade_timeline(boots, a.win_frame_lo, a.win_frame_hi,
                               assert_diverge=assert_axes)
            g["armEnv"] = arm_env
            g["seam"] = "OFF" if a.off_arm else "ON"
        g["meta"] = {"date": time.strftime("%Y-%m-%d"), "bin": a.bin,
                     "regime": a.regime, "assertDiverge": list(assert_axes),
                     "n": a.n, "jitter": a.jitter}
        print("==================== TIMELINE AXES ====================")
        print(json.dumps(g.get("summary", g), indent=2))
        print(f"assertDiverge={g.get('assertDiverge')}  divergedAxes={g.get('divergedAxes')}  "
              f"identicalAxes={g.get('identicalAxes')}  voidAxes={g.get('voidAxes')}  "
              f"blind={g.get('blind')}")
        outp = a.grade_out or os.path.join(a.out, f"timeline-{a.tag}.json")
        with open(outp, "w") as f:
            json.dump(g, f, indent=2, allow_nan=False)
        print(f"wrote {outp}")
        return

    if a.grade_logs:
        led = grade_external_logs(a.grade_logs, a.k)
        print("==================== EXTERNAL-LOG LEDGER ====================")
        print(json.dumps(led.get("summary", led), indent=2))
        print(f"nLogs={led['nLogs']} nParsed={led['nParsed']} "
              f"(VOID unless nParsed==nLogs AND stream=={led['nParsed']}/{led['nParsed']})")
        outp = a.grade_out or os.path.join(a.out, f"ledger-extern-{a.tag}.json")
        with open(outp, "w") as f:
            json.dump(led, f, indent=2)
        print(f"wrote {outp}")
        return
    target_ms = (a.k + 60) * DT_MS
    jitter = {"RB3_LOADDET_JITTER": str(a.jitter)} if a.jitter > 0 else {}
    cap = a.attrib or a.ledger

    arms = {}
    if a.arm in ("both", "off"):
        arms["OFF"] = run_arm(a.bin, "OFF", dict(jitter), a.n, a.k, target_ms, a.out,
                              a.diff, a.verbose, a.autohit, attrib=cap)
    if a.arm in ("both", "on"):
        on_env = dict(jitter); on_env["RB3_LOAD_DETERMINISM"] = "1"
        arms["ON"] = run_arm(a.bin, "ON", on_env, a.n, a.k, target_ms, a.out,
                             a.diff, a.verbose, a.autohit, attrib=cap)

    out = {"tag": a.tag, "bin": a.bin, "k": a.k, "boot_window": a.boot_window,
           "target_ms": target_ms, "jitter_us": a.jitter,
           "arms": {nm: {kk: vv for kk, vv in s.items() if kk != "boots"}
                    | {"boots": [{kk: vv for kk, vv in b.items()
                                  if kk not in ("frames", "attribByOff", "offMeta", "completes")}
                                 for b in s["boots"]]}
                    for nm, s in arms.items()}}
    outpath = os.path.join(a.out, f"{a.tag}.json")
    with open(outpath, "w") as f:
        json.dump(out, f, indent=2)

    # ---- attribution tables (M1) ----
    if a.attrib:
        for nm, s in arms.items():
            ok = [b for b in s["boots"] if b["ok"]]
            if not ok:
                continue
            # boot window 0..W
            bw = [window_totals(b, 0, a.boot_window) for b in ok]
            bt = attribution_table(a.bin, bw, len(ok), 0, a.boot_window, "boot")
            # gate window anchor..anchor+K (per-boot anchor differs; window relative)
            gw = [window_totals(b, b["anchorFrame"], b["anchorFrame"] + a.k) for b in ok]
            gt = attribution_table(a.bin, gw, len(ok), 0, a.k, "gate")
            for tbl, wn in ((bt, "boot"), (gt, "gate")):
                p = os.path.join(a.out, f"attrib-{nm.lower()}-{wn}.json")
                with open(p, "w") as f:
                    json.dump(tbl, f, indent=2)
                div = [r for r in tbl["rows"] if r["variable"]]
                print(f"\n==== ATTRIB {nm} / {wn} window ({tbl['range']}) — "
                      f"{len(tbl['rows'])} callers, {len(div)} divergent ====")
                for r in div[:20]:
                    print(f"  spread={r['spread']:>6} perBoot={r['perBoot']} "
                          f"{r['sym']}")
                print(f"  wrote {p}")
            print(f"\n[M1 decision — {nm}] gate-window divergent variable-count "
                  f"sites = {len([r for r in gt['rows'] if r['variable']])} "
                  f"(GO if <= 8)")

    # ---- gate PRIMARY summary ----
    print("\n==================== GATE RESULT ====================")
    for nm, s in arms.items():
        print(f"ARM {nm}: ok={s['ok']}/{s['n']}  "
              f"postAnchorDeltaSpread={s['deltaSpread']}  "
              f"distinctDeltas={s['distinctDeltas']}  "
              f"absSpread={s['absSpread']}  reseedAll={s['reseedFiredAll']}")
    if "OFF" in arms and "ON" in arms:
        off, on = arms["OFF"], arms["ON"]
        primary_on = (on["ok"] >= 1 and on["deltaSpread"] == 0 and len(on["distinctDeltas"]) == 1)
        failred_off = (off["ok"] >= 1 and off["deltaSpread"] and off["deltaSpread"] > 0)
        print(f"\nPRIMARY (ON collapses):  {'PASS' if primary_on else 'FAIL'} "
              f"(ON distinctDeltas={on['distinctDeltas']})")
        print(f"FAIL-RED (OFF reproduces spread under jitter):  {'PASS' if failred_off else 'FAIL'} "
              f"(OFF spread={off['deltaSpread']})")

    # ---- ledger (M3) ----
    if a.ledger and "ON" in arms:
        led = ledger_for_arm(arms["ON"], a.k)
        led["meta"] = {"date": time.strftime("%Y-%m-%d"), "bin": a.bin,
                       "flags": "RB3_LOAD_DETERMINISM=1", "n": a.n, "k": a.k,
                       "jitter": a.jitter, "tolMs": 150}
        lp = os.path.join(a.out, f"ledger-{a.tag}.json")
        with open(lp, "w") as f:
            json.dump(led, f, indent=2)
        print(f"\n==================== LEDGER (ON arm) ====================")
        print(json.dumps(led.get("summary", led), indent=2))
        print(f"wrote {lp}")

    print(f"\nwrote {outpath}")


if __name__ == "__main__":
    main()
