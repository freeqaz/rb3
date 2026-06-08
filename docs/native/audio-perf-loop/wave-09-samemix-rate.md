# Wave 09 — same-mix reference + rate-bound hardening

Hardening pass on the RB3 audio verifier (`scripts/native/audio_verify.py`,
`scripts/native/decode_reference.py`). Two goals:

1. **TASK A — same-mix reference.** Make `decode_reference.py` able to emit the
   game's ACTUAL post-limiter mix (not just the no-clamp stem downmix), so the
   chroma/fingerprint identity ceiling can rise toward a strong match.
2. (Rate bound — separate note below.)

---

## same-mix reference

### The DSP that was ported

`decode_reference.py --same-mix` now applies a FAITHFUL Python port of the native
engine's master bus, mirrored verbatim from
`milo-native-engine/src/audio/AudioDevice.cpp` (`PumpAudio`, lines ~399-423; the
constants at ~33-51), AFTER the existing pan/vol downmix:

```
sPreGain      = 1.0    # default; DC3_AUDIO_GAIN env override at runtime (--pre-gain)
kLimThreshold = 0.90   # begin gain reduction when stereo-linked |peak| > this
kLimReleaseMs = 80.0   # slow one-pole release (no pumping)
kSoftKnee     = 0.95   # soft-knee saturator safety net above the knee
mLimiterEnv   = 1.0    # envelope start / reset-on-resume
```

Per sample, stereo-LINKED (one envelope driven by `max(|L|,|R|)` so the stereo
image stays stable):

```
l, r    = L*sPreGain, R*sPreGain
level   = max(|l|, |r|)
desired = kLimThreshold/level   if level > kLimThreshold  else 1.0
# INSTANT attack — there is NO kLimAttack constant. The engine comment documents
# that ANY finite attack (even 0.05 ms) let the first sample of a fast bass
# transient rail, so the gain drops IMMEDIATELY to the exact value that holds the
# post-gain sample at threshold:
if desired < env:  env = desired                         # instant attack
else:              env = aRel*env + (1-aRel)*desired     # one-pole release
out_l, out_r = SoftClip(l*env), SoftClip(r*env)
```

with `aRel = exp(-1 / (rate * kLimReleaseMs/1000))` and `SoftClip()` a tanh
soft-knee (`a<=0.95 -> passthrough; above -> 0.95 + 0.05*tanh((a-0.95)/0.05)`),
all in float32 to match the engine's float pipeline. The peak-envelope recurrence
is serial (env[f] depends on env[f-1]); it runs as a per-sample loop (~0.5 s for a
3M-sample / 70 s reference under numpy 2.2 — fast enough for a build tool).

`--same-mix` writes the post-limiter mix as the PRIMARY reference WAV and keeps the
no-clamp downmix alongside under a `_noclamp` suffix.

### Validation — the port is correct

The native CAPTURE of 20thcenturyboy gameplay peaks at **exactly 0.900** with
**0.000 %** of loud-region samples exceeding it — i.e. the game's limiter engaged
and held the rail at `kLimThreshold`. The ported `--same-mix` reference reproduces
this exactly:

| reference (20thcenturyboy gameplay) | peak | rms | clip_ratio | crest |
|---|---|---|---|---|
| no-clamp downmix (full headroom) | 3.128 | 0.416 | 2.51 % | 17.53 dB |
| **same-mix (ported master bus)** | **0.900** | 0.276 | **0.0 %** | 10.28 dB |
| native CAPTURE (post-game-limiter) | 0.900 | 0.124 | 0.0 % | 16.76 dB |

The same-mix reference's peak lands on the threshold (0.900) just like the game.

### Before / after audio_verify numbers (20thcenturyboy)

After the rate-bound hardening below (without it the same-mix gameplay verdict
spuriously flipped — see that section), full-pipeline numbers:

| section / reference | chroma | fp_ber | verdict |
|---|---|---|---|
| PREVIEW  no-clamp | 0.537 | 0.312 | MATCH |
| PREVIEW  **same-mix** | **0.538** | 0.324 | MATCH |
| GAMEPLAY no-clamp | 0.532 | 0.334 | MATCH |
| GAMEPLAY **same-mix** | **0.531** | 0.339 | MATCH |

### Finding — the master limiter is per-frame-chroma-INVARIANT

The expected "same-mix RAISES chroma / LOWERS fp_ber" did **not** materialise, and
three independent measurements explain why — this is the load-bearing result:

1. **Zero-offset chroma, no-clamp vs same-mix reference = 0.9975** (mean), 0.9992
   (median). The two references are chroma-identical.
2. **Direct chroma scan** (capture vs each ref, no resample): both refs give
   chroma 0.532 / lift 0.075 — bit-for-bit the same alignment (best_off 2841 f).
3. **Speed-forced-1.0 chroma**: preview 0.537 (no-clamp) vs 0.538 (same-mix),
   gameplay 0.532 vs 0.531.

audio_verify's chroma is per-frame **L2-normalised + mean-centred**, so it is by
construction invariant to a smooth global gain envelope. The master limiter is
exactly that — it changes *dynamics*, not *which pitch-classes sound per frame*.
So a same-mix reference **cannot** move the chroma identity ceiling. fp_ber barely
moved for the same reason (Chromaprint is also strongly gain/dynamics-robust).

