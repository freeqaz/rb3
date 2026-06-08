#!/usr/bin/env python3
"""
audio_drift.py — SAMPLE-ACCURATE playback-rate / clock-drift proof.

WHY THIS EXISTS
---------------
audio_verify.py reports a `speed_ratio` from a resample-grid search over an
onset-strength envelope. On real RB3 captures that search returns the right
ballpark (~1.000, no chipmunk) but its CONFIDENCE is low — the game's mix and
our no-clamp reference downmix differ enough that the onset envelopes only
weakly cross-correlate, so audio_verify can only make a FLAT-speed-curve
argument ("no off-1.0 peak"), not a tight numeric bound. The grid step (0.001)
also caps its resolution at ~0.1% in the best case and far coarser when the
correlation is weak.

This tool proves the rate a different, much tighter way: by measuring TIME DRIFT
across the capture. If playback runs at exactly the source rate, a feature event
that occurs T seconds into the capture lands at the SAME reference time T
(plus a constant start offset) no matter where in the capture it is. If playback
runs `r` times too fast, the matched reference time advances `r` per capture
second, so the start-vs-end offsets drift apart linearly:

    ref_time(cap_t)  =  off0 + r * cap_t
    => r = 1 + (off_late - off_early) / (cap_t_late - cap_t_early)

Crucially, the start offset (off0, an unknown menu/boot/count-in delay) and any
constant mix-vs-reference bias CANCEL in the difference. We do not need a strong
absolute correlation — we only need each of two well-separated windows to lock
to its OWN best reference offset, and we measure the SLOPE between them. The
longer the elapsed span between the two windows, the tighter the bound:

    rate precision  ~  (feature frame period) / (elapsed span)

With an onset hop of 512 @ 44100 (11.6 ms/frame) and a ~40 s span between
windows, that is 0.0116 / 40 ~ 0.03% — already < 0.1%. A 100 s span gives
~0.01%. Sub-frame parabolic interpolation of the correlation peak tightens it
further.

HOW IT FINDS EACH WINDOW'S OFFSET
---------------------------------
Reuses audio_verify's mix-robust features:
  * onset-strength envelope (spectral flux) — primary, sharp in time
  * chroma (12 pitch-class) — fallback / corroboration, more mix-robust
For each capture window we slide it across the WHOLE reference (audio_verify's
per-window-normalised NCC, `ncc_slide`) and take the peak; a parabolic fit on
the three samples around the peak gives a sub-frame offset. Two windows ->
slope -> rate.

A correct rate gives drift ~0 -> ratio 1.000 +/- bound. A chipmunk (1.088x)
gives the two offsets drifting apart by 0.088 * span seconds — unmissable.

USAGE
-----
  audio_drift.py CAPTURE.wav --song 20thcenturyboy --section gameplay
  audio_drift.py CAPTURE.wav --ref REFERENCE.wav
  audio_drift.py CAPTURE.wav --ref REF.wav --early 5 --late 40 --win 6
  audio_drift.py --selftest        # PROVE it recovers injected rates first
  audio_drift.py CAPTURE.wav --ref REF.wav --json OUT.json

Bound convention: we report `speed_ratio` and a +/- bound. |ratio-1| <= bound and
|ratio-1| < 0.001 -> rate PROVEN correct to < 0.1%.
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile
import wave

import numpy as np
from scipy import signal

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "scripts", "native"))

# Reuse audio_verify's proven feature extractors / loaders / matchers verbatim so
# this tool measures the SAME features audio_verify aligns on — no re-derivation.
import audio_verify as av  # load_audio, onset_env, chroma, ncc_slide, active_bounds

RATE_TIGHT = 0.001        # |ratio-1| below this AND within bound -> "< 0.1%" proven
PEAK_USABLE = 0.45        # min per-window NCC peak for a TRUSTWORTHY lock. Below
                          #   this the window did not lock cleanly (the game-vs-
                          #   divergent-reference case) and its slope is unreliable;
                          #   the verdict reports INCONCLUSIVE rather than a spurious
                          #   off-rate. The self-reference control locks at ~0.85-1.0.
DRIFT_BAND_FRAC = 0.15    # late window may only have drifted +/-15% of the span from
                          #   its rate=1 prediction (covers a 1.088 chipmunk's
                          #   ~8.8% drift with margin; rejects chorus-repeat jumps)
# Local per-window un-stretch grid: lets each window lock cleanly even when global
# playback is off-rate (a 1.05x window NCC's poorly against the unstretched ref).
RATE_GRID = np.round(np.concatenate([
    np.arange(0.95, 1.00, 0.01), [1.0], np.arange(1.01, 1.10 + 1e-9, 0.01)]), 4)


# ---------------------------------------------------------------------------
# Sub-frame peak: locate a window inside the reference to better than one frame.
# ---------------------------------------------------------------------------
def _locate(window_feat, ref_feat, search_lo=None, search_hi=None):
    """Slide window_feat across ref_feat; return (offset_frames_subsample, peak).

    offset_frames is where window_feat[0] best aligns inside ref_feat, refined to
    sub-frame precision by a parabolic fit through the NCC peak and its two
    neighbours (the standard 3-point vertex estimate). This removes the integer-
    frame quantisation that would otherwise floor the bound.

    search_lo/search_hi (integer ref-frame offsets) RESTRICT the argmax to a band.
    This is essential: real songs (and the synthetic test tones) repeat choruses,
    so an unconstrained global argmax can lock the LATE window onto an EARLIER
    chorus repeat and report a huge spurious negative drift. Once the early
    window pins the absolute alignment, the late window can only have drifted by
    a few seconds at any plausible rate, so we search a band around its predicted
    position and reject the repeats.

    window_feat may be 1-D (onset env) or 2-D [D,T] (chroma): for chroma we sum
    the per-dimension NCC curves so all 12 pitch classes vote on one offset."""
    w = np.asarray(window_feat, float)
    r = np.asarray(ref_feat, float)
    if w.ndim == 1:
        ncc = _ncc_curve(w, r)
    else:
        # chroma: build a combined NCC curve across the 12 dims at every offset.
        D, n = w.shape
        m = r.shape[1]
        if n < 4 or m < n:
            return 0.0, 0.0
        acc = np.zeros(m - n + 1)
        for d in range(D):
            acc += _ncc_curve(w[d], r[d], whiten=False)
        ncc = acc / D
    if len(ncc) < 1:
        return 0.0, 0.0
    # restrict the argmax to the search band (everything outside is masked out)
    if search_lo is not None or search_hi is not None:
        lo = max(0, int(search_lo) if search_lo is not None else 0)
        hi = min(len(ncc), int(search_hi) + 1 if search_hi is not None else len(ncc))
        if hi <= lo:
            lo, hi = 0, len(ncc)
        masked = np.full(len(ncc), -np.inf)
        masked[lo:hi] = ncc[lo:hi]
        k = int(np.argmax(masked))
    else:
        k = int(np.argmax(ncc))
    peak = float(ncc[k])
    # parabolic refine around the integer argmax
    if 0 < k < len(ncc) - 1:
        y0, y1, y2 = ncc[k - 1], ncc[k], ncc[k + 1]
        denom = (y0 - 2 * y1 + y2)
        delta = 0.5 * (y0 - y2) / denom if abs(denom) > 1e-12 else 0.0
        delta = float(np.clip(delta, -1.0, 1.0))
    else:
        delta = 0.0
    return float(k) + delta, float(peak)


def _ncc_curve(template, sig, whiten=True):
    """Full per-offset normalised cross-correlation curve (the vector ncc_slide
    takes the argmax of). Mirrors audio_verify.ncc_slide's math exactly."""
    template = np.asarray(template, float)
    sig = np.asarray(sig, float)
    n, m = len(template), len(sig)
    if n < 4 or m < n:
        return np.zeros(max(1, m - n + 1))
    t = template - template.mean()
    tn = float(np.sqrt(np.dot(t, t))) + 1e-12
    csum = np.concatenate([[0.0], np.cumsum(sig)])
    csum2 = np.concatenate([[0.0], np.cumsum(sig * sig)])
    nwin = m - n + 1
    wsum = csum[n:n + nwin] - csum[:nwin]
    wsum2 = csum2[n:n + nwin] - csum2[:nwin]
    wnorm = np.sqrt(np.maximum(wsum2 - wsum * wsum / n, 1e-12))
    num = signal.correlate(sig, t, mode="valid")
    return num / (tn * wnorm)


