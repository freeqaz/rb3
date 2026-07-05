#!/usr/bin/env python3
"""
stub_census_smoke.py — boot-smoke + drift gate for the loud-by-default weak-stub
census (W0.2).

TWO MODES
---------
Default (boot smoke): launch rb3-native headless (RB3_HTTP=1), wait for the
embedded HTTP debug server, poll /api/health until boot has settled, then read
the engine's stderr census. Parses the `[STUB CENSUS] linked=...` banner and the
per-symbol `[STUB] first call to <sym>` first-hit lines (plus the atexit
`[STUB CENSUS]   hit:` dump if the run exited cleanly) into the authoritative
clean-boot hit-list, cross-references each hit against
native/src/band3_stub_registry.tsv, and EXITS NON-ZERO if any hit is classified
`assert-unreachable` (a swallowed call the port believes is unreachable, à la the
DrawParticlesBillboard / EndGame invisible-failure bugs). Prints the full
boot hit-list for the record.

--check-sync (drift gate): regenerate band3_link_stubs.s + band3_stub_table.inc
from the registry and fail red if the committed files have drifted from what the
generator emits (registry↔output must stay in lockstep). Runs the generator's
own `--check` (idempotency) gate. No engine boot.

USAGE
-----
    python3 scripts/native/stub_census_smoke.py [--port N] [--data DIR]
            [--bin PATH] [--min-frames N] [--settle SEC] [--verbose] [--keep-log]
    python3 scripts/native/stub_census_smoke.py --check-sync

Exit 0 = no assert-unreachable stub hit during boot (or, --check-sync: no drift).
Exit 1 = an assert-unreachable stub fired / boot failed / registry drift.
"""

import argparse
import http.client
import json
import os
import re
import signal
import socket
import subprocess
import sys
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-agent-W0.2", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")
REGISTRY = os.path.join(REPO, "native", "src", "band3_stub_registry.tsv")
GENERATOR = os.path.join(REPO, "scripts", "native", "gen_band3_link_stubs.py")

SERVER_READY_TIMEOUT = 40
BOOT_SETTLE_TIMEOUT = 120  # boot to a stable screen (intro/menu) with stubs fired

FIRST_HIT_RE = re.compile(r"^\[STUB\] first call to (\S+)")
ATEXIT_HIT_RE = re.compile(r"^\[STUB CENSUS\]\s+hit:\s+(\S+)")
BANNER_RE = re.compile(r"^\[STUB CENSUS\] linked=(\d+) \(func=(\d+) data=(\d+)\)")


def log(msg):
    print(f"[stub-census-smoke] {msg}", flush=True)


def load_registry_classes(path):
    """symbol -> class ('assert-unreachable' | 'ok-noop' | 'data-blob')."""
    classes = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            parts = line.rstrip("\n").split("\t")
            if len(parts) != 4:
                continue
            classes[parts[0]] = parts[2]
    return classes


# ---------------------------------------------------------------------------
# --check-sync: registry <-> generated .s/.inc drift gate.
# ---------------------------------------------------------------------------
def check_sync():
    log("regenerating band3_link_stubs.s + band3_stub_table.inc from registry (drift gate)")
    rc = subprocess.call([sys.executable, GENERATOR, "--check"])
    if rc == 0:
        log("PASS: no drift — committed .s/.inc match the registry.")
    else:
        log("FAIL: band3_link_stubs.s / band3_stub_table.inc have drifted from the "
            "registry. Re-run: python3 scripts/native/gen_band3_link_stubs.py")
    return rc


# ---------------------------------------------------------------------------
# Boot-smoke helpers (HTTP driver, pure stdlib — mirrors song-end-test.py).
# ---------------------------------------------------------------------------
def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def health(port):
    try:
        conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
        try:
            conn.request("GET", "/api/health")
            r = conn.getresponse()
            if r.status != 200:
                return None
            d = json.loads(r.read().decode("utf-8", "replace"))["data"]
            return int(d["frame"]), str(d["currentScreen"])
        finally:
            conn.close()
    except Exception:
        return None


