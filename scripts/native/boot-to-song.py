#!/usr/bin/env python3
"""
boot-to-song.py — CANONICAL harness: boot rb3-native headless all the way INTO
GAMEPLAY of a song, then hold there capturing periodic screenshots.

This is the one entry point for "I need eyes on actual gameplay" — visual bug
sweeps, flag A/B comparisons (pass flags with --env), prop/character checks,
crowd checks, HUD checks. It reuses the proven nav from song-end-test.py /
capture_song_gameplay.py (select: verbs to song_select, real pad-press
part:/diff: verbs through the overshell flow) and the shared HTTP helpers from
keyboard-to-gameplay.py.

USAGE
-----
    # default: first playable song, guitar/expert, 30s in gameplay, shot every 2s
    python3 scripts/native/boot-to-song.py

    # named song, vocals part, flag A/B, custom output dir
    python3 scripts/native/boot-to-song.py --song beastandtheharlot \
        --part vocals --env RB3_PROP_POSE_FULL=1 --out /tmp/prop-on

    # keep the engine alive after reaching gameplay (prints port for /api pokes)
    python3 scripts/native/boot-to-song.py --keep

Screenshots land in --out as gameplay_NNN.png plus milestone shots
(song_select / part_difficulty / first-frame). health.jsonl records
frame/songMs/screen per shot. Exit 0 = gameplay reached + shots captured.
"""
import argparse, importlib.util, json, os, signal, socket, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location(
    "kbd2game", os.path.join(HERE, "keyboard-to-gameplay.py"))
k = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k)

REPO = k.REPO

# Boot -> main_hub -> PLAY NOW -> QUICKPLAY -> song_select (song targeting is
# done over HTTP after song_select so --song can scroll to a named shortname).
NAV_TO_SELECT = ("@10:start,@30:confirm,"
                 "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn")
GAMEPLAY_SONGMS = 2000.0
PARTS = ["guitar", "bass", "drums", "vocals", "keys"]


