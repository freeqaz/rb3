# Audio UNDER-RUN fix — adversarial correctness review

**Date:** 2026-06-20
**Reviewer:** correctness-review agent (adversarial).
**Under review:**
- Engine commit `3b61fc6c028f9a542767e898837de4d2b2509da4` (branch `wt-audio-underrun`,
  worktree `/home/free/code/milohax/milo-native-engine-worktrees/audio-underrun`).
- RB3 native commit `a028698d32a4fc84a10a87efe4093f01e6239594` (native mirror).
- Diagnosis: `diagnose-rootcause.md`; implementation: `implement-fix.md`.

---

## VERDICT: CONFIRM (with residuals)

The under-run is **genuinely reduced at the cause**, not merely concealed. I independently
re-derived the cadence math, audited the diff for new races/latency/regressions, reproduced the
baseline, and re-measured the fixed build on a fresh web build I compiled myself against the paired
engine worktree. The under-run rate dropped from a reproduced **4.52%** baseline to **0.46%** on
the fix (~10x; matches the implement doc's 4.39%→0.57%). The two biggest boot stalls (183ms, 382ms)
went from +346/+519 under-run events to **ZERO**, and steady-state (t>10s) shows **ZERO** new
under-runs. Residual concerns below are real but non-blocking.

---

## What I verified

### 1. The diff does what the docs claim (read every hunk)
- **A (worklet fade):** hard zero-pad replaced with hold-last-sample + ~3ms linear ramp-to-zero,
  ramp-up from 0 on recovery. `fadeGain`/`lastL`/`lastR` persist across quanta. After a full fade
  (`g→0`), output is `lastL*0` = TRUE silence — no DC offset, no held-sample buzz. Recovery ramps
  the *live* incoming samples up from 0 → click-free re-entry.
- **A (native mirror):** identical hold-last + ramp on the miniaudio callback's zero-fill remainder
  (`rb3_stream_receiver_native.cpp`). `framesToRender==0` (full starvation) handled: cursor does NOT
  advance, fade continues from prior `g`. Post-volume sample held.
- **B (pump law):** floor 50→180ms, shrink 25%→12% of (target−floor), shrink gated behind 8
  consecutive clean windows (hysteresis via new `sCleanRun`), grow 40→60ms. `sCleanRun` is reset
  on every dirty/near-miss window — correct.
- **C (prime):** worklet holds silence (no cursor advance, NOT counted as under-run) until the ring
  first reaches a ~120ms cushion; one-shot (`primed` never re-arms). Pump `kStartMs` 120→200ms.

### 2. No new SAB race (key adversarial check — PASS)
The fix adds **zero** new atomic operations and **zero** new shared state. `fadeGain`, `lastL/R`,
`primed`, `primeFrames` are all worklet-thread-local instance fields. `sCleanRun` is pump-side
(main-thread) static. The SPSC ring contract is unchanged: worklet `Atomics.load`s both cursors and
`Atomics.store`s only readPos (index 1); producer only loads. The prime gate `return true`s WITHOUT
advancing readPos, so it cannot corrupt the cursor.

### 3. Buffer math fits (PASS)
`RING_FRAMES=32768` (743ms @44100). Floor 180ms=7938 frames. Effective max =
min(500ms=22050, 80%·32768=26214)=22050. Adaptive target ∈ [7938, 22050] ⊂ ring. No overflow; the
≥20% headroom cap is preserved. The prime cushion (120ms) < pump start target (200ms), so the pump
fills past the prime threshold → no startup deadlock.

### 4. Prime gate does NOT launder the metric (key adversarial check — PASS)
The biggest risk was that the prime gate fakes the improvement by emitting un-counted silence. It
does NOT: (a) it is one-shot — after the first prime it never re-arms, so steady-state gameplay
under-runs are counted normally; (b) a mid-song full drain hits the *fade* path (counted), not the
prime path; (c) emitting silence before any audio exists at boot is the *correct* output, not a
hidden starvation. The under-run counter still counts genuinely-starved quanta, so the metric
measures real elimination from B+C.

### 5. Independent reproduction
- **Baseline (deployed pre-fix build on :8421):** I ran the exact diagnosis repro
  (`audio-stall-measure.mjs --play-secs 30`) → **4.52% frames silence-padded** (1325 events /
  168311 / 3720192), boot burst t=2–5s dominating, big boot stalls 186ms→+346 / 384ms→+519 events.
  Matches the diagnosis (4.05%) and implement (4.39%) baselines — the bug and diagnosis are real.
- **Fixed build:** I compiled BOTH web release+debug against the paired engine worktree
  (`MILO_ENGINE_PATH=…/milo-native-engine-worktrees/audio-underrun scripts/web/build.sh`), served on
  my own port :8599, confirmed the served worklet contains the fade+prime code and the release wasm
  is fresh (carries the B+C pump law). Re-ran the same harness → **0.46%** (129 events / 16394 /
  3543040). Boot stalls 183ms@0.8s and 382ms@1.3s → **+0/+0** under-run events each (baseline was
  +346/+519). Per-second under-run deltas are **0 from t=9s onward** — steady-state gameplay is
  clean. The residual 0.46% is one 36-event blip at the t≈9s first-song load. This is a genuine
  cause-fix (B deeper buffer + C prime gate), independently reproduced by me, not a doc claim taken
  on faith.

---

## RESIDUALS (real, non-blocking)

1. **SFX latency regression.** The whole point of the adaptive cap was "keep latency as LOW as
   possible for snappy SFX" — a one-shot menu/hit SFX queues behind the buffered music. Raising the
   floor 50→180ms means keydown→audio for one-shots grows by ~130–190ms. For a rhythm game the
   *gameplay note audio* is part of the continuous music stream (gem-hit timing unaffected), but
   menu navigation clicks and one-shot hit-flame SFX will feel ~130ms laggier. This is a deliberate
   tradeoff (robustness ↔ snappiness), not a bug, and is env-tunable (`RB3_AUDIO_LAT_MIN_MS`). 180ms
   is on the aggressive side; 120–150ms would still cover p99=83ms while halving the SFX penalty.

2. **A stall LARGER than the current target still under-runs** (then is faded, not eliminated).
   The diagnosis saw a 381ms boot stall; even a grown 240ms target under-runs on a >240ms stall.
   Fade (A) conceals it and prime (C) covers the *boot* case, but a mid-song >240ms longtask
   (GC / WebGPU pipeline compile / asset decode) will still briefly fade. This is inherent to the
   single-threaded producer cadence; the real cause-fix is D (off-main-thread mix), correctly
   deferred. The fix reduces frequency and makes the residual inaudible, which is the right call.

3. **Native real-device path unverified at runtime** (no Pulse/PipeWire / not in `audio` group on
   this host — same env limit the diagnosis hit). The native mirror compiles, is byte-for-byte the
   same fade logic, and the producer-clean guard confirms the decode→mix→limiter chain is unchanged,
   but the native consumer fade is not exercised here. Acknowledged honestly in the docs.

## Limiter / resampler regression check (PASS)
The fix touches only the consumer empty-ring fallback and the pump's queued-depth target. It does
NOT touch the carry-all resampler, the master-bus one-pole peak limiter, or the decimation fix. The
implement doc's producer-clean guard (`capture_gameplay_audio.py`: peak 0.8708, clip 0%, identical
with/without the native change) confirms the producer chain is byte-identical. audio_verify --rank
self-identifies the right song at the right pitch.
