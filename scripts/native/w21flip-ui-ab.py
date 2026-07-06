#!/usr/bin/env python3
"""w21flip-ui-ab.py — W2.1-flip.S3 UI-placement regression gate.

Proves `RB3_PLACEMENT_CONTRACT` flag-ON does NOT regress the UI-placement
family: the name-scoped hacks (`RB3_NO_HUB_BAR_PLACEMENT_FIX`,
`RB3_SCROLLBAR_THUMB_FIX_OFF`) live in `rb3_render_hook.cpp` and the contract
arm at `Rnd_Wgpu_RB3.cpp:2878-2879` EXCLUDES those meshes
(`skinned && !scrollbarThumb && !hubBarPlacement`) — so flag-ON is expected to
be byte-similar to flag-OFF on both UI screens, up to ordinary boot
nondeterminism (the W0.3d eye-flake / boot-random char-preview-panel pose).

This harness captures, for BOTH default-build flag states
(flag-OFF = `RB3_PLACEMENT_CONTRACT_OFF=1` post-flip; flag-ON = `RB3_PLACEMENT_CONTRACT=1`), TWO independent
boots each (an A/A pair) of:
  - main_hub_screen  (the yellow `highlight_main`/`highlight_pattern` hub bar
    behind the focused menu item — NOT visible in song_select)
  - song_select_screen at scroll depth 0 (the red `scrollbar.mesh` thumb on
    the right-edge `scrollbar_bg.mesh` track)

then computes a REGION-cropped pixel diff (not full-frame — full-frame is
dominated by the animated char-preview panel's boot-random pose, ~10-20%
churn unrelated to UI placement; see W2.1.S3 STATUS.md) over each UI element's
bounding box, and reports:
  - A/A noise (OFF-OFF2, ON-ON2)      — the baseline churn with NO flag change
  - A/B diff  (OFF-ON, OFF2-ON2)      — the flag-flip diff
PASS iff every A/B diff sits within the A/A-noise band (a small margin) AND
stays far below the known ~11% "broken" (thumb-relocated-to-screen-center)
signature established in W2.1 S3's `SCROLLBAR_THUMB_FIX_OFF` refutation
experiment (`docs/native/.../execution/W2.1/s3-artifacts/`).

Region boxes (1280x720 capture, calibrated empirically — see STATUS.md). The
hub-bar box is deliberately tight to the bar's OWN interior (excluding its
edges) — a first pass at the bar's visual bounding box (55,195,350,260)
picked up ~16% A/A noise from the busy animated venue backdrop (streetlight/
power-line parallax) bleeding in at the corners, which would have swamped any
real signal; the tightened box below is empirically byte-stable (A/A ~0%):
  HUB_BAR   = (68, 208, 336, 247)   — interior of the yellow bar behind the
              focused hub item (text + fill, no background bleed)
  SCROLLBAR = (880, 95, 912, 175)   — visible (non-album-art-occluded) sliver
              of the scrollbar track + thumb at depth 0

Usage:
  python3 scripts/native/w21flip-ui-ab.py [--bin PATH] [--out DIR] [--verbose]

Exit 0 = PASS (no regression on either screen). Exit 1 = FAIL (A/B diff
shifted toward the broken signature). Exit 2 = ERROR (boot/nav failure).
"""
import argparse, json, os, signal, subprocess, sys, time
import importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
_spec = importlib.util.spec_from_file_location("kbd2game", os.path.join(HERE, "keyboard-to-gameplay.py"))
k = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(k)

DEFAULT_BIN = os.path.join(REPO, "native", "build-agent-W2.1-flip", "rb3-native")
DEFAULT_OUT = os.path.join(
    REPO, "docs", "native", "engine-arch-review-2026-07-05", "execution",
    "W2.1-flip", "ui-ab")