# ---------------------------------------------------------------------------
# Drift measurement
# ---------------------------------------------------------------------------
def _global_anchor(cap, ref_mono, sr):
    """Robustly find where the (trimmed) capture sits inside the reference using
    audio_verify's full-length chroma cross-correlation. Returns (off_s, mean_corr):
    capture time 0 aligns to reference time off_s. This is the ANCHOR the per-window
    drift search hangs off of: real game audio vs the no-clamp reference only
    correlates ~0.5, far too weak for a 6 s window to lock unaided, but the FULL
    overlap locks reliably (that is exactly why audio_verify aligns globally)."""
    Ccap, fps_c = av.chroma(cap, sr)
    Cref, _ = av.chroma(ref_mono, sr)
    amean, amed, n, null, lift, best_cf = av.chroma_identity(Ccap, Cref, fps_c)
    return best_cf / fps_c, amean


def measure_drift(cap_mono, ref_mono, sr, early_s=None, late_s=None,
                  win_s=6.0, feature="onset", trim=True, verbose=False,
                  global_off_s=None, band_s=4.0):
    """Return a dict with speed_ratio, bound, the two window offsets, span, peak.

    Picks an EARLY and a LATE window of the capture's loud region, locates each
    inside the reference, and takes the slope of matched-reference-time vs
    capture-time as the playback rate.

    Each window is located in a TIGHT band (+/- band_s) around its globally-
    predicted position: capture-time t (relative to the loud-region start) is
    expected at reference time global_off_s + t. This anchoring is what makes weak
    real-audio per-window peaks (~0.2 against the divergent reference mix) still
    lock to the RIGHT position instead of a spurious far-away correlation. A real
    off-rate still drifts WITHIN the band (a 1.088x chipmunk over a 30 s span
    drifts ~2.6 s; band_s defaults to 4 s, raise it for very long spans)."""
    # Trim the capture to its loud song region so the windows fall on real music
    # (boot/menu/count-in lead-in only adds noise).
    if trim:
        s0, e0 = av.active_bounds(cap_mono, sr)
        if e0 <= s0 + sr:
            s0, e0 = 0, len(cap_mono)
    else:
        s0, e0 = 0, len(cap_mono)
    cap = cap_mono[s0:e0]
    cap_dur = len(cap) / sr

    def feat(x):
        if feature == "chroma":
            C, fps = av.chroma(x, sr)
            return C, fps
        e, fps = av.onset_env(x, sr)
        return e, fps

    ref_feat, fps = feat(ref_mono)
    frame_period = 1.0 / fps

    # GLOBAL anchor: where capture-time 0 (loud-region start) sits in the reference.
    if global_off_s is None:
        global_off_s, anchor_corr = _global_anchor(cap, ref_mono, sr)
    else:
        anchor_corr = float("nan")

    win_s = float(min(win_s, max(1.0, cap_dur / 4.0)))
    # default windows: early starts ~5% in, late ends ~5% before the end, both a
    # win_s block, maximally separated.
    if early_s is None:
        early_s = max(0.0, 0.05 * cap_dur)
    if late_s is None:
        late_s = max(early_s + win_s + 1.0, cap_dur - win_s - 0.05 * cap_dur)
    early_s = float(early_s)
    late_s = float(late_s)
    if late_s + win_s > cap_dur:
        late_s = max(early_s + win_s + 0.5, cap_dur - win_s)
    span = late_s - early_s
    if span <= win_s:
        raise SystemExit(f"capture too short: span {span:.1f}s <= window {win_s:.1f}s "
                         f"(need a longer capture or smaller --win)")

    def window_feat(center_start_s):
        a = int(center_start_s * sr)
        b = int((center_start_s + win_s) * sr)
        return cap[a:b]

    def locate_window(center_start_s):
        """Locate a capture window inside the reference, in a tight band around its
        globally-predicted reference position, searching a small grid of LOCAL
        stretch ratios. Playback runs at one constant rate; if it is off by `r`,
        the window's pattern is time-scaled by `r` and a direct NCC against the
        unstretched reference correlates poorly (a 1.05x window barely locks). Un-
        stretching the window by trial ratios recovers a sharp peak; we keep the
        (offset, peak) of the best ratio. The offset is in REFERENCE frames (the
        reference is never stretched), so the slope between the two windows'
        offsets is still the true rate."""
        seg = window_feat(center_start_s)
        predicted = (global_off_s + center_start_s) * fps   # rate==1 ref frame
        band = band_s * fps
        lo, hi = predicted - band, predicted + band
        best = (predicted, -2.0)
        for s in RATE_GRID:
            ss = signal.resample(seg, int(round(len(seg) * s))) if abs(s - 1) > 1e-9 else seg
            wf, _ = feat(ss)
            off, peak = _locate(wf, ref_feat, lo, hi)
            if peak > best[1]:
                best = (off, peak)
        return best

    off_e_frames, peak_e = locate_window(early_s)
    off_l_frames, peak_l = locate_window(late_s)

    # window_feat[0] corresponds to capture time = window-start. Its matched
    # reference time is off_frames * frame_period. The drift is the change in
    # (ref_time - cap_time) across the two windows; the rate is the slope of
    # ref_time vs cap_time.
    ref_e = off_e_frames * frame_period
    ref_l = off_l_frames * frame_period
    cap_e = early_s
    cap_l = late_s
    # rate = slope of ref_time vs cap_time. span == cap_l - cap_e, so this is
    # exactly  1 + ((ref_l-ref_e) - span)/span  =  (ref_l-ref_e)/span.
    speed_ratio = (ref_l - ref_e) / span

    # +/- bound: each window offset is uncertain by a fraction of a frame after the
    # parabolic refine. A sharp peak localises to ~half a frame; a WEAK peak (the
    # game-vs-no-clamp-reference case, ~0.2) has a broader correlation lobe and
    # localises less precisely. Model per-window uncertainty as
    #   loc_unc_frames = max(0.5, 0.5/peak)   (0.5 frame at peak 1.0, ~2.5 at 0.2),
    # and propagate two independent window errors into the slope:
    #   bound = sqrt(2) * loc_unc * frame_period / span.
    peak = min(peak_e, peak_l)
    loc_unc_frames = max(0.5, 0.5 / max(peak, 0.05))    # in frames, per window
    bound = float(np.sqrt(2.0) * loc_unc_frames * frame_period / span)

    if verbose:
        print(f"  feature={feature} fps={fps:.2f} frame={frame_period*1000:.1f}ms "
              f"anchor=ref {global_off_s:.2f}s (corr {anchor_corr:.3f})")
        print(f"  early win @cap {cap_e:6.2f}s -> ref {ref_e:7.3f}s  peak {peak_e:.3f}")
        print(f"  late  win @cap {cap_l:6.2f}s -> ref {ref_l:7.3f}s  peak {peak_l:.3f}")
        print(f"  span={span:.2f}s  drift={(ref_l-ref_e)-(cap_l-cap_e):+.4f}s")

    return dict(
        feature=feature, fps=round(fps, 3), frame_ms=round(frame_period * 1000, 3),
        win_s=round(win_s, 2), band_s=round(band_s, 2),
        anchor_ref_s=round(global_off_s, 3),
        anchor_corr=None if anchor_corr != anchor_corr else round(anchor_corr, 3),
        early_cap_s=round(cap_e, 3), late_cap_s=round(cap_l, 3), span_s=round(span, 3),
        early_ref_s=round(ref_e, 4), late_ref_s=round(ref_l, 4),
        early_peak=round(peak_e, 3), late_peak=round(peak_l, 3),
        drift_s=round((ref_l - ref_e) - (cap_l - cap_e), 4),
        speed_ratio=round(speed_ratio, 6),
        bound=round(bound, 6),
        cap_loud_s=round(cap_dur, 2),
    )


