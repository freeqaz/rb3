#!/usr/bin/env python3
"""d3-bonedump-capture.py — Wave-17 R1-DOLPHIN Lane D3 native bone-probe harness.

Boots rb3-native headless with the bone probe enabled (RB3_BONE_PROBE_OUT), drives
it to a posed-band context matched to the Wii D2 capture, and POSTs
/api/call {symbol: rb3bp_dump_bones} to dump each band member's OWN hand-chain bone
world matrices. Output = the native half of the Wii-vs-native inter-bone delta table.

Context strategy (matched-clock, PLAN §3.6):
  1. First try the SHELL context (main_hub_screen) — the native equivalent of D2's
     ui/overshell shell. If RB3_CHAR_PREVIEW has posed band members with hand
     skeletons, this is the closest apples-to-apples match to D2 and the dump
     returns >0 hand bones.
  2. If the shell has no posed hand bones (dump==0), continue nav to game_screen
     (band members posed with instruments, animating) — the gameplay/venue context
     the mission blesses as the fallback (extend BOTH sides). We label the scene so
     the join records which context each side used.

RB3_FIXED_CLOCK=1 + a settle loop gives a repeatable pose. Read-only probe.

Usage:
  python3 scripts/native/d3-bonedump-capture.py --out /tmp/d3 [--force-gameplay]
"""
import argparse, json, os, sys, time, subprocess, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import importlib.util
spec = importlib.util.spec_from_file_location(
    "kbd2game", os.path.join(HERE, "keyboard-to-gameplay.py"))
k = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k)
REPO = k.REPO


def nm_vaddr(binpath, symbol):
    """Static vaddr of an extern-C symbol from nm (the replay-API static_addr path;
    neither rb3rc_capture_sweep nor rb3bp_dump_bones is in .dynsym, so /api/call
    resolves them as static_addr + PIE load bias, not by dlsym)."""
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


def dump_bones(port, probe_out, scene, static_addr, settle_frames=3):
    """POST the probe a few times (settle), return (count, parsed_json)."""
    last = None
    for i in range(settle_frames):
        r = api_call(port, "rb3bp_dump_bones", static_addr)
        # /api/call returns {"ok":true,"data":{"ret_i":N,...}}
        data = r.get("data", r) if isinstance(r, dict) else {}
        n = int(data.get("ret_i", 0)) if isinstance(data, dict) else 0
        k.log(f"  dump[{scene}] attempt {i}: ret_i={n}")
        time.sleep(0.5)
        if n > 0:
            last = n
    parsed = None
    if os.path.exists(probe_out):
        with open(probe_out) as f:
            parsed = json.load(f)
        # snapshot the raw dump for this scene (both contexts kept for A/B).
        import shutil
        shutil.copy(probe_out, os.path.join(os.path.dirname(probe_out),
                                             f"native_bones_{scene}.json"))
    return last or 0, parsed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="/tmp/d3")
    ap.add_argument("--bin", default=k.DEFAULT_BIN)
    ap.add_argument("--data", default=k.DEFAULT_DATA)
    ap.add_argument("--overlay", default=os.path.join(REPO, "native", "dta"))
    ap.add_argument("--force-gameplay", action="store_true")
    ap.add_argument("--song-downs", type=int, default=4)
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    static_addr = nm_vaddr(args.bin, "rb3bp_dump_bones")
    if static_addr is None:
        k.log("FAIL: rb3bp_dump_bones not found in binary symtab (rebuild?)"); return 1
    k.log(f"rb3bp_dump_bones static vaddr = 0x{static_addr:x}")
    probe_out = os.path.join(args.out, "native_bones_raw.json")
    if os.path.exists(probe_out):
        os.remove(probe_out)
    port = k.free_port()
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_DATA": args.data, "RB3_DTA_OVERLAY": args.overlay,
        "RB3_INPUT_DEBUG": "1",
        "RB3_REPLAY_API": "1",          # enable /api/call
        "RB3_FIXED_CLOCK": "1",          # matched-clock repeatable pose
        "RB3_CHAR_PREVIEW": "1",         # load posed band bodies in the shell
        "RB3_BONE_PROBE_OUT": probe_out,
    })
    log_path = os.path.join(args.out, f"boot-{port}.log")
    logf = open(log_path, "w")
    k.log(f"launching rb3-native (port {port}); log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    result = {"scene": None, "count": 0, "bones": None}
    try:
        if k.wait_screen(port, lambda s: True, 40, proc) is None:
            k.log("FAIL: HTTP server never came up"); return 1
        k.log("HTTP server up")

        # --- reach main_hub (shell) ---
        k.wait_screen(port, lambda s: s and s != "", 60, proc, args.verbose)
        for _ in range(8):
            if k.health(port)[2] == "main_hub_screen": break
            k.press(port, k.START); k.drain_pad(port); time.sleep(0.6)
        if k.wait_screen(port, "main_hub_screen", 30, proc, args.verbose) is None:
            k.log("FAIL: never reached main_hub_screen"); return 1
        k.log("reached main_hub_screen (shell)")

        if not args.force_gameplay:
            # settle char-preview load, then try the shell dump (D2-matched context)
            for _ in range(20):
                env["RB3_BONE_PROBE_SCENE"] = "shell:main_hub"
                cp = k.dta(port, "{rb3_char_probe 0}")
                k.log(f"  char_probe slot0: {cp}")
                if cp and "skinned=" in cp and "skinned=0" not in cp and "loading=0" in cp:
                    break
                time.sleep(1.0)
            # the probe reads RB3_BONE_PROBE_SCENE from the child env at boot; set via
            # a DTA-visible tag is not available, so scene label is written by the probe
            n, parsed = dump_bones(port, probe_out, "shell", static_addr, settle_frames=4)
            if n > 0:
                result.update(scene="shell:main_hub", count=n, bones=parsed)
                k.log(f"SHELL dump OK: {n} hand bones")

        if result["count"] == 0:
            # --- fall to gameplay (band posed with instruments) ---
            k.log("shell had no posed hand bones; navigating to gameplay")
            rc = navigate_to_gameplay(port, proc, args)
            if rc != 0:
                k.log("FAIL: could not reach gameplay"); return 1
            # settle a few frames then dump
            time.sleep(2.0)
            n, parsed = dump_bones(port, probe_out, "gameplay", static_addr, settle_frames=5)
            result.update(scene="gameplay:game_screen", count=n, bones=parsed)
            k.log(f"GAMEPLAY dump: {n} hand bones")

        # persist a manifest
        with open(os.path.join(args.out, "manifest.json"), "w") as f:
            json.dump({"scene": result["scene"], "count": result["count"],
                       "port": port, "probe_out": probe_out}, f, indent=1)
        return 0 if result["count"] > 0 else 3
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), 15)
        except Exception:
            pass


