#!/usr/bin/env python3
"""render-asset.py — thin wrapper around rb3-native's --viewer mode.

Rebuilds rb3-native if stale, runs the standalone .milo asset renderer headless,
and prints the output PNG path + a short load census. See
docs/native/asset-viewer-2026-07-02/VIEWER.md for the full CLI.

Examples:
  scripts/native/render-asset.py char/main/hair/female/gen/female_hair_long_resource.milo_xbox
  scripts/native/render-asset.py <milo> --out /tmp/hair.png --sim 30
  scripts/native/render-asset.py <milo> --list
  scripts/native/render-asset.py <milo> --azimuth 40 --elevation 20 --distance 30

All flags after the milo path are passed straight through to rb3-native --viewer
(--out, --frames, --sim, --subdir, --hide, --only-showing, --azimuth,
--elevation, --distance, --cam-dir, --width, --height, --list, --verbose).
"""
import os
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BUILD_DIR = os.path.join(REPO, "native", "build-native")
BIN = os.path.join(BUILD_DIR, "rb3-native")
DATA = os.environ.get("RB3_DATA", os.path.join(REPO, "orig-assets", "extracted"))


def build_if_stale():
    """Rebuild rb3-native (ninja is a no-op when nothing changed)."""
    print("[render-asset] building rb3-native (no-op if up to date)...", flush=True)
    r = subprocess.run(
        ["cmake", "--build", BUILD_DIR, "--target", "rb3-native"],
        cwd=REPO, capture_output=True, text=True,
    )
    if r.returncode != 0:
        sys.stderr.write(r.stdout + r.stderr)
        sys.stderr.write("[render-asset] BUILD FAILED\n")
        sys.exit(r.returncode)


def main():
    args = sys.argv[1:]
    if not args or args[0] in ("-h", "--help"):
        print(__doc__)
        # Also show the binary's own usage if it exists.
        if os.path.exists(BIN):
            subprocess.run([BIN, "--viewer", "--help"])
        return 0

    build_if_stale()

    env = dict(os.environ)
    env["RB3_DATA"] = DATA
    env["MILO_HEADLESS"] = "1"

    cmd = [BIN, "--viewer"] + args
    print("[render-asset] $ " + " ".join(cmd), flush=True)
    proc = subprocess.run(cmd, env=env, capture_output=True, text=True)
    out = proc.stdout

    # Surface the interesting lines: census, the dir summary, the PNG path.
    png = None
    census = []
    in_census = False
    for line in out.splitlines():
        if line.startswith("=== census"):
            in_census = True
        if in_census:
            census.append(line)
        if line.startswith("=== end census"):
            in_census = False
        if "rb3-viewer: dir " in line:
            print("[render-asset] " + line.split("rb3-viewer: ", 1)[1])
        if line.startswith("rb3-viewer: wrote "):
            png = line.split("rb3-viewer: wrote ", 1)[1].strip()
        if line.startswith("rb3-viewer: non-clear"):
            print("[render-asset] " + line.split("rb3-viewer: ", 1)[1])

    if census:
        print("\n".join(census))

    if proc.returncode != 0:
        sys.stderr.write(proc.stderr[-4000:])
        sys.stderr.write(f"\n[render-asset] rb3-native exited {proc.returncode}\n")
        # GPU-sandbox hint (exit 2 == our GPU-init-failed code).
        if proc.returncode == 2:
            sys.stderr.write("[render-asset] (GPU init may be sandboxed — retry "
                             "with the sandbox disabled)\n")
        return proc.returncode

    if png:
        print(f"[render-asset] PNG: {png}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