def proof_line(r, label="rate"):
    pct = (r["speed_ratio"] - 1.0) * 100
    bpct = r["bound"] * 100
    proven = abs(r["speed_ratio"] - 1.0) < RATE_TIGHT and r["bound"] < RATE_TIGHT
    verdict = ("PROVEN < 0.1%" if proven else
               ("WITHIN BOUND" if abs(r["speed_ratio"] - 1.0) <= r["bound"] else
                "OFF-RATE"))
    return (f"{label}: {r['speed_ratio']:.5f}x ({pct:+.3f}% +/- {bpct:.3f}%) "
            f"[{r['feature']}, span {r['span_s']:.1f}s, peaks "
            f"{r['early_peak']:.2f}/{r['late_peak']:.2f}] -> {verdict}")


# ---------------------------------------------------------------------------
# CONSTANT-rate fit — the robust complement to per-window drift.
# ---------------------------------------------------------------------------
def rate_fit(cap_mono, ref_mono, sr, lo=0.94, hi=1.10, coarse=0.01, fine=0.0025,
             verbose=False):
    """Bound a CONSTANT playback-rate error WITHOUT needing strong per-window
    peaks. We hypothesise the capture was played at rate `s`, undo it (resample the
    capture's loud region by `s`), and measure the BEST global chroma alignment to
    the unstretched source. The `s` that maximises the global chroma fit is the
    playback rate; a chipmunk (1.088) collapses the fit, so 1.000 wins decisively
    even when the absolute correlation is only ~0.5.

    This is the right tool when drift's per-window NCC can't lock against a mix-
    divergent reference: it pools the WHOLE overlap into one correlation per
    candidate rate. A parabolic fit of the fit-vs-rate curve gives a sub-grid
    estimate, and the curvature gives a resolution. Returns a dict."""
    s0, e0 = av.active_bounds(cap_mono, sr)
    if e0 <= s0 + sr:
        s0, e0 = 0, len(cap_mono)
    loud = cap_mono[s0:e0]
    Cref, fps = av.chroma(ref_mono, sr)

    def fit(s):
        r = signal.resample(loud, int(round(len(loud) * s))) if abs(s - 1) > 1e-9 else loud
        Cc, _ = av.chroma(r, sr)
        amean, _, novl, _, _, _ = av.chroma_identity(Cc, Cref, fps)
        return amean, novl

    grid = np.round(np.arange(lo, hi + 1e-9, coarse), 4)
    fits = [(s,) + fit(s) for s in grid]
    best = max(fits, key=lambda t: t[1])
    bc = best[0]
    fgrid = np.round(np.arange(max(lo, bc - coarse), min(hi, bc + coarse) + 1e-9, fine), 4)
    ffits = [(s,) + fit(s) for s in fgrid]
    fbest = max(ffits, key=lambda t: t[1])

    # parabolic vertex on the fine curve for a sub-grid estimate
    xs = np.array([f[0] for f in ffits])
    ys = np.array([f[1] for f in ffits])
    k = int(np.argmax(ys))
    if 0 < k < len(ys) - 1:
        y0, y1, y2 = ys[k - 1], ys[k], ys[k + 1]
        denom = y0 - 2 * y1 + y2
        d = 0.5 * (y0 - y2) / denom if abs(denom) > 1e-12 else 0.0
        vertex = float(xs[k] + d * fine)
    else:
        vertex = float(xs[k])
    # chipmunk contrast: fit at 1.000 vs at 1.088 (the 48000/44100 bug)
    fit_unity = dict((round(s, 4), a) for s, a, _ in fits)
    f1 = fit_unity.get(1.0) or fit(1.0)[0]
    fchip = fit_unity.get(1.088) or fit(1.088)[0]
    if verbose:
        print("  rate-fit curve (chroma global fit vs hypothesised playback rate):")
        for s, a, _ in fits:
            mark = "  <-- unity" if abs(s - 1.0) < 1e-6 else (
                   "  <-- chipmunk" if abs(s - 1.088) < 1e-6 else "")
            print(f"    {s:.3f}  {a:.4f}{mark}")
    return dict(
        best_rate=round(vertex, 5),
        best_rate_grid=round(fbest[0], 4),
        best_fit=round(fbest[1], 4),
        fit_unity=round(float(f1), 4),
        fit_chipmunk=round(float(fchip), 4),
        chipmunk_rejected=bool(f1 - fchip > 0.05),
        overlap_s=round(best[2] / fps, 1),
        grid=[[round(s, 4), round(a, 4)] for s, a, _ in fits],
    )


