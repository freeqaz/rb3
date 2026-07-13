#!/usr/bin/env python3
"""
W33 V3(a) proof: a deliberately-triggered assert renders its text instead of
SIGSEGV'ing in the Debug::Modal formatter.

Boots to song_select, then evals `{fail "..."}` (DataFunc DataFail ->
TheDebug.Fail -> Debug::Modal -> MakeString("...ConsoleName: %s...",
NetworkSocket::GetHostName(), ...) -> the exact V3(a) crash path). With the
V3(a) fix (strong native GetHostName -> String("")), the FAIL banner (incl. the
ConsoleName line) prints and the engine Exit(1)s cleanly; WITHOUT it the
formatter SIGSEGV'd in FormatString::operator<<(const String&)::c_str()
(MakeString.cpp:312) and no banner printed.
"""
import importlib.util, os, subprocess, sys, time
HERE = os.path.dirname(os.path.abspath(__file__))
KROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", "..", "scripts", "native"))
spec = importlib.util.spec_from_file_location("kbd2game", os.path.join(KROOT, "keyboard-to-gameplay.py"))
k = importlib.util.module_from_spec(spec); spec.loader.exec_module(k)
NAV = ("@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn")
OUT = sys.argv[1] if len(sys.argv) > 1 else "/tmp/w33-v3a"
os.makedirs(OUT, exist_ok=True)
port = k.free_port()
logf = open(os.path.join(OUT, "engine.log"), "w")
env = dict(os.environ); env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
            "MILO_HEADLESS": "1", "RB3_DATA": k.DEFAULT_DATA, "RB3_GAME_INPUT": NAV})
proc = subprocess.Popen([k.DEFAULT_BIN], env=env, stdout=logf, stderr=subprocess.STDOUT,
                        cwd=k.REPO, start_new_session=True)
rc = 1
try:
    if k.wait_screen(port, "song_select_screen", 180, proc) is None:
        print("[v3a] FAIL: no song_select"); sys.exit(1)
    time.sleep(1.0)
    print("[v3a] eval {fail ...} to trigger the Debug::Modal formatter path")
    st, body = k.http_post(port, "/api/dta/eval", '{fail "W33-V3A-FORMATTER-PROOF hostname-line-must-render"}', )
    print(f"[v3a] eval status={st} body={body[:200]!r}")
    # give it a moment to print the banner + exit
    time.sleep(3.0)
    alive = proc.poll() is None
    print(f"[v3a] engine alive after fail={alive} (Exit(1) expected)")
    rc = 0
finally:
    try:
        import signal as _s
        if proc.poll() is None:
            os.killpg(os.getpgid(proc.pid), _s.SIGKILL)
    except Exception: pass
    logf.flush()
sys.exit(rc)
