# DIAGNOSIS — the off-main stem-ring "743 ms fill" is a MEASUREMENT ARTIFACT; the ring already fills to ~7–8 s

**Date:** 2026-06-21
**Task:** Pin the binding constraint that pins the off-main stem-ring fill at ~743 ms and lift it (decode-ahead).
**Worktree:** rb3 `wt-deepring-diag1` (base `be594213` / `cde73588` = MVP-1 off-main, byte-identical to the reviewer's `fcc950cf`), engine `wt-deepring-diag1` (base `805e701` = MVP-1 off-main).
**Verdict:** **The premise is wrong.** The stem rings do NOT fill to only 743 ms in steady play — they fill to **~7.3–8.2 s** (measured directly, two independent ways). The "~743 ms" the reviewer/build-doc quoted is the **stall-bench `minRing` column, which is hard-clamped to `bufFrames` = the 32768-frame OUTPUT/SFX ring (= 32768/44100 = 743 ms)** and therefore can never report more than 743 ms regardless of how deep the stem ring actually is. There is no 743 ms cap to lift. The remaining 400/800 ms@interval-250 dropout is **path-B decode starvation** (rAF-gated `TheSynth->Poll` gets ≈0 cycles during a sustained ~100 %-duty main-thread freeze train), not a fill-depth cap.

---

## 1. What I measured (empirical, flag ON, real `20thcenturyboy` web playback)

### 1a. Direct producer-side fill depth — instrumented `PumpAudioOffMainStems`

Added `RB3_OFFMAIN_DEPTH_LOG=1` instrumentation that logs, per stem, the published
`availFrames` plus the producer back-pressure state (`mRingWrittenSpace`,
`mRingFreeSpace`, `mRingSize`) every ~1 s during steady play.
(`AudioDevice.h` `OffMainStemState` diag fields + `rb3_stream_receiver_native.cpp`
`OffMainSnapshot` populate them; `AudioDevice_Web.cpp::PumpAudioOffMainStems` logs.)

**Steady-state (35 s clean play, kPlaying samples, dropping the prime ramp):**

| stem | availMs (SAB fill) | writtenMs (`mRingWrittenSpace`) | freeMs (`mRingFreeSpace`) | ringMs (capacity) |
|---|---|---|---|---|
| 0 | min 6531 / **med 7140** / max 7741 | **med 8917** | **med 0** | 8917 |
| 1 | min 6551 / **med 7161** / max 7750 | med 8917 | med 0 | 8917 |
| 4 | min 6551 / **med 7152** / max 7750 | med 8917 | med 0 | 8917 |

- `mRingWrittenSpace` (`writtenMs`) sits at **8917 ms = the FULL 16-chunk ring** (`freeMs ≈ 0`). The producer decodes the ENTIRE ring ahead of the free frontier — decode-ahead is already maxed.
- The published `availFrames` (`availMs`, what the worklet plays from) is **~7.1 s median**; the ~1.8 s gap below `writtenMs` is the `consumed` region (frames already played by the worklet but not yet freed because their chunk hasn't been fully drained — back-pressure refunds in whole 0xC000 chunks).
- **There is no 743 ms anywhere in the producer state.**

### 1b. Consumer-side confirmation — the worklet's own `minAvail`

`scripts/web/audio-stall-bench.mjs` records the worklet's raw `minRingDepthFrames`
(= `minAvail = (writePos − readPos) mod ringFrames`, the actual SAB depth the mix
reads — `audio-worklet.js:209,311`). The bench's per-window raw samples (`curve.json`
`windows[0].urSamples`) for the **stall=0 ms** step:

```
minDepth frames: 319872, 320896, 322304, … up to 359552
              => 7253 ms … 8153 ms   (≈ 7.3–8.2 s)
```

The worklet independently sees the stem ring **~7.3–8.2 s deep**, matching the producer-side number. Two independent measurements agree: **the stem ring fills to ~7–8 s, not 743 ms.**

---

## 2. Where the "743 ms" actually comes from — the bench `minRing` clamp

`scripts/web/audio-stall-bench.mjs` computes the reported `minRing` like this:

```js
let minDepth = snap.bufFrames || 0;                     // line ~397  <-- SEED = OUTPUT ring (32768)
for (const u of us) if (u.minDepth > 0 && u.minDepth < minDepth) minDepth = u.minDepth;  // line ~402
const minDepthMs = ctxRate ? +(minDepth / ctxRate * 1000) : 0;                            // line ~407
```

`snap.bufFrames` is `window._rb3Audio.bufFrames` = the **output SAB ring = 32768 frames**
(`AudioDevice_Web.cpp` init: `ring 32768 frames`). Every raw stem `minDepth` sample is
**~320k–360k frames ≫ 32768**, so the `min(...)` is ALWAYS pinned at `bufFrames` = 32768
→ `32768/44100 = 743 ms`. **The `minRing` column is structurally incapable of exceeding
743 ms — it is the output-ring size, not the stem-ring fill.** `743 ms = 32768/44100` is
not a coincidence with the "old adaptive target"; it literally IS the 32768-frame ring.

This is the single bug behind the whole "stem rings only fill to 743 ms" framing: the
build doc + independent review both read the bench's clamped `minRing=743ms` column and
concluded the stem ring was shallow. It is not.

### Reproduced bench curve (my run, flag ON, identical to the reviewer's)

```
 stall(ms) | underrun% | minRing(ms) | minRingFrames(raw) | rafMax  | note
     0     |   0.00    |    743      |  32768 (CLAMPED)   |   33.3  | raw stem depth 7.3–8.2 s
   200     |   0.00    |    743      |  32768 (CLAMPED)   |  216.7  | raw stem depth ~7 s
   400     |  13.55    |     29      |   1280             |  433.3  | ring GENUINELY drained (decode-starved)
   800     |   7.32    |    180      |   7936             |  816.6  | ring partly drained
```

At 0/200 ms the `minRingFrames` equals `bufFrames` exactly (clamp). At 400 ms it falls to
**1280 frames (29 ms)** — proof the ring DID drain there, but only because the
back-to-back 400 ms train (>250 ms interval ⇒ ~100 %-duty for the whole 12 s step)
starved the rAF decode pump for the full step and the ~7–8 s buffer bled out over 12 s.

---

## 3. The real binding constraint (with file:line)

**There is no target-depth cap, no chunk back-pressure cap, and no mogg-availability cap
holding the steady-state fill at 743 ms — because the steady-state fill is ~7–8 s, and
`mRingWrittenSpace` is already at the full 16-chunk ring (`freeMs ≈ 0`).** Walking the four
candidates the task listed:

1. **Off-main `PumpAudio` target-depth cap** — *NOT present.* `AudioDevice_Web.cpp::PumpAudioOffMainStems` (`src/audio/AudioDevice_Web.cpp:874-937`) publishes the raw `st.availFrames` with NO cap (`js_offmain_publish_stem(..., availEnd, availEnd, ...)` at line ~926). `sOffMainTargetFrames` (`js_offmain_set_target`, line ~936) is the fixed **70 ms OUTPUT floor**, applied only to the worklet's output quantum — it never bounds the stem fill.

2. **Base chunk back-pressure (`SendDoneImpl`/`mRingFreeSpace`)** — *NOT the cap.* In steady state `mRingFreeSpace = 0` and `mRingWrittenSpace = mRingSize` (full ring) — i.e. the producer decodes the ENTIRE 16-chunk ring ahead of the free frontier. The send/`SendDoneImpl` loop (`src/system/synth/StreamReceiver.cpp:135-234`, native bridge `native/src/rb3_stream_receiver_native.cpp:272-301`) frees chunks in whole-0xC000 steps behind the play cursor; that only explains the ~1.8 s gap between `writtenMs` (8917) and the published `availMs` (~7100), not a 743 ms cap.

3. **rAF decode cadence (`TheSynth->Poll`)** — *the constraint ONLY under sustained ~100 %-duty stalls (path B), and even then it is decode STARVATION, not a steady-state cap.* `StandardStream::PollStream` decodes with budget `Max(frameMs*0.2, 1.0)` ms/frame (`throttle 0.2`, `config/synth.dta`). In clean play that budget is more than enough to keep `mRingWrittenSpace` pinned at full-ring; the rings stay ~7–8 s. The 400/800 ms@interval-250 dropout is the rAF loop getting ≈0 cycles during the back-to-back freeze train (`App.cpp` `RunOneFrame` → `TheSynth->Poll` is rAF-gated), so the already-deep ring drains. Spaced stalls (interval=1000, decode recovers) = **0 %** at both 400 and 800 ms — confirming the ring is deep and the residual is purely sustained-duty decode starvation.

4. **mogg range-fetch availability** — *not the steady-state limiter* (the ring reaches full-ring depth, so bytes are present); could bite a cold seek, out of scope for the steady-state question.

**Mechanism, stated exactly:** the stem ring is NOT pinned at 743 ms. It fills to the full
16-chunk ring (`mRingWrittenSpace = mRingSize`, `freeMs ≈ 0`; published `availFrames`
~7.1 s median, raw worklet `minAvail` 7.3–8.2 s). "743 ms" is the stall-bench `minRing`
column, hard-clamped to the 32768-frame OUTPUT ring (`audio-stall-bench.mjs:~397`,
`minDepth = snap.bufFrames`), which by construction can never exceed `32768/44100 = 743 ms`.

---

## 4. Consequence for the fix

The fix the task envisioned — "raise the target depth + relax the back-pressure window so
the base decodes more chunks ahead" — is a **no-op here**, because the base ALREADY decodes
the entire 16-chunk ring ahead (`freeMs ≈ 0`, `mRingWrittenSpace = mRingSize`). There is no
shallower cap to lift. The deep ring already exists and already rides ~7–8 s in steady play;
that is why the MVP-1 already converts the spaced-stall dropout (24–32 %) and the 800 ms
catastrophe (38 % → 7–8 %) the build/review docs reported.

### What to actually do

1. **Fix the measurement (the highest-value, lowest-risk change).** Make the bench report
   the TRUE stem-ring fill. In `scripts/web/audio-stall-bench.mjs`, stop seeding `minDepth`
   with `snap.bufFrames`; seed it from the first observed raw `minDepth` sample (or report a
   separate `stemRingMs` from `windows[].urSamples` max). Re-run → the 0/200 ms `minRing`
   column will show ~7–8 s, and the curve narrative ("ring depth ~743 ms is the theoretical
   stall budget") will be corrected to ~7–8 s. This retires the false premise.

2. **Re-scope the remaining 400/800 ms@interval-250 residual as path B (decode-ahead OFF the
   main thread), exactly as the build doc already concluded.** No finite ring survives a
   sustained ~100 %-duty main-thread freeze for the whole 12 s step; the only real fix is to
   run `TheSynth->Poll` decode off the rAF/main thread (a decode worker / SharedArrayBuffer
   producer), which is out of MVP-1 (path A) scope and needs the threading flags the task
   forbids here.

3. **Optional micro-win (native-only, small):** the published `availFrames` (~7.1 s) trails
   `mRingWrittenSpace` (8.9 s) by the ~1.8 s `consumed`-but-not-yet-freed region because
   `SendDoneImpl` frees in whole 0xC000 chunks one-at-a-time gated on the monotonic play
   counter. This costs ~1.8 s of *publishable* cushion that is physically present in
   `mBuffer`. Publishing availability up to `mRingWritePos` (the true data frontier) instead
   of `mRingWrittenSpace − consumed` would expose the full ~8.9 s to the worklet. This is a
   real (if modest, ~1.8 s) deepening and is native/web-only (it only changes what
   `OffMainSnapshot` reports, not the Wii path). It does NOT close the 400 ms@interval-250
   gate (that needs path B) but it is the only genuine "more decode-ahead" available, since
   `mRingWrittenSpace` is already maxed.

### Default-ON recommendation

The off-main path is **correct** (audio_verify MATCH, chroma 0.98, 0 % clip — confirmed by
the independent review) and already delivers the deep ring (~7–8 s) + fixed ~70 ms music
floor + the spaced-stall 0 %. The only thing blocking a confident "default-ON" story was the
**false 743 ms framing**, which is a bench artifact, not a real shallow ring. With the bench
fix (item 1) the case for default-ON is clean: keep the env opt-out `RB3_WEB_OFFMAIN_MIX`
(now flip default-ON) and `RB3_VENUE_*`-style opt-out semantics; the 400/800 ms@interval-250
back-to-back residual is an honest, documented path-B limitation (sustained 100 %-duty), not
a regression vs the baseline (which only "passes" 400 ms by riding to a 500 ms latency
ceiling the user rejects).

---

## 5. Repro

```bash
# worktree wt-deepring-diag1, paired engine:
export MILO_ENGINE_PATH_OVERRIDE="$(cat .engine-path)"
scripts/web/build.sh --debug
python3 native/web/server.py --port "$(cat .worktree-port)" &     # 8975

# A) direct producer-side fill depth (instrumented):
node scripts/web/_deepring_depth_probe.mjs --port 8975 --secs 35 \
     --env 'RB3_WEB_OFFMAIN_MIX=1;RB3_OFFMAIN_DEPTH_LOG=1'
#   -> availMs med ~7100, writtenMs med ~8917 (full ring), freeMs ~0

# B) bench (shows the 743 ms clamp + the raw 7–8 s samples in curve.json windows[]):
node scripts/web/audio-stall-bench.mjs --port 8975 --env RB3_WEB_OFFMAIN_MIX=1 \
     --stalls 0,200,400,800 --step-secs 12 --interval 250 --out /tmp/deepring-bench-on
python3 - <<'PY'
import json; d=json.load(open('/tmp/deepring-bench-on/curve.json'))
us=d['windows'][0]['urSamples']; fr=[u['minDepth'] for u in us if u.get('minDepth')]
print('stall=0 raw stem minDepth: %d..%d frames = %.0f..%.0f ms'%(min(fr),max(fr),min(fr)/44.1,max(fr)/44.1))
PY
#   -> stall=0 raw stem minDepth: ~320k..360k frames = ~7253..8153 ms
```

## 6. Files touched (diagnostic instrumentation only — DIAG-gated, match-neutral)

- engine `wt-deepring-diag1`: `src/audio/AudioDevice.h` (+4 diag fields on `OffMainStemState`),
  `src/audio/AudioDevice_Web.cpp` (`RB3_OFFMAIN_DEPTH_LOG` periodic log in `PumpAudioOffMainStems`).
- rb3 `wt-deepring-diag1`: `native/src/rb3_stream_receiver_native.cpp` (populate the 4 diag
  fields in `OffMainSnapshot`); `scripts/web/_deepring_depth_probe.mjs` (NEW steady-play
  depth probe). No Wii-`.o` source touched (`StreamReceiver.cpp` unchanged here).