# ---------------------------------------------------------------------------
# Top-level run (capture + reference -> measured rate)
# ---------------------------------------------------------------------------
def run(cap_path, ref_path, early_s, late_s, win_s, feature, verbose, band_s=4.0):
    sr = 44100
    cap_mono, _, _ = av.load_audio(cap_path, want_sr=sr)
    ref_mono, _, _ = av.load_audio(ref_path, want_sr=sr)
    if len(cap_mono) < sr:
        raise SystemExit("capture too short/empty")

    # TRIM the capture to its loud song region ONCE here, compute the global anchor
    # on THAT exact trimmed region, then pass the trimmed capture with trim=False to
    # every measure_drift call. (If run() and measure_drift each trimmed
    # independently, the anchor — relative to run()'s trim start — would be
    # misaligned with measure_drift's windows.) The onset envelope is too mix-
    # fragile to anchor on its own, so the chroma full-overlap anchor is shared.
    s0, e0 = av.active_bounds(cap_mono, sr)
    if e0 <= s0 + sr:
        s0, e0 = 0, len(cap_mono)
    loud = cap_mono[s0:e0]
    anchor_s, anchor_corr = _global_anchor(loud, ref_mono, sr)
    # Auto-widen the search band so even a chipmunk's drift over the whole loud
    # span stays inside it: a 1.1x rate over an S-second span drifts ~0.1*S, so
    # band = max(4s, 0.12 * loud_span) covers it with margin while still rejecting
    # the many-second spurious far matches.
    band = max(band_s, 0.12 * (len(loud) / sr))

    results = {"_anchor_corr": round(anchor_corr, 3), "_loud_s": round(len(loud) / sr, 1)}
    feats = [feature] if feature != "both" else ["chroma", "onset"]
    for f in feats:
        try:
            results[f] = measure_drift(loud, ref_mono, sr, early_s, late_s,
                                       win_s, f, verbose=verbose, trim=False,
                                       global_off_s=anchor_s, band_s=band)
            results[f]["anchor_corr"] = round(anchor_corr, 3)
        except SystemExit as e:
            results[f] = {"error": str(e)}
    return results