def log(m): print(f"[boot2song] {m}", flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--song", default=None,
                    help="song shortname to scroll to (default: first via N downs)")
    ap.add_argument("--song-downs", type=int, default=1,
                    help="downs before selecting when --song is not given")
    ap.add_argument("--part", default="guitar", choices=PARTS)
    ap.add_argument("--diff", default="expert",
                    choices=["easy", "medium", "hard", "expert"])
    ap.add_argument("--hold", type=float, default=30.0,
                    help="seconds to stay in gameplay capturing shots")
    ap.add_argument("--interval", type=float, default=2.0,
                    help="seconds between gameplay screenshots")
    ap.add_argument("--out", default="/tmp/rb3-boot2song")
    ap.add_argument("--env", action="append", default=[],
                    metavar="KEY=VAL", help="extra env for the engine (repeatable)")
    ap.add_argument("--no-autohit", action="store_true",
                    help="don't autohit (default keeps the run stable headless)")
    ap.add_argument("--fixed-clock", action="store_true",
                    help="RB3_FIXED_CLOCK=1 for deterministic A/B frame pairing")
    ap.add_argument("--keep", action="store_true",
                    help="leave the engine running after the hold (prints port/pid)")
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=k.DEFAULT_BIN)
    ap.add_argument("--data", default=k.DEFAULT_DATA)
    ap.add_argument("--max-downs", type=int, default=140)
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}"); return 1
    os.makedirs(args.out, exist_ok=True)
    port = args.port or k.free_port()
    log_path = os.path.join(args.out, "engine.log")
    logf = open(log_path, "w")

    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_GAME_INPUT": NAV_TO_SELECT})
    if args.fixed_clock:
        env["RB3_FIXED_CLOCK"] = "1"
    for kv in args.env:
        if "=" not in kv:
            log(f"FAIL: bad --env '{kv}' (want KEY=VAL)"); return 1
        key, val = kv.split("=", 1)
        env[key] = val
    log(f"launching (port {port}) part={args.part} diff={args.diff} "
        f"song={args.song or f'<{args.song_downs} downs>'} extra_env={args.env}")
    log(f"engine log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf,
                            stderr=subprocess.STDOUT, cwd=REPO,
                            start_new_session=True)
    rc, shots = 1, 0
    health_log = open(os.path.join(args.out, "health.jsonl"), "w")

    def snap(name):
        ok = k.screenshot(port, os.path.join(args.out, name))
        h = k.health(port)
        if h:
            health_log.write(json.dumps(
                {"shot": name, "frame": h[0], "songMs": h[1], "screen": h[2]}) + "\n")
            health_log.flush()
        return ok

    try:
        # --- boot to song_select (RB3_GAME_INPUT drives the menus) -----------
        if k.wait_screen(port, "song_select_screen", 180, proc) is None:
            log("FAIL: never reached song_select_screen"); return 1
        time.sleep(1.5)
        snap("00_song_select.png")

        # --- target the song --------------------------------------------------
        if args.song:
            target = args.song.strip().lower()
            log(f"scrolling to '{target}'")
            found, last = False, None
            for i in range(args.max_downs):
                hl = k.dta(port, "{{music_library get_highlighted_node} get_token}")
                hls = ("" if hl is None else str(hl)).strip().lower()
                if hls != last:
                    log(f"  down {i}: token='{hl}'"); last = hls
                if hls == target:
                    found = True; break
                k.verb(port, "down"); time.sleep(0.18)
            if not found:
                log(f"FAIL: never highlighted '{args.song}' "
                    f"in {args.max_downs} downs (last='{last}')"); return 2
        else:
            for _ in range(args.song_downs):
                k.verb(port, "down"); time.sleep(0.25)
        hl = k.dta(port, "{{music_library get_highlighted_node} get_token}")
        log(f"selecting song: '{hl}'")
        k.verb(port, "msg:music_library:select_highlighted_node")

        # --- part_difficulty: real pad-press part:/diff: verbs ---------------
        if k.wait_screen(port, "part_difficulty_screen", 60, proc) is None:
            log("FAIL: never reached part_difficulty_screen"); return 1
        time.sleep(1.0)
        snap("01_part_difficulty.png")
        log(f"committing {args.part}/{args.diff}")
        k.verb(port, f"part:{args.part}"); time.sleep(1.0)
        k.verb(port, f"diff:{args.diff}"); time.sleep(1.0)
        k.verb(port, "nofail"); time.sleep(0.3)
        if not args.no_autohit:
            k.verb(port, "autohit"); time.sleep(0.3)

        # --- wait for gameplay (song clock past the intro) --------------------
        dl = time.time() + 150
        started = None
        while time.time() < dl:
            if proc.poll() is not None:
                log(f"FAIL: proc exited {proc.returncode} before gameplay"); return 1
            h = k.health(port)
            if h and h[2] == "game_screen" and h[1] > GAMEPLAY_SONGMS:
                started = h; break
            time.sleep(0.5)
        if not started:
            h = k.health(port)
            log(f"FAIL: never reached gameplay "
                f"(screen={h[2] if h else '?'} songMs={h[1] if h else -1})"); return 1
        log(f"GAMEPLAY: frame={started[0]} songMs={started[1]:.0f}")
        snap("02_gameplay_first.png")

        # --- hold: periodic screenshots ---------------------------------------
        n = max(1, int(args.hold / args.interval))
        log(f"holding {args.hold:.0f}s, {n} shots every {args.interval:.1f}s -> {args.out}")
        for i in range(n):
            if proc.poll() is not None:
                log(f"FAIL: engine died mid-hold at shot {i} "
                    f"(exit {proc.returncode})"); return 1
            if not args.no_autohit:
                k.verb(port, "autohit")
            ok = snap(f"gameplay_{i:03d}.png")
            shots += 1 if ok else 0
            h = k.health(port)
            log(f"  shot {i:03d} ok={ok} songMs={h[1]:.0f}" if h else
                f"  shot {i:03d} ok={ok} (health unavailable)")
            time.sleep(args.interval)

        h = k.health(port)
        if h and h[2] != "game_screen":
            log(f"WARN: left game_screen during hold (now '{h[2]}')")
        rc = 0 if shots > 0 else 1
        log(f"{'PASS' if rc == 0 else 'FAIL'}: {shots}/{n} shots captured")

        if args.keep:
            log(f"KEEP: engine stays up — port {port}, pid {proc.pid} "
                f"(kill with: kill -- -{os.getpgid(proc.pid)})")
            log("Ctrl-C to stop.")
            try:
                while proc.poll() is None:
                    time.sleep(2.0)
                log(f"engine exited ({proc.returncode})")
            except KeyboardInterrupt:
                pass
        return rc
    finally:
        health_log.close()
        if not args.keep or proc.poll() is not None:
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
                try: proc.wait(timeout=8)
                except subprocess.TimeoutExpired:
                    os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception:
                pass
        logf.close()
        log(f"engine log: {log_path}")
        log(f"output dir: {args.out}")


if __name__ == "__main__":
    sys.exit(main())
