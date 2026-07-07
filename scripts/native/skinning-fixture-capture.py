#!/usr/bin/env python3
"""
skinning-fixture-capture.py — R2 (Wave 17, Lane S) oracle-fixture capture.

Wraps the HANDS-adjudication capture protocol (keyboard-to-gameplay.py --song-downs
3 --game-burst 15 --burst-interval 0.3, RB3_FIXED_CLOCK=1) once PER ARM with that
arm's environment + the RB3_PALETTE_DUMP engine probe (committed engine 04651e1).
Collects the per-(mesh,owner,frame) PaletteFrame dumps + matched burst screenshots
into  docs/native/engine-arch-review-2026-07-05/execution/R2-FIXTURES/evidence/<arm>/
(OPTIONS.md §4.7 — evidence committed), then curates the fixture subset into
native/tests/goldens/r2-skinning/<arm>/ with a MANIFEST.txt.

The five arms (PLAN-R2 §3.1):
  good-body    default (all hands flags unset)                 known-GOOD
  bad-ceiling  default                                         known-BAD (rigid displacement; Tier-1 87.3/42.6)
  bad-torn     RB3_HANDS_AUTHORED_REPOINT=1  (A3 carve-out)    known-BAD (Wave-16 spike-fan; Tier-1/2 GREEN)
  arm-w        RB3_NO_HEAD_REBIND=1 RB3_NO_SKIN_CLAMP=1 SHARD_GUARD_OFF=1   verdict-table pin
  arm-s        RB3_HANDS_SHELL_FIX=1                           verdict-table pin

A3 CARVE-OUT: only `bad-torn` sets the REFUTED flag RB3_HANDS_AUTHORED_REPOINT=1,
and ONLY here in the capture harness — never in a gate/verify arm.

Usage:
  scripts/native/skinning-fixture-capture.py --arm bad-torn [--bin PATH] [--mesh hands_naked,finger,glove]
  scripts/native/skinning-fixture-capture.py --all           # every arm, sequentially

Refresh discipline: re-run this script, then re-run Suite A/B
(`rb3-tests --gtest_filter='OracleValidation.*:VerdictTable.*'`); only then commit
new goldens.
"""
import argparse, os, subprocess, sys, shutil, glob, time, json

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
KBD = os.path.join(REPO, "scripts", "native", "keyboard-to-gameplay.py")
EXEC = os.path.join(REPO, "docs", "native", "engine-arch-review-2026-07-05",
                    "execution", "R2-FIXTURES")

# arm tag -> extra env applied on top of the fixed-clock capture protocol.
ARM_ENV = {
    "good-body":   {},
    "bad-ceiling": {},
    "bad-torn":    {"RB3_HANDS_AUTHORED_REPOINT": "1"},   # A3 carve-out (known-bad only)
    "arm-w":       {"RB3_NO_HEAD_REBIND": "1", "RB3_NO_SKIN_CLAMP": "1", "SHARD_GUARD_OFF": "1"},
    "arm-s":       {"RB3_HANDS_SHELL_FIX": "1"},
}
# per-arm mesh selector (good-body dumps the torso/body; the rest dump appendages).
ARM_MESH = {
    "good-body":   "polple,body,torso,shirt,pant",   # body meshes; adjust after a first dump lists names
    "bad-ceiling": "hands_naked,finger,glove",
    "bad-torn":    "hands_naked,finger,glove",
    "arm-w":       "hands_naked",
    "arm-s":       "hands_naked",
}