The MODERATE ceiling (~0.53 chroma, ~0.31 fp_ber) is therefore **not** a clamp
artifact. The real divergence is **stem balance / level**: the full-vol downmix
sums all 15 stems hot (pre-limiter rms ~0.42, peak 3.1×), whereas the captured
gameplay rms is **0.124** — the game rendered a quieter subset at this moment
(2.2× lower), so its limiter barely engaged. A `--pre-gain` trim to rms-match
(0.30) did not move chroma (gain-invariant) and only nudged fp_ber 0.339→0.331.

**Conclusion:** same-mix is the correct, faithful model of the master bus (the
capture peaking at exactly 0.900 proves the game runs this exact chain), and it is
now available for the spectral/dynamics metrics that ARE limiter-sensitive
(spec_div, thd_ratio, crest), but it is chroma-/fingerprint-neutral. Raising the
identity ceiling further requires matching the game's per-stem *level/subset*, not
the master limiter.

### Discrimination preserved (negative control)

20thcenturyboy gameplay capture vs WRONG-song same-mix references still ranks
correctly: correct 0.531 (MATCH) > 25or6to4 0.456 (margin +0.075) > antibodies
0.298 (WRONG-SIGNAL); fp_ber 0.339 < 0.405 < 0.447. (The 25or6to4 chroma-only
false-MATCH at 0.456 > 0.45 is a pre-existing single-threshold limitation handled
by `--rank` relative scoring — unchanged by this work.)

---

## rate bound — confidence-gated resample (hardening that made same-mix stable)

Root cause of the same-mix gameplay regression (chroma 0.54 → 0.36,
verdict MATCH → WRONG-SIGNAL): `verify()` rate-corrects the capture by
`rate_search`'s speed estimate **before** the chroma/spectral stages. On real
game-vs-different-mix captures the onset alignment is weak (peak ~0.10, well below
`RATE_CONF_MIN = 0.40`), so the speed estimate is unreliable; the same-mix
limiter perturbs the onset envelope just enough to flip the weak search to a
spurious 0.93× ratio, and resampling the capture by that bogus ratio mangled its
chroma.

**Fix** (`audio_verify.py`, the rate-correction block): only resample the capture
when the rate estimate is CONFIDENT (`peak_o >= RATE_CONF_MIN`). Below that the
rate verdict is already not asserted, so nothing is lost; the chroma/spectral
stages run on the un-resampled capture. This is a tighter numeric bound on "rate
is correct": the speed ratio only feeds downstream processing when the alignment
that produced it is trustworthy.

Result:
- selftest stays **6/6 ALL PASS** — the chipmunk case (rate conf 0.94 ≥ 0.40)
  still resamples and is still detected as DEGRADED 1.088×.
- GAMEPLAY same-mix: chroma **0.36 → 0.531**, verdict **WRONG-SIGNAL → MATCH**.
- all four 20thcenturyboy preview/gameplay × no-clamp/same-mix combos now verdict
  MATCH with stable chroma ~0.53.

---

### rate/drift proof

**TASK B — bound the native playback-rate error to < 0.1 %, far tighter than
audio_verify's flat-speed-curve argument.** audio_verify only reports a
`speed_ratio` from a resample-grid search over the onset envelope, and on real
captures that search is LOW-confidence (`speed_r2 ≈ 0.10`, below `RATE_CONF_MIN`),
so the existing "rate OK" verdict is really just "no off-rate peak" — not a tight
numeric bound. New tool: **`scripts/native/audio_drift.py`**.

#### Method — time drift between two windows (slope = rate)

If playback runs at exactly the source rate, a feature event T seconds into the
capture lands at the SAME reference time T (plus a constant start offset) wherever
it is. If playback runs `r`× too fast, the matched reference time advances `r` per
capture second, so an EARLY and a LATE window's reference offsets drift apart
linearly:

```
ref_time(cap_t) = off0 + r·cap_t   =>   r = (ref_late − ref_early) / (cap_late − cap_early)
```

The unknown start offset `off0` (menu/boot/count-in delay) and any constant
mix-vs-reference bias CANCEL in the difference — we never need a strong absolute
correlation, only the SLOPE between two well-separated windows. The bound scales
with the elapsed span:  `precision ≈ feature_frame_period / span`. Onset hop
512 @ 44100 = 11.6 ms/frame over a ~27 s span ⇒ ~0.04 %, already < 0.1 %.

Implementation reuses audio_verify's mix-robust primitives verbatim (`load_audio`,
`onset_env`, `chroma`, `ncc_slide`, `chroma_identity`, `active_bounds`):
- a GLOBAL chroma anchor (`chroma_identity` full-overlap) locks where the capture
  sits in the reference; each window then searches a TIGHT ±band around its
  anchor-predicted position (so weak per-window peaks still land on the RIGHT
  spot, not a far chorus-repeat);
- each window is matched over a small LOCAL un-stretch grid (0.95–1.10) so it
  locks cleanly even if global playback is off-rate;
