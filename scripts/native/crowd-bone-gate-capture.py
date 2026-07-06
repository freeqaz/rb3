#!/usr/bin/env python3
"""crowd-bone-gate-capture.py — W2.3.S1 crowd bone-source seam capture + gate.

SYS-1 bone-source half. The GPU bone palette for a skinned crowd/extras mesh is
composed from the mesh's GeomOwner bones (owner->BoneTransAt / BoneOffsetAt), never
from the drawn instance's OWN bones. The crowd renders correctly today only because
the draw-time RebindCrowdCharBonesToOwnSkeleton (Crowd.cpp) rebakes those owner
offsets; disable it (RB3_NO_CROWD_REBIND=1) and the composed skin flings past the
12u SKIN_CLAMP → the crowd shards / co-bunches.

This harness boots rb3-native headless, navigates to gameplay (reusing the proven
placement-gate nav), pins a wide venue shot so the crowd is in-frustum, and captures
the engine's RB3_CROWD_BONE_PROBE lines under TWO conditions:

  * BASELINE  — rebind ON  (RB3_NO_CROWD_REBIND unset): owner offsets rebaked clean.
  * CANDIDATE — rebind OFF (RB3_NO_CROWD_REBIND=1):      raw poisoned owner offsets.

Each condition ALSO runs with RB3_PLACEMENT_CONTRACT=1 by default so placement is
correct and the ONLY axis that varies between the two captures is the bone-source
seam (isolates the bone signal from the W2.1 placement co-location). Pass
--no-placement-contract to capture with the W1-era identity placement instead.

It then runs the CrowdBoneOracle (native/tests/test_crowd_bone_oracle.cpp via
rb3-tests, RealCaptureNotElevated) over the two probe logs, and prints the Q1/Q2
readout + the SHARED / SELF+POISON / MIXED DECISION.

On the UNCHANGED 6852caa build the candidate (rebind OFF) shard-drop SPIKES vs the
baseline (rebind ON) → the oracle goes RED. That red is the S1 fail-red. Under a
future W2.3-flag-ON build the candidate is RB3_CROWD_OWN_BONES=1 RB3_NO_CROWD_REBIND=1
and must turn GREEN.

Usage:
  python3 scripts/native/crowd-bone-gate-capture.py \
      [--bin native/build-agent-W2.3/rb3-native] \
      [--tests native/build-agent-W2.3/rb3-tests] \
      [--song-downs 4] [--out /tmp/rb3-crowd-bone-gate] \
      [--extra-env RB3_CROWD_OWN_BONES=1]   # applied to the CANDIDATE only \
      [--no-placement-contract] [--expect-red] [--verbose]

Exit code:
  0 = oracle PASS (shard-drop not elevated) — a fixed build.
  1 = oracle RED (shard-drop spiked with rebind OFF) — the current-build fail-red.
  2 = ERROR (never reached gameplay / no probe lines / capture failure).
Pass --expect-red to invert (exit 0 iff the gate went RED — fail-red audit).
"""
import argparse, json, os, re, signal, subprocess, sys, time
import importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

# Reuse the placement-gate harness's proven nav + wide-shot pin verbatim.
_spec = importlib.util.spec_from_file_location(
    "placement_gate", os.path.join(HERE, "placement-gate-capture.py"))
pg = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(pg)
k = pg.k
REPO = pg.REPO


def log(m): print(f"[crowd-bone-gate] {m}", flush=True)


PROBE_RE = re.compile(r"\[CROWD_BONE_PROBE\]")