def run_arm(arm, binpath, mesh_override, downs, burst, interval, maxframes):
    assert arm in ARM_ENV, f"unknown arm {arm}"
    ev_dir = os.path.join(EXEC, "evidence", arm)
    dump_dir = os.path.join(ev_dir, "dumps")
    shot_dir = os.path.join(ev_dir, "shots")
    for d in (dump_dir, shot_dir):
        os.makedirs(d, exist_ok=True)
    mesh = mesh_override or ARM_MESH[arm]

    env = dict(os.environ)
    env.update(ARM_ENV[arm])
    env.update({
        "RB3_FIXED_CLOCK": "1",
        "RB3_PALETTE_DUMP": mesh,
        "RB3_PALETTE_DUMP_DIR": dump_dir,
        "RB3_PALETTE_DUMP_EVERY": "40",
        "RB3_PALETTE_DUMP_MAXFRAMES": str(maxframes),
        "RB3_PALETTE_DUMP_TAG": arm,
        "MILO_SCREENSHOT_DIR": shot_dir,
    })
    cmd = [sys.executable, KBD, "--bin", binpath,
           "--song-downs", str(downs), "--game-burst", str(burst),
           "--burst-interval", str(interval), "--out", shot_dir, "--verbose"]
    print(f"[fixture-capture] arm={arm} mesh='{mesh}' env+={ARM_ENV[arm]}")
    print(f"[fixture-capture]   {' '.join(cmd)}")
    t0 = time.time()
    rc = subprocess.call(cmd, env=env)
    dumps = sorted(glob.glob(os.path.join(dump_dir, "palette_*.txt")))
    print(f"[fixture-capture] arm={arm} rc={rc} dumps={len(dumps)} ({time.time()-t0:.1f}s)")
    # record engine provenance with the capture (WAVE16_REVIEW A7 discipline)
    try:
        eng = os.path.join(REPO, "..", "milo-native-engine")
        head = subprocess.check_output(["git", "-C", eng, "rev-parse", "HEAD"]).decode().strip()
        stat = subprocess.check_output(["git", "-C", eng, "status", "--short"]).decode()
        with open(os.path.join(ev_dir, "provenance.txt"), "w") as f:
            f.write(f"arm={arm}\nengine_head={head}\nengine_status:\n{stat}\n"
                    f"arm_env={ARM_ENV[arm]}\nmesh={mesh}\n"
                    f"palette_dump_env=RB3_PALETTE_DUMP={mesh} RB3_PALETTE_DUMP_DIR={dump_dir} "
                    f"RB3_PALETTE_DUMP_EVERY=40 RB3_PALETTE_DUMP_MAXFRAMES={maxframes} RB3_PALETTE_DUMP_TAG={arm}\n"
                    f"cmd={' '.join(cmd)}\nrc={rc}\ndumps={len(dumps)}\n")
    except Exception as e:
        print(f"[fixture-capture] provenance record failed: {e}")
    return rc, dumps

def curate(arm, dumps, maxkeep):
    """Copy a curated subset of the dumps into the committed goldens dir."""
    if not dumps:
        print(f"[fixture-capture] arm={arm}: no dumps, nothing to curate")
        return
    gdir = os.path.join(REPO, "native", "tests", "goldens", "r2-skinning", arm)
    os.makedirs(gdir, exist_ok=True)
    keep = dumps[:maxkeep]
    for d in keep:
        shutil.copy(d, gdir)
    with open(os.path.join(gdir, "MANIFEST.txt"), "w") as f:
        f.write(f"arm={arm}\nfixtures={len(keep)}\nsource_dumps={len(dumps)}\n")
        for d in keep:
            f.write(os.path.basename(d) + "\n")
    print(f"[fixture-capture] arm={arm}: curated {len(keep)} goldens -> {gdir}")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--arm", choices=list(ARM_ENV))
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--mesh", default=None, help="override RB3_PALETTE_DUMP mesh selector")
    ap.add_argument("--song-downs", type=int, default=3)
    ap.add_argument("--game-burst", type=int, default=15)
    ap.add_argument("--burst-interval", type=float, default=0.3)
    ap.add_argument("--maxframes", type=int, default=6)
    ap.add_argument("--curate", type=int, default=4, help="goldens to keep per arm (0=capture only)")
    args = ap.parse_args()

    arms = list(ARM_ENV) if args.all else ([args.arm] if args.arm else [])
    if not arms:
        ap.error("pass --arm <tag> or --all")
    if not os.path.exists(args.bin):
        ap.error(f"binary not found: {args.bin} (build rb3-native with the RB3_PALETTE_DUMP probe first)")

    any_dumps = False
    for arm in arms:
        rc, dumps = run_arm(arm, args.bin, args.mesh, args.song_downs,
                            args.game_burst, args.burst_interval, args.maxframes)
        if dumps:
            any_dumps = True
            if args.curate > 0:
                curate(arm, dumps, args.curate)
    sys.exit(0 if any_dumps else 1)

if __name__ == "__main__":
    main()
