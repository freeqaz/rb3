#!/usr/bin/env python3
"""
song-select-surface-test.py — regression pin for the SongSort.cpp mTree-iterator fix.

WHAT IT PINS
------------
Two HX_NATIVE-guarded fixes prevent the song library from regressing to ~3 nodes:

  1. src/band3/meta_band/SongSort.cpp:132-138 and :153-159 —
     converts the std::equal_range raw ShortcutNode** pointer pair into a real
     host vector iterator for mTree.insert.  On Wii STLport iterator==pointer,
     but host STL requires a real iterator; without this fix mTree stays nearly
     empty and only ~3 nodes surface.

  2. src/band3/meta_band/MusicLibrary.cpp:1048-1110 —
     clear-slot + UILabel fallback for the 360-ARK song_select.milo.

If either fix is accidentally reverted (e.g., SongSort.cpp edited without a
native rebuild), {music_library num_data} drops from ~109 to ~3-4.

ASSERTIONS
----------
  1. song_select_screen reached via pure-keyboard nav.
  2. {music_library num_data} >= 100  (ground truth: 109).
  3. {music_library get_current_sort_name 0} == 'by_song'.
  4. After several Down presses, {music_library get_current_shortcut_ix} > 0
     (confirms the shortcut tree is populated and scrolling surfaces new groups).

NAV SEQUENCE (pure pad bits, no select:/msg: aids):
  pad 11 (Start)   x1+ : splash -> main_hub
  pad 6  (Confirm) x2  : main_hub -> PLAY NOW -> QUICKPLAY -> song_select

Usage:
    python3 scripts/native/song-select-surface-test.py [--port N] [--verbose]

Exit 0 = all assertions passed. Exit 1 = launch/nav failure or assertion failure.
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

# JoypadButton bits (src/system/os/Joypad.h) — same as keyboard-to-gameplay.py
START, CONFIRM = 11, 6
DDOWN = 14

SERVER_READY_TIMEOUT = 40
NAV_TIMEOUT = 90   # generous for slow machines

MIN_NUM_DATA = 100  # ground truth ~109; regression threshold


def log(m): print(f"[ss-surface-test] {m}", flush=True)

def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p

def http_get(port, path, timeout=10):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("GET", path)
        r = c.getresponse()
        return r.status, r.read()
    finally:
        c.close()

def http_post(port, path, body, timeout=15):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = c.getresponse()
        return r.status, r.read().decode("utf-8", "replace")
    finally:
        c.close()

def health(port):
    try:
        st, b = http_get(port, "/api/health")
        if st != 200:
            return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception:
        return None

def dta(port, expr):
    try:
        st, b = http_post(port, "/api/dta/eval", expr, timeout=10)
        if st != 200:
            return None
        d = json.loads(b)
        if not d.get("ok"):
            return None
        return d["data"].get("value")
    except Exception:
        return None

def press(port, bit):
    http_post(port, "/api/input", f"pad:{bit}")

def drain_pad(port):
    """Give the pad queue enough wall-time to drain (a hold + a few poll cycles)."""
    time.sleep(0.25)

def screenshot(port, path):
    st, data = http_get(port, "/api/screenshot", timeout=20)
    if st != 200 or not data or data[:8] != b"\x89PNG\r\n\x1a\n":
        return False
    with open(path, "wb") as f:
        f.write(data)
    return True

def wait_screen(port, want, timeout, proc, verbose=False):
    dl = time.time() + timeout
    last = None
    while time.time() < dl:
        if proc.poll() is not None:
            log(f"FAIL: process exited (code {proc.returncode})")
            return None
        h = health(port)
        if h:
            f, m, s = h
            if verbose and s != last:
                log(f"    ...waiting '{want}': frame={f} screen='{s}'")
                last = s
            if (callable(want) and want(s)) or s == want:
                return h
        time.sleep(0.3)
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--overlay", default=os.path.join(REPO, "native", "dta"))
    ap.add_argument("--out", default="/tmp/rb3-ss-surface-test")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}")
        return 1

    os.makedirs(args.out, exist_ok=True)
    port = args.port or free_port()
    log_path = os.path.join("/tmp", f"rb3-ss-surface-{port}.log")
    logf = open(log_path, "w")

    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1",
        "RB3_HTTP": "1",
        "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1",
        "RB3_DATA": args.data,
        "RB3_DTA_OVERLAY": args.overlay,
        "RB3_INPUT_DEBUG": "1",
    })

    log(f"launching rb3-native (port {port}, headless); log -> {log_path}")
    proc = subprocess.Popen(
        [args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
        cwd=REPO, start_new_session=True,
    )

    failures = []
    rc = 1
    try:
        # --- Wait for HTTP server -----------------------------------------------
        if wait_screen(port, lambda s: True, SERVER_READY_TIMEOUT, proc) is None:
            log("FAIL: HTTP server never came up")
            return 1
        log("HTTP server up")

        # --- Splash: press Start until main_hub_screen --------------------------
        wait_screen(port, lambda s: s and s != "", 60, proc, args.verbose)
        for attempt in range(8):
            cur = health(port)
            if cur and cur[2] == "main_hub_screen":
                break
            press(port, START)
            drain_pad(port)
            time.sleep(0.6)
        if wait_screen(port, "main_hub_screen", 30, proc, args.verbose) is None:
            log("FAIL: never reached main_hub_screen via Start")
            return 1
        log("reached main_hub_screen")

        # --- main_hub: Confirm x2 -> PLAY NOW -> QUICKPLAY -> song_select ------
        for attempt in range(10):
            cur = health(port)
            if cur and cur[2] == "song_select_screen":
                break
            press(port, CONFIRM)
            drain_pad(port)
            time.sleep(0.7)
        if wait_screen(port, "song_select_screen", 40, proc, args.verbose) is None:
            log("FAIL: never reached song_select_screen")
            return 1
        log("reached song_select_screen")

        # Let the list populate and enter animation settle.
        time.sleep(2.0)

        # --- ASSERTION 1: song_select_screen reached (already confirmed above) --
        h = health(port)
        if h and h[2] == "song_select_screen":
            log("ASSERT 1 PASS: song_select_screen reached via pure keyboard")
        else:
            msg = f"ASSERT 1 FAIL: screen='{h[2] if h else '?'}', expected song_select_screen"
            log(msg)
            failures.append(msg)

        # Save a debug screenshot (optional, not asserted).
        ss_path = os.path.join(args.out, "00_song_select.png")
        screenshot(port, ss_path)
        log(f"debug screenshot -> {ss_path}")

        # --- ASSERTION 2: num_data >= MIN_NUM_DATA ------------------------------
        num_data_raw = dta(port, "{music_library num_data}")
        try:
            num_data = int(num_data_raw)
        except (TypeError, ValueError):
            num_data = -1
        if num_data >= MIN_NUM_DATA:
            log(f"ASSERT 2 PASS: num_data={num_data} >= {MIN_NUM_DATA} "
                f"(SongSort mTree-iterator fix is intact)")
        else:
            msg = (f"ASSERT 2 FAIL: num_data={num_data} (raw='{num_data_raw}'), "
                   f"expected >= {MIN_NUM_DATA}. "
                   f"SongSort.cpp mTree-iterator fix may have regressed!")
            log(msg)
            failures.append(msg)

        # --- ASSERTION 3: current sort name == 'by_song' -----------------------
        sort_name_raw = dta(port, "{music_library get_current_sort_name 0}")
        # DTA returns quoted symbols like "'by_song'" — strip surrounding quotes.
        sort_name = (sort_name_raw or "").strip("'\"") if sort_name_raw else ""
        if sort_name == "by_song":
            log(f"ASSERT 3 PASS: get_current_sort_name='{sort_name}'")
        else:
            msg = f"ASSERT 3 FAIL: get_current_sort_name='{sort_name}' (raw='{sort_name_raw}'), expected 'by_song'"
            log(msg)
            failures.append(msg)

        # --- ASSERTION 4: shortcut_ix advances after scrolling Down -------------
        ix_before_raw = dta(port, "{music_library get_current_shortcut_ix}")
        try:
            ix_before = int(ix_before_raw)
        except (TypeError, ValueError):
            ix_before = -1
        log(f"shortcut_ix before scroll: {ix_before} (raw='{ix_before_raw}')")

        # Press Down several times to scroll past the first group header.
        for _ in range(6):
            press(port, DDOWN)
            drain_pad(port)
            time.sleep(0.2)

        ss_path2 = os.path.join(args.out, "01_after_scroll.png")
        screenshot(port, ss_path2)
        log(f"debug screenshot (post-scroll) -> {ss_path2}")

        ix_after_raw = dta(port, "{music_library get_current_shortcut_ix}")
        try:
            ix_after = int(ix_after_raw)
        except (TypeError, ValueError):
            ix_after = -1
        log(f"shortcut_ix after scroll: {ix_after} (raw='{ix_after_raw}')")

        if ix_after > ix_before:
            log(f"ASSERT 4 PASS: get_current_shortcut_ix advanced {ix_before} -> {ix_after} "
                "(shortcut tree is populated, scroll surfaces new groups)")
        else:
            msg = (f"ASSERT 4 FAIL: get_current_shortcut_ix did not advance "
                   f"({ix_before} -> {ix_after}). mTree may be under-populated.")
            log(msg)
            failures.append(msg)

        # --- Summary ------------------------------------------------------------
        if not failures:
            log(f"RESULT: PASS — all 4 assertions passed "
                f"(num_data={num_data}, sort='{sort_name}', "
                f"shortcut_ix {ix_before}->{ix_after})")
            rc = 0
        else:
            log(f"RESULT: FAIL — {len(failures)} assertion(s) failed:")
            for f in failures:
                log(f"  * {f}")
            rc = 1
        return rc

    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try:
                proc.wait(timeout=8)
            except subprocess.TimeoutExpired:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception:
            pass
        logf.close()
        log(f"engine log: {log_path}")
        log(f"output dir: {args.out}")


if __name__ == "__main__":
    sys.exit(main())
