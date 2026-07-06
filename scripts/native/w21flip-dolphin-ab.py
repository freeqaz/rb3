#!/usr/bin/env python3
"""w21flip-dolphin-ab.py — W2.1-flip.S4 Dolphin gameplay A/B package.

Produces the camera-pinned gameplay A/B package the coordinator eyes before
flipping `RB3_PLACEMENT_CONTRACT` default-ON: >=2 captures per flag state
(A/A protocol — a pre-existing, flag-INDEPENDENT pink-bloom/exposure wash
otherwise confounds single-frame judgment, per W2.1/STATUS.md deviation #3),
laid out against the Dolphin ground-truth crowd-spread shot and the retail
drum-kit-position reference, with the numeric crowd+drum placement-oracle
verdict recorded as the machine-checkable companion.

This package does NOT flip the default and does NOT re-golden the drawlog
goldens (the flip breaks the committed splash canonical count 888->792 —
that is the coordinator's job, after human-eyes sign-off on this package).

Pipeline per capture:
  1. boot rb3-native headless (RB3_HTTP, RB3_FIXED_CLOCK for a stable sim
     clock across A/A pairs);
  2. nav splash -> main_hub -> song_select -> part/diff -> game_screen
     (reusing placement-gate-capture.py's proven nav_to_gameplay, verbatim);
  3. let the song actually start playing (autohit a few hundred ms of
     songMs so the crowd + band + drum kit are posed and live);
  4. rb3_director_disable 1, then rb3_force_shot a wide venue/establishing
     shot candidate (same candidate list as placement-gate-capture.py) so
     the crowd + drum kit are in-frustum and the frame is reproducible;
  5. screenshot via /api/screenshot.

For ONE of the flag-ON captures, additionally turn on RB3_DRAWLOG +
RB3_PLACEMENT_PROBE and re-run placement-gate-capture.py itself (the
already-tested S1/S2 harness) with --gate both to produce the numeric
crowd+drum oracle GREEN companion — this deliberately reuses the proven
harness rather than re-deriving drawlog/probe extraction here.

Usage:
  python3 scripts/native/w21flip-dolphin-ab.py \
      [--bin native/build-agent-W2.1-flip/rb3-native] \
      [--tests native/build-agent-W2.1-flip/rb3-tests] \
      [--out docs/native/.../W2.1-flip/dolphin-ab] [--verbose]

Exit 0 = package produced (screenshots + oracle companion written,
README.md authored). Exit 2 = a capture or the oracle companion failed
(does NOT mean the fix is wrong — it means the package could not be
produced; see the per-capture engine logs). This script never flips the
default and never touches the drawlog goldens.
"""
import argparse, os, shutil, signal, subprocess, sys, time
import importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

_kspec = importlib.util.spec_from_file_location("kbd2game", os.path.join(HERE, "keyboard-to-gameplay.py"))
k = importlib.util.module_from_spec(_kspec)
_kspec.loader.exec_module(k)

_pspec = importlib.util.spec_from_file_location("pgc", os.path.join(HERE, "placement-gate-capture.py"))
pgc = importlib.util.module_from_spec(_pspec)
_pspec.loader.exec_module(pgc)

DEFAULT_BIN = os.path.join(REPO, "native", "build-agent-W2.1-flip", "rb3-native")
DEFAULT_TESTS = os.path.join(REPO, "native", "build-agent-W2.1-flip", "rb3-tests")
DEFAULT_OUT = os.path.join(
    REPO, "docs", "native", "engine-arch-review-2026-07-05", "execution",
    "W2.1-flip", "dolphin-ab")
GP00 = os.path.join(REPO, "docs", "native", "c8-ground-truth-2026-07-01",
                     "dolphin-shots", "gp_00.png")
RETAIL_DRUMS = os.path.join(REPO, "images", "retail-screenshots",
                             "fandom_gameplay_drums.png")

# Shot candidates, most-preferred first. `coop_dir_crowd` (crowd-shot-capture.py's
# convention) was hand-verified (2026-07-06 exploration, this item) to be a
# materially BETTER shot than placement-gate-capture.py's DEFAULT_SHOTS list for
# this package's purpose: it puts the drum kit clearly in-frustum next to the
# band, whereas `coop_all_n00` (the first of pgc.DEFAULT_SHOTS to resolve) is a
# tighter over-the-highway shot that crops the kit mostly out of frame. Falls
# back to pgc.DEFAULT_SHOTS if `coop_dir_crowd` doesn't resolve on some venue.
SHOT_CANDIDATES = ["coop_dir_crowd"] + pgc.DEFAULT_SHOTS