def capture_condition(args, label, rebind_off, extra_env):
    """Boot rb3-native, nav to gameplay, pin a wide shot, and extract the
    [CROWD_BONE_PROBE] lines into out/<label>.probe.log. Returns the probe path or
    None on failure."""
    out_probe = os.path.join(args.out, f"{label}.probe.log")
    port = k.free_port()
    log_path = os.path.join(args.out, f"engine-{label}-{port}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_DTA_OVERLAY": args.overlay,
                "RB3_CROWD_BONE_PROBE": "1", "SKIN_CLAMP_PROBE": "1",
                "RB3_FIXED_CLOCK": "1"})
    if not args.no_placement_contract:
        env["RB3_PLACEMENT_CONTRACT"] = "1"
    if rebind_off:
        env["RB3_NO_CROWD_REBIND"] = "1"
    for kv in extra_env:
        if "=" in kv:
            key, val = kv.split("=", 1); env[key] = val
    log(f"[{label}] launching rb3-native (port {port}); rebind_off={rebind_off} "
        f"placement_contract={not args.no_placement_contract} extra={extra_env}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    ok = False
    try:
        if not pg.nav_to_gameplay(port, proc, args.song_downs,
                                  k.DIFF_INDEX[args.diff], args.verbose):
            log(f"[{label}] FAIL: never reached gameplay"); return None
        k.verb(port, "nofail")
        # let the song start so the crowd/venue scene is live.
        start = -1; dl = time.time() + 60
        while time.time() < dl:
            ip = k.dta(port, "{game is_playing}"); h = k.health(port)
            if ip is not None and int(ip) == 1 and start < 0 and h and h[1] >= 0:
                start = h[1]
            k.verb(port, "autohit")
            if start >= 0 and h and h[1] > start + 300:
                break
            time.sleep(0.5)
        log(f"[{label}] is_playing songMs={k.health(port)[1]:.0f}")
        # pin a wide shot so the crowd is in-frustum + reproducible.
        if not args.no_force_shot:
            dz = k.dta(port, "{rb3_director_disable 1}")
            if isinstance(dz, int) and dz == 1:
                pinned = None
                for cand in [s.strip() for s in args.shot.split(",") if s.strip()]:
                    resolved = pg.force_shot(port, cand)
                    if resolved:
                        pinned = resolved; break
                if pinned:
                    log(f"[{label}] pinned wide shot {pinned!r}")
        # settle several frames so every crowd instance poses + draws (the probe
        # fires for the first few instances per mesh name).
        for _ in range(12):
            k.verb(port, "autohit"); time.sleep(0.2)
        logf.flush()
        ok = True
    finally:
        logf.flush()
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM); proc.wait(timeout=10)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass
        logf.close()
    if not ok:
        return None
    nprobe = 0
    with open(log_path, "r", errors="replace") as lf, open(out_probe, "w") as pf:
        for line in lf:
            if PROBE_RE.search(line):
                pf.write(line if line.endswith("\n") else line + "\n")
                nprobe += 1
    log(f"[{label}] {nprobe} [CROWD_BONE_PROBE] lines -> {out_probe}")
    if nprobe == 0:
        log(f"[{label}] WARN: no CROWD_BONE_PROBE lines — crowd may be empty in this "
            f"venue, or the probe was not honored")
        return None
    return out_probe


