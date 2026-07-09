#!/usr/bin/env python3
"""_w24_forearm_capture.py — W24 FOREARM anatomical-detachment capture.

Boots rb3-native headless with BAND_ANIM_PROBE='*' BAND_ANIM_ANAT=1 (+the T1
ANAT probe compiled into BandCharacter.cpp), drives to gameplay via the proven
keyboard-to-gameplay nav, then:
  - optionally pins band-closeup camera shots (rb3_director_disable + rb3_force_shot),
  - advances the song clock deterministically (autohit) between captures,
  - grabs frame-tagged screenshots: reads /api/health frame just before AND after
    each /api/screenshot so each PNG is bracketed by a [lo,hi] engine-frame window.

The ANAT probe writes evt=ANAT / evt=ANATBEAT to stderr (-> the boot log). After
capture we parse the log and cross-reference ANAT events to the screenshot frame
windows so a spike-fan PNG can be matched to the exact bones/members/clips that
were anatomically detached at that frame.

pgid-only cleanup; free port; never pkill by name.
"""
import argparse, json, os, re, signal, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

def _load(mod_name, fname):
    spec = importlib.util.spec_from_file_location(
        mod_name, os.path.join(os.path.dirname(os.path.abspath(__file__)), fname))
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); return m

k = _load("kbd2game", "keyboard-to-gameplay.py")

DEFAULT_BIN = k.DEFAULT_BIN
DEFAULT_DATA = k.DEFAULT_DATA

# guitarist closeups (from band-closeup-capture MEMBER_SHOTS guitar list, common set)
GUITAR_SHOTS = ["coop_g_cg", "coop_g_cls", "coop_g_n0", "coop_g_n1",
                "coop_bg_cls", "coop_v_cls", "coop_d_cls", "coop_b_cls"]


def force_shot(port, name):
    for cand in ([name] if name.endswith(".shot") else [name, name + ".shot"]):
        r = k.dta(port, '{rb3_force_shot "%s"}' % cand)
        if r == "force_shot ok":
            return cand
        if isinstance(r, int):
            return None
    return None


def cap(port, out, tag, manifest):
    """Frame-bracketed screenshot: health-frame before, shot, health-frame after."""
    h0 = k.health(port)
    f0 = h0[0] if h0 else -1
    path = os.path.join(out, tag + ".png")
    ok = k.screenshot(port, path)
    h1 = k.health(port)
    f1 = h1[0] if h1 else -1
    cur = k.dta(port, "{rb3_cur_shot}")
    rec = {"tag": tag, "file": path, "ok": bool(ok),
           "frame_lo": f0, "frame_hi": f1,
           "songMs": (h1[1] if h1 else -1), "cur_shot": cur}
    manifest.append(rec)
    print(f"[w24] cap {tag}: ok={ok} frames=[{f0},{f1}] songMs={rec['songMs']:.0f} shot={cur!r}")
    return rec