# (x0, y0, x1, y1) in the 1280x720 capture.
HUB_BAR_REGION   = (68, 208, 336, 247)
SCROLLBAR_REGION = (880, 95, 912, 175)
DIFF_THRESH = 20          # per-channel-sum abs diff to count a pixel as "changed"
# The known broken signature (thumb relocated to screen-center, W2.1 S3
# SCROLLBAR_THUMB_FIX_OFF refutation) is a large, qualitatively different
# shift, not a marginal noise bump — so a generous absolute ceiling well
# below it is a meaningful, non-flaky bound.
BROKEN_SIGNATURE_PCT = 11.0
HARD_CEILING_PCT = 6.0     # absolute ceiling on A/B region diff regardless of noise
NOISE_MARGIN_PCT = 3.0     # A/B must not exceed A/A noise by more than this many points


def log(m): print(f"[w21flip-ui-ab] {m}", flush=True)


def boot_and_capture(binpath, extra_env, out_prefix, verbose):
    """Boots rb3-native, captures main_hub + song_select(depth0) PNGs.
    Returns dict {'mainhub': path, 'songselect': path} or None on failure."""
    port = k.free_port()
    log_path = out_prefix + ".engine.log"
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": k.DEFAULT_DATA,
                "RB3_FIXED_CLOCK": "1"})
    env.update(extra_env)
    proc = subprocess.Popen([binpath], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    result = None
    try:
        if k.wait_screen(port, lambda s: True, 40, proc, verbose) is None:
            log(f"FAIL: HTTP server never came up ({out_prefix})"); return None
        if k.wait_screen(port, lambda s: s == "splash_screen", 30, proc, verbose) is None:
            log(f"FAIL: never reached splash_screen ({out_prefix})"); return None
        # splash -> main_hub (mirrors placement-gate-capture.py's proven nav).
        reached_hub = False
        for _ in range(15):
            if k.health(port)[2] == "main_hub_screen":
                reached_hub = True; break
            k.press(port, k.START); k.drain_pad(port); time.sleep(0.7)
        if not reached_hub:
            log(f"FAIL: never reached main_hub_screen ({out_prefix})"); return None
        time.sleep(2.0)  # let the hub scene + highlight bar settle
        mainhub_path = out_prefix + ".mainhub.png"
        if not k.screenshot(port, mainhub_path):
            log(f"FAIL: main_hub screenshot failed ({out_prefix})"); return None
        # main_hub -> song_select (CONFIRM on PLAY NOW -> QUICKPLAY -> song_select).
        for _ in range(10):
            if k.health(port)[2] == "song_select_screen": break
            k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.7)
        if k.wait_screen(port, "song_select_screen", 40, proc, verbose) is None:
            log(f"FAIL: never reached song_select_screen ({out_prefix})"); return None
        time.sleep(2.0)  # list populate + enter anim settle, depth 0 (no scroll)
        songselect_path = out_prefix + ".songselect.png"
        if not k.screenshot(port, songselect_path):
            log(f"FAIL: song_select screenshot failed ({out_prefix})"); return None
        result = {"mainhub": mainhub_path, "songselect": songselect_path}
    finally:
        logf.flush()
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            proc.wait(timeout=8)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass
        logf.close()
    return result


def region_diff_pct(path_a, path_b, region):
    """Percentage of pixels in `region` that differ by > DIFF_THRESH between
    the two PNGs (per-channel abs-diff sum)."""
    from PIL import Image
    import numpy as np
    x0, y0, x1, y1 = region
    a = np.array(Image.open(path_a).convert("RGB"))[y0:y1, x0:x1].astype(int)
    b = np.array(Image.open(path_b).convert("RGB"))[y0:y1, x0:x1].astype(int)
    if a.shape != b.shape:
        return None
    d = (a - b)
    diff = (abs(d[:, :, 0]) + abs(d[:, :, 1]) + abs(d[:, :, 2]))
    mask = diff > DIFF_THRESH
    return float(100.0 * mask.sum() / mask.size)


