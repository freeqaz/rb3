#!/usr/bin/env python3
"""
crowd-origin-posdump.py — drive rb3-native headless to gameplay, call the
{rb3_pos_dump} debug DTA func, and print a verdict table that adjudicates the
crowd/band-gear "everything at origin" hypotheses (H1/H2/H4) from
docs/native/crowd-origin/PLAN.md.

WHAT IT MEASURES
----------------
In gameplay the crowd + band gear (incl. the DRUM KIT) collapse to one spot
(looks like origin), while a static venue prop (dartboard) renders correctly.
This harness boots the engine to a live song, then calls the native-only
{rb3_pos_dump} func (native/src/rb3_http_handlers.cpp) which walks the live
object tree and emits, per object:
  - crowd members:  m3DChars[i].unk0.v  (the authored per-member world position)
  - band chars:     WorldXfm().v, TransParent() name, mInstDir world-sphere center
  - static props:   WorldXfm().v + Name + ClassName (the dartboard control)

VERDICT FORKS (PLAN.md §1/§3)
-----------------------------
  crowd SPREAD + band roots ORIGIN + props fine  => H2 (band-only); the crowd
        "pile" is shard-amplified spread, not origin. Drum-kit fix is band-side.
  crowd ORIGIN + band roots ORIGIN + props fine   => H1 (shared venue reparent)
        or H1+H4.
  crowd ORIGIN + props ALSO origin                => blanket transform decode
        failure (H4 broadened).

The smoking-gun for H2 is a band root at origin with a NULL / non-venue parent.

USAGE
-----
    python3 scripts/native/crowd-origin-posdump.py [--port N] [--data DIR]
            [--bin PATH] [--verbose] [--keep-log]

Always keeps the engine log when it can't reach gameplay (so you can see where
it stalled). Exit 0 if the dump was produced, 1 otherwise.
"""

import argparse
import http.client
import json
import math
import os
import re
import signal
import socket
import subprocess
import sys
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

# Canonical headless boot-to-song nav (copied verbatim from song-end-test.py —
# the working gameplay nav from scout-repro-tooling.md). No jump: we want the
# crowd + band live, mid-song.
NAV_SCRIPT = (
    "@10:start,@30:confirm,"
    "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,"
    "@320:down,@350:msg:music_library:select_highlighted_node,"
    "@380:track:guitar,@390:difficulty:expert,@450:msg:overshell:end_override_flow:1:0,"
    "@500:nofail,@520:autohit"
)

SERVER_READY_TIMEOUT = 40
GAMEPLAY_TIMEOUT = 240    # boot + nav + cinematic intro pre-roll
GAMEPLAY_SONGMS = 2000.0  # song clock past the intro = gameplay underway
SETTLE_SECONDS = 3.0      # let the venue/band/crowd finish placing after song start


def log(msg):
    print(f"[posdump] {msg}", flush=True)


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def http_get(port, path, timeout=5):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        conn.request("GET", path)
        r = conn.getresponse()
        return r.status, r.read().decode("utf-8", "replace")
    finally:
        conn.close()


def http_post(port, path, body, timeout=20):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        conn.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = conn.getresponse()
        return r.status, r.read().decode("utf-8", "replace")
    finally:
        conn.close()


def dta_eval(port, expr, timeout=20):
    """POST a DTA expression to /api/dta/eval; return its value or None."""
    try:
        status, body = http_post(port, "/api/dta/eval", expr, timeout=timeout)
        if status != 200:
            return None
        d = json.loads(body)
        if not d.get("ok"):
            return None
        data = d["data"]
        if data.get("type") in ("int", "float"):
            return data["value"]
        return data.get("value")
    except Exception:
        return None