# ---------------------------------------------------------------------------
# Self-test: synthetic exact-rate AND a known 1.05x stretch.
# ---------------------------------------------------------------------------
def _stretch(x, ratio):
    """Resample x so it plays `ratio` times faster (shorter, like the chipmunk).
    A capture at `ratio`x speed has fewer samples: n/ratio."""
    return signal.resample(x, int(round(len(x) / ratio)))


def _unique_song(sr, secs, seed=0):
    """A rhythmic test signal whose harmonic content is UNIQUE at every point in
    time — no repeating chord loop. This is essential for the drift self-test:
    drift is the SLOPE of matched-ref-time vs cap-time, which requires each window
    to lock to its ONE true position. A periodic loop (like audio_verify's
    _song_like) matches at every repeat and makes absolute alignment ambiguous, so
    the slope is unrecoverable in synthesis (real songs are far less periodic over
    a 70 s span). Here every beat draws a fresh, non-repeating chord from a long
    pseudo-random walk so each window is harmonically distinct."""
    rng = np.random.default_rng(seed)
    n = int(secs * sr)
    t = np.arange(n) / sr
    out = np.zeros(n)
    bpm = 120
    beat = 60.0 / bpm
    pos = 0.0
    root = 48
    while pos < secs:
        # random-walk the root so the progression never repeats
        root = int(np.clip(root + rng.integers(-4, 5), 36, 64))
        chord = [root, root + 4, root + 7, root + 11]  # a unique maj7-ish stack
        seg_n = int(beat * sr)
        idx = slice(int(pos * sr), int(pos * sr) + seg_n)
        tt = t[idx] - pos
        if len(tt) == 0:
            break
        env = np.exp(-tt * 2.5)
        for mnote in chord:
            f = 440.0 * 2 ** ((mnote - 69) / 12.0)
            for h, amp in [(1, 1.0), (2, 0.45), (3, 0.22)]:
                seg = out[idx]
                m = min(len(seg), len(tt))
                seg[:m] += amp * env[:m] * np.sin(2 * np.pi * f * h * tt[:m])
        out[idx.start: idx.start + 200] += 0.3 * rng.normal(0, 1, 200)
        pos += beat
    out = out / (np.max(np.abs(out)) + 1e-9) * 0.7
    return out


