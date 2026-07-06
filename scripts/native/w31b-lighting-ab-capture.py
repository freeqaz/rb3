#!/usr/bin/env python3
# W3.1b throwaway A/B capture: boot rb3-native -> gameplay, pin a wide venue shot,
# screenshot under the env passed in os.environ. Reuses band-closeup-capture nav.
import os, sys, time, importlib.util
HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("bandcloseup", os.path.join(HERE, "band-closeup-capture.py"))
bc = importlib.util.module_from_spec(spec); spec.loader.exec_module(bc)
k = bc.k; REPO = bc.REPO

BIN = sys.argv[1]
OUT = sys.argv[2]
DATA = os.path.join(REPO, "orig-assets", "extracted")
OVERLAY = os.path.join(REPO, "native", "dta")

port = k.free_port()
log_path = f"/tmp/rb3-w31b-{os.getpid()}.log"
logf = open(log_path, "w")
env = dict(os.environ)
env.update({"RB3_GAME":"1","RB3_HTTP":"1","RB3_HTTP_PORT":str(port),
            "MILO_HEADLESS":"1","RB3_DATA":DATA,"RB3_DTA_OVERLAY":OVERLAY,
            "RB3_INPUT_DEBUG":"1","RB3_FIXED_CLOCK":"1"})
import subprocess
proc = subprocess.Popen([BIN], env=env, stdout=logf, stderr=subprocess.STDOUT,
                        cwd=REPO, start_new_session=True)
try:
    if k.wait_screen(port, lambda s: True, 40, proc) is None:
        print("FAIL: no HTTP"); sys.exit(2)
    k.wait_screen(port, lambda s: s and s != "", 60, proc)
    for _ in range(8):
        if k.health(port)[2] == "main_hub_screen": break
        k.press(port, k.START); k.drain_pad(port); time.sleep(0.6)
    if k.wait_screen(port, "main_hub_screen", 30, proc) is None:
        print("FAIL: no hub"); sys.exit(2)
    for _ in range(10):
        if k.health(port)[2] == "song_select_screen": break
        k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.7)
    if k.wait_screen(port, "song_select_screen", 40, proc) is None:
        print("FAIL: no song_select"); sys.exit(2)
    time.sleep(1.5)
    for _ in range(4):
        k.press(port, k.DDOWN); k.drain_pad(port); time.sleep(0.2)
    # confirm through part/diff into gameplay
    for _ in range(12):
        h = k.health(port)
        if h and h[2] == "game_screen": break
        k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.7)
    if k.wait_screen(port, "game_screen", 60, proc) is None:
        print("FAIL: no game_screen"); sys.exit(2)
    # deterministic advance + pin a wide venue camera
    bc.director_disable(port, 1)
    for s in ("coop_g_n03.shot","coop_g_b.shot","coop_all","coop_wide"):
        if bc.force_shot(port, s): break
    # Frame-LOCK the A/B: jump to an exact songMs so both captures share the SAME
    # pose/animation, isolating the fog delta from character motion.
    JUMP_MS = int(os.environ.get("W31B_JUMP_MS", "12000"))
    k.verb(port, f"msg:game:jump:{JUMP_MS}")
    time.sleep(0.5)
    for _ in range(3):
        k.verb(port, "autohit"); time.sleep(0.1)
    h = k.health(port); ms = h[1] if h else -1.0
    time.sleep(0.3)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    ok = k.screenshot(port, OUT)
    print(f"CAP ok={ok} songMs={ms} shot={bc.cur_shot(port)} -> {OUT}")
finally:
    try: proc.terminate(); proc.wait(timeout=5)
    except Exception:
        try: proc.kill()
        except Exception: pass
