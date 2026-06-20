# Audio UNDER-RUN — fix implementation (engine-side, match-neutral)

**Date:** 2026-06-20
**Author:** implement agent (reads `diagnose-rootcause.md`).
**Base engine pin:** `4087e92e76c64830d9d7d23cfdaa1398312d6f38` (the pin the diagnosis ran against).
**Engine branch (paired worktree):** `wt-audio-underrun` — commit `3b61fc6c028f9a542767e898837de4d2b2509da4`.
**rb3 native branch (paired worktree):** `wt-audio-underrun` — commit `a028698d32a4fc84a10a87efe4093f01e6239594`.

---

## What the fix does (the diagnosis's A + B + C, in that order of ROI)

The residual click is a **consumer-side SAB-ring under-run on the WEB path**: the
AudioWorklet drains at the real-time audio clock, the producer (`PumpAudio` inside
`RunOneFrame`) only refills once per rAF frame, and any main-thread longtask longer
than the buffered depth empties the ring → the worklet **hard-stepped to 0** (the
click). Baseline: **~4.4% of output frames silence-padded**, 1:1 with main-thread
stalls, with ~85% of the under-run frames concentrated in the song-start burst, and
an adaptive-latency law that oscillated and sat too shallow.

I implemented all three of the diagnosis's recommended fixes, engine-side only:

### A — Graceful empty-ring fallback (defense-in-depth, conceals any residual dip)
Both the web worklet and the native mirror replaced the **hard zero-pad** with a
**hold-last-sample + short (~3 ms) linear ramp-to-zero**, ramping back up from 0 on
recovery. The same dip is now a smooth de-zipper rather than a step discontinuity,
so any under-run that still slips through is an inaudible smear, not a pop. The
under-run *counter* still counts these quanta (so the metric measures genuine
starvation, A only changes how it *sounds*).

- `engine/src/platform/web/assets/audio-worklet.js` — per-quantum `fadeGain` /
  `lastL`/`lastR` state; ramp-down on the pad loop, ramp-up on the read loop.
- `rb3/native/src/rb3_stream_receiver_native.cpp` — identical hold-last + ramp on
  the miniaudio render callback's zero-fill remainder (native real-device mirror).

### B — Deeper buffer + a law that stops self-defeating (attacks the cause)
`engine/src/audio/AudioDevice_Web.cpp` adaptive-latency law:
- **Floor 50 → 180 ms** (`RB3_AUDIO_LAT_MIN_MS` default). The measured stalls are
  p99 = 83 ms, max = 200–233 ms; a 50 ms floor sat *below* the stalls it had to
  ride out. 180 ms keeps the steady-state buffer above the p99 stall, with the
  adaptive target stacking higher on top.
- **Shrink slowed 25% → 12% of (target−floor)** and gated behind **hysteresis**
  (`kShrinkHoldWindows = 8`): only begin shrinking after 8 consecutive clean
  windows (~4 s), so the buffer holds its depth for a few seconds after a stall
  instead of diving back to the floor and getting re-caught — this kills the
  grow↔shrink oscillation the diagnosis logged.
- **Grow per window 40 → 60 ms** so one sustained-stall window climbs decisively.

### C — Song-start prime (removes the ~85%-of-frames boot burst)
- Worklet: a one-time **prime gate** — outputs silence (no read-cursor advance, not
  counted as an under-run) until the ring first reaches a ~120 ms cushion, then
  begins normal draining. The worklet no longer starts draining into an unprimed
  ring while the two biggest boot-time longtasks land.
- Pump: **`kStartMs` 120 → 200 ms** so the adaptive target *starts* deep (above the
  180 ms floor) rather than climbing into it.

### D — off-main-thread mix: NOT done (blocked on the JSPI single-thread build, as
the diagnosis says). A+B+C only.

**Untouched (already landed, per the diagnosis):** the carry-all resampler, the
master-bus one-pole peak limiter, the decimation fix.

---

## Files changed

ENGINE (paired worktree `milo-native-engine-worktrees/audio-underrun`, branch `wt-audio-underrun`):
- `src/platform/web/assets/audio-worklet.js` — A (hold-last + ramp) + C (prime gate).
- `src/audio/AudioDevice_Web.cpp` — B (floor 180 ms, slow+hysteretic shrink, grow 60 ms) + C (kStartMs 200 ms).

RB3 NATIVE (paired worktree `rb3/.claude/worktrees/audio-underrun`, branch `wt-audio-underrun`):
- `native/src/rb3_stream_receiver_native.cpp` — A native mirror (hold-last + ramp on the miniaudio callback).

> Match-neutrality: all three files are native-only (`#ifdef __EMSCRIPTEN__` /
> `#ifdef HX_NATIVE`), NOT in the Wii `.o` set → no gating needed, no match impact.

---

## Build status

