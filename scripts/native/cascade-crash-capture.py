#!/usr/bin/env python3
"""
cascade-crash-capture.py — root-cause the C11 guest-profile cascade.

Boots rb3-native UNDER gdb (batch) with RB3_GUEST_PROFILE=1 (+ optional extra
env), drives HTTP nav splash->main_hub (where the guest profile installs) and
then toward the customize closet, and captures the exact fault: signal + full
backtrace + registers + all-thread backtraces. gdb stops on SIGSEGV/SIGABRT so
we get the precise faulting frame even when MILO's own handler would mask it.

Usage:
  scripts/native/cascade-crash-capture.py                       # guest profile only
  scripts/native/cascade-crash-capture.py --extra-env RB3_CHAR_PREVIEW=1
  scripts/native/cascade-crash-capture.py --to customize        # push into customize submenu
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

START, CONFIRM, CANCEL = 11, 6, 5
DUP, DRIGHT, DDOWN, DLEFT = 12, 13, 14, 15


def log(m): print(f"[cascade] {m}", flush=True)

def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p

def http_get(port, path, timeout=20):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally: c.close()

def http_post(port, path, body, timeout=10):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = c.getresponse(); return r.status, r.read().decode("utf-8", "replace")
    finally: c.close()

def health(port):
    try:
        st, b = http_get(port, "/api/health", timeout=5)
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), float(d.get("songMs", 0)), str(d["currentScreen"])
    except Exception: return None

def press(port, bit):
    try: http_post(port, "/api/input", f"pad:{bit}")
    except Exception: pass

def alive(proc): return proc.poll() is None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--overlay", default=os.path.join(REPO, "native", "dta"))
    ap.add_argument("--extra-env", default="")
    ap.add_argument("--to", choices=["main_hub", "customize"], default="customize")
    ap.add_argument("--gdblog", default="/tmp/rb3-cascade-gdb.log")
    ap.add_argument("--timeout", type=float, default=120.0)
    args = ap.parse_args()

    port = args.port or free_port()
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_DTA_OVERLAY": args.overlay, "RB3_GUEST_PROFILE": "1"})
    for kv in args.extra_env.split():
        if "=" in kv:
            k, v = kv.split("=", 1); env[k] = v

    gdb_cmds = [
        "set pagination off",
        "set confirm off",
        "handle SIGSEGV stop print nopass",
        "handle SIGABRT stop print nopass",
        "handle SIGPIPE nostop noprint pass",
        "run",
        "printf \"\\n===SIGNAL CAUGHT===\\n\"",
        "printf \"\\n===BACKTRACE (faulting thread)===\\n\"",
        "bt",
        "printf \"\\n===BACKTRACE FULL (top frames)===\\n\"",
        "bt full 12",
        "printf \"\\n===REGISTERS===\\n\"",
        "info registers rip rax rbx rcx rdx rsi rdi rbp rsp r8 r9 r10 r11 r12 r13 r14 r15",
        "printf \"\\n===FAULTING INSTRUCTION===\\n\"",
        "x/4i $pc",
        "printf \"\\n===ALL THREADS===\\n\"",
        "thread apply all bt",
        "printf \"\\n===END===\\n\"",
    ]
    cmd = ["gdb", "-batch"]
    for c in gdb_cmds: cmd += ["-ex", c]
    cmd += ["--args", args.bin]

    logf = open(args.gdblog, "w")
    log(f"launching under gdb (port {port}); gdb log -> {args.gdblog}")
    log(f"extra-env: {args.extra_env or '(none)'}; target screen: {args.to}")
    proc = subprocess.Popen(cmd, env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    deadline = time.time() + args.timeout
    try:
        # wait for HTTP server
        up = False
        while time.time() < deadline and alive(proc):
            if health(port): up = True; break
            time.sleep(0.4)
        if not up:
            log("server never came up (may have crashed at boot — check gdb log)")
            proc.wait(timeout=20)
            return 0
        log("server up; driving splash -> main_hub")

        # splash -> main_hub (guest profile installs here)
        reached_hub = False
        for _ in range(14):
            if not alive(proc): break
            h = health(port)
            if h and h[2] == "main_hub_screen": reached_hub = True; break
            press(port, START); time.sleep(0.7)
        log(f"after start spam: alive={alive(proc)} screen={health(port)[2] if (alive(proc) and health(port)) else 'CRASHED/none'}")

        if alive(proc) and reached_hub and args.to == "customize":
            # main_hub: scroll DOWN to 'customize' then confirm into submenu
            log("at main_hub; navigating down to customize + confirm")
            time.sleep(1.0)
            for _ in range(3):
                if not alive(proc): break
                press(port, DDOWN); time.sleep(0.5)
            if alive(proc):
                press(port, CONFIRM); time.sleep(1.0)
            # submenu: try confirming the first entry (customize band) too
            for _ in range(2):
                if not alive(proc): break
                h = health(port)
                log(f"  customize nav: alive={alive(proc)} screen={h[2] if h else '?'}")
                press(port, CONFIRM); time.sleep(1.0)

        # let any deferred cascade fire; poll until crash or short idle
        idle_until = time.time() + 15
        while time.time() < idle_until and alive(proc):
            time.sleep(0.5)

        if alive(proc):
            log("NO CRASH within window — boot/nav clean. Terminating.")
            try: os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            except Exception: pass
        else:
            log("PROCESS EXITED (likely crash) — gdb backtrace captured.")
        proc.wait(timeout=25)
    except subprocess.TimeoutExpired:
        log("gdb wait timed out; killing")
        try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
    finally:
        logf.close()
    log(f"--- gdb log tail ({args.gdblog}) ---")
    with open(args.gdblog) as f:
        lines = f.readlines()
    # print the signal+backtrace section if present, else tail
    joined = "".join(lines)
    if "===SIGNAL CAUGHT===" in joined:
        idx = joined.index("===SIGNAL CAUGHT===")
        sys.stdout.write(joined[idx:])
    else:
        sys.stdout.write("".join(lines[-60:]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