def navigate_to_gameplay(port, proc, args):
    """Minimal nav main_hub -> game_screen (subset of keyboard-to-gameplay)."""
    for _ in range(10):
        if k.health(port)[2] == "song_select_screen": break
        k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.7)
    if k.wait_screen(port, "song_select_screen", 40, proc, args.verbose) is None:
        return 1
    time.sleep(1.5)
    for _ in range(args.song_downs):
        k.press(port, k.DDOWN); k.drain_pad(port); time.sleep(0.2)
    k.press(port, k.CONFIRM); k.drain_pad(port)
    if k.wait_screen(port, "part_difficulty_screen", 60, proc, args.verbose) is None:
        return 1
    ov = k.wait_view(port, lambda v: v.startswith("choose_part") or v == "choose_diff",
                     30, proc, "choose_part", args.verbose)
    if ov and ov[0].startswith("choose_part"):
        k.press(port, k.CONFIRM); k.drain_pad(port)
    ov = k.overshell(port); g = 0
    while ov[0] == "confirm_action" and g < 4:
        k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.5); ov = k.overshell(port); g += 1
    ov = k.wait_view(port, lambda v: v == "choose_diff", 30, proc, "choose_diff", args.verbose)
    if ov:
        for _ in range(k.DIFF_INDEX[args.diff] if hasattr(args, "diff") else 2):
            k.press(port, k.DDOWN); k.drain_pad(port); time.sleep(0.25)
        k.press(port, k.CONFIRM); k.drain_pad(port)
    ov = k.overshell(port); g = 0
    while ov[0] == "confirm_action" and g < 4:
        k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.5); ov = k.overshell(port); g += 1
    k.wait_view(port, lambda v: v == "ready_to_play", 30, proc, "ready_to_play", args.verbose)
    if k.wait_screen(port, "game_screen", 90, proc, args.verbose) is None:
        return 1
    k.verb(port, "nofail")
    for _ in range(20):
        ip = k.dta(port, "{game is_playing}")
        k.verb(port, "autohit")
        if ip is not None and int(ip) == 1:
            break
        time.sleep(0.5)
    return 0


if __name__ == "__main__":
    sys.exit(main())