def wait_for(port, predicate, timeout, label, verbose, proc):
    deadline = time.time() + timeout
    last = None
    while time.time() < deadline:
        if proc.poll() is not None:
            log(f"process exited (code {proc.returncode}) while waiting for {label}")
            return None
        h = health(port)
        if h is not None:
            frame, screen = h
            if verbose and (frame // 5, screen) != last:
                log(f"  ...{label}: frame={frame} screen='{screen}'")
                last = (frame // 5, screen)
            if predicate(frame, screen):
                return h
        time.sleep(0.5)
    return None


def parse_hits(log_path):
    """Union of per-symbol first-hit lines + any atexit dump lines. Returns
    (hits:set[str], banner:tuple|None)."""
    hits = set()
    banner = None
    try:
        with open(log_path, encoding="utf-8", errors="replace") as f:
            for line in f:
                m = FIRST_HIT_RE.match(line)
                if m:
                    hits.add(m.group(1))
                    continue
                m = ATEXIT_HIT_RE.match(line)
                if m:
                    hits.add(m.group(1))
                    continue
                if banner is None:
                    m = BANNER_RE.match(line)
                    if m:
                        banner = (int(m.group(1)), int(m.group(2)), int(m.group(3)))
    except OSError:
        pass
    return hits, banner


def boot_smoke(args):
    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}")
        log("      build it: cmake --build native/build-agent-W0.2 --target rb3-native -j8")
        return 1
    if not os.path.isdir(args.data):
        log(f"FAIL: asset dir not found: {args.data}")
        return 1

    classes = load_registry_classes(REGISTRY)
    port = args.port or free_port()
    log_path = os.path.join("/tmp", f"rb3-stub-census-{port}.log")
    logf = open(log_path, "w")

    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1",
        "RB3_HTTP": "1",
        "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1",
        "RB3_DATA": args.data,
        # Loud census must be ON (do not set RB3_STUB_QUIET).
    })
    env.pop("RB3_STUB_QUIET", None)

    log(f"launching {os.path.basename(args.bin)} (port {port}, headless), log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)

    rc = 1
    try:
        if wait_for(port, lambda f, s: True, SERVER_READY_TIMEOUT,
                    "server-ready", args.verbose, proc) is None:
            log("FAIL: HTTP server never came up")
            return 1
        log("HTTP server up")

        # Let boot settle: wait until frame advances past the boot spine so the
        # per-frame + init stubs have all fired at least once.
        h = wait_for(port, lambda f, s: f >= args.min_frames, BOOT_SETTLE_TIMEOUT,
                     "boot-settle", args.verbose, proc)
        if h is None:
            log(f"FAIL: never reached frame >= {args.min_frames} (boot did not settle)")
            return 1
        log(f"boot settled: frame={h[0]} screen='{h[1]}'")

        # Grace window for any once-per-frame stubs to log their first hit.
        time.sleep(args.settle)

        # The PLAN's "GET /api/health, exit" — one final probe, then terminate.
        _ = health(port)
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

    hits, banner = parse_hits(log_path)
    if banner is not None:
        log(f"census banner: linked={banner[0]} (func={banner[1]} data={banner[2]})")
    else:
        log("WARN: no [STUB CENSUS] banner in log — is the loud shim linked?")

    if not hits:
        log("WARN: no stub first-hit lines captured — boot may not have exercised the "
            "stub surface (or RB3_STUB_QUIET leaked into the env).")

    # Classify the boot hit-list.
    unclassified = sorted(h for h in hits if h not in classes)
    assert_hits = sorted(h for h in hits if classes.get(h) == "assert-unreachable")
    ok_hits = sorted(h for h in hits if classes.get(h) == "ok-noop")

    log(f"boot hit-list: {len(hits)} weak stub(s) fired "
        f"({len(ok_hits)} ok-noop, {len(assert_hits)} assert-unreachable, "
        f"{len(unclassified)} unclassified):")
    for h in sorted(hits):
        log(f"    [{classes.get(h, '??unclassified')}] {h}")

    if unclassified:
        log(f"FAIL: {len(unclassified)} hit stub(s) have no registry row (drift): "
            + ", ".join(unclassified))
        rc = 1
    elif assert_hits:
        log(f"FAIL: {len(assert_hits)} `assert-unreachable` stub(s) hit during boot — "
            "a swallowed call the port believes is unreachable:")
        for h in assert_hits:
            log(f"    {h}")
        log("      Promote to ok-noop (with a dated justification) in the registry if "
            "genuinely reached, or fix the invisible-failure bug.")
        rc = 1
    else:
        log("PASS: no assert-unreachable stub hit during boot.")
        rc = 0

    if rc == 0 and not args.keep_log:
        try:
            os.remove(log_path)
        except OSError:
            pass
    else:
        log(f"engine log: {log_path}")
    return rc


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check-sync", action="store_true",
                    help="registry<->generated .s/.inc drift gate (no engine boot)")
    ap.add_argument("--port", type=int, default=0, help="HTTP port (0 = auto-pick)")
    ap.add_argument("--bin", default=DEFAULT_BIN, help="rb3-native binary path")
    ap.add_argument("--data", default=DEFAULT_DATA, help="RB3_DATA asset dir")
    ap.add_argument("--min-frames", type=int, default=15,
                    help="wait until /api/health reports at least this many frames")
    ap.add_argument("--settle", type=float, default=2.0,
                    help="grace seconds after boot-settle for per-frame stubs to log")
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--keep-log", action="store_true")
    args = ap.parse_args()

    if args.check_sync:
        return check_sync()
    return boot_smoke(args)


if __name__ == "__main__":
    sys.exit(main())
