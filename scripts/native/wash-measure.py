#!/usr/bin/env python3
"""wash-measure.py — W2.1-flip-blocker S2 measurement (Opus measurer).

Executes the S2 protocol in W2.1-flip-blocker/PLAN.md §2: interleaved-sequential
OFF/ON gameplay boots, each capture PINNED to a songMs window (default 21000±250)
so lighting-animation drift cannot masquerade as a flag-caused "wash". Each PNG is
scored with `wash_score.score_image()`; after every OFF/ON pair the pre-declared
decision rule (`wash_score.compare()`) is evaluated with EARLY-STOP:

  * A/A-variable   — a wash-class capture (PINK|WHITE) appears in ANY flag-OFF boot
                     (existence proof — decided immediately).
  * flag-ON-specific — no flag-OFF wash AND all wash mass flag-ON AND Mann-Whitney
                     two-sided p<alpha on the luma distributions.
  * inconclusive   — neither by the N=16/state cap (defaults to do-not-flip).

Nav + shot-pin reuse the proven placement-gate-capture.py / w21flip-dolphin-ab.py
recipe VERBATIM (imported, not re-derived). The ONE change vs the Wave-5 package
harness is the songMs pin: instead of wall-clock settle sleeps at an unrecorded
songMs, the director is disabled + the wide venue shot pinned ONCE early, then the
screenshot fires the instant songMs enters the window — and boots that overshoot
the window before the shot fires are DISCARDED and re-run (an off-window frame is
exactly the confound this stage removes).

Raw PNGs are written under --raws (default /tmp/wave6-flipblocker-captures) — they
are NOT committed. The committed artifacts are the scores/verdict JSON + a montage.

Usage:
  python3 scripts/native/wash-measure.py \
      --bin native/build-agent-W2.1-flip-blocker/rb3-native \
      --raws /tmp/wave6-flipblocker-captures \
      --out docs/native/engine-arch-review-2026-07-05/execution/W2.1-flip-blocker/measure \
      [--songms 21000] [--tol 250] [--cap 16] [--probe]

Exit 0 = a verdict was reached (or the cap hit -> inconclusive). Exit 2 = harness
failure (could not produce enough valid captures to decide).
"""
import argparse
import importlib.util
import json
import os
import signal
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))


def _load(mod, fname):
    spec = importlib.util.spec_from_file_location(mod, os.path.join(HERE, fname))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


k = _load("kbd2game", "keyboard-to-gameplay.py")
pgc = _load("pgc", "placement-gate-capture.py")
ws = _load("wash_score", "wash_score.py")

# Same shot candidates as the Wave-5 dolphin-ab package (coop_dir_crowd wide venue
# establishing shot preferred; falls back to placement-gate-capture DEFAULT_SHOTS).
SHOT_CANDIDATES = ["coop_dir_crowd"] + pgc.DEFAULT_SHOTS

DEFAULT_BIN = os.path.join(REPO, "native", "build-agent-W2.1-flip-blocker", "rb3-native")
DEFAULT_RAWS = "/tmp/wave6-flipblocker-captures"
DEFAULT_OUT = os.path.join(
    REPO, "docs", "native", "engine-arch-review-2026-07-05", "execution",
    "W2.1-flip-blocker", "measure")


def log(m):
    print(f"[wash-measure] {m}", flush=True)


