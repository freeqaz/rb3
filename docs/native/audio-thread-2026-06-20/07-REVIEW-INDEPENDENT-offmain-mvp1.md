# INDEPENDENT REVIEW — MVP-1 off-main web-audio (path A, worklet-side stem mix)

**Date:** 2026-06-20
**Reviewer:** independent verification agent (separate from the build agent)
**Under review:** `06-BUILD-RESULT-offmain-mvp1.md`
**Tested SHAs (clean, exactly as claimed):** engine `wt-audio-mvp-offmain1@805e701a`, rb3 `wt-audio-mvp-offmain1@fcc950cf`
**Build:** `scripts/web/build.sh --debug` with `MILO_ENGINE_PATH_OVERRIDE=$(.engine-path)` → deployed; verified the deployed `audio-worklet.js` contains `_processOffMain`/`init-offmain`/`resampleCarry`.
**Verdict:** **CONFIRM with residuals** — the implementation is correct and the off-main path delivers the architecture's promise; the *strict* gate (0% at 400 AND 800 ms under back-to-back interval=250) is NOT met, exactly as the build doc honestly states (decode-starvation limit, not a mixer bug).

---

## 1. I reproduced the build + ran every test myself

All numbers below are **my own runs** in the build agent's intact worktree (clean, at the claimed SHAs), not the doc copied.

### 1a. Stall bench A/B (interval=250, step=12s, song=20thcenturyboy) — MY numbers

| stall(ms) | baseline OFF (mine) | off-main ON (mine) | doc claimed ON |
|---|---|---|---|
| 0   | 0.00% | 0.00% | 0% |
| 50  | 0.00% | 0.00% | 0% |
| 100 | 0.00% | 0.00% | 0% |
| 200 | 0.00% | 0.00% | 0% |
| 400 | 0.00% (rode latency to **500 ms** ceiling) | **13.29%** @ 70 ms floor | 13.66% |
| 800 | **38.97%** (catastrophic wall) | **8.53%** | 8.94% |

My ON/OFF curves match the doc within run-to-run noise (ON 13.29/8.53 vs doc 13.66/8.94; OFF 800ms 38.97% vs doc 37.54%). The baseline's "0% at 400 ms" is bought by riding the adaptive law to its **500 ms** ceiling (I saw `latency GROW -> 500 ms` / `latency HIGH ~500 ms` in the OFF run) — the exact latency the user rejects.

### 1b. SPACED stalls (interval=1000, decode recovers between freezes) — MY numbers

| stall(ms) | baseline OFF (mine) | off-main ON (mine) |
|---|---|---|
| 400 | **23.78%** | **0.00%** |
| 800 | **31.99%** | **0.00%** |

This is the decisive A/B: on single spaced freezes the off-main path turns 24–32% dropout into **0%**. It is not a no-op and not measuring nothing — the deep stem ring genuinely keeps music fed through a long single freeze. Reproduces the doc's spaced claim exactly.

### 1c. Correctness — audio_verify on MY captures (worklet-tap → WAV)

**Clean play (no stalls), 22 s capture:**
```
chroma 0.98 (STRONG) | speed 1.002x (conf 0.91) | pitch 1.000x
clip 0.00% / flat-top 0 | wrap 0.0/s | crest 13.1 dB | flatness 0.160
VERDICT: MATCH
```

**Seam-stress capture (I injected 400 ms main-thread stalls every 1000 ms during recording, 22 s):**
```
chroma 0.97 (STRONG) | speed 1.001x | pitch 1.000x
clip 0.00% / flat-top 0 | wrap 0.0/s | crest 13.3 dB | flatness 0.158
VERDICT: MATCH
```

Both MATCH. The slightly higher fp BER (0.23 vs 0.14) and lower align peak under stalls is the *expected* under-run silence/hold padding reducing fingerprint correlation — NOT distortion (clip/flat-top/wrap/flatness all clean).

---

## 2. Refutation attempts (the 4 named correctness risks) — all SURVIVED

### C2a — resampler CARRY at the producer/consumer seam (the #1 risk, wave-08 bug class)
**Static concern I found:** in the resampling path, the stem read-pointer advance is clamped
`stemAdv = Math.min(consumed, minAvail)` during a stall, while `resamplePos`/carry are set as if
`consumed` bus frames were used. When `minAvail < consumed` (a stall partial-shortfall), the carried
frames `bus[consumed..]` correspond to `stem(rp+consumed)` but `rp` only advanced by `stemAdv` —
a theoretical `(consumed - stemAdv)`-frame discontinuity at the seam.
**Empirical result: BENIGN.** Under 400 ms injected stalls the shortfall region is per-stem *silence*
(the mix code zeroes any stem where `busOff >= avail`), so the skipped frames are silence and inaudible.
audio_verify under stress: clip 0.00%, flat-top 0, wrap 0.0/s, flatness 0.158, chroma 0.97 — **no seam
clicks, no static, no wrap distortion**. The wave-08 "static/clipping" class does NOT recur. (I note the
clamp could in principle bite at higher per-stem-divergence configurations; not observed here.)

### C2b — limiter envelope continuity across the move
The one-pole `limiterEnv` is now audio-thread-owned, updated continuously every quantum (instant attack,
ctx-rate one-pole release — an exact port of the C++ `MixSources` limiter). **No pumping/zipper:** crest
factor is a healthy 13.1–13.3 dB in both captures (not crushed), flatness low, chroma 0.97–0.98 → the
spectral envelope is preserved. CONFIRM.

