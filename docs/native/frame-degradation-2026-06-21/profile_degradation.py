#!/usr/bin/env python3
"""profile_degradation.py — reproduce + profile in-song frame-rate degradation.

Launches rb3-native (worktree binary) headless with RB3_FRAME_TRACE on, drives
to gameplay via the SAME pad-verb logic as keyboard-to-gameplay.py (imported),
then HOLDS the process alive and samples per-frame dt (from the frame trace) and
process RSS (/proc/<pid>/status) every few seconds for N seconds of gameplay.

Output: a dt+RSS-over-time table + the raw frame-trace JSONL path, so we can
decide LEAK (RSS climbs) vs CPU-GROWTH (dt climbs, RSS flat) and pin onset/rate.
"""
import argparse, json, os, signal, subprocess, sys, time

SCRIPTS = "/home/free/code/milohax/rb3/scripts/native"
sys.path.insert(0, SCRIPTS)
import importlib.util
spec = importlib.util.spec_from_file_location("k2g", os.path.join(SCRIPTS, "keyboard-to-gameplay.py"))
k2g = importlib.util.module_from_spec(spec); spec.loader.exec_module(k2g)

REPO = "/home/free/code/milohax/rb3"


def rss_kb(pid):
    try:
        with open(f"/proc/{pid}/status") as f:
            vm_rss = vm_hwm = 0
            for line in f:
                if line.startswith("VmRSS:"): vm_rss = int(line.split()[1])
                elif line.startswith("VmHWM:"): vm_hwm = int(line.split()[1])
            return vm_rss, vm_hwm
    except Exception:
        return -1, -1


def read_trace_tail(path, last_n=400):
    """Return list of frame dicts (the last_n frames)."""
    out = []
    try:
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"): continue
                try: out.append(json.loads(line))
                except Exception: pass
    except FileNotFoundError:
        return []
    return out[-last_n:] if last_n else out


