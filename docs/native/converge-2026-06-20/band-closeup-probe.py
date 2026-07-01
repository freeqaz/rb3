#!/usr/bin/env python3
"""Deterministic band-member-closeup probe (converge-2026-06-20 batch).

Boots rb3-native -> gameplay, FREEZES the venue director's auto-pick
({$band_director set disabled 1}) so a forced shot STICKS, then force_shots a
list of BAND-MEMBER-CLOSEUP shots (resolved by milo object name in the venue
dir) and captures, per shot:
  * a determinism pair (frame A, frame B ~2.5s later -- camera must STAY)
  * the screenshot for downstream A/B (run this script twice: once with the
    shard guard ON, once with SHARD_GUARD_OFF=1, SAME shots, MATCHED frames).

The SHARD_RATIO_DBG/SHARD_DBG drop lines land in the engine log (binary; parse
with -a / python). One pass = one process => one env. Drive with --tag to keep
the on/off passes' files distinct.

Usage:
  SHARD_RATIO_DBG=1 SHARD_DBG=1 python3 band-closeup-probe.py \
      --port 8755 --out shots --tag guardon
  SHARD_GUARD_OFF=1 SHARD_RATIO_DBG=1 python3 band-closeup-probe.py \
      --port 8756 --out shots --tag guardoff
"""
import argparse, os, sys, time, subprocess, signal

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPTS = os.path.join(HERE, "..", "..", "..", "scripts", "native")
sys.path.insert(0, SCRIPTS)
import importlib.util
spec = importlib.util.spec_from_file_location(
    "kbd2game", os.path.join(SCRIPTS, "keyboard-to-gameplay.py"))
k = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k)
REPO = k.REPO

# Band-member CLOSEUP shots (milo object names in the arena_01 venue dir).
# _cls = explicit closeup; the bare g00/b00/d00 are per-member medium shots.
# Roles: g=guitar b=bass d=drums k=keys v=vocals.
DEFAULT_SHOTS = [
    "coop_dir_g_cls00",   # guitar closeup
    "coop_dir_g_cls01",
    "coop_dir_b_cls00",   # bass closeup
    "coop_dir_g_np_m00",  # guitar near-player (frames torso/feet wider)
    "coop_dir_d_pnt_m00", # drums point (drummer, may show feet/kit)
    "coop_dir_d_lt00",    # drums look-through (kit area)
    "coop_dir_g00",       # guitar medium (full body -> footwear/shoe skin)
    "coop_dir_b00",       # bass medium (full body)
]