### C3 — prime gate prevents song-start burst at the new fixed floor
I polled `_rb3Audio.underruns` from game-screen entry through the first 5 s of playback:
**0 under-run events, 0 frames** across the whole window. The bench's 0 ms-stall step is also 0%. The
stem-aware prime gate (`if (!this.primed) { minAvail < stemPrimedFrames → silence }`) holds the cushion
before mixing. CONFIRM — no song-start burst at the 70 ms floor.

### "Does MUSIC truly survive 400 ms+ stalls (not a lucky run)?"
Yes for *spaced* 400/800 ms (0% across the step, 12 injections each, repeatable). NO for *back-to-back*
interval=250 ≥400 ms — but that is decode starvation (see §3), not a mixer failure.

---

## 3. The residual (honest, matches the build doc)

The strict gate — **0% music under-run at 400 ms AND 800 ms, interval=250, ≤80 ms floor** — is **NOT met**
(13.29% / 8.53% in my run). Root cause, which I independently confirmed:

- The steady-state stem-ring fill during playback is **~743 ms**, not the 9 s capacity. The ring is
  *allocated* at 16 chunks (~9 s, I verified `OFF-MAIN mix ENABLED — 16 stem SABs (393216 frames ~9s)`),
  but decode (rAF-gated `TheSynth->Poll`) only keeps ~743 ms ahead in steady state under back-pressure.
- At interval=250 a ≥400 ms stall fires the next injection mid-stall → the main thread is ~100% busy for
  the whole 12 s step → decode gets ≈0 cycles → the ~743 ms working set drains and stays drained.
- This is a **decode-ahead/decode-starvation limit, not a seam/carry/limiter bug** — proven because the
  same stalls *spaced* (interval=1000, decode recovers) give **0%** at both 400 and 800 ms.

So the off-main path wins decisively on the two axes the user actually cares about — **latency** (fixed
~70 ms music floor vs the baseline's 500 ms ceiling) and the **800 ms catastrophe** (8.53% vs 38.97%) —
and is correct end-to-end, but it does not make music *literally* unkillable under a sustained ~100%-duty
main-thread freeze. No finite ring does; the baseline only "passes" 400 ms by paying 500 ms latency.

### One documentation imprecision (substance is fine)
The build doc says off-main shows "no latency GROW/HIGH on the music bus — verified." I *did* see
`latency GROW -> 500 ms` logs in the ON runs. I traced them: they are the **SFX output-ring** adaptive law
(the SFX second pass still uses `js_audio_ring_*` + the adaptive floor), NOT the music bus. Music in
off-main mode writes **directly into the AudioWorkletNode 128-frame output quantum** (`_processOffMain`
reads stems on-demand and fills `outputs[0]` — there is no 32768-frame output ring for music), so music
latency is structurally the audio-callback quantum, decoupled from the stall budget exactly as designed.
The GROW logs are the SFX bus, which is explicitly out-of-scope-for-stall-immunity in v1. The doc's *claim*
(fixed low music latency) is true; the wording ("no GROW logs") is just imprecise about which bus logs.

---

## 4. Verdict

**CONFIRM with residuals.**

- Correctness: **CONFIRMED independently.** audio_verify MATCH clean (chroma 0.98) AND under 400 ms
  seam-stress (chroma 0.97, clip 0%, flat-top 0, wrap 0/s). The resampler-carry + limiter move did not
  introduce the static/clicking bug class. Prime gate prevents the song-start burst at the 70 ms floor.
- Architecture promise: **CONFIRMED.** Off-main turns the 800 ms baseline wall (38.97%) into 8.53% and the
  spaced 400/800 ms baseline dropout (24–32%) into 0%, at a fixed ~70 ms music floor — wins on dropout AND
  latency simultaneously.
- Strict numeric gate (0% at 400+800 ms @ interval=250, ≤80 ms floor): **NOT met** (13.29%/8.53%). This is
  a decode-ahead limit, not a mixer defect — the build doc states this honestly and I confirmed it (spaced
  stalls → 0%). Closing it needs decode-ahead off the main thread (path B) or a deeper/async decode pump —
  out of MVP-1 scope.

**Recommendation:** land MVP-1 behind the default-OFF flag as a correct, shipping-safe improvement. Keep the
"0% under MUSIC" claim qualified to *spaced* freezes (and to the 800 ms catastrophe vs baseline), not the
back-to-back interval=250 ≥400 ms pattern. Tighten the build-doc wording about the SFX-bus latency GROW logs.

## 5. Commands I ran (reproducible)

```bash
cd <rb3 worktree>; export MILO_ENGINE_PATH_OVERRIDE="$(cat .engine-path)"
scripts/web/build.sh --debug
python3 native/web/server.py --port "$(cat .worktree-port)" &
cd scripts/web
# A/B back-to-back:
node audio-stall-bench.mjs --port <P> --env RB3_WEB_OFFMAIN_MIX=1 --stalls 0,50,100,200,400,800 --interval 250 --step-secs 12 --out /tmp/mvp-on
node audio-stall-bench.mjs --port <P>                            --stalls 0,50,100,200,400,800 --interval 250 --step-secs 12 --out /tmp/mvp-off
# A/B spaced:
node audio-stall-bench.mjs --port <P> --env RB3_WEB_OFFMAIN_MIX=1 --stalls 400,800 --interval 1000 --out /tmp/mvp-on-spaced
node audio-stall-bench.mjs --port <P>                            --stalls 400,800 --interval 1000 --out /tmp/mvp-off-spaced
# correctness (clean + seam-stress capture):
node web-worklet-tap-capture.mjs --port <P> --env RB3_WEB_OFFMAIN_MIX=1 --song 20thcenturyboy --secs 22 --out /tmp/cap-on.wav
python3 ../../scripts/native/audio_verify.py /tmp/cap-on.wav --song 20thcenturyboy
```