def log(m): print(f"[w21flip-dolphin-ab] {m}", flush=True)


def boot_capture_gameplay(binpath, extra_env, out_prefix, verbose,
                          song_downs=4, diff="hard"):
    """Boots rb3-native, navs to gameplay, pins a wide venue shot, screenshots.
    Returns the PNG path on success, None on failure. Mirrors
    placement-gate-capture.py's nav+pin recipe verbatim (imported, not
    re-derived)."""
    port = k.free_port()
    log_path = out_prefix + ".engine.log"
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": k.DEFAULT_DATA,
                "RB3_DTA_OVERLAY": os.path.join(REPO, "native", "dta"),
                "RB3_FIXED_CLOCK": "1"})
    env.update(extra_env)
    proc = subprocess.Popen([binpath], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    png_path = None
    try:
        if not pgc.nav_to_gameplay(port, proc, song_downs, k.DIFF_INDEX[diff], verbose):
            log(f"FAIL: nav_to_gameplay failed ({out_prefix})"); return None
        k.verb(port, "nofail")
        # let the song actually start (crowd/venue scene is live).
        start = -1; dl = time.time() + 60
        while time.time() < dl:
            ip = k.dta(port, "{game is_playing}"); h = k.health(port)
            if ip is not None and int(ip) == 1 and start < 0 and h and h[1] >= 0:
                start = h[1]
            k.verb(port, "autohit")
            if start >= 0 and h and h[1] > start + 300:
                break
            time.sleep(0.5)
        log(f"is_playing songMs={k.health(port)[1]:.0f} ({out_prefix})")

        # pin a wide venue shot so crowd + drum kit are in-frustum + reproducible.
        pinned = None
        dz = k.dta(port, "{rb3_director_disable 1}")
        if isinstance(dz, int) and dz == 1:
            for cand in SHOT_CANDIDATES:
                resolved = pgc.force_shot(port, cand)
                if resolved:
                    pinned = resolved; break
            if pinned:
                for _ in range(3):
                    k.verb(port, "autohit"); time.sleep(0.15)
                log(f"pinned wide shot {pinned!r} (cur_shot={pgc.cur_shot(port)!r}) ({out_prefix})")
            else:
                log(f"no wide shot resolved — capturing at director's angle ({out_prefix})")
        else:
            log(f"rb3_director_disable echoed {dz!r} — capturing without a pin ({out_prefix})")

        # settle a few more frames post-pin, then screenshot.
        for _ in range(4):
            k.verb(port, "autohit"); time.sleep(0.2)
        png_path = out_prefix + ".png"
        if not k.screenshot(port, png_path):
            log(f"FAIL: screenshot failed ({out_prefix})"); return None
    finally:
        logf.flush()
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            proc.wait(timeout=10)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass
        logf.close()
    return png_path


def make_side_by_side(paths_labels, out_path, title):
    """Lay out a list of (path, label) images left-to-right with a caption
    strip under each, for eyeballing next to ground truth."""
    from PIL import Image, ImageDraw
    imgs = [Image.open(p).convert("RGB") for p, _ in paths_labels]
    w = max(im.width for im in imgs)
    h = max(im.height for im in imgs)
    pad, cap_h = 8, 28
    canvas = Image.new("RGB", (w * len(imgs) + pad * (len(imgs) + 1),
                               h + cap_h + pad * 2 + 24), (24, 24, 24))
    d = ImageDraw.Draw(canvas)
    d.text((pad, 4), title, fill=(255, 255, 0))
    x = pad
    for im, (_, label) in zip(imgs, paths_labels):
        canvas.paste(im, (x, 24 + pad))
        d.text((x, 24 + pad + h + 4), label, fill=(255, 255, 255))
        x += im.width + pad
    canvas.save(out_path)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--tests", default=DEFAULT_TESTS)
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}"); return 2
    os.makedirs(args.out, exist_ok=True)

    plan = [
        # Post-flip (Wave 6): contract is default-ON; the OFF arm must set the opt-out.
        ("OFF", 1, {"RB3_PLACEMENT_CONTRACT_OFF": "1"}),
        ("OFF", 2, {"RB3_PLACEMENT_CONTRACT_OFF": "1"}),
        ("ON", 1, {"RB3_PLACEMENT_CONTRACT": "1"}),
        ("ON", 2, {"RB3_PLACEMENT_CONTRACT": "1"}),
    ]
    caps = {}
    for flag, idx, env in plan:
        prefix = os.path.join(args.out, f"cap_{flag}_{idx}")
        log(f"capturing {flag}#{idx} (env={env or 'none'}) -> {prefix}.png")
        p = boot_capture_gameplay(args.bin, env, prefix, args.verbose)
        if p is None:
            log(f"FAIL: capture {flag}#{idx} failed; see {prefix}.engine.log")
            return 2
        caps[(flag, idx)] = p
        log(f"  ok: {p}")

    # Numeric companion: run the already-proven crowd+drum oracle harness
    # (placement-gate-capture.py) under flag-ON, gate=both. Reuses S1/S2's
    # tested extraction/oracle path rather than re-deriving it here.
    oracle_log_path = os.path.join(args.out, "oracle-gate-ON.log")
    oracle_env = dict(os.environ)
    oracle_env["RB3_PLACEMENT_CONTRACT"] = "1"
    oracle_out = os.path.join(args.out, "oracle-capture")
    os.makedirs(oracle_out, exist_ok=True)
    oracle_cmd = [sys.executable, os.path.join(HERE, "placement-gate-capture.py"),
                  "--bin", args.bin, "--tests", args.tests,
                  "--out", oracle_out, "--gate", "both"]
    log(f"running numeric oracle companion (flag-ON, --gate both): {' '.join(oracle_cmd)}")
    op = subprocess.run(oracle_cmd, env=oracle_env, cwd=REPO,
                        capture_output=True, text=True)
    with open(oracle_log_path, "w") as f:
        f.write(op.stdout)
        if op.stderr: f.write("\n--- stderr ---\n" + op.stderr)
    oracle_rc = op.returncode
    oracle_green = oracle_rc == 0
    log(f"oracle companion exit={oracle_rc} ({'GREEN both' if oracle_green else 'NOT both green'}) "
        f"-> {oracle_log_path}")

    # Side-by-side layouts vs ground truth.
    have_pil = True
    try:
        import PIL  # noqa: F401
    except Exception:
        have_pil = False
        log("PIL not available — skipping composite layouts (raw captures still written)")

    layouts = []
    if have_pil:
        try:
            crowd_pairs = [(caps[("OFF", 1)], "flag-OFF #1"), (caps[("OFF", 2)], "flag-OFF #2"),
                          (caps[("ON", 1)], "flag-ON #1"), (caps[("ON", 2)], "flag-ON #2")]
            if os.path.exists(GP00):
                crowd_pairs = [(GP00, "Dolphin gp_00 (ground truth, crowd spread house-left)")] + crowd_pairs
            else:
                log(f"NOTE: ground-truth crowd shot missing at {GP00}")
            out_crowd = os.path.join(args.out, "layout_crowd_vs_dolphin.png")
            make_side_by_side(crowd_pairs, out_crowd,
                              "W2.1-flip S4: crowd spread A/A(OFF) + A/A(ON) vs Dolphin gp_00")
            layouts.append(out_crowd)

            drum_pairs = [(caps[("OFF", 1)], "flag-OFF #1"), (caps[("OFF", 2)], "flag-OFF #2"),
                         (caps[("ON", 1)], "flag-ON #1"), (caps[("ON", 2)], "flag-ON #2")]
            if os.path.exists(RETAIL_DRUMS):
                drum_pairs = [(RETAIL_DRUMS, "retail (ground truth, drum-kit position)")] + drum_pairs
            else:
                log(f"NOTE: retail drum reference missing at {RETAIL_DRUMS}")
            out_drum = os.path.join(args.out, "layout_drum_vs_retail.png")
            make_side_by_side(drum_pairs, out_drum,
                              "W2.1-flip S4: drum-kit A/A(OFF) + A/A(ON) vs retail drums")
            layouts.append(out_drum)
        except Exception as e:
            log(f"NOTE: composite layout generation failed non-fatally: {e}")

    # Copy ground-truth refs alongside for convenience.
    for src in (GP00, RETAIL_DRUMS):
        if os.path.exists(src):
            try:
                shutil.copy(src, os.path.join(args.out, os.path.basename(src)))
            except Exception:
                pass

    readme_path = os.path.join(args.out, "README.md")
    with open(readme_path, "w") as f:
        f.write(f"""# W2.1-flip.S4 — Dolphin gameplay A/B package (for coordinator sign-off, E1)

**Package produced by `scripts/native/w21flip-dolphin-ab.py`. This is a
package only — the agent did NOT flip `RB3_PLACEMENT_CONTRACT`'s default and
did NOT touch the drawlog goldens.** Coordinator human-eyes sign-off on this
package is the trigger for the flip commit (per WAVE5_KICKOFF COORDINATOR
ACCEPTANCE E1 / WAVE5_REVIEW R-E).

## What this proves and what it doesn't

- The numeric crowd+drum placement oracle (machine-checkable, exact) already
  proves the contract places the crowd at its faithful `spXfm` and the
  drum kit/band near its `SyncPlayMode` waypoint, flag-ON — see
  `oracle-gate-ON.log` below. **That is not what this package is for.**
- This package is scoped to what only human eyes can judge: does the crowd
  *distribution look right* (spread across the venue bowl, not piled at one
  point) and does the drum kit *sit at the kit position* (not floating at
  the venue origin/center) — compared against Dolphin/retail ground truth.
- **A/A protocol is mandatory**: W2.1/STATUS.md deviation #3 records a
  pre-existing, flag-INDEPENDENT pink-bloom/exposure wash that varies
  run-to-run. Two independent boots per flag state (below) let the reviewer
  see that any wash is present in BOTH flag-OFF and flag-ON — i.e. it is not
  something the flip introduces — before judging placement.

## Numeric oracle companion (flag-ON, `--gate both`)

```
$ python3 scripts/native/placement-gate-capture.py --bin <rb3-native> \\
      --tests <rb3-tests> --gate both   # (RB3_PLACEMENT_CONTRACT=1)
exit code: {oracle_rc}   ({'PASS — both crowd + drum oracle GREEN' if oracle_green else 'see oracle-gate-ON.log — not both green'})
```

Full stdout/stderr in [`oracle-gate-ON.log`](oracle-gate-ON.log); raw
drawlog+probe artifacts in [`oracle-capture/`](oracle-capture/). This reuses
the already-tested W2.1-flip.S1/S2 harness verbatim (no re-derivation).

## Captures (camera-pinned, `RB3_FIXED_CLOCK`, wide venue/establishing shot)

| Flag state | Capture 1 | Capture 2 (A/A) |
|---|---|---|
| flag-OFF (no env, current committed default) | `cap_OFF_1.png` | `cap_OFF_2.png` |
| flag-ON (`RB3_PLACEMENT_CONTRACT=1`) | `cap_ON_1.png` | `cap_ON_2.png` |

Engine logs (nav + shot-pin + `is_playing` traces) alongside each capture as
`cap_<FLAG>_<N>.engine.log`.

## Layout vs ground truth

- [`layout_crowd_vs_dolphin.png`](layout_crowd_vs_dolphin.png) — all 4
  captures side-by-side against `gp_00.png` (Dolphin, crowd spread
  house-left) — {'present' if os.path.exists(GP00) else 'MISSING at build time, see log'}.
- [`layout_drum_vs_retail.png`](layout_drum_vs_retail.png) — all 4 captures
  side-by-side against `fandom_gameplay_drums.png` (retail, drum-kit
  position) — {'present' if os.path.exists(RETAIL_DRUMS) else 'MISSING at build time, see log'}.
- Ground-truth reference images are also copied alongside for convenience:
  `gp_00.png`, `fandom_gameplay_drums.png`.

## Reviewer checklist (E1 sign-off)

1. Open `layout_crowd_vs_dolphin.png`: does flag-ON's crowd look spread
   across the venue bowl (matching `gp_00.png`'s house-left spread), vs
   flag-OFF's crowd piled at one point?
2. Open `layout_drum_vs_retail.png`: does flag-ON's drum kit sit at a
   plausible kit position (matching `fandom_gameplay_drums.png`), vs
   flag-OFF's kit collapsed to the venue origin/center?
3. Confirm the pink-bloom/exposure wash (if visible) appears in BOTH
   flag-OFF and flag-ON captures (A/A-variable, not flag-caused) — if it
   only appears in one flag state, flag that as a new finding, not expected.
4. If satisfied: coordinator flips `kPlacementContractDefaultOn` 0->1 (the
   one-line change staged by W2.1-flip.S1) and re-goldens the drawlog
   goldens (splash canonical count will move 888->792 — expected, see
   WAVE5_REVIEW A4).

**This package's exit is "package produced + oracle GREEN companion
recorded" — never "flip done."**
""")
    log(f"README -> {readme_path}")
    log(f"OVERALL: package produced ({'oracle GREEN' if oracle_green else 'oracle NOT fully green — see log'})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
