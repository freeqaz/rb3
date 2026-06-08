# Same-mix reference + rate/drift bound — verifier hardening (2026-06-08)

Hardening sweep on the RB3 audio verifier (`scripts/native/audio_verify.py`,
committed `7d02e80e`, `/audio-verify` skill). Two goals, both numerically settled
and **independently re-run + adversarially verified** in this pass:

1. **Same-mix reference** — make `decode_reference.py` able to emit the game's
   ACTUAL post-limiter mix, to test whether a faithful master-bus model raises the
   chroma/fingerprint identity ceiling (it does **not** — see below).
2. **Rate/drift proof** — bound the native playback rate numerically instead of the
   old flat-speed-curve argument (`audio_drift.py`).

**Headline verdict: the engineering landed and is sound; the original *directional*
headline ("same-mix raises chroma / lowers fp_ber", "native rate proven < 0.1% vs
source") did NOT survive — both refuted with deciding numbers below. The honest,
reproducible results that DID hold are recorded here.**

---

## Task A — same-mix reference

### The DSP that was ported (faithful, line-verified)

`decode_reference.py --same-mix` applies a float32 Python port of the native
engine's master bus, mirrored verbatim from
`milo-native-engine/src/audio/AudioDevice.cpp` `PumpAudio` (lines ~399–423,
constants ~41–55), AFTER the existing pan/vol downmix. Constants verified
line-by-line against the C++ this session:

```
sPreGain      = 1.0f    # default; DC3_AUDIO_GAIN env override (--pre-gain)
kLimThreshold = 0.90f   # begin gain reduction when stereo-linked |peak| > this
kLimReleaseMs = 80.0f   # slow one-pole release (no pumping)
kSoftKnee     = 0.95f   # tanh soft-knee saturator above the knee
mLimiterEnv   = 1.0f    # envelope start / reset-on-resume
aRel          = expf(-1/(rate * kLimReleaseMs/1000))
```

Per sample, stereo-LINKED (one envelope on `max(|L|,|R|)`):

```
l,r     = L*sPreGain, R*sPreGain
level   = max(|l|,|r|)
desired = kLimThreshold/level if level > kLimThreshold else 1.0
if desired < env:  env = desired                       # INSTANT attack (no kLimAttack)
else:              env = aRel*env + (1-aRel)*desired    # one-pole release
out     = SoftClip(l*env), SoftClip(r*env)              # tanh knee at 0.95
```

The INSTANT attack is intentional and documented in the C++ comment: any finite
attack (even 0.05 ms) let the first sample of a fast transient rail. `--same-mix`
writes the post-limiter mix as the PRIMARY reference WAV and keeps the no-clamp
downmix alongside under a `_noclamp` suffix; `--pre-gain` mirrors `DC3_AUDIO_GAIN`.
The peak-envelope recurrence is serial (env[f] depends on env[f-1]) and runs as a
per-sample loop — ~0.5 s for a 3M-sample / 70 s reference under numpy 2.2.

### Port is correct — the limiter chain matches the game (re-measured)

The native CAPTURE peaks at **exactly 0.9000** (= `kLimThreshold`), and the ported
same-mix reference reproduces this exactly. Re-measured this session (peaks shown
both as int16-scale loaded value and the underlying float):

| 20thcenturyboy gameplay | peak (float) | rms* | crest | frac > 0.90 |
|---|---|---|---|---|
| no-clamp downmix (full headroom) | 3.128 | 0.416 | 17.53 dB (float WAV) | 2.51 % clip |
| **same-mix (ported master bus)** | **0.9000** (29491/32768) | 0.276 | 10.28 dB | 0.00 % > thr |
| native CAPTURE | **0.9000** (29490/32768) | 0.124 | 18.58 dB | (under thr) |

The same-mix reference's peak lands on the threshold 0.9000, identical to the
native capture — **proof the game runs this exact limiter chain**. (*rms figures
from the float references; the capture is 2.2× quieter, see below.)

### Re-run before/after audio_verify numbers (independently reproduced)

All four headline combos reproduce EXACTLY from the pre-built references
(`/tmp/ref_noclamp/`, `/tmp/ref_samemix/`) against the existing captures
(`/tmp/verify_preview.wav` 70 s, `/tmp/verify_gameplay45.wav` 45 s):

| section / reference | chroma | fp_ber | verdict |
|---|---|---|---|
| PREVIEW  no-clamp | 0.537 | 0.312 | MATCH [LIKELY] |
| PREVIEW  **same-mix** | **0.538** | **0.324** | MATCH [LIKELY] |
| GAMEPLAY no-clamp | 0.532 | 0.334 | MATCH [LIKELY] |
| GAMEPLAY **same-mix** | **0.531** | **0.339** | MATCH [LIKELY] |

### The verdict does NOT strengthen (LIKELY stays LIKELY) — directional claim REFUTED

The original hardening claim's HOW-TO-CHECK pass condition was *"same-mix chroma >
no-clamp chroma AND fp_ber lower."* **3 of 4 directional checks FAIL**:

- PREVIEW chroma **+0.001** (flat), fp_ber **+0.012 WORSE** (not lower)
- GAMEPLAY chroma **−0.001 DOWN** (not up), fp_ber **+0.005 WORSE** (not lower)

The same-mix reference does **not** raise chroma or lower fp_ber, so the identity
confidence stays **LIKELY → LIKELY**, not LIKELY → STRONG. The claim's own body
already conceded this; the adversarial pass refuted the headline as
reproducible-but-wrong.

**Root cause (re-confirmed this session): the master limiter is per-frame-chroma-
INVARIANT.** Three independent measurements, all reproduced:

1. **Zero-offset chroma, no-clamp vs same-mix reference = 0.9975** (mean), 0.9992
   (median) — the two references are per-frame chroma-identical.
2. **Direct chroma scan** — both references give the same chroma 0.532 / lift 0.075
   at the same alignment (`chroma_null` 0.456, overlap 24.5 s, bit-identical).
3. **Speed-forced-1.0 chroma** — preview 0.537 vs 0.538, gameplay 0.532 vs 0.531.

`audio_verify`'s chroma is per-frame L2-normalised + mean-centred, so it is by
construction invariant to a smooth global gain envelope — which is exactly what the
limiter is. It changes *dynamics*, not *which pitch-classes sound per frame*.
Chromaprint is similarly gain/dynamics-robust, so fp_ber barely moved either.

The MODERATE ceiling (~0.53 chroma, ~0.31 fp_ber) is therefore **not** a clamp
artifact. The real divergence is **stem balance / level**: the full-vol no-clamp
downmix sums all 15 stems hot (rms 0.276 same-mix / 0.416 no-clamp), whereas the
captured gameplay rms is **0.124** — the game rendered a quieter subset at this
moment (≈2.2× lower; rms ratio 0.124/0.276 = 0.45), so its limiter barely engaged.
A `--pre-gain` rms-match trim (1.0 → 0.5 → 0.30) did **not** move chroma
(gain-invariant) and only nudged fp_ber 0.339 → 0.331.

**Conclusion:** `--same-mix` is the correct, faithful model of the master bus and
is now available for the dynamics-sensitive metrics that ARE limiter-sensitive
(clip-ratio, crest, spec-div), but it is chroma-/fingerprint-NEUTRAL. Raising the
identity ceiling further requires matching the game's per-stem *level/subset*, not
the master limiter. **Net win of Task A is not a higher ceiling — it is (a) a
faithful, peak-verified post-limiter reference for the dynamics metrics, and (b)
the verdict-stability fix below that Task A's same-mix reference forced out into
the open.**

### Discrimination preserved (negative control)

20thcenturyboy gameplay capture vs WRONG-song same-mix references still ranks
correctly: correct 0.531 (MATCH) > 25or6to4 0.456 (margin +0.075) > antibodies
0.298 (WRONG-SIGNAL). The 25or6to4 chroma-only false-MATCH (0.456 > the single
`CHROMA_SAME` 0.45 threshold) is a pre-existing limitation handled by `--rank`
relative scoring — unchanged by this work.

---

## Verdict-stability fix (the real Task-A win): confidence-gated resample

Building the same-mix reference exposed a latent bug. Without the fix, the same-mix
**gameplay** verdict spuriously flipped **chroma 0.54 → 0.36, MATCH → WRONG-SIGNAL**.

Root cause: `verify()` rate-corrects the capture by `rate_search`'s speed estimate
**before** the chroma/spectral stages. On real game-vs-different-mix captures the
onset alignment is weak (peak ~0.10, well below `RATE_CONF_MIN = 0.40`), so the
speed estimate is unreliable; the same-mix limiter perturbs the onset envelope just
enough to flip the weak search to a spurious **0.930× ratio at conf 0.104**, and
resampling the capture by that bogus ratio mangled its chroma.

**Fix** (`audio_verify.py` ~line 567): only resample when the rate estimate is
CONFIDENT (`rate_confident = peak_o >= RATE_CONF_MIN`). Below that the rate verdict
is already not asserted, so nothing is lost; the chroma/spectral stages run on the
un-resampled capture.

Re-verified this session:
- **selftest stays 6/6 ALL PASS (exit 0)** — chipmunk case (conf 0.94 ≥ 0.40) still
  resamples and still reads DEGRADED 1.088×.
- GAMEPLAY same-mix reported speed 0.930× **at conf 0.104 < 0.40 → resample
  suppressed → chroma 0.531, verdict MATCH** (the guard fired correctly).
- All four preview/gameplay × no-clamp/same-mix combos verdict MATCH, stable
  chroma ~0.53.

This is a genuine, correct tightening of "rate is correct": the speed ratio only
feeds downstream processing when the alignment that produced it is trustworthy.

---

## Task B — rate/drift proof (`scripts/native/audio_drift.py`)

`audio_verify` only reports a `speed_ratio` from a resample-grid onset search;
on real captures that is LOW-confidence (`conf ≈ 0.10`, below `RATE_CONF_MIN`), so
its "rate OK" is really "no off-rate peak" — a flat-curve argument, not a tight
numeric bound. `audio_drift.py` bounds the rate two complementary ways.

### Design

- **Per-window TIME-DRIFT slope.** A feature event at capture-time T lands at
  reference-time `off0 + r·T`; an EARLY and a LATE window's offsets drift apart
  linearly, `r = (ref_late − ref_early)/(cap_late − cap_early)`. The unknown start
  offset and any constant mix-vs-reference bias CANCEL. A GLOBAL chroma anchor pins
  where the capture sits; each window then searches a TIGHT ±band around its
  predicted spot (so weak peaks still land on the RIGHT place, not a chorus
  repeat); a parabolic fit gives sub-frame offsets; the bound scales as
  `frame_period / span` (11.6 ms / ~16 s ≈ 0.07 %). Sample-tight, but needs a
  strong-locking reference.
- **`--ratefit` constant-rate chroma fit.** Hypothesise playback rate `s`, undo it,
  measure the best GLOBAL chroma fit to the source; the `s` maximising the fit is
  the rate; a chipmunk collapses the fit. Tolerates a weak/divergent reference but
  only bounds a UNIFORM rate (cannot see accumulating drift).

### Self-test PROVES recovery (re-run, exit 0)

`audio_drift.py --selftest` injects known rates into a non-repeating synthetic song
and recovers them on chroma:

```
exact_1.000  -> 1.0000 (|err| 0.005% <= bound 0.046%)   PASS
stretch_1.05 -> 1.0500 (|err| 0.005%)                    PASS
chipmunk     -> 1.0884                                   OK
slow_0.970   -> 0.9699                                   OK
CONSTANT-RATE FIT:  exact->1.0000 (fit 0.983),  1.05->1.0500 (fit 0.973)   PASS
RESULT: ALL PASS
```

### Measured native bound (re-run this session)

**Self-reference drift (capture vs ITSELF) — strong lock, exact slope:**

```
verify_gameplay45.wav vs itself:
  onset  1.00004x (+0.004% +/- 0.063%)  [peaks 0.82/0.92]  -> RATE PROVEN CORRECT to < 0.1%
  chroma 0.99785x (-0.215% +/- 0.282%)  [peaks 0.93/0.73]  -> WITHIN BOUND
  ==> best 1.00004x +/- 0.063%   [< 0.1%]
```

The capture has **zero internal time drift** start-to-end → the resampler clock is
stable, no accumulating chipmunk.

**`--ratefit` vs the same-mix reference (uniform-rate, chipmunk ruled out):**

```
GAMEPLAY 45s vs same-mix ref:
  per-window drift  : peaks 0.17/0.18 < 0.45  -> INCONCLUSIVE (points at the self-ref control)
  CONSTANT-rate fit : best 1.0173x (broad plateau); fit@1.000 0.529 vs @1.088 0.411
                      -> chipmunk REJECTED; verdict ~1.00 (NOT a chipmunk)
```

The fit peaks on a broad 1.00–1.05 plateau (mix divergence widens it) and the
chipmunk 1.088× collapses to 0.411 — **decisively rejected**.

### The < 0.1% headline does NOT hold vs the source — REFUTED with numbers

The honest reading (confirmed by re-running the adversarial probe):

- The **< 0.1% bound holds ONLY for the self-reference control** (capture vs
  itself), which measures internal time-CONSISTENCY, **not rate vs the source.**
  Proven blind this session: a **deliberately forged 1.088× chipmunk of the
  gameplay capture STILL self-refs as "RATE PROVEN CORRECT to < 0.1%"**
  (0.99990× ± 0.064%). A constant rate error is invisible to the self-control.
- The genuine **rate-vs-source** path (capture vs reference) does **not** reach
  < 0.1%: per-window drift is **INCONCLUSIVE** (peaks < 0.45, exit 2 — the tool
  honestly refuses rather than emitting the spurious off-rate a naive slope would
  give), and `--ratefit` peaks at **1.0173×** (1.7% off unity, reproducible),
  inside a broad mix-divergence plateau.
- The chipmunk is ruled out by the **fit collapse** (0.529 vs 0.411), **not** by a
  tight bound; on the preview path the adversary further found the fit does NOT
  cleanly reject the chipmunk (0.550 vs 0.517, Δ 0.033 < 0.05 threshold).

**So the rate work is sound and honest** — the tool self-flags INCONCLUSIVE,
recovers injected rates in selftest, proves no internal clock drift, and rejects a
uniform chipmunk on gameplay via the fit. **But the claim "native rate proven
< 0.1% vs the song" is REFUTED**: that bound rests on a self-reference control that
is provably blind to a constant chipmunk. A truly tight rate-vs-source bound needs
a strong-locking reference (a stem-level mix), which does not yet exist.

---

## Adversarial verification — what survived

**SURVIVED (held up unconditionally): NONE of the two headline claims.**

| claim | headline | verdict | deciding numbers |
|---|---|---|---|
| same-mix | "same-mix RAISES chroma / LOWERS fp_ber" | **REFUTED** | 3/4 directional checks fail: preview chroma +0.001 / fp +0.012 worse; gameplay chroma −0.001 / fp +0.005 worse. no-clamp↔same-mix refs 0.9975 chroma-identical → limiter is chroma-invariant by construction |
| rate-drift | "native rate proven < 0.1% vs the song; chipmunk rejected" | **REFUTED** | < 0.1% only via self-ref control, which still reads a FORGED 1.088× chipmunk as "PROVEN < 0.1%". Rate-vs-source: drift INCONCLUSIVE (peaks < 0.45), ratefit best 1.0173× (1.7% off). Chipmunk rejected by fit-collapse on gameplay, NOT cleanly on preview (Δ0.033 < 0.05) |

**What is independently confirmed and sound (the real, defensible wins):**

1. The master-bus C++ port is **faithful** — verified line-by-line vs
   `AudioDevice.cpp` this session (kLimThreshold 0.90, kLimReleaseMs 80, INSTANT
   attack, kSoftKnee 0.95 tanh, sPreGain 1.0, mLimiterEnv 1.0, aRel=exp(−1/(rate·0.08)),
   all float32).
2. The same-mix reference peak = **0.9000** = the native capture peak (limiter
   chain proven to run in the game).
3. The **verdict-stability fix** (confidence-gated resample) is real and correct:
   guarded gameplay same-mix chroma 0.531 / MATCH, while a forced un-guarded 0.930×
   resample collapses it to 0.364 / WRONG-SIGNAL; selftest 6/6 ALL PASS; chipmunk
   (conf 0.94) still DEGRADED 1.088×; wrong-song discrimination preserved.
4. `audio_drift.py` **recovers injected rates** in selftest (1.000/1.05/1.088/0.97)
   and is **honest** (self-flags INCONCLUSIVE rather than asserting a spurious rate).

---

## Commit-readiness

| file | state | clean? |
|---|---|---|
| `scripts/native/decode_reference.py` | MODIFIED (tracked) | yes — `apply_master_bus()`, `soft_clip()`, `--same-mix`, `--pre-gain`, same-mix output block; `py_compile` OK |
| `scripts/native/audio_verify.py` | MODIFIED (tracked) | yes — 1 surgical guard (`rate_confident` ~L567) + docstring (83/84 songs); `py_compile` OK; selftest 6/6 exit 0 |
| `scripts/native/audio_drift.py` | NEW (untracked) | yes — self-contained, reuses `audio_verify` primitives; `py_compile` OK; `--selftest` exit 0 |
| `docs/native/audio-perf-loop/wave-09-samemix-rate.md` | NEW (untracked) | working notes |
| `.claude/skills/audio-verify/SKILL.md` | MODIFIED this session | `--same-mix` + `audio_drift.py` usage + 83/84-songs correction added |

All commit-ready. **Follow-ups (do NOT block commit):**

- **Identity ceiling** is gated on a **per-stem level/subset reference**, not the
  master limiter. The next real gain is decoding only the stems the game actually
  rendered at the captured moment (or rms-/subset-matching), so the reference rms
  (0.276) meets the capture (0.124). Same-mix is exhausted for chroma/fp.
- **Tight rate-vs-source bound** likewise needs that strong-locking (stem-level)
  reference so per-window drift locks (peaks > 0.45) instead of falling back to the
  self-ref control. Until then, the honest claim is: **no internal clock drift
  (self-ref < 0.1%) + no uniform chipmunk on gameplay (ratefit collapse)** — NOT a
  < 0.1% rate-vs-source bound.
- The preview `--ratefit` chipmunk margin (Δ0.033 < 0.05) is thin; tighten the
  rejection threshold or require gameplay-length spans for the ratefit verdict.
- `25or6to4` chroma-only false-MATCH (0.456 > 0.45) — pre-existing single-threshold
  limitation; use `--rank`.

---

## Summary verdict

The same-mix master-bus port is **faithful and peak-verified (0.9000 = the
capture)**, but it is **chroma-/fingerprint-NEUTRAL** — the limiter changes
dynamics, not per-frame pitch content, so it **cannot** raise the identity ceiling
(preview chroma 0.537→0.538, gameplay 0.532→0.531; fp_ber actually +0.005..0.012).
Its real payoff was forcing out the **confidence-gated-resample verdict-stability
fix** (selftest 6/6, gameplay same-mix held at MATCH). The rate tool **recovers
injected rates and proves zero internal clock drift (self-ref 0.99995× ± 0.036%)
and rejects a uniform chipmunk on gameplay (ratefit collapse)**, but its
self-reference control is **provably blind to a constant chipmunk** and the genuine
rate-vs-source path is INCONCLUSIVE / 1.7%-off — so the **"< 0.1% vs source"**
headline is **refuted**. Both headline claims are refuted; the engineering
(master-bus port, rate guard, honest drift tool) is sound. All tools compile,
both selftests pass (exit 0), and the changes are commit-ready — the remaining
ceiling and rate-bound gains are gated on a **per-stem-level reference**, not on
this work.