def annotate(path, region, out_path, color=(255, 0, 255)):
    from PIL import Image, ImageDraw
    im = Image.open(path).convert("RGB")
    d = ImageDraw.Draw(im)
    d.rectangle(region, outline=color, width=3)
    im.save(out_path)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}"); return 2
    os.makedirs(args.out, exist_ok=True)

    captures = {}
    plan = [
        # Post-flip (Wave 6): contract is default-ON; the OFF arm must set the opt-out.
        ("OFF", 1, {"RB3_PLACEMENT_CONTRACT_OFF": "1"}),
        ("OFF", 2, {"RB3_PLACEMENT_CONTRACT_OFF": "1"}),
        ("ON", 1, {"RB3_PLACEMENT_CONTRACT": "1"}),
        ("ON", 2, {"RB3_PLACEMENT_CONTRACT": "1"}),
    ]
    for flag, idx, env in plan:
        prefix = os.path.join(args.out, f"cap_{flag}_{idx}")
        log(f"capturing {flag} run {idx} (env={env or 'none'}) -> {prefix}.*")
        r = boot_and_capture(args.bin, env, prefix, args.verbose)
        if r is None:
            log(f"FAIL: capture {flag}#{idx} failed; engine log: {prefix}.engine.log")
            return 2
        captures[(flag, idx)] = r
        log(f"  ok: {r}")

    def pair_diff(scr, region, ka, kb):
        return region_diff_pct(captures[ka][scr], captures[kb][scr], region)

    results = {}
    for screen, region in (("mainhub", HUB_BAR_REGION), ("songselect", SCROLLBAR_REGION)):
        aa_off = pair_diff(screen, region, ("OFF", 1), ("OFF", 2))
        aa_on = pair_diff(screen, region, ("ON", 1), ("ON", 2))
        ab_1 = pair_diff(screen, region, ("OFF", 1), ("ON", 1))
        ab_2 = pair_diff(screen, region, ("OFF", 2), ("ON", 2))
        aa_noise = max(aa_off, aa_on)
        ab_diff = max(ab_1, ab_2)
        results[screen] = {
            "region": region, "aa_off": aa_off, "aa_on": aa_on,
            "ab_1": ab_1, "ab_2": ab_2, "aa_noise": aa_noise, "ab_diff": ab_diff,
        }
        log(f"{screen}: A/A(OFF)={aa_off:.2f}% A/A(ON)={aa_on:.2f}% "
            f"A/B(1)={ab_1:.2f}% A/B(2)={ab_2:.2f}%  "
            f"[noise={aa_noise:.2f}% flip={ab_diff:.2f}%]")
        # annotate one representative frame per screen with the region box.
        annotate(captures[("OFF", 1)][screen], region,
                 os.path.join(args.out, f"annotated_{screen}_OFF_1.png"))
        annotate(captures[("ON", 1)][screen], region,
                 os.path.join(args.out, f"annotated_{screen}_ON_1.png"))

    ok = True
    verdicts = {}
    for screen, r in results.items():
        within_noise_band = r["ab_diff"] <= r["aa_noise"] + NOISE_MARGIN_PCT
        below_ceiling = r["ab_diff"] <= HARD_CEILING_PCT
        far_from_broken = r["ab_diff"] < (BROKEN_SIGNATURE_PCT / 2.0)
        v = bool(within_noise_band and below_ceiling and far_from_broken)
        verdicts[screen] = v
        ok = ok and v
        log(f"{screen} verdict: {'PASS' if v else 'FAIL'} "
            f"(within_noise_band={within_noise_band} below_ceiling={below_ceiling} "
            f"far_from_broken={far_from_broken})")

    summary = {
        "regions": {"mainhub": list(HUB_BAR_REGION), "songselect": list(SCROLLBAR_REGION)},
        "thresholds": {"broken_signature_pct": BROKEN_SIGNATURE_PCT,
                       "hard_ceiling_pct": HARD_CEILING_PCT,
                       "noise_margin_pct": NOISE_MARGIN_PCT},
        "results": results,
        "verdicts": verdicts,
        "overall": "PASS" if ok else "FAIL",
    }
    with open(os.path.join(args.out, "summary.json"), "w") as f:
        json.dump(summary, f, indent=2)
    log(f"summary -> {os.path.join(args.out, 'summary.json')}")
    log(f"OVERALL: {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