def selftest():
    sr = 44100
    secs = 90               # long reference so windows are well separated
    print("=" * 78)
    print("audio_drift SELF-TEST")
    print("=" * 78)

    # A NON-REPEATING reference (unique harmonics every beat) so absolute window
    # alignment is unambiguous — the precondition for measuring the drift slope in
    # synthesis. The self-test passes trim=False and places windows explicitly so
    # the injected-rate recovery is exactly checkable.
    ref = _unique_song(sr, secs, seed=7)

    LEAD = 6.0   # fixed menu-ish lead-in: forces a nonzero absolute offset to find

    def lead(x):
        rng = np.random.default_rng(3)
        return np.concatenate([0.02 * rng.normal(0, 1, int(LEAD * sr)), x])

    cases = []
    # (a) EXACT rate: capture == reference (after the lead). ratio must be 1.000.
    cases.append(("exact_1.000", lead(ref.copy()), 1.000))
    # (b) REQUIRED known 1.05x stretch: capture plays 1.05x too fast (shorter). The
    #     matched reference time advances 1.05 per capture second -> ratio ~1.05.
    cases.append(("stretch_1.050", lead(_stretch(ref, 1.05)), 1.050))
    # (c) chipmunk 1.088x — the real 48000/44100 bug magnitude.
    cases.append(("chipmunk_1.088", lead(_stretch(ref, 1.088)), 1.088))
    # (d) slow 0.97x (under-rate) — proves sign + sub-1 recovery.
    cases.append(("slow_0.970", lead(_stretch(ref, 0.97)), 0.970))

    tmp = tempfile.mkdtemp(prefix="audio_drift_st_")
    refp = os.path.join(tmp, "ref.wav")
    av._wav16(refp, ref, sr)

    # explicit windows, well separated, both safely inside the shortest capture
    # (the 1.088x case song span is 90/1.088 ~ 82.7s; LEAD pushes the late window
    # start to LEAD + late_in_song). We place windows by their position IN THE
    # CAPTURE; the injected stretch then dictates the expected reference offsets.
    win = 8.0
    early = LEAD + 2.0          # 2s into the (stretched) song
    # late window: leave headroom for the most-stretched (shortest) song.
    song_min = secs / 1.088
    late = LEAD + min(song_min - win - 2.0, secs - win - 2.0)

    hdr = (f"{'case':<16}{'inject':>9}{'onset':>10}{'chroma':>10}"
           f"{'bnd%':>7}{'span':>7}  ok")
    print(hdr)
    print("-" * len(hdr))
    all_ok = True
    detail = {}
    for name, sigc, inject in cases:
        capp = os.path.join(tmp, name + ".wav")
        av._wav16(capp, sigc, sr)
        cap_mono, _, _ = av.load_audio(capp, want_sr=sr)
        # The selftest controls the lead-in exactly, so it supplies the known global
        # anchor (capture-time 0 == lead start aligns to reference time -LEAD) and a
        # wide band that admits the full injected drift. This isolates the SLOPE
        # math under test from the anchor search (validated separately on real
        # audio, where _global_anchor finds the anchor from the chroma alignment).
        ro = measure_drift(cap_mono, ref, sr, early_s=early, late_s=late,
                           win_s=win, feature="onset", trim=False,
                           global_off_s=-LEAD, band_s=8.0)
        rc = measure_drift(cap_mono, ref, sr, early_s=early, late_s=late,
                           win_s=win, feature="chroma", trim=False,
                           global_off_s=-LEAD, band_s=8.0)
        # CHROMA is the primary feature (mix/stretch-robust); judge PASS on it. The
        # estimate must recover the injected rate within max(its bound, 0.3%).
        ok = (abs(rc["speed_ratio"] - inject) <= max(rc["bound"], 0.003))
        all_ok &= ok
        detail[name] = {"inject": inject, "onset": ro, "chroma": rc}
        print(f"{name:<16}{inject:>9.3f}{ro['speed_ratio']:>10.4f}"
              f"{rc['speed_ratio']:>10.4f}{rc['bound']*100:>7.3f}"
              f"{rc['span_s']:>7.1f}  {'OK' if ok else 'XX'}")
    print("-" * len(hdr))
    # explicit recovery proof for the two REQUIRED cases (on chroma)
    ex = detail["exact_1.000"]["chroma"]
    st = detail["stretch_1.050"]["chroma"]
    print(f"\nREQUIRED CASES (chroma feature):")
    print(f"  exact-rate   : recovered {ex['speed_ratio']:.4f}x "
          f"(inject 1.000, |err| {abs(ex['speed_ratio']-1.0)*100:.3f}% "
          f"<= bound {ex['bound']*100:.3f}%)  "
          f"{'PASS' if abs(ex['speed_ratio']-1.0)<=max(ex['bound'],0.003) else 'FAIL'}")
    print(f"  1.05x stretch: recovered {st['speed_ratio']:.4f}x "
          f"(inject 1.050, |err| {abs(st['speed_ratio']-1.05)*100:.3f}%)  "
          f"{'PASS' if abs(st['speed_ratio']-1.05)<=max(st['bound'],0.003) else 'FAIL'}")

    # --- CONSTANT-rate-fit self-test: prove the mix-robust rate_fit path also
    #     recovers an injected rate and rejects the chipmunk. Use the exact-rate and
    #     1.05x captures; the fit grid should peak at ~1.000 and ~1.050 respectively.
    print(f"\nCONSTANT-RATE FIT (rate_fit path):")
    rf_ok = True
    for name, expect_peak in [("exact_1.000", 1.000), ("stretch_1.050", 1.050)]:
        capp = os.path.join(tmp, name + ".wav")
        cm, _, _ = av.load_audio(capp, want_sr=sr)
        # the fit undoes a hypothesised capture rate; an injected r-stretch capture
        # is best un-stretched at ~r, so the fit peaks at the injected rate.
        rf = rate_fit(cm, ref, sr, lo=expect_peak - 0.08, hi=expect_peak + 0.08)
        good = abs(rf["best_rate"] - expect_peak) < 0.02
        rf_ok &= good
        print(f"  {name:<16}: best-fit {rf['best_rate']:.4f}x (expect {expect_peak:.3f}, "
              f"fit {rf['best_fit']:.3f})  {'PASS' if good else 'FAIL'}")
    all_ok &= rf_ok

    print(f"\nRESULT: {'ALL PASS' if all_ok else 'FAILURE'} — the tool recovers "
          "injected playback rates from time-drift (exact 1.000 + 1.05x proven) and "
          "from the constant-rate fit.")
    return 0 if all_ok else 1


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("capture", nargs="?", help="captured game-output WAV")
    ap.add_argument("--ref", help="reference WAV (pre-decoded ground truth)")
    ap.add_argument("--song", help="song id -> build reference via decode_reference.py")
    ap.add_argument("--section", default="gameplay",
                    choices=["gameplay", "full", "preview"])
    ap.add_argument("--out-dir", default="/tmp")
    ap.add_argument("--early", type=float, default=None,
                    help="early window start (s into the loud capture region)")
    ap.add_argument("--late", type=float, default=None,
                    help="late window start (s into the loud capture region)")
    ap.add_argument("--win", type=float, default=6.0, help="window length (s)")
    ap.add_argument("--band", type=float, default=4.0,
                    help="min +/- search band (s) around each window's globally-"
                         "anchored position; auto-widened to 0.12*loud-span")
    ap.add_argument("--feature", default="both",
                    choices=["onset", "chroma", "both"])
    ap.add_argument("--ratefit", action="store_true",
                    help="ALSO run the CONSTANT-rate fit (robust to weak per-window "
                         "peaks): the rate that maximises the global chroma fit of "
                         "the rate-undone capture to the source. Bounds a uniform "
                         "rate error / chipmunk even when drift can't lock.")
    ap.add_argument("--json", help="write metrics JSON")
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()

    if args.selftest:
        return selftest()

    if not args.capture:
        ap.error("need a capture WAV (or --selftest)")
    ref = args.ref
    if not ref:
        if not args.song:
            ap.error("need --ref or --song")
        print(f"[drift] building reference for {args.song}/{args.section}")
        ref = av.build_reference(args.song, args.section, args.out_dir)

    results = run(args.capture, ref, args.early, args.late, args.win,
                  args.feature, args.verbose, band_s=args.band)

    print(f"\n  capture   : {args.capture}")
    print(f"  reference : {ref}")
    print(f"  anchor    : global chroma corr {results.get('_anchor_corr')} "
          f"over {results.get('_loud_s')}s loud region")
    print(f"  {'-'*68}")
    worst = None
    for f, r in results.items():
        if f.startswith("_") or f == "ratefit":
            continue
        if "error" in r:
            print(f"  [{f}] ERROR: {r['error']}")
            continue
        print("  " + proof_line(r, label=f))
        # the "rate proven" claim rests on the feature whose windows actually LOCKED
        # (min peak >= a usable floor) and that then has the tighter bound. A weak-
        # peak feature's slope is unreliable and must not win the verdict.
        usable = min(r["early_peak"], r["late_peak"]) >= PEAK_USABLE
        r["usable"] = usable
        if usable and (worst is None or r["bound"] < worst["bound"]):
            worst = r
    if worst is None:  # nothing locked strongly — fall back to the best available
        cand = [r for k, r in results.items()
                if not k.startswith("_") and k != "ratefit" and "error" not in r]
        worst = min(cand, key=lambda r: r["bound"]) if cand else None
    print(f"  {'-'*68}")
    rc = 0
    if worst is not None:
        locked = worst.get("usable", False)
        proven = (locked and abs(worst["speed_ratio"] - 1.0) < RATE_TIGHT
                  and worst["bound"] < RATE_TIGHT)
        within = abs(worst["speed_ratio"] - 1.0) <= worst["bound"]
        if not locked:
            tag = (f"INCONCLUSIVE: windows did not lock (peaks < {PEAK_USABLE}); the "
                   f"reference mix diverges too far for per-window drift — see the "
                   f"self-reference control (capture vs itself) for the rate proof")
            rc = 2
        elif proven:
            tag = "RATE PROVEN CORRECT to < 0.1%"
        elif within:
            tag = "rate within measured bound (consistent with 1.000)"
        else:
            tag = "OFF-RATE DETECTED"
            rc = 1
        print(f"  ==> best: {worst['speed_ratio']:.5f}x +/- {worst['bound']*100:.3f}%"
              f"  (peaks {worst['early_peak']:.2f}/{worst['late_peak']:.2f})  [{tag}]")

    # CONSTANT-rate fit — the robust complement (runs regardless of whether per-
    # window drift could lock; it pools the whole overlap into one correlation per
    # candidate rate, so a uniform rate error / chipmunk is bounded even when the
    # absolute correlation is weak).
    if args.ratefit:
        sr = 44100
        cap_mono, _, _ = av.load_audio(args.capture, want_sr=sr)
        ref_mono, _, _ = av.load_audio(ref, want_sr=sr)
        rf = rate_fit(cap_mono, ref_mono, sr, verbose=args.verbose)
        results["ratefit"] = rf
        print(f"  {'-'*68}")
        print(f"  CONSTANT-rate fit (mix-robust, pools the {rf['overlap_s']}s overlap):")
        print(f"    best-fit rate     : {rf['best_rate']:.4f}x "
              f"(grid {rf['best_rate_grid']:.4f}, fit {rf['best_fit']:.3f})")
        print(f"    fit@1.000 vs 1.088: {rf['fit_unity']:.3f} vs {rf['fit_chipmunk']:.3f}"
              f"  -> chipmunk {'REJECTED' if rf['chipmunk_rejected'] else 'NOT rejected'}")
        ok = abs(rf['best_rate'] - 1.0) < 0.03 and rf['chipmunk_rejected']
        print(f"    => uniform-rate verdict: "
              + ("~1.00 (NOT a chipmunk); best-fit within the broad chroma plateau"
                 if ok else "off-rate candidate — investigate"))

    if args.json:
        open(args.json, "w").write(json.dumps(results, indent=2))
        print(f"  json -> {args.json}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