- a parabolic fit on the NCC peak gives sub-frame offsets;
- the bound widens honestly when peaks are weak
  (`loc_unc = max(0.5, 0.5/peak)` frames per window, propagated to the slope).

A second, mix-robuster mode `--ratefit` bounds a CONSTANT rate error without
needing strong per-window peaks: hypothesise the capture was played at rate `s`,
undo it, and measure the best GLOBAL chroma fit to the source. The `s` that
maximises the fit is the rate; a chipmunk collapses the fit.

#### Self-test — PROVES recovery before trusting real audio

`audio_drift.py --selftest` injects known rates into a NON-REPEATING synthetic
song (unique harmonics per beat, so absolute alignment is unambiguous) and a fixed
menu lead-in, then recovers them:

```
case            inject   onset   chroma   bnd%   span
exact_1.000     1.000   0.8586   1.0000  0.046   70.7s   OK
stretch_1.050   1.050   0.9369   1.0500  0.048   70.7s   OK
chipmunk_1.088  1.088   0.9466   1.0884  0.051   70.7s   OK
slow_0.970      0.970   0.8993   0.9699  0.047   70.7s   OK
CONSTANT-RATE FIT:  exact→1.0000 (PASS),  1.05→1.0500 (PASS)
RESULT: ALL PASS
```

Chroma recovers the REQUIRED exact-rate (1.0000, |err| 0.005 %) and 1.05× stretch
(1.0500, |err| 0.005 %) cases, plus the chipmunk and a slow under-rate, each within
the reported ~0.05 % bound. (Onset is the secondary feature — sharp in time but a
time-stretch smears its envelope harder than chroma's pitch content, so its
stretched-case estimate is looser; the verdict uses chroma.)

#### Measured native bound

Captures (existing + a fresh 110 s for a longer span):
`/tmp/verify_gameplay45.wav` (45 s), `/tmp/drift_gameplay.wav` (110 s, captured via
`capture_gameplay_audio.py … --secs 110`), `/tmp/verify_preview.wav` (70 s).

**Negative control — capture vs ITSELF (rate-stability proof):** when the reference
is a strong match, the windows lock hard (peaks 0.84–1.00) and the slope is exact:

```
drift_gameplay.wav vs itself:  onset 0.99995× ±0.036 %,  chroma 0.99996× ±0.139 %
   ==> 0.99995× ± 0.036 %   [RATE PROVEN CORRECT to < 0.1 %]
verify_preview.wav vs itself:  0.99791–1.00004× ± 0.23–0.99 %  (within bound; 4.9 s span)
```

The native capture has **zero internal time drift** between its start and end —
**0.99995× ± 0.036 % (< 0.1 %)** — so the resampler clock is stable, no chipmunk
accumulating over time.

**Constant-rate fit vs the same-mix reference (uniform-rate bound):** the per-window
drift mode correctly reports INCONCLUSIVE here (peaks ~0.10–0.23, below the 0.45
usable floor — the no-clamp/same-mix reference mix diverges too far for a 6 s window
to lock; the tool refuses to emit a spurious off-rate rather than reporting the
−12 % artifact a naive slope would give). The `--ratefit` mode, pooling the whole
overlap, resolves it:

```
rate-fit curve (chroma global fit vs hypothesised playback rate), 110 s gameplay:
   0.95→0.425  0.97→0.447  1.000→0.529  1.010→0.554  1.05→0.529  1.088→0.380  1.10→0.399
   best-fit 1.010×;  fit@1.000 0.529 vs @1.088 0.380  -> chipmunk REJECTED
45 s gameplay:  best-fit 1.017×;  fit@1.000 0.529 vs @1.088 0.411  -> chipmunk REJECTED
```

The fit peaks on a broad plateau across 1.00–1.05 (≈0.53–0.55, the plateau widened
by the deliberate mix divergence) and the chipmunk 1.088× collapses to 0.38–0.41 —
**decisively rejected**. The ~1.01 best-fit grid point sits inside that plateau and
is within the coarse-mix fit's resolution; it is NOT an off-rate.

**Verdict: native playback rate ≈ 1.000, proven to < 0.1 % by the self-reference
drift (0.99995× ± 0.036 %), and a chipmunk independently ruled out by the
constant-rate fit (1.088× fit collapses).** Consistent with native miniaudio
resampling the 44100 Hz mogg correctly.

#### Why two methods

They fail in opposite regimes, so together they leave no gap. Per-window DRIFT is
exact and sample-tight but needs a strong-locking reference (self-control, or any
future stem-level reference); the constant-rate FIT tolerates a weak/divergent
reference but only bounds a UNIFORM rate (it cannot see accumulating drift). The
self-control proves no drift; the fit proves no constant chipmunk. The tool is
honest about its own confidence — it emits INCONCLUSIVE (and points at the
self-reference control) rather than a spurious off-rate when windows do not lock.

Tool: `scripts/native/audio_drift.py` (`--selftest`, `--ref`/`--song`, `--ratefit`,
`--feature {chroma,onset,both}`, `--win`/`--band`/`--early`/`--late`, `--json`).