def _boot(binpath, extra_env, log_path):
    port = k.free_port()
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": k.DEFAULT_DATA,
                "RB3_DTA_OVERLAY": os.path.join(REPO, "native", "dta"),
                "RB3_FIXED_CLOCK": "1"})
    env.update(extra_env)
    proc = subprocess.Popen([binpath], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    return port, proc, logf


def _kill(proc, logf):
    logf.flush()
    try:
        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        proc.wait(timeout=10)
    except Exception:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception:
            pass
    logf.close()


def capture_pinned(binpath, extra_env, out_prefix, songms_lo, songms_hi,
                   overshoot_max, song_downs=4, diff="hard", verbose=False,
                   probe=False):
    """Boot -> nav -> pin wide shot early -> screenshot the instant songMs is in
    [songms_lo, songms_hi]. Returns (png_path, actual_songMs) on success, or
    (None, reason) on failure/overshoot (reason is a short string)."""
    log_path = out_prefix + ".engine.log"
    port, proc, logf = _boot(binpath, extra_env, log_path)
    try:
        if not pgc.nav_to_gameplay(port, proc, song_downs, k.DIFF_INDEX[diff], verbose):
            return None, "nav_failed"
        k.verb(port, "nofail")

        # Wait for the song to actually start playing.
        start = -1
        dl = time.time() + 60
        while time.time() < dl:
            ip = k.dta(port, "{game is_playing}")
            h = k.health(port)
            if ip is not None and int(ip) == 1 and h and h[1] >= 0:
                start = h[1]
                break
            k.verb(port, "autohit")
            time.sleep(0.2)
        if start < 0:
            return None, "song_never_played"

        # Pin the wide venue shot ONCE, early (well before the window), so the
        # heavy director-disable/force-shot/settle work does not itself push
        # songMs out of the window at screenshot time.
        pinned = None
        dz = k.dta(port, "{rb3_director_disable 1}")
        if isinstance(dz, int) and dz == 1:
            for cand in SHOT_CANDIDATES:
                resolved = pgc.force_shot(port, cand)
                if resolved:
                    pinned = resolved
                    break
        if pinned:
            for _ in range(3):
                k.verb(port, "autohit")
                time.sleep(0.05)
            log(f"pinned {pinned!r} (cur={pgc.cur_shot(port)!r}) start_songMs={start:.0f} ({os.path.basename(out_prefix)})")
        else:
            log(f"no wide shot resolved — director angle (start_songMs={start:.0f}) ({os.path.basename(out_prefix)})")

        # Now race to the songMs window. Poll fast (no fixed sleep) and autohit
        # to keep the song alive + frames advancing. Screenshot the moment
        # songMs is in-window. Discard if we blow past overshoot_max first.
        rate_samples = []
        last_ms = start
        last_t = time.time()
        dl = time.time() + 120
        while time.time() < dl:
            h = k.health(port)
            if not h:
                time.sleep(0.02)
                continue
            ms = h[1]
            now = time.time()
            if ms > last_ms and now > last_t:
                rate_samples.append((ms - last_ms) / (now - last_t))
            last_ms, last_t = ms, now

            if probe and len(rate_samples) >= 30:
                # probe mode: just report the songMs advance rate + step and bail
                import statistics
                med_rate = statistics.median(rate_samples)
                steps = [b - a for a, b in zip(
                    [start] + [], [])]  # placeholder
                return None, f"probe rate~{med_rate:.0f}ms/s samples={len(rate_samples)} cur_ms={ms:.0f}"

            if songms_lo <= ms <= songms_hi:
                png_path = out_prefix + ".png"
                if not k.screenshot(port, png_path):
                    return None, "screenshot_failed"
                log(f"CAPTURED at songMs={ms:.0f} -> {os.path.basename(png_path)}")
                return png_path, ms
            if ms > overshoot_max:
                return None, f"overshoot_songMs={ms:.0f}"
            k.verb(port, "autohit")
            time.sleep(0.01)
        return None, "window_timeout"
    finally:
        _kill(proc, logf)


def make_montage(entries, refs, out_path, title):
    """entries: list of (png_path, label). refs: list of (path,label) prepended."""
    from PIL import Image, ImageDraw
    items = [(p, l) for p, l in refs if p and os.path.exists(p)] + \
            [(p, l) for p, l in entries if p and os.path.exists(p)]
    if not items:
        return None
    imgs = [Image.open(p).convert("RGB") for p, _ in items]
    # scale each to a common height for a compact strip
    th = 320
    scaled = []
    for im in imgs:
        w = max(1, int(im.width * th / im.height))
        scaled.append(im.resize((w, th)))
    pad, cap_h, top = 6, 40, 26
    total_w = sum(im.width for im in scaled) + pad * (len(scaled) + 1)
    canvas = Image.new("RGB", (total_w, th + cap_h + top + pad), (20, 20, 20))
    d = ImageDraw.Draw(canvas)
    d.text((pad, 6), title, fill=(255, 255, 0))
    x = pad
    for im, (_, label) in zip(scaled, items):
        canvas.paste(im, (x, top))
        for i, line in enumerate(label.split("\n")):
            d.text((x, top + th + 2 + i * 12), line, fill=(230, 230, 230))
        x += im.width + pad
    canvas.save(out_path)
    return out_path


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--raws", default=DEFAULT_RAWS)
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--songms", type=float, default=21000.0)
    ap.add_argument("--tol", type=float, default=250.0)
    ap.add_argument("--overshoot-max", type=float, default=None,
                    help="discard a boot once songMs passes this without a capture "
                         "(default songms+tol)")
    ap.add_argument("--cap", type=int, default=16, help="max valid captures per state")
    ap.add_argument("--min-pairs", type=int, default=1,
                    help="keep sampling to at least this many pairs even after the "
                         "A/A-variable existence proof fires (strengthens the distribution "
                         "+ S3/S4 dataset; the verdict is monotone — more data cannot flip "
                         "A/A-variable to flag-ON-specific, which requires zero flag-OFF wash)")
    ap.add_argument("--max-retries", type=int, default=6,
                    help="max discarded (overshoot/nav) boots per needed capture")
    ap.add_argument("--alpha", type=float, default=0.05)
    ap.add_argument("--probe", action="store_true",
                    help="one OFF boot: report songMs advance rate and exit")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}")
        return 2
    os.makedirs(args.raws, exist_ok=True)
    os.makedirs(args.out, exist_ok=True)
    lo, hi = args.songms - args.tol, args.songms + args.tol
    omax = args.overshoot_max if args.overshoot_max is not None else hi

    if args.probe:
        pre = os.path.join(args.raws, "probe_OFF")
        _, reason = capture_pinned(args.bin, {}, pre, lo, hi, omax,
                                   verbose=args.verbose, probe=True)
        log(f"PROBE: {reason}")
        return 0

    STATES = [("OFF", {}), ("ON", {"RB3_PLACEMENT_CONTRACT": "1"})]
    scores = {"OFF": [], "ON": []}
    batch_log = []
    verdict = None

    def env_for(state):
        return dict(STATES[0][1] if state == "OFF" else STATES[1][1])

    pair = 0
    while pair < args.cap and verdict is None:
        pair += 1
        for state in ("OFF", "ON"):
            got = None
            for attempt in range(1, args.max_retries + 1):
                idx = len(scores[state]) + 1
                prefix = os.path.join(args.raws, f"{state}_{idx:02d}_try{attempt}")
                log(f"pair {pair} {state}#{idx} attempt {attempt} -> {os.path.basename(prefix)}")
                png, info = capture_pinned(args.bin, env_for(state), prefix, lo, hi, omax,
                                           verbose=args.verbose)
                if png is not None:
                    m = ws.score_image(png)
                    m["flag"] = state
                    m["songMs"] = float(info)
                    m["attempt"] = attempt
                    # move raw to a stable per-capture name incl. songMs
                    final = os.path.join(args.raws, f"{state}_{idx:02d}_{int(info)}.png")
                    try:
                        os.replace(png, final)
                        m["path"] = final
                    except Exception:
                        pass
                    scores[state].append(m)
                    batch_log.append(m)
                    got = m
                    log(f"  scored: class={m['wash_class']} mean={m['mean_luma']:.3f} "
                        f"hi={m['hi_frac']:.1f} lo={m['lo_frac']:.1f} pink={m['pink_frac']:.1f} "
                        f"songMs={m['songMs']:.0f}")
                    break
                else:
                    log(f"  discarded ({info})")
            if got is None:
                log(f"FAIL: could not get a valid {state} capture after {args.max_retries} tries")
                # persist what we have before bailing
                break

        # evaluate decision rule after each completed pair
        if scores["OFF"] and scores["ON"]:
            c = ws.compare(scores["OFF"], scores["ON"], alpha=args.alpha)
            log(f"pair {pair} interim verdict={c['verdict']} (off_wash={c['off_wash_count']} "
                f"on_wash={c['on_wash_count']} p={c['mannwhitney_p']})")
            if c["verdict"] == "A/A-variable":
                verdict = c  # locked; kept fresh each pair as the sample grows
                if pair >= args.min_pairs:
                    log(f"EARLY-STOP: existence proof (flag-OFF wash-class capture) "
                        f"at pair {pair} >= min_pairs {args.min_pairs}")
                    break
                log(f"existence proof at pair {pair}; continuing to min_pairs "
                    f"{args.min_pairs} for a robust distribution")

    # final verdict at stop / cap
    if verdict is None and scores["OFF"] and scores["ON"]:
        verdict = ws.compare(scores["OFF"], scores["ON"], alpha=args.alpha)

    # write artifacts
    with open(os.path.join(args.out, "batch_log.json"), "w") as f:
        json.dump(batch_log, f, indent=2)
    if verdict is not None:
        with open(os.path.join(args.out, "verdict.json"), "w") as f:
            json.dump(verdict, f, indent=2)
        log(f"VERDICT: {verdict['verdict']} — {verdict['reason']}")
        log(f"  Mann-Whitney U={verdict['mannwhitney_U']} p={verdict['mannwhitney_p']} "
            f"(n_off={verdict['n_off']} n_on={verdict['n_on']})")

    # montage: representative captures from each state (up to 3 each)
    try:
        def rep(state):
            arr = scores[state]
            # prefer to show at least one wash-class if present
            washed = [m for m in arr if m["is_wash"]]
            neutral = [m for m in arr if not m["is_wash"]]
            picked = (washed[:3] + neutral[:2])[:4] if arr else []
            return [(m["path"], f"{state}#{arr.index(m)+1}\n{m['wash_class']} L{m['mean_luma']:.2f}\n@{int(m['songMs'])}ms")
                    for m in picked]
        entries = rep("OFF") + rep("ON")
        out_m = os.path.join(args.out, "montage.png")
        vlabel = verdict['verdict'] if verdict else "n/a"
        make_montage(entries, [], out_m,
                     f"W2.1-flip-blocker S2 — songMs-pinned {int(lo)}-{int(hi)} — VERDICT: {vlabel}")
        log(f"montage -> {out_m}")
    except Exception as e:
        log(f"NOTE: montage failed non-fatally: {e}")

    if verdict is None:
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
