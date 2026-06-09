# Adversarial re-verification of the audio "clipped noise" limiter fix (2026-06-06)

> Independent skeptical re-check by a separate verification agent. Does NOT modify
> STATE.md / wave-*.md. Binary: `native/build-native/rb3-native` built 03:35 (contains
> engine commit 513dcd5's limiter; `pregain` format string present, AudioDevice.cpp
> mtime 02:54 < build 03:35). NO build performed.

## TL;DR
- **The metrics are largely valid**, BUT two real defects: (1) `audio_coherence`'s
  `active_region` + global `clip_ratio` can MASK clipping that is confined to a loud
  region surrounded by silence; (2) `overflow_wraps` runs on the INTERLEAVED stream,
  so ~all of its counts on stereo content are L↔R cross-channel jumps, not true
  single-channel overflow (the per-channel count is the honest one, and it is HIGHER).
- **crest_db ambiguity is HANDLED correctly** — the clipping verdict rests on
  `clip_ratio`/`flat_top`, not crest. The KEY synthetic test (soft-limited pure tone)
  does get mislabeled `clipping` via `flat_top`, but that is a pure-tone degeneracy.
- **THE FIX DOES NOT WORK AT THE SHIPPED DEFAULT.** Real 17 s sustained loud gameplay
  (20thcenturyboy) at unity pre-gain: `clip_ratio 0.027`, **1.48% of samples pinned at
  full scale (±32767)**, 25 true rail-to-rail wraps/s, flat_top 7. Top-level coherence
  verdict = DEGRADED (clipping + overflow_wrapping). The square-wave artifacts
  (`[…11249, -32767, -32767, 32767, 32767, -11337…]`, 245 full-rail sign-flips) are the
  exact "clipped noise" signature the fix was meant to remove.
- **The limiter does NOT hold under overload.** `--gain 3.0`: clip_ratio 0.048,
  flat_top 20, 471 wraps/s. Root cause: a 3 ms attack on a *sample-peak* limiter
  (raw |sample|, not an envelope follower) cannot duck a per-cycle peak of bass/mid
  content (¼-cycle of 200 Hz = 1.25 ms < 3 ms), so peaks pass to the softclip, which
  saturates them at the rail.
- `--gain 0.30` IS clean — but only because 0.30 keeps the ~3.1× sum near unity so the
  limiter barely engages; it does NOT demonstrate the limiter working.

## Loud-region measurements (the decisive metric; analyze_loud.py)
| capture | clip_ratio | flat_top | FS%(±32767) | crest | true wraps/s | verdict |
|---|---|---|---|---|---|---|
| default (unity)   | 0.027 | 7  | 1.48% | 12.3 dB | 24.9 | **CLIPPING** |
| gain 0.30         | 0.0015| 4  | 0.00% | 18.4 dB | 0.0  | clean |
| gain 3.0 (stress) | 0.048 | 20 | 3.01% | 9.9 dB  | 471  | **CLIPPING** |

## Reference decode (independent, decrypt_mogg + ffmpeg)
- 20thcenturyboy decode: OK (OggS valid), 15 ch, 236 s. No-clamp stereo
  **peak 3.13× FS, crest 17.53 dB, clip_ratio(≥1.0) 0.025** — confirms the over-unity
  multitrack sum root cause exactly.
- Whole-file correlation default-vs-ref = WRONG-SIGNAL (xcorr 0.01) due to the capture's
  boot/intro/silent-gap offset, not a wrong song. With manual envelope alignment the
  capture's loud region maps to ref t≈184 s and **spectrogram-shape corr = 0.435**
  (just over the 0.4 "same music" threshold) — it IS 20thcenturyboy, merely clipped +
  misaligned. The post-fix "clean correlation number" remains effectively un-pinnable
  with this capture structure; the clip metrics are the reliable judge.

## Metric validity (synthetic suite, scripts in /tmp/verify/)
- sine −6 dBFS → COHERENT, crest 3.01 dB (= theory), clip 0, wraps 0. OK.
- white noise → noise_like=True (flatness 0.996, zcr 0.50, pitch 0.016). OK.
- hard-clipped sine → clipping=True, flat_top 134, wraps 0 (clamp doesn't wrap). OK.
- soft-limited PURE tone → mislabeled clipping=True (flat_top 112) — pure-tone
  degeneracy (a steady sine pinned at threshold has flat curvature). On realistic
  multi-tone+tremolo "music" through the limiter the false-clip persists at heavy
  overload but a well-staged quiet mix limits to flat_top 0 / clean. CAVEAT, not fatal.
- overflow_wraps mechanism: clamped→0, true int16 overflow→many, limited→0. Mechanism
  is real; the INTERLEAVED-stream defect over-attributes stereo L↔R jumps as wraps.
- audio_correlate --selftest: ALL 6 cases PASS.

## Code review
- One-pole coeffs `exp(-1/(sr*ms/1000))` CORRECT; fast-down/slow-up
  (`coeff = desired<env ? aAtk : aRel`) CORRECT in both directions; stereo-link
  (`level = max(|L|,|R|)`, single env) CORRECT; `mLimiterEnv` persists across blocks,
  reset in native `Resume()`. Web has no Resume reset but is single-threaded → benign.
  Soft-knee + kept hard clamp backstop is correct in principle.
- **DC3 safety: the change is content-adaptive (no RB3 magic constant) → structurally
  DC3-safe**, and it removes the prior 1.1× boost that would have double-attenuated
  DC3's −2 dB master. No risk of gain-reduction on a quiet mix (desired=1.0 below
  threshold). Web path mirrors native constants. The defect is tuning, not DC3-safety.

## Recommendation
The limiter ARCHITECTURE is correct and DC3-safe, but the PARAMETERS are too weak for
the 3.1× RB3 sum: it still rails ~1.5% of loud samples at unity. Make it a proper
envelope-follower peak limiter (rectify + peak-hold, sub-ms attack ≤0.5 ms, or add a
small look-ahead), OR lower the default pre-gain / add a fixed ~0.5–0.6× mix-bus trim
under the limiter so the post-limiter peak truly stays < 0.95. Re-measure on the LOUD
region (not the global stream) — global clip_ratio is diluted by the boot/silent gap and
under-reports. Fix `overflow_wraps` to run per-channel.
