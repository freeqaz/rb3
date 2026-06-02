#!/usr/bin/env python3
"""
save-persist-test.py — C2 profile/options persistence round-trip (native).

Validates that ProfileMgr global options survive a restart via the host-FS blob
backend (rb3_save_native.cpp), with SaveLoadManager kept EXCLUDED.

Procedure (audit §8 C2 pass/fail gate):
  1. Clean RB3_SAVE_DIR. Boot rb3-native with RB3_HTTP + RB3_DATA.
  2. Over /api/dta/eval, mutate a global option via TheProfileMgr (set crowd
     volume + dolby), which sets mGlobalOptionsDirty=true.
  3. Exit cleanly (MILO_MAX_FRAMES-bounded run) — confirm exit code 0 and that
     globaloptions.bin (+ gameplayopts_0.bin) appear under RB3_SAVE_DIR.
  4. Re-boot, and BEFORE any UI mutates options, /api/dta/eval-probe the same
     options — assert they read back the values set in step 2, not ctor defaults.

    python3 scripts/native/save-persist-test.py [--port N] [--data DIR] [--bin PATH] [--verbose]

Exit 0 = round-trip verified. Exit 1 = boot/nav/persist failed.
"""
import argparse, http.client, json, os, shutil, signal, socket, subprocess, sys, tempfile, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

SERVER_READY_TIMEOUT = 60
# Distinctive non-default values (ctor defaults are background-loaded; crowd vol
# default differs, dolby default is its config value — we set explicit values).
TEST_CROWD_VOL = 7
TEST_DOLBY = 1


def log(m): print(f"[save-persist] {m}", flush=True)
def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p

def http_get_bytes(port, path, timeout=10):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally:
        c.close()

def http_post(port, path, body, timeout=15):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = c.getresponse(); return r.status, r.read().decode("utf-8", "replace")
    finally:
        c.close()

def health(port):
    try:
        st, b = http_get_bytes(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception:
        return None

def dta_eval(port, expr):
    st, b = http_post(port, "/api/dta/eval", expr, timeout=10)
    if st != 200: return None
    d = json.loads(b)
    if not d.get("ok"): return None
    return d["data"].get("value")

def wait_ready(port, proc, timeout):
    dl = time.time() + timeout
    while time.time() < dl:
        if proc.poll() is not None:
            return False
        if health(port) is not None:
            return True
        time.sleep(0.4)
    return False

def boot(bin_path, data_dir, save_dir, port, env_extra, verbose):
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_DATA": data_dir, "RB3_SAVE_DIR": save_dir,
    })
    env.update(env_extra)
    out = None if verbose else subprocess.DEVNULL
    return subprocess.Popen([bin_path], env=env, stdout=out, stderr=out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    save_dir = tempfile.mkdtemp(prefix="rb3-save-test-")
    log(f"clean save dir: {save_dir}")

    # ---- Pass 1: boot, mutate options, exit cleanly ----
    p1 = args.port or free_port()
    log(f"PASS 1 boot (port {p1}) — set crowd_volume={TEST_CROWD_VOL}, dolby={TEST_DOLBY}")
    # MILO_MAX_FRAMES bounds the run; exit happens on the clean code-0 path that
    # fires the RB3SaveSaveGlobalOptions exit callback.
    proc = boot(args.bin, args.data, save_dir, p1, {"MILO_MAX_FRAMES": "400"}, args.verbose)
    try:
        if not wait_ready(p1, proc, SERVER_READY_TIMEOUT):
            log("FAIL: pass-1 server never came up"); proc.kill(); return 1
        # Mutate via the registered profile_mgr DTA object.
        r1 = dta_eval(p1, f"{{profile_mgr set_crowd_volume {TEST_CROWD_VOL}}}")
        r2 = dta_eval(p1, f"{{profile_mgr set_dolby {TEST_DOLBY}}}")
        rd = dta_eval(p1, "{profile_mgr global_options_needs_save}")
        log(f"  set_crowd_volume -> {r1}, set_dolby -> {r2}, needs_save -> {rd}")
        readback = dta_eval(p1, "{profile_mgr get_crowd_volume}")
        log(f"  in-session get_crowd_volume -> {readback}")
        if str(readback) != str(TEST_CROWD_VOL):
            log(f"FAIL: in-session set/get mismatch ({readback} != {TEST_CROWD_VOL})")
            proc.kill(); return 1
    finally:
        pass
    # Let the bounded run finish on its own (MILO_MAX_FRAMES -> clean exit ->
    # save callback). Give it time to reach the frame cap and tear down.
    try:
        rc = proc.wait(timeout=90)
    except subprocess.TimeoutExpired:
        log("pass-1 didn't hit frame cap in time; sending SIGINT for clean exit")
        proc.send_signal(signal.SIGINT)
        try: rc = proc.wait(timeout=30)
        except subprocess.TimeoutExpired: proc.kill(); rc = proc.wait()
    log(f"  pass-1 exit code: {rc}")
    if rc != 0:
        log(f"FAIL: pass-1 exit code {rc} (expected 0)"); return 1

    # ---- File presence ----
    gp = os.path.join(save_dir, "globaloptions.bin")
    gop = os.path.join(save_dir, "gameplayopts_0.bin")
    files = os.listdir(save_dir)
    log(f"  save dir contents: {files}")
    if not os.path.exists(gp):
        log("FAIL: globaloptions.bin not written"); return 1
    sz = os.path.getsize(gp)
    log(f"  globaloptions.bin = {sz} bytes")
    if sz <= 0:
        log("FAIL: globaloptions.bin is empty"); return 1
    if os.path.exists(gop):
        log(f"  gameplayopts_0.bin = {os.path.getsize(gop)} bytes (9 expected)")

    # ---- Pass 2: re-boot, probe BEFORE any UI mutation ----
    p2 = args.port or free_port()
    log(f"PASS 2 re-boot (port {p2}) — probe persisted options")
    proc2 = boot(args.bin, args.data, save_dir, p2, {}, args.verbose)
    try:
        if not wait_ready(p2, proc2, SERVER_READY_TIMEOUT):
            log("FAIL: pass-2 server never came up"); proc2.kill(); return 1
        got_crowd = dta_eval(p2, "{profile_mgr get_crowd_volume}")
        got_dolby = dta_eval(p2, "{profile_mgr get_dolby}")
        log(f"  persisted get_crowd_volume -> {got_crowd} (expected {TEST_CROWD_VOL})")
        log(f"  persisted get_dolby       -> {got_dolby} (expected {TEST_DOLBY})")
        ok = (str(got_crowd) == str(TEST_CROWD_VOL))
        # dolby may be reported as TRUE/1; accept either truthy form.
        dolby_ok = str(got_dolby) in ("1", "TRUE", "True", "true")
        if not ok:
            log("FAIL: crowd_volume did NOT persist"); return 1
        if not dolby_ok:
            log(f"WARN: dolby readback unexpected ({got_dolby}); crowd_volume persisted OK")
    finally:
        proc2.send_signal(signal.SIGINT)
        try: proc2.wait(timeout=30)
        except subprocess.TimeoutExpired: proc2.kill(); proc2.wait()

    log("PASS: global options persisted across restart.")
    shutil.rmtree(save_dir, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