def summarize(probe_path, label):
    """Print a per-mesh Q1/Q2 readout from a probe log (worst-instance reduction)."""
    if not probe_path or not os.path.exists(probe_path):
        return
    per = {}
    line_re = re.compile(
        r"tag=(\S+).*mesh='([^']*)'.*ownerEqMesh=(\d+).*diffInstance=(\d+).*"
        r"worstOwnerExtent=([\-0-9.]+)u.*worstOwnExtent=([\-0-9.]+)u")
    with open(probe_path, errors="replace") as f:
        for ln in f:
            m = line_re.search(ln)
            if not m: continue
            tag, mesh, oe, di, wo, wn = m.groups()
            if tag != "crowdextra": continue
            e = per.setdefault(mesh, {"shared": False, "wo": -1.0, "wn": -1.0, "n": 0})
            e["n"] += 1
            if int(di) > 0 or int(oe) == 0: e["shared"] = True
            e["wo"] = max(e["wo"], float(wo)); e["wn"] = max(e["wn"], float(wn))
    log(f"[{label}] per-crowd-mesh (worst instance): {len(per)} meshes")
    for mesh, e in sorted(per.items()):
        q1 = "SHARED" if e["shared"] else "SELF"
        q2 = "OWN-CLEAN" if e["wn"] <= 12.0 else "OWN-POISON"
        log(f"  {mesh:32s} inst={e['n']} {q1:6s} "
            f"ownerExtent={e['wo']:6.1f}u ownExtent={e['wn']:6.1f}u {q2}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", default=os.path.join(REPO, "native", "build-agent-W2.3", "rb3-native"))
    ap.add_argument("--tests", default=os.path.join(REPO, "native", "build-agent-W2.3", "rb3-tests"))
    ap.add_argument("--data", default=k.DEFAULT_DATA)
    ap.add_argument("--overlay", default=os.path.join(REPO, "native", "dta"))
    ap.add_argument("--diff", default="hard", choices=list(k.DIFF_INDEX))
    ap.add_argument("--song-downs", type=int, default=4)
    ap.add_argument("--shot", default=",".join(pg.DEFAULT_SHOTS))
    ap.add_argument("--no-force-shot", action="store_true")
    ap.add_argument("--no-placement-contract", action="store_true",
                    help="capture with W1-era identity placement (do NOT set "
                         "RB3_PLACEMENT_CONTRACT); by default it is ON so only the "
                         "bone-source axis varies between baseline and candidate")
    ap.add_argument("--extra-env", action="append", default=[],
                    help="KEY=VAL applied to the CANDIDATE capture only (e.g. a "
                         "future RB3_CROWD_OWN_BONES=1)")
    ap.add_argument("--out", default="/tmp/rb3-crowd-bone-gate")
    ap.add_argument("--expect-red", action="store_true")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}"); return 2
    os.makedirs(args.out, exist_ok=True)

    log("=== BASELINE (rebind ON) ===")
    base = capture_condition(args, "baseline", rebind_off=False, extra_env=[])
    log("=== CANDIDATE (rebind OFF) ===")
    cand = capture_condition(args, "candidate", rebind_off=True, extra_env=args.extra_env)
    if not base or not cand:
        log("FAIL: one or both captures produced no crowd probe lines"); return 2

    summarize(base, "baseline")
    summarize(cand, "candidate")

    if not os.path.exists(args.tests):
        log(f"NOTE: rb3-tests not found at {args.tests}; probe logs written, oracle not run.")
        log(f"  run: RB3_CROWD_BONE_BASELINE={base} RB3_CROWD_BONE_CANDIDATE={cand} \\")
        log(f"       {args.tests} --gtest_filter=CrowdBoneOracle.RealCaptureNotElevated")
        return 0
    tenv = dict(os.environ)
    tenv.update({"RB3_CROWD_BONE_BASELINE": base, "RB3_CROWD_BONE_CANDIDATE": cand})
    log("running oracle: CrowdBoneOracle.RealCaptureNotElevated")
    tp = subprocess.run([args.tests, "--gtest_filter=CrowdBoneOracle.RealCaptureNotElevated"],
                        env=tenv, capture_output=True, text=True)
    sys.stdout.write(tp.stdout)
    if tp.stderr: sys.stderr.write(tp.stderr)
    went_red = tp.returncode != 0
    if "SKIPPED" in tp.stdout and not went_red:
        log("ORACLE INCONCLUSIVE (capture did not reach a crowd frame)."); return 2
    if went_red:
        log("ORACLE RED — crowd shard-drop elevated with the rebind OFF (fail-red).")
    else:
        log("ORACLE GREEN — crowd shard-drop not elevated.")
    if args.expect_red:
        return 0 if went_red else 1
    return 1 if went_red else 0


if __name__ == "__main__":
    sys.exit(main())