def to_gameplay(port, proc, song_downs, diff_idx, verbose):
    if k.wait_screen(port, lambda s: True, 40, proc) is None:
        print("FAIL: HTTP server never came up"); return False
    print("[probe] HTTP up")
    k.wait_screen(port, lambda s: s and s != "", 60, proc, verbose)
    for _ in range(8):
        if k.health(port)[2] == "main_hub_screen": break
        k.press(port, k.START); k.drain_pad(port); time.sleep(0.6)
    if k.wait_screen(port, "main_hub_screen", 30, proc, verbose) is None:
        print("FAIL: no main_hub"); return False
    for _ in range(10):
        if k.health(port)[2] == "song_select_screen": break
        k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.7)
    if k.wait_screen(port, "song_select_screen", 40, proc, verbose) is None:
        print("FAIL: no song_select"); return False
    time.sleep(1.5)
    for _ in range(song_downs):
        k.press(port, k.DDOWN); k.drain_pad(port); time.sleep(0.2)
    hl = k.dta(port, "{music_library get_highlighted_node}")
    print(f"[probe] song node: {hl}")
    k.press(port, k.CONFIRM); k.drain_pad(port)
    if k.wait_screen(port, "part_difficulty_screen", 60, proc, verbose) is None:
        print("FAIL: no part_difficulty"); return False
    ov = k.wait_view(port, lambda v: v.startswith("choose_part") or v == "choose_diff",
                     30, proc, "choose_part", verbose)
    if ov and ov[0].startswith("choose_part"):
        k.press(port, k.CONFIRM); k.drain_pad(port)
    ov = k.overshell(port); g = 0
    while ov[0] == "confirm_action" and g < 4:
        k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.5)
        ov = k.overshell(port); g += 1
    ov = k.wait_view(port, lambda v: v == "choose_diff", 30, proc, "choose_diff", verbose)
    if ov:
        for _ in range(diff_idx):
            k.press(port, k.DDOWN); k.drain_pad(port); time.sleep(0.25)
        k.press(port, k.CONFIRM); k.drain_pad(port)
    ov = k.overshell(port); g = 0
    while ov[0] == "confirm_action" and g < 4:
        k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.5)
        ov = k.overshell(port); g += 1
    k.wait_view(port, lambda v: v == "ready_to_play", 30, proc, "ready", verbose)
    if k.wait_screen(port, "game_screen", 90, proc, verbose) is None:
        print("FAIL: no game_screen"); return False
    print("[probe] game_screen reached")
    k.verb(port, "nofail")
    is_playing = 0; start = -1; dl = time.time() + 60
    while time.time() < dl:
        ip = k.dta(port, "{game is_playing}"); h = k.health(port)
        if ip is not None and int(ip) == 1:
            is_playing = 1
            if start < 0 and h and h[1] >= 0: start = h[1]
        k.verb(port, "autohit")
        if is_playing and h and h[1] > start + 200 and start >= 0: break
        time.sleep(0.5)
    for _ in range(10):
        k.verb(port, "autohit"); time.sleep(0.4)
    print(f"[probe] is_playing={is_playing} songMs={k.health(port)[1]:.0f}")
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8755)
    ap.add_argument("--bin", default=k.DEFAULT_BIN)
    ap.add_argument("--data", default=k.DEFAULT_DATA)
    ap.add_argument("--overlay", default=os.path.join(REPO, "native", "dta"))
    ap.add_argument("--song-downs", type=int, default=4)
    ap.add_argument("--out", default=os.path.join(HERE, "shots"))
    ap.add_argument("--tag", default="cap")
    ap.add_argument("--shots", default=",".join(DEFAULT_SHOTS))
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        print(f"FAIL: binary not found: {args.bin}"); return 1
    out = args.out if os.path.isabs(args.out) else os.path.join(HERE, args.out)
    os.makedirs(out, exist_ok=True)
    port = args.port
    log_path = os.path.join("/tmp", f"rb3-bandcloseup-{args.tag}-{port}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_DTA_OVERLAY": args.overlay, "RB3_INPUT_DEBUG": "1"})
    print(f"[probe] launching rb3-native tag={args.tag} port={port}; "
          f"SHARD_GUARD_OFF={env.get('SHARD_GUARD_OFF')} log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf,
                            stderr=subprocess.STDOUT, cwd=REPO,
                            start_new_session=True)
    rc = 1
    try:
        if not to_gameplay(port, proc, args.song_downs, 2, args.verbose):
            return 1
        # Freeze the director auto-pick so a forced shot STICKS.
        print("[probe] disable director cam:",
              k.dta(port, "{$band_director set disabled 1}"))
        shots = [s.strip() for s in args.shots.split(",") if s.strip()]
        for shot in shots:
            r = k.dta(port, f'{{band_director force_shot "{shot}"}}')
            for _ in range(3):  # settle the forced cam
                k.verb(port, "autohit"); time.sleep(0.2)
            pa = os.path.join(out, f"{args.tag}_{shot}_A.png")
            oka = k.screenshot(port, pa)
            msA = k.health(port)[1]
            t0 = time.time()
            while time.time() < t0 + 2.5:
                k.verb(port, "autohit"); time.sleep(0.3)
            pb = os.path.join(out, f"{args.tag}_{shot}_B.png")
            okb = k.screenshot(port, pb)
            msB = k.health(port)[1]
            print(f"[probe] {shot} force={r} A={oka}({msA:.0f}ms) "
                  f"B={okb}({msB:.0f}ms)")
        rc = 0
        return rc
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
        print(f"[probe] engine log: {log_path}")
        print(f"[probe] screenshots: {out}")


if __name__ == "__main__":
    sys.exit(main())