- Native (`cmake --build native/build-native --target rb3-native`): **GREEN** (worktree, clang).
- Web (`scripts/web/build.sh`, release + debug): **GREEN** — `AudioDevice_Web.cpp` compiled
  from the paired engine, `audio-worklet.js` deployed from the paired engine (verified the
  deployed asset contains the fade + prime-gate code).

---

## Before / after metric (the EXACT diagnosis repro)

`node scripts/web/audio-stall-measure.mjs --port <p> --play-secs 45`
(baseline run was against the pre-fix deployed pin build on :8421, same env; after-fix
against the worktree build on :8572.)

| metric | baseline (pre-fix pin build) | **after fix** |
|---|---|---|
| **underrun rate (% frames silence-padded)** | **4.39%** | **0.57%** |
| underrunEvents | 1490 | 193 |
| under-run frames | 189649 / 4318080 | 24586 / 4318080 |
| correlation: 189 ms boot stall | +346 events / +44288 frames | **+0 / +0** |
| correlation: 380 ms boot stall | +519 events / +66432 frames | **+0 / +0** |
| steady-state (t>10 s) new under-runs | sporadic each render/GC stall | **0** (events frozen at 193 from ~t=4 s through the whole 45 s) |

**The two biggest boot stalls (189 ms, 380 ms) — which each previously produced
hundreds of under-run events — now produce ZERO.** The residual 0.57% is entirely
the first ~10 s boot/first-song region (one 42-event blip at a t≈9.9 s song-load
longtask); sustained gameplay (the residual clicks the user heard) is clean.

`node scripts/web/audio-jitter-profile.mjs --port <p> --play-secs 35` (secondary):
| metric | baseline | **after fix** |
|---|---|---|
| RING LOW-WATER min (frames) | 133 (~3 ms) | **4274 (~97 ms)** — `dips<240 frames=0` |
| UNDERRUNS /s events | 3.56 | **0** (0 frames/s; events frozen at 131 from boot) |
| urIncrements nearGC frac | 0.667 | **0** (no under-run increments at all in steady state) |

The adaptive law settled at a stable depth (low-water min 4274 / p50 7170 frames)
and did **not** oscillate — the GROW->240 ms then hold-via-hysteresis worked. The
ring never approaches empty during sustained gameplay, so the starvation is fixed
at the cause, not merely concealed by the fade.

Producer-clean regression guard
(`scripts/native/capture_gameplay_audio.py /tmp/p.wav --secs 25`, worktree binary
WITH the native fade change):
- **peak 0.8708, clip 0.0000%, 60.5% non-zero** — clean. Identical to the binary
  WITHOUT the native change (peak 0.8708, clip 0%), confirming the native receiver
  edit only affects the consumer empty-ring fallback and leaves the
  decode→mix→limiter producer chain byte-for-byte unchanged.
- audio_verify self-test: 6/6 PASS. `audio_verify --rank` on the worktree-binary
  producer capture self-identifies the song as **20thcenturyboy, chroma 0.977 →
  MATCH** (margin +0.197 over #2 → CONFIDENT identity); the other candidates score
  WRONG-SIGNAL/DEGRADED. So the captured audio is the RIGHT song at the RIGHT pitch
  with no clip — audio fidelity is intact (and is orthogonal to this fix, which
  doesn't touch decode/mix/resample/limiter).

---

## Verification status — VERIFIED (web)

The SAME diagnosis repro (`audio-stall-measure.mjs`, the exact primary command) was
run before (pin build) and after (worktree build with my engine+native commits),
and the under-run is **gone**: 4.39% → 0.57%, with the two biggest boot stalls now
producing ZERO under-run events (was hundreds each) and steady-state gameplay
producing ZERO new under-runs. The secondary jitter profile confirms the ring
low-water rose from ~3 ms to ~97 ms and steady-state under-run rate is 0/s. This
attacks the *cause* (starvation), with the graceful fade as a defense-in-depth net.

## Build-plumbing note (NOT part of the fix, worktree-local only)

To build the web target against the *paired* engine worktree, I made a one-line
local edit to the worktree's `scripts/web/build.sh` so it honors a pre-set
`MILO_ENGINE_PATH` (its candidate probe otherwise resolves to the canonical main
engine checkout, which lacks the fix). This edit is **uncommitted and worktree-only**
— it is build plumbing, not part of the audio fix, and should not be cherry-picked.
The canonical build (main repo, after the coordinator bumps `MILO_ENGINE_PIN` to
`3b61fc6`) needs no such edit.

## Honest limitations

- The **native real-device** path could not be exercised on this host (no Pulse/
  PipeWire server, process not in the `audio` group — same env limit the diagnosis
  hit). The native mirror (A) compiles and is byte-for-byte the same fade logic as
  the web worklet; it is unverified at runtime here.
- The web verify is the authoritative one (the bug is web-path; the diagnosis's
  HIGH-confidence repro is web).
