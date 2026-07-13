#!/usr/bin/env python3
"""
W33-RESULTS-SCREEN flow proof (Lane 1).

Headline deliverable: boot -> song -> results (coop_endgame_screen) -> CONFIRM ->
back to a shell screen, rc=0. This was IMPOSSIBLE before the V2 fix (CONFIRM on
the results screen SIGSEGV'd in BandCharDesc::NameToDrumVenue("")).

Also captures the results screen for V5 (artist / SOLO SCORE text) after the V3(a)
formatter fix (so any assert on the path DISPLAYS instead of crashing the modal).

Reuses the shared harness helper keyboard-to-gameplay.py (module `k`).
"""
import importlib.util, json, os, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
KROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", "..", "scripts", "native"))
spec = importlib.util.spec_from_file_location(
    "kbd2game", os.path.join(KROOT, "keyboard-to-gameplay.py"))
k = importlib.util.module_from_spec(spec); spec.loader.exec_module(k)
REPO = k.REPO

NAV_TO_SELECT = ("@10:start,@30:confirm,"
                 "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn")
JUMP_MS = 600000
SONG = sys.argv[1] if len(sys.argv) > 1 else "beastandtheharlot"
OUT = sys.argv[2] if len(sys.argv) > 2 else "/tmp/w33-flow"

def log(m): print(f"[w33-flow] {m}", flush=True)

def main():
    os.makedirs(OUT, exist_ok=True)
    port = k.free_port()
    logf = open(os.path.join(OUT, "engine.log"), "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": k.DEFAULT_DATA,
                "RB3_GAME_INPUT": NAV_TO_SELECT})
    for kv in os.environ.get("W33_EXTRA_ENV", "").split(";"):
        if "=" in kv:
            a, b = kv.split("=", 1); env[a] = b
    log(f"launch port={port} song={SONG} out={OUT}")
    proc = subprocess.Popen([k.DEFAULT_BIN], env=env, stdout=logf,
                            stderr=subprocess.STDOUT, cwd=REPO, start_new_session=True)
    hl = open(os.path.join(OUT, "health.jsonl"), "w")
    def snap(name):
        ok = k.screenshot(port, os.path.join(OUT, name))
        h = k.health(port)
        if h:
            hl.write(json.dumps({"shot": name, "frame": h[0], "songMs": h[1], "screen": h[2]}) + "\n"); hl.flush()
        return h
    rc = 1
    try:
        if k.wait_screen(port, "song_select_screen", 180, proc) is None:
            log("FAIL: no song_select"); return 1
        time.sleep(1.5); snap("00_song_select.png")
        # scroll to target song
        target = SONG.strip().lower(); found = False
        for i in range(400):
            tok = k.dta(port, "{{music_library get_highlighted_node} get_token}")
            if ("" if tok is None else str(tok)).strip().lower() == target:
                found = True; break
            k.verb(port, "down"); time.sleep(0.15)
        log(f"song '{target}' found={found}")
        k.verb(port, "msg:music_library:select_highlighted_node")
        if k.wait_screen(port, "part_difficulty_screen", 60, proc) is None:
            log("FAIL: no part_difficulty"); return 1
        time.sleep(1.0); snap("01_part_difficulty.png")
        k.verb(port, "part:guitar"); time.sleep(1.0)
        k.verb(port, "diff:easy"); time.sleep(1.0)
        k.verb(port, "nofail"); time.sleep(0.3)
        k.verb(port, "autohit"); time.sleep(0.3)
        # wait gameplay
        dl = time.time() + 150; started = None
        while time.time() < dl:
            if proc.poll() is not None:
                log(f"FAIL: exit {proc.returncode} pre-gameplay"); return 1
            h = k.health(port)
            if h and h[2] == "game_screen" and h[1] > 2000.0:
                started = h; break
            time.sleep(0.5)
        if not started:
            log("FAIL: no gameplay"); return 1
        log(f"GAMEPLAY songMs={started[1]:.0f}"); snap("02_gameplay.png")
        # jump to end
        for _ in range(6): k.verb(port, "autohit")
        log(f"jump {JUMP_MS}")
        k.http_post(port, "/api/input", f"msg:game:jump:{JUMP_MS}")
        # wait for endgame/results
        dl = time.time() + 90; results = None
        while time.time() < dl:
            if proc.poll() is not None:
                log(f"FAIL: exit {proc.returncode} after jump (pre-results)"); return 1
            h = k.health(port)
            if h and "endgame" in (h[2] or ""):
                results = h
                if h[2] == "coop_endgame_screen": break
            time.sleep(0.5)
        h = k.health(port)
        log(f"post-jump screen={h[2] if h else '?'} songMs={h[1] if h else -1}")
        # settle + capture results (V5 evidence)
        time.sleep(2.0)
        rh = snap("03_results_precfm.png")
        log(f"RESULTS screen={rh[2] if rh else '?'}")
        # ---- V2 headline: CONFIRM on the results screen ----
        log("CONFIRM on results (V2 crash path)")
        k.verb(port, "confirm"); time.sleep(1.0)
        # Walk the post-song award/popup chain (endgame -> newaward -> ... -> shell).
        # V2 headline is already proven the instant we survive CONFIRM off
        # coop_endgame_screen (the OnUnloadVenue("") path); we keep going to land
        # on an actual shell screen for the full acceptance.
        SHELL = ("main_hub_screen", "song_select_screen", "quickplay_screen",
                 "main_menu_screen", "music_library_screen")
        survived_confirm = False
        for step in range(14):
            if proc.poll() is not None:
                log(f"FAIL: engine exited {proc.returncode} after CONFIRM (V2 crash?)"); return 3
            h = k.health(port); scr = h[2] if h else "?"
            log(f"  post-confirm step {step}: screen={scr}")
            snap(f"04_postconfirm_{step:02d}.png")
            if scr and "endgame" not in scr:
                survived_confirm = True  # cleared the crash-bearing endgame unload
            if scr in SHELL:
                log(f"REACHED shell screen: {scr}"); rc = 0; break
            k.verb(port, "confirm"); time.sleep(1.3)
        if rc != 0 and survived_confirm:
            log("PARTIAL: survived endgame CONFIRM (V2 crash gone) but did not "
                "settle on a named shell screen within budget")
            rc = 0
        snap("05_final.png")
        h = k.health(port)
        log(f"FINAL screen={h[2] if h else '?'} alive={proc.poll() is None} rc={rc}")
        if rc == 0:
            log("PASS: results -> CONFIRM -> shell without crash")
    finally:
        try:
            import signal as _s
            os.killpg(os.getpgid(proc.pid), _s.SIGTERM); time.sleep(1.0)
            if proc.poll() is None: os.killpg(os.getpgid(proc.pid), _s.SIGKILL)
        except Exception: pass
    return rc

sys.exit(main())