def health(port):
    try:
        status, body = http_get(port, "/api/health")
    except Exception:
        return None
    if status != 200:
        return None
    try:
        d = json.loads(body)["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception:
        return None


def wait_for(port, predicate, timeout, label, verbose, proc):
    deadline = time.time() + timeout
    last = None
    while time.time() < deadline:
        if proc.poll() is not None:
            log(f"FAIL: process exited (code {proc.returncode}) while waiting for {label}")
            return None
        h = health(port)
        if h is not None:
            frame, song_ms, screen = h
            if verbose and (frame // 50, screen) != last:
                log(f"  ...{label}: frame={frame} songMs={song_ms:.0f} screen='{screen}'")
                last = (frame // 50, screen)
            if predicate(frame, song_ms, screen):
                return h
        time.sleep(0.5)
    return None


# --- [POSDUMP] log parsing ---------------------------------------------------

CROWD_RE = re.compile(
    r"\[POSDUMP\] kind=crowd\s+name=(\S+)\s+i=(\d+)\s+pos=([-\d.eE]+),([-\d.eE]+),([-\d.eE]+)"
)
BAND_RE = re.compile(
    r"\[POSDUMP\] kind=band\s+name=(\S+)\s+root=([-\d.eE]+),([-\d.eE]+),([-\d.eE]+)\s+"
    r"parent=(\S+)\s+inst=([-\d.eE]+),([-\d.eE]+),([-\d.eE]+)(.*)"
)
PROP_RE = re.compile(
    r"\[POSDUMP\] kind=prop\s+name=(\S+)\s+class=(\S+)\s+world=([-\d.eE]+),([-\d.eE]+),([-\d.eE]+)"
)
SHARD_RE = re.compile(
    r"\[SHARD_GUARD\] dropped degenerate skinned mesh='([^']*)'.*?dir='([^']*)'\s+bone0=\(([-\d.]+),([-\d.]+),([-\d.]+)\)"
)


def mag(x, y, z):
    return math.sqrt(x * x + y * y + z * z)


def parse_log(path):
    crowd, band, props, shards = [], [], [], []
    try:
        with open(path, "r", errors="replace") as f:
            for line in f:
                m = CROWD_RE.search(line)
                if m:
                    crowd.append({
                        "name": m.group(1), "i": int(m.group(2)),
                        "x": float(m.group(3)), "y": float(m.group(4)), "z": float(m.group(5)),
                    })
                    continue
                m = BAND_RE.search(line)
                if m:
                    band.append({
                        "name": m.group(1),
                        "rx": float(m.group(2)), "ry": float(m.group(3)), "rz": float(m.group(4)),
                        "parent": m.group(5),
                        "ix": float(m.group(6)), "iy": float(m.group(7)), "iz": float(m.group(8)),
                        "note": m.group(9).strip(),
                    })
                    continue
                m = PROP_RE.search(line)
                if m:
                    props.append({
                        "name": m.group(1), "cls": m.group(2),
                        "x": float(m.group(3)), "y": float(m.group(4)), "z": float(m.group(5)),
                    })
                    continue
                m = SHARD_RE.search(line)
                if m:
                    shards.append({
                        "mesh": m.group(1), "dir": m.group(2),
                        "bx": float(m.group(3)), "by": float(m.group(4)), "bz": float(m.group(5)),
                    })
    except FileNotFoundError:
        pass
    return crowd, band, props, shards


def classify(crowd, band, props, shards):
    """Return (verdict, reasoning) per PLAN.md §3 forks.

    NOTE: the WorldCrowd CONTAINER is not always enumerable from the walked roots
    (it lives under the per-song venue proxy subdir). When it is absent we fall
    back to the engine [SHARD_GUARD] band/crowd drops, whose bone0 world position
    is a faithful proxy for where those skinned characters actually render."""
    crowd_origin = sum(1 for c in crowd if mag(c["x"], c["y"], c["z"]) < 1.0)
    band_origin = sum(1 for b in band if mag(b["rx"], b["ry"], b["rz"]) < 1.0)
    prop_origin = sum(1 for p in props if mag(p["x"], p["y"], p["z"]) < 1.0)

    crowd_all_origin = len(crowd) > 0 and crowd_origin == len(crowd)
    crowd_spread = len(crowd) > 0 and crowd_origin < max(1, int(0.5 * len(crowd)))
    band_all_origin = len(band) > 0 and band_origin == len(band)
    band_spread = len(band) > 0 and band_origin == 0
    band_parents_null = all(b["parent"] in ("NULL", "?") for b in band) if band else False
    # props "fine" = the venue WorldDirs + most static props are NOT at origin.
    # (Cameras/lights/archetype-Characters legitimately sit at origin, so a raw
    # at-origin count over-reports; key on the named venue WorldDir control.)
    props_fine = len(props) > 0 and prop_origin < max(1, int(0.5 * len(props)))

    # SHARD_GUARD fallback for the crowd distribution: bone0 world positions of the
    # dropped skinned crowd/band meshes. If these are SPREAD (not at origin), the
    # characters render at real venue positions and only the V24 skin-ratio guard
    # is dropping them (a SKINNING issue, not a placement-at-origin one).
    shard_origin = sum(1 for s in shards if mag(s["bx"], s["by"], s["bz"]) < 1.0)
    shard_spread = len(shards) > 0 and shard_origin == 0
    shard_dirs = {s["dir"] for s in shards}
    inst_shards = any(d == "instrument" for d in shard_dirs)

    # GROUND-TRUTH FORK (measured 2026-06-20): band roots SPREAD + instrument/crowd
    # shard bone0 SPREAD + venue WorldDirs at their own origin = placement is FINE;
    # the visible "pile" is the V24 shard guard DROPPING heavily-deformed skinned
    # meshes (worldExt/bindExt ~4.3-4.7), NOT an at-origin collapse.
    if band_spread and shard_spread and len(shards) > 0:
        return ("REFUTES H1/H2/H4 — skinning-guard drop, not placement-at-origin",
                "band roots SPREAD (NOT origin) + instrument/crowd shard bone0 "
                "SPREAD (NOT origin) + venue placed => H1/H2/H4 (everything-at-"
                "origin) are all REFUTED by the live data. The collapse is the V24 "
                "[SHARD_GUARD] dropping over-deformed skinned meshes "
                f"(dirs={sorted(shard_dirs)}, inst_kit={inst_shards}), a SKINNING "
                "issue. The fix is the skin-deform / shard-guard path, not venue "
                "placement.")

    if crowd_all_origin and band_all_origin and props_fine:
        return ("H1 (or H1+H4)",
                "crowd at origin + band roots at origin + props fine => one shared "
                "venue reparent collapse (WorldInstance::SyncDir). A single "
                "HX_NATIVE fix in world/Instance.cpp covers both halves.")
    if crowd_spread and band_all_origin and props_fine:
        nullp = " (parents NULL/non-venue => smoking gun)" if band_parents_null else ""
        return ("H2 (band-only)",
                "crowd SPREAD + band roots at origin + props fine => band character "
                f"root never placed into the venue stage spot{nullp}. The crowd "
                "'pile' is shard-amplified spread, not origin. Fix is band-side "
                "(place mTargets[i] under player_<inst>0_base).")
    if band_all_origin and shard_origin > 0 and shard_origin == len(shards):
        nullp = " (parents NULL/non-venue)" if band_parents_null else ""
        return ("H2/H1 (band + crowd at origin via shards)",
                f"band roots at origin{nullp} AND shard bone0 at origin => "
                "placement collapse for both halves (H1 if one seam, else H2+H4).")
    return ("inconclusive",
            "distribution did not match a clean fork; inspect the table below.")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--keep-log", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}")
        log("      build it: cmake --build native/build-native --target rb3-native")
        return 1
    if not os.path.isdir(args.data):
        log(f"FAIL: asset dir not found: {args.data}")
        return 1

    port = args.port or free_port()
    log_path = os.path.join("/tmp", f"rb3-posdump-{port}.log")
    snap_path = os.path.join("/tmp", f"rb3-posdump-{port}.snapshot.log")
    logf = open(log_path, "w")

    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1",
        "RB3_HTTP": "1",
        "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1",
        "RB3_DATA": args.data,
        "RB3_GAME_INPUT": NAV_SCRIPT,
        "POS_DUMP_VERBOSE": "1",   # emit [POSDUMP] per-object lines
        "SHARD_DBG": "1",          # cross-check [SHARD_GUARD] drops
    })

    log(f"launching {os.path.basename(args.bin)} (port {port}, headless), log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)

    rc = 1
    summary = None
    reached_gameplay = False
    try:
        if wait_for(port, lambda f, m, s: True, SERVER_READY_TIMEOUT,
                    "server-ready", args.verbose, proc) is None:
            log("FAIL: HTTP server never came up")
            return 1
        log("HTTP server up")

        h = wait_for(port, lambda f, m, s: m > GAMEPLAY_SONGMS,
                     GAMEPLAY_TIMEOUT, "gameplay-start", args.verbose, proc)
        if h is None:
            log("FAIL: never reached gameplay (song clock never advanced past intro)")
            log("      Dumping whatever objects ARE present anyway (may be empty)...")
        else:
            reached_gameplay = True
            log(f"gameplay underway: frame={h[0]} songMs={h[1]:.0f} screen='{h[2]}'")
            log(f"settling {SETTLE_SECONDS:.0f}s for venue/band/crowd placement...")
            time.sleep(SETTLE_SECONDS)

        # The verdict line (always returned, even if no objects).
        summary = dta_eval(port, "{rb3_pos_dump}")
        log(f"RAW SUMMARY: {summary}")

        # Give the engine a moment to flush the [POSDUMP] lines to the log, then
        # snapshot it BEFORE teardown. The child's stderr buffer can be lost on
        # SIGTERM, and a regular-file log can be re-truncated by a same-port run,
        # so we copy the live bytes now and parse from the durable snapshot.
        time.sleep(1.0)
        logf.flush()
        os.fsync(logf.fileno())
        try:
            with open(log_path, "rb") as src, open(snap_path, "wb") as dst:
                dst.write(src.read())
        except Exception as e:
            log(f"WARN: could not snapshot log: {e}")
        rc = 0 if summary else 1
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

    # Parse the verbose dump from the durable snapshot (taken before teardown).
    parse_path = snap_path if os.path.exists(snap_path) else log_path
    crowd, band, props, shards = parse_log(parse_path)

    print("\n========================= POSDUMP RESULTS =========================")
    print(f"reached_gameplay={reached_gameplay}")
    print(f"summary: {summary}")
    print(f"counts:  crowd={len(crowd)} band={len(band)} props={len(props)} "
          f"shard_drops={len(shards)}")

    print("\n--- BAND CHARACTERS (root WorldXfm, TransParent, mInstDir/kit) ---")
    if not band:
        print("  (none enumerated)")
    for b in band:
        rmag = mag(b["rx"], b["ry"], b["rz"])
        imag = mag(b["ix"], b["iy"], b["iz"])
        flag = "  <-- AT ORIGIN" if rmag < 1.0 else ""
        pflag = "  [parent NULL/non-venue]" if b["parent"] in ("NULL", "?") else ""
        print(f"  {b['name']:>16}  root=({b['rx']:.1f},{b['ry']:.1f},{b['rz']:.1f}) "
              f"|{rmag:.1f}|  parent={b['parent']}{pflag}  "
              f"inst=({b['ix']:.1f},{b['iy']:.1f},{b['iz']:.1f}) |{imag:.1f}| "
              f"{b['note']}{flag}")

    print("\n--- CROWD MEMBERS (m3DChars[i].unk0.v authored positions) ---")
    if not crowd:
        print("  (none enumerated)")
    else:
        c_origin = sum(1 for c in crowd if mag(c["x"], c["y"], c["z"]) < 1.0)
        xs = [c["x"] for c in crowd]
        ys = [c["y"] for c in crowd]
        zs = [c["z"] for c in crowd]
        print(f"  count={len(crowd)}  at_origin={c_origin}  "
              f"x=[{min(xs):.1f},{max(xs):.1f}] y=[{min(ys):.1f},{max(ys):.1f}] "
              f"z=[{min(zs):.1f},{max(zs):.1f}]")
        for c in crowd[:10]:
            cm = mag(c["x"], c["y"], c["z"])
            flag = "  <-- ORIGIN" if cm < 1.0 else ""
            print(f"    {c['name']} i={c['i']}: ({c['x']:.1f},{c['y']:.1f},{c['z']:.1f}) "
                  f"|{cm:.1f}|{flag}")
        if len(crowd) > 10:
            print(f"    ... ({len(crowd) - 10} more)")

    print("\n--- STATIC PROPS (control; sample, NON-origin expected) ---")
    if not props:
        print("  (none enumerated)")
    else:
        p_origin = sum(1 for p in props if mag(p["x"], p["y"], p["z"]) < 1.0)
        print(f"  count={len(props)}  at_origin={p_origin}")
        # Show a sample: prefer named non-origin props + any dartboard-like control.
        shown = 0
        for p in props:
            pm = mag(p["x"], p["y"], p["z"])
            if shown < 10 and (pm >= 1.0 or "dart" in p["name"].lower()):
                flag = "  <-- ORIGIN" if pm < 1.0 else ""
                print(f"    {p['name']} [{p['cls']}]: "
                      f"({p['x']:.1f},{p['y']:.1f},{p['z']:.1f}) |{pm:.1f}|{flag}")
                shown += 1

    print("\n--- SHARD_GUARD cross-check (engine [SHARD_GUARD] drops) ---")
    if not shards:
        print("  (no shard drops logged)")
    else:
        band_dir_drops = [s for s in shards if s["dir"].startswith("player")]
        origin_bone = [s for s in shards if mag(s["bx"], s["by"], s["bz"]) < 1.0]
        print(f"  total_drops={len(shards)}  player_*_dir_drops={len(band_dir_drops)}  "
              f"bone0_at_origin={len(origin_bone)}")
        for s in shards[:8]:
            print(f"    mesh='{s['mesh']}' dir='{s['dir']}' "
                  f"bone0=({s['bx']:.1f},{s['by']:.1f},{s['bz']:.1f})")

    verdict, reasoning = classify(crowd, band, props, shards)
    print("\n========================= VERDICT =========================")
    print(f"  {verdict}")
    print(f"  {reasoning}")
    print("==========================================================\n")

    if os.path.exists(snap_path):
        log(f"verbose snapshot log: {snap_path}")
    if rc != 0 or not reached_gameplay or args.keep_log:
        log(f"engine log: {log_path}")
    else:
        log(f"engine log: {log_path}  (kept for reference)")

    return rc


if __name__ == "__main__":
    sys.exit(main())