def advance(port, ms):
    dl = time.time() + 8.0
    h = k.health(port); target = (h[1] if h else 0) + ms
    while time.time() < dl:
        k.verb(port, "autohit")
        h = k.health(port)
        if h and h[1] >= target:
            return
        time.sleep(0.1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--overlay", default=os.path.join(REPO, "native", "dta"))
    ap.add_argument("--diff", default="hard")
    ap.add_argument("--song-downs", type=int, default=3)
    ap.add_argument("--out", default="/tmp/rb3-w24-forearm")
    ap.add_argument("--shots", default=",".join(GUITAR_SHOTS))
    ap.add_argument("--frames-per-shot", type=int, default=3)
    ap.add_argument("--frame-dt", type=int, default=400)
    ap.add_argument("--free-burst", type=int, default=16,
                    help="also capture N free (un-pinned) burst frames for auto-director closeups")
    ap.add_argument("--burst-interval", type=float, default=0.3)
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    port = args.port or k.free_port()
    log_path = os.path.join(args.out, f"forearm-boot-{port}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_DTA_OVERLAY": args.overlay, "RB3_INPUT_DEBUG": "1",
                "BAND_ANIM_PROBE": "*", "BAND_ANIM_ANAT": "1"})
    print(f"[w24] launching rb3-native port={port} log={log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    diff_idx = k.DIFF_INDEX[args.diff]
    manifest = []
    try:
        if k.wait_screen(port, lambda s: True, 40, proc) is None:
            print("FAIL: HTTP never up"); return 2
        k.wait_screen(port, lambda s: s and s != "", 60, proc, args.verbose)
        for _ in range(8):
            if k.health(port)[2] == "main_hub_screen": break
            k.press(port, k.START); k.drain_pad(port); time.sleep(0.6)
        if k.wait_screen(port, "main_hub_screen", 30, proc, args.verbose) is None:
            print("FAIL: no main_hub"); return 2
        for _ in range(10):
            if k.health(port)[2] == "song_select_screen": break
            k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.7)
        if k.wait_screen(port, "song_select_screen", 40, proc, args.verbose) is None:
            print("FAIL: no song_select"); return 2
        time.sleep(1.5)
        for _ in range(args.song_downs):
            k.press(port, k.DDOWN); k.drain_pad(port); time.sleep(0.2)
        k.press(port, k.CONFIRM); k.drain_pad(port)
        if k.wait_screen(port, "part_difficulty_screen", 60, proc, args.verbose) is None:
            print("FAIL: no part_difficulty"); return 2
        ov = k.wait_view(port, lambda v: v.startswith("choose_part") or v == "choose_diff",
                         30, proc, "choose_part", args.verbose)
        if ov and ov[0].startswith("choose_part"):
            k.press(port, k.CONFIRM); k.drain_pad(port)
        ov = k.overshell(port); g = 0
        while ov[0] == "confirm_action" and g < 4:
            k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.5); ov = k.overshell(port); g += 1
        ov = k.wait_view(port, lambda v: v == "choose_diff", 30, proc, "choose_diff", args.verbose)
        if ov:
            for _ in range(diff_idx):
                k.press(port, k.DDOWN); k.drain_pad(port); time.sleep(0.25)
            k.press(port, k.CONFIRM); k.drain_pad(port)
        ov = k.overshell(port); g = 0
        while ov[0] == "confirm_action" and g < 4:
            k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.5); ov = k.overshell(port); g += 1
        k.wait_view(port, lambda v: v == "ready_to_play", 30, proc, "ready", args.verbose)
        if k.wait_screen(port, "game_screen", 90, proc, args.verbose) is None:
            print("FAIL: no game_screen"); return 2
        print("[w24] game_screen reached")
        k.verb(port, "nofail")
        is_playing = 0; start = -1; dl = time.time() + 60
        while time.time() < dl:
            ip = k.dta(port, "{game is_playing}"); h = k.health(port)
            if ip is not None and int(ip) == 1:
                is_playing = 1
                if start < 0 and h and h[1] >= 0: start = h[1]
            k.verb(port, "autohit")
            if is_playing and h and h[1] > start + 200 and start >= 0:
                break
            time.sleep(0.5)
        print(f"[w24] is_playing={is_playing} songMs={k.health(port)[1]:.0f}")

        # --- PASS A: free auto-director burst (captures natural count-in / cuts)
        for i in range(args.free_burst):
            k.verb(port, "autohit")
            if k.health(port) is None:
                print("[w24] engine died in free burst"); break
            cap(port, args.out, f"free_{i:02d}", manifest)
            time.sleep(args.burst_interval)

        # --- PASS B: pinned band-closeup shots ------------------------------
        di = k.dta(port, "{rb3_director_disable 1}")
        print(f"[w24] director_disable -> {di!r}")
        shots = [s.strip() for s in args.shots.split(",") if s.strip()]
        for shot in shots:
            if k.health(port) is None: break
            resolved = force_shot(port, shot)
            if not resolved:
                print(f"[w24] shot {shot!r} not_found, skip"); continue
            for fi in range(args.frames_per_shot):
                if fi > 0:
                    advance(port, args.frame_dt)
                cap(port, args.out, f"pin_{shot}_{fi}", manifest)

        rc = 0
    finally:
        with open(os.path.join(args.out, "manifest.json"), "w") as f:
            json.dump({"manifest": manifest, "log": log_path}, f, indent=2)
        print("[w24] killing pgid")
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception:
            pass
        try: logf.close()
        except Exception: pass
    return rc


if __name__ == "__main__":
    sys.exit(main())
