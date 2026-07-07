#!/usr/bin/env python3
"""d4-bonedump-sweep.py — Wave-17 R1-DOLPHIN Lane D4 native dense frame-sweep.

Wave-C (the frame-matched join). D3 delivered the machinery but its native+Wii
shells were captured at UN-SYNCED frames (D3_findings §Interpretation): the delta
necessarily included a pose-frame difference, so middle/ring divergence could not be
separated from a real skeleton delta. This harness closes that by capturing the
native shell band across a DENSE range of frames of its idle vignette clip (each dump
carries the member's CharClipDriver clip name + frame — the D4 probe extension), so
the join (interbone_diff.py --sweep) can:
  (a) pick the native frame that best-matches the Wii frozen frame on the KNOWN-shared
      bones (anchor+thumb), reporting the frame residual, and
  (b) bound the frame-explainable envelope per pair (min/max delta over the sweep),
      so a middle/ring divergence that SURVIVES the best frame is a real signal.

RB3_FIXED_CLOCK=1 advances the sim by a real 1/60s per frame (rb3_replay.cpp
RB3FixedClockDt), so the vignette clip progresses between dumps — a genuine sweep.

Usage:
  python3 scripts/native/d4-bonedump-sweep.py --out /tmp/d4sweep --n 24 --dt 0.35
"""
import argparse, json, os, sys, time, subprocess, shutil, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import importlib.util
spec = importlib.util.spec_from_file_location(
    "kbd2game", os.path.join(HERE, "keyboard-to-gameplay.py"))
k = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k)
REPO = k.REPO


def nm_vaddr(binpath, symbol):
    out = subprocess.check_output(["nm", binpath]).decode()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] == symbol and parts[1] in ("T", "t"):
            return int(parts[0], 16)
    return None


def api_call(port, symbol, static_addr):
    body = json.dumps({"symbol": symbol, "static_addr": static_addr, "args": []}).encode()
    req = urllib.request.Request(f"http://127.0.0.1:{port}/api/call", data=body,
                                 headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=20) as r:
            return json.loads(r.read().decode())
    except Exception as e:
        return {"error": str(e)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="/tmp/d4sweep")
    ap.add_argument("--bin", default=k.DEFAULT_BIN)
    ap.add_argument("--data", default=k.DEFAULT_DATA)
    ap.add_argument("--overlay", default=os.path.join(REPO, "native", "dta"))
    ap.add_argument("--n", type=int, default=24, help="number of sweep dumps")
    ap.add_argument("--dt", type=float, default=0.35, help="wall seconds between dumps")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    static_addr = nm_vaddr(args.bin, "rb3bp_dump_bones")
    if static_addr is None:
        k.log("FAIL: rb3bp_dump_bones not in symtab (rebuild?)"); return 1
    k.log(f"rb3bp_dump_bones static vaddr = 0x{static_addr:x}")
    probe_out = os.path.join(args.out, "native_bones_raw.json")
    port = k.free_port()
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_DATA": args.data, "RB3_DTA_OVERLAY": args.overlay,
        "RB3_INPUT_DEBUG": "1", "RB3_REPLAY_API": "1",
        "RB3_FIXED_CLOCK": "1", "RB3_CHAR_PREVIEW": "1",
        "RB3_BONE_PROBE_OUT": probe_out, "RB3_BONE_PROBE_SCENE": "shell:main_hub",
    })
    log_path = os.path.join(args.out, f"boot-{port}.log")
    logf = open(log_path, "w")
    k.log(f"launching rb3-native (port {port}); log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    frames = []
    try:
        if k.wait_screen(port, lambda s: True, 40, proc) is None:
            k.log("FAIL: HTTP never up"); return 1
        k.wait_screen(port, lambda s: s and s != "", 60, proc, args.verbose)
        for _ in range(8):
            if k.health(port)[2] == "main_hub_screen": break
            k.press(port, k.START); k.drain_pad(port); time.sleep(0.6)
        if k.wait_screen(port, "main_hub_screen", 30, proc, args.verbose) is None:
            k.log("FAIL: no main_hub_screen"); return 1
        k.log("reached main_hub_screen; settling char preview")
        for _ in range(20):
            cp = k.dta(port, "{rb3_char_probe 0}")
            if cp and "skinned=" in cp and "loading=0" in cp:
                break
            time.sleep(1.0)

        manifest = []
        for i in range(args.n):
            if os.path.exists(probe_out):
                os.remove(probe_out)
            r = api_call(port, "rb3bp_dump_bones", static_addr)
            data = r.get("data", r) if isinstance(r, dict) else {}
            n = int(data.get("ret_i", 0)) if isinstance(data, dict) else 0
            snap = os.path.join(args.out, f"sweep_{i:03d}.json")
            fr = None
            if n > 0 and os.path.exists(probe_out):
                shutil.copy(probe_out, snap)
                d = json.load(open(snap))
                # main.drv frame of slot0 as the sweep clock
                for m in d.get("members", []):
                    for dr in m.get("drivers", []):
                        if dr.get("driver") == "main.drv":
                            fr = dr.get("frame"); break
                    if fr is not None: break
                frames.append(fr)
                manifest.append({"i": i, "snap": os.path.basename(snap), "count": n,
                                 "main_frame": fr})
                k.log(f"  sweep[{i}] count={n} main.drv frame={fr}")
            else:
                k.log(f"  sweep[{i}] count={n} (no dump)")
            time.sleep(args.dt)

        json.dump({"port": port, "n": args.n, "dt": args.dt, "frames": frames,
                   "sweep": manifest}, open(os.path.join(args.out, "sweep_manifest.json"), "w"),
                  indent=1)
        uniq = sorted(set(f for f in frames if f is not None))
        k.log(f"SWEEP DONE: {len(frames)} dumps, {len(uniq)} distinct main.drv frames; "
              f"range [{uniq[0] if uniq else '-'} .. {uniq[-1] if uniq else '-'}]")
        return 0
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), 15)
        except Exception:
            pass


if __name__ == "__main__":
    sys.exit(main())