def drive_to_gameplay(port, proc, diff, song_downs, extra_env_label):
    """Replicate keyboard-to-gameplay.py's nav, returning True on game_screen."""
    log = k2g.log
    if k2g.wait_screen(port, lambda s: True, 40, proc) is None:
        log("FAIL: HTTP never came up"); return False
    log("HTTP up")
    # splash -> main_hub
    for _ in range(8):
        if k2g.health(port)[2] == "main_hub_screen": break
        k2g.press(port, k2g.START); k2g.drain_pad(port); time.sleep(0.6)
    if k2g.wait_screen(port, "main_hub_screen", 30, proc) is None:
        log("FAIL: no main_hub"); return False
    # main_hub -> song_select
    for _ in range(10):
        if k2g.health(port)[2] == "song_select_screen": break
        k2g.press(port, k2g.CONFIRM); k2g.drain_pad(port); time.sleep(0.7)
    if k2g.wait_screen(port, "song_select_screen", 40, proc) is None:
        log("FAIL: no song_select"); return False
    # pick a song: DDown a few times, confirm
    for _ in range(song_downs):
        k2g.press(port, k2g.DDOWN); k2g.drain_pad(port); time.sleep(0.25)
    k2g.press(port, k2g.CONFIRM); k2g.drain_pad(port); time.sleep(0.8)
    # crossing part/diff select -> ready -> game. Use the module's own flow by
    # spamming CONFIRM and watching for game_screen; set difficulty via verb.
    k2g.verb(port, f"difficulty:{k2g.DIFF_INDEX[diff]}")
    deadline = time.time() + 60
    while time.time() < deadline:
        h = k2g.health(port)
        if h and h[2] == "game_screen": break
        k2g.press(port, k2g.CONFIRM); k2g.drain_pad(port); time.sleep(0.6)
    h = k2g.health(port)
    if not h or h[2] != "game_screen":
        log(f"FAIL: never reached game_screen (screen={h[2] if h else '?'})"); return False
    log("reached game_screen")
    k2g.verb(port, "nofail")
    k2g.verb(port, "autohit")
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--data", default=os.path.join(REPO, "orig-assets/extracted"))
    ap.add_argument("--overlay", default=os.path.join(REPO, "native/dta"))
    ap.add_argument("--diff", default="hard")
    ap.add_argument("--song-downs", type=int, default=4)
    ap.add_argument("--seconds", type=int, default=120, help="gameplay sampling window")
    ap.add_argument("--interval", type=float, default=4.0, help="sample period s")
    ap.add_argument("--trace", default="/tmp/rb3-degrade-trace.jsonl")
    ap.add_argument("--out", default="/tmp/rb3-degrade")
    ap.add_argument("--label", default="baseline")
    ap.add_argument("--extra-env", action="append", default=[],
                    help="KEY=VAL env to inject (subsystem A/B)")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    port = k2g.free_port()
    log_path = f"/tmp/rb3-degrade-{args.label}-{port}.log"
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_DTA_OVERLAY": args.overlay, "RB3_INPUT_DEBUG": "1",
                "RB3_FRAME_TRACE": args.trace})
    for kv in args.extra_env:
        k, v = kv.split("=", 1); env[k] = v
    if os.path.exists(args.trace): os.remove(args.trace)
    k2g.log(f"[{args.label}] launch {args.bin} port={port} extra={args.extra_env}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    samples = []
    try:
        if not drive_to_gameplay(port, proc, args.diff, args.song_downs, args.label):
            k2g.log("drive failed; aborting"); return 2
        gp_start = time.time()
        k2g.log(f"[{args.label}] gameplay reached; sampling for {args.seconds}s")
        last_frame = 0
        while time.time() - gp_start < args.seconds:
            if proc.poll() is not None:
                k2g.log(f"engine died (code {proc.returncode})"); break
            # keep autohit/nofail alive (some screens re-arm)
            k2g.verb(port, "autohit")
            h = k2g.health(port)
            rss, hwm = rss_kb(proc.pid)
            tail = read_trace_tail(args.trace, last_n=120)
            # mean dt over the most recent ~120 frames
            if tail:
                recent = tail[-60:]
                mean_dt = sum(f["dt"] for f in recent) / len(recent)
                # also bucket means for attribution
                def bm(key): return sum(f.get(key, 0) for f in recent) / len(recent)
                cur_frame = tail[-1]["f"]
                song_ms = tail[-1].get("?", None)
            else:
                mean_dt = -1; cur_frame = -1
                bm = lambda k: 0
            t = time.time() - gp_start
            rec = {
                "t": round(t, 1),
                "songMs": round(h[1]) if h else -1,
                "frame": cur_frame,
                "fps": round(1000.0 / mean_dt, 1) if mean_dt and mean_dt > 0 else -1,
                "dt_ms": round(mean_dt, 1),
                "rss_mb": round(rss / 1024.0, 1),
                "hwm_mb": round(hwm / 1024.0, 1),
                "texMs": round(bm("texMs"), 2), "texN": round(bm("texN"), 1),
                "meshMs": round(bm("meshMs"), 2), "meshN": round(bm("meshN"), 1),
                "unpackMs": round(bm("unpackMs"), 2), "unpackN": round(bm("unpackN"), 1),
                "pipeMs": round(bm("pipeMs"), 2),
                "objMs": round(bm("objMs"), 2),
                "primeMs": round(bm("primeMs"), 2),
                "lp": round(bm("lp"), 2), "lpu": round(bm("lpu"), 2),
                "pend": round(bm("pend"), 1),
                "ld": round(bm("ld"), 2), "st": round(bm("st"), 2),
            }
            samples.append(rec)
            k2g.log(f"[{args.label}] t={rec['t']:5.0f}s song={rec['songMs']:6d}ms "
                    f"fps={rec['fps']:5.1f} dt={rec['dt_ms']:6.1f}ms rss={rec['rss_mb']:6.1f}MB "
                    f"hwm={rec['hwm_mb']:6.1f} | tex={rec['texMs']:.1f}/{rec['texN']:.0f} "
                    f"mesh={rec['meshMs']:.1f}/{rec['meshN']:.0f} unpack={rec['unpackMs']:.1f}/{rec['unpackN']:.0f} "
                    f"pipe={rec['pipeMs']:.1f} obj={rec['objMs']:.1f} lp={rec['lp']:.1f} lpu={rec['lpu']:.1f} pend={rec['pend']:.0f}")
            time.sleep(args.interval)
    finally:
        out_json = os.path.join(args.out, f"samples_{args.label}.json")
        with open(out_json, "w") as f:
            json.dump({"label": args.label, "extra_env": args.extra_env, "samples": samples}, f, indent=2)
        k2g.log(f"[{args.label}] wrote {out_json}  (trace={args.trace}, log={log_path})")
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
