#!/usr/bin/env python3
"""W33 V5: reach the results screen and dump label text (/api/uidump) to
characterize the artist='j0' / SOLO SCORE='GO' corruption and its string source."""
import importlib.util, json, os, subprocess, sys, time
HERE = os.path.dirname(os.path.abspath(__file__))
KROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", "..", "scripts", "native"))
spec = importlib.util.spec_from_file_location("kbd2game", os.path.join(KROOT, "keyboard-to-gameplay.py"))
k = importlib.util.module_from_spec(spec); spec.loader.exec_module(k)
NAV = ("@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn")
OUT = sys.argv[1] if len(sys.argv) > 1 else "/tmp/w33-v5"
os.makedirs(OUT, exist_ok=True)
port = k.free_port()
logf = open(os.path.join(OUT, "engine.log"), "w")
env = dict(os.environ); env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
            "MILO_HEADLESS": "1", "RB3_DATA": k.DEFAULT_DATA, "RB3_GAME_INPUT": NAV})
proc = subprocess.Popen([k.DEFAULT_BIN], env=env, stdout=logf, stderr=subprocess.STDOUT,
                        cwd=k.REPO, start_new_session=True)
try:
    if k.wait_screen(port, "song_select_screen", 180, proc) is None:
        print("[v5] FAIL: no song_select"); sys.exit(1)
    time.sleep(1.0)
    target = "beastandtheharlot"
    for _ in range(400):
        tok = k.dta(port, "{{music_library get_highlighted_node} get_token}")
        if ("" if tok is None else str(tok)).strip().lower() == target: break
        k.verb(port, "down"); time.sleep(0.15)
    k.verb(port, "msg:music_library:select_highlighted_node")
    if k.wait_screen(port, "part_difficulty_screen", 60, proc) is None:
        print("[v5] FAIL: no part_difficulty"); sys.exit(1)
    time.sleep(1.0); k.verb(port, "part:guitar"); time.sleep(1.0)
    k.verb(port, "diff:easy"); time.sleep(1.0); k.verb(port, "nofail"); time.sleep(0.3)
    k.verb(port, "autohit"); time.sleep(0.3)
    dl = time.time() + 150
    while time.time() < dl:
        h = k.health(port)
        if h and h[2] == "game_screen" and h[1] > 2000.0: break
        time.sleep(0.5)
    for _ in range(8): k.verb(port, "autohit"); time.sleep(0.05)
    time.sleep(3.0)  # accrue some real score before jumping
    k.http_post(port, "/api/input", "msg:game:jump:600000")
    dl = time.time() + 90
    while time.time() < dl:
        h = k.health(port)
        if h and h[2] == "coop_endgame_screen": break
        time.sleep(0.5)
    time.sleep(3.0)  # settle so labels populate + render
    h = k.health(port); print(f"[v5] screen={h[2] if h else '?'}")
    k.screenshot(port, os.path.join(OUT, "results.png"))
    st, body = k.http_get(port, "/api/uidump")
    open(os.path.join(OUT, "uidump.json"), "w").write(body.decode("utf-8", "replace") if isinstance(body, bytes) else body)
    print(f"[v5] uidump status={st} bytes={len(body)}")
finally:
    try:
        import signal as _s
        if proc.poll() is None: os.killpg(os.getpgid(proc.pid), _s.SIGKILL)
    except Exception: pass
    logf.flush()
