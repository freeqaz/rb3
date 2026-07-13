#!/usr/bin/env python3
"""
W33 V3 proof (V3a + V3b together). Reaches main_hub with player-1 joined, then
fires `overshell:attempt_register_online` — the genuine "PLAY ON XBOX LIVE"
action, executed on the MAIN THREAD via rb3_game_input.cpp (NOT dta/eval, so the
dta/eval sigsetjmp guard that skips Debug::Modal does not apply). This is the
exact V3 path: AttemptRegisterOnline -> BeginOverrideFlow(kOverrideFlow_
RegisterOnline) -> UpdateState -> GenerateCurrentState -> GetSlotState(id).

Expected with the V3(a) fix: if the online-register flow requests an unregistered
OvershellSlotState id, MILO_FAIL("OvershellSlotState %d does not exist") fires and
Debug::Modal now FORMATS + PRINTS its banner (incl. the ConsoleName line fed by
the fixed NetworkSocket::GetHostName()) instead of SIGSEGV'ing in the formatter,
then Exit(1)s. The printed %d documents V3(b).
"""
import importlib.util, os, subprocess, sys, time
HERE = os.path.dirname(os.path.abspath(__file__))
KROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", "..", "scripts", "native"))
spec = importlib.util.spec_from_file_location("kbd2game", os.path.join(KROOT, "keyboard-to-gameplay.py"))
k = importlib.util.module_from_spec(spec); spec.loader.exec_module(k)
NAV = "@10:start,@30:confirm"   # join player 1, land on main_hub
OUT = sys.argv[1] if len(sys.argv) > 1 else "/tmp/w33-v3"
os.makedirs(OUT, exist_ok=True)
port = k.free_port()
logf = open(os.path.join(OUT, "engine.log"), "w")
env = dict(os.environ); env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
            "MILO_HEADLESS": "1", "RB3_DATA": k.DEFAULT_DATA, "RB3_GAME_INPUT": NAV})
proc = subprocess.Popen([k.DEFAULT_BIN], env=env, stdout=logf, stderr=subprocess.STDOUT,
                        cwd=k.REPO, start_new_session=True)
try:
    if k.wait_screen(port, "main_hub_screen", 180, proc) is None:
        h = k.health(port); print(f"[v3] note: main_hub not reached, screen={h[2] if h else '?'}")
    time.sleep(2.0)
    h = k.health(port); print(f"[v3] screen={h[2] if h else '?'}")
    k.screenshot(port, os.path.join(OUT, "00_hub.png"))
    print("[v3] firing overshell:attempt_register_online (PLAY ON XBOX LIVE)")
    st, body = k.verb(port, "overshell:attempt_register_online")
    print(f"[v3] verb status={st} body={body[:160]!r}")
    time.sleep(4.0)
    alive = proc.poll() is None
    print(f"[v3] engine alive after action={alive} rc={proc.returncode}")
    k.screenshot(port, os.path.join(OUT, "01_after.png"))
finally:
    try:
        import signal as _s
        if proc.poll() is None: os.killpg(os.getpgid(proc.pid), _s.SIGKILL)
    except Exception: pass
    logf.flush()
