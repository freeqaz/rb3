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

ANCHOR_RE = re.compile(r"\[LOADDET\] anchor frame=(\d+) gdraw=(\d+)")
FRAME_RE = re.compile(r"\[LOADDET\] frame=(\d+) gdraw=(\d+)")
RESEED_RE = re.compile(r"\[LOADDET\] reseed anchor=(\S+) seed=(\S+) gdrawBefore=(\d+)")
ATTRIB_RE = re.compile(r"\[LOADDET\] attrib frame=(-?\d+) pc=(\S+) off=(\S+) sym=(\S+) mod=(\S+) draws=(\d+)")
COMPLETE_RE = re.compile(r"\[LOADDET\] complete frame=(\d+) gdraw=(\d+) kind=(\S+) name=(\S+)")
DT_MS = 1000.0 / 60.0


def log(m):
    print(f"[loaddet-gate] {m}", flush=True)


def boot_measure(binpath, arm_env, k_frames, target_ms, log_path, diff="hard",
                 song_downs=4, verbose=False, autohit=False, attrib=False):
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

    anchor_frame = None; g_anchor = None; reseed_fired = False
    frames = {}
    attrib_by_off = {}   # off -> {frame: draws}
    off_meta = {}        # off -> (example_pc, mod)
    completes = []       # (frame, kind, name)
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
        result["reason"] = "no_anchor_marker"; return result
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
    })
    return result


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
        return (b["gTarget"], _md5(",".join(map(str, seq))))

    def order_sig(b):
        comp = _md5("|".join(f"{c[1]}:{c[2]}" for c in (b["completes"] or [])))
        # caller sequence (attrib): sorted per-frame offs, post-anchor window
        a = b["anchorFrame"]
        callers = []
        for off, fr_map in (b["attribByOff"] or {}).items():
            for fr in fr_map:
                if a <= fr <= a + k_frames:
                    callers.append((fr, off))
        callers.sort()
        return _md5(comp + "#" + _md5("|".join(f"{fr}:{off}" for fr, off in callers)))

    def clock_sig(b):
        return f"anchor={b['anchorFrame']}"

    rc = count_sig(ref); ro = order_sig(ref); rk = clock_sig(ref)
    rows = []
    for i, b in enumerate(ok):
        c = count_sig(b); o = order_sig(b); s = b["postAnchorDelta"]; cl = clock_sig(b)
        rows.append({
            "boot": i,
            "axes": {
                "count":  {"value": f"{c[0]}+{c[1]}", "pass": c == rc},
                "order":  {"value": o, "pass": o == ro},
                "stream": {"value": s, "pass": s == ref["postAnchorDelta"]},
                "clock":  {"value": cl, "pass": b["anchorFrame"] == ref["anchorFrame"]},
            },
        })
    n = len(ok)

    def tally(ax):
        return f"{sum(1 for r in rows if r['axes'][ax]['pass'])}/{n}"
    primary = all(r["axes"]["stream"]["pass"] for r in rows)
    return {
        "referenceBoot": 0, "boots": rows,
        "summary": {"count": tally("count"), "order": tally("order"),
                    "stream": tally("stream"), "clock": tally("clock"),
                    "PRIMARY": "PASS" if primary else "FAIL"},
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
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
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
