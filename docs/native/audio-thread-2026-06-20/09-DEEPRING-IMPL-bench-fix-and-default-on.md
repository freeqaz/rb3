# DEEPRING IMPL — bench measurement fix + off-main default-ON (the "743 ms" was an artifact, the ring is ~7-8 s)

**Date:** 2026-06-21
**Worktree:** rb3 `wt-deepring-benchfix` (base `be594213`), engine `wt-deepring-benchfix` (base `805e701`).
**Builds on:** `08-DIAGNOSIS-stem-ring-fill-depth.md` (the diagnosis — premise was wrong; the off-main stem rings already fill to ~7-8 s; "743 ms" is a stall-bench `minRing` clamp to the 32768-frame OUTPUT ring).
**Result:** **No producer cap to lift** (confirmed empirically — the base already decodes the entire 16-chunk ring ahead, `availFrames` already reaches the data frontier `mRingWritePos`). The actionable fix is (1) correct the bench measurement so it reports the TRUE stem-ring depth, and (2) flip `RB3_WEB_OFFMAIN_MIX` **default-ON** (`=0` opt-out). Both done, built, benchmarked, audio-verified.

---

## TL;DR for the next agent

- The off-main stem ring is **~7.2 s deep in steady play** (bench `minRing` now reports it correctly), **~6 s** even under genuinely-spaced 400/800 ms freezes, and **0 % dropout** there. The "743 ms fill" everyone quoted was the bench seeding its window-min with `bufFrames` (the 32768-frame OUTPUT ring) → `32768/44100 = 743 ms` clamp, never the stem ring.
- There is **no decode-ahead cap to raise**: `mRingWrittenSpace == mRingSize` (full 16-chunk ring) in steady play (`freeMs ≈ 0`), and the published `availFrames` already equals `(mRingWritePos − mAudioReadPos) mod ring` — i.e. it already reaches the true data frontier. The diagnosis's "optional micro-win" (publish up to `mRingWritePos`) is a **no-op** — see §3.
- **audio_verify: MATCH** (chroma 0.98, fp BER 0.14, 1.003x, 0.00 % clip). Output music floor **fixed 70 ms** (unchanged). Memory **unchanged** (16 stem SABs ~786 KB each ~12.6 MB; `mBuffer[0xC0000]` is a static array — deepening is decode work, not new alloc).
- The **400/800 ms @ interval-250 back-to-back** residual is **path B** (decode off the main thread), out of scope here. No finite ring survives a sustained ~100 %-duty 12 s freeze train; the rAF-gated `TheSynth->Poll` decode pump gets ≈0 cycles and the deep ring bleeds out. Documented honestly; needs a decode worker + threading flags (not path A / MVP-1).

---

## 1. What changed

### (a) Bench measurement fix — `scripts/web/audio-stall-bench.mjs` (rb3, script only — no rebuild)

The window `minRing` was seeded `let minDepth = snap.bufFrames || 0;` (= the 32768-frame output ring) and then min'd over the raw stem samples. Every raw stem `minDepth` is ~320k-360k frames ≫ 32768, so the min was **always** pinned at `bufFrames → 743 ms`, regardless of true stem depth. Fixed: seed `minDepth` from the **first observed raw sample** (no `bufFrames` floor) so the column reports the true stem-ring fill. Also corrected the VERDICT text: the stall budget is the STEM-ring fill, not the output ring (which only bounds output-quantum latency).

Validated against the diagnosis's already-captured `curve.json` (same data, old vs new logic):

| stall | OLD minRing (seed=bufFrames) | NEW minRing (true stem) | underrun% |
|---|---|---|---|
| 0   | 743 ms (32768 clamp) | **7253 ms** (319872 fr) | 0.00 |
| 200 | 743 ms (32768 clamp) | **4058 ms** (178944 fr) | 0.00 |
| 400 | 29 ms (1280 fr)      | 29 ms (genuinely drained) | 13.55 |
| 800 | 180 ms (7936 fr)     | 180 ms (genuinely drained) | 7.32 |

(At 400/800 the ring really did drain below 32768, so old == new there — the clamp only ever masked the deep steady-state.)

### (b) Off-main default-ON — engine + rb3 (both env-gated, opt-out `RB3_WEB_OFFMAIN_MIX=0`)

- engine `src/audio/AudioDevice_Web.cpp`: `sOffMainMix = (omEnv && omEnv[0] && omEnv[0] != '0');` → `sOffMainMix = !(omEnv && omEnv[0] == '0');` (worklet-side off-main mix now default-ON).
- rb3 `src/system/synth/StreamReceiver.cpp` (inside the existing `#ifdef HX_NATIVE` block — **match-neutral**, Wii `#else` path byte-identical): the 16-chunk (~9 s) deep ring is now default-ON too (`if (!(om && om[0]=='0') && chunks<kMaxChunks)`). The two flags must agree or the worklet would mix off-main against a shallow ring; both now default-ON.

These are the ONLY product changes. No output ring/floor change, no new alloc, no new threading/emcc flags.

---

## 2. Bench curves (this build, flag ON) — `minRing` now = TRUE stem depth

```
interval=250 (BACK-TO-BACK, ~100%-duty for stall>=250)
  stall=   0ms  underrun=  0.00%  minRing(stem)= 7209.8ms      <- steady ring depth
  stall= 200ms  underrun=  0.00%  minRing(stem)= 3834.2ms
  stall= 400ms  underrun= 15.61%  minRing(stem)=  423.8ms      <- PATH B (decode starved)
  stall= 800ms  underrun=  0.06%  minRing(stem)= 1253.9ms      (run-to-run phase variance)

interval=1000 (800ms here is still ~80%-duty -> only 200ms/cycle for the pump)
  stall=   0ms  underrun=  0.00%  minRing(stem)= 7276.6ms
  stall= 400ms  underrun=  0.00%  minRing(stem)= 4969.1ms      <- single 400ms freeze: 0% drop, ~5s left
  stall= 800ms  underrun=  3.21%  minRing(stem)=   95.8ms      (80%-duty still starves decode)

interval=2000 (GENUINELY spaced -> ~40%-duty for 800ms)
  stall= 400ms  underrun=  0.00%  minRing(stem)= 5990.7ms      <- 0% drop, ~6s cushion left
  stall= 800ms  underrun=  0.00%  minRing(stem)= 3337.9ms      <- 0% drop, ~3.3s cushion left
```

**Reading:** the deep ring is real (~7.2 s steady; ~6 s low-water even under spaced 800 ms freezes) and it rides single/spaced freezes at **0 % dropout**. The ONLY dropouts are at near-100 %-duty back-to-back trains (interval ≤ stall), which starve the rAF-gated DECODE pump — the path-B problem, not a shallow ring. The 400 ms@interval-250 underrun is accompanied by the existing adaptive output `latency GROW -> 500 ms` log: the ring drained and the output stretched to absorb it (this is the prior behaviour, not introduced here).

### vs the MVP-1 743 ms baseline

The MVP-1 "743 ms ring depth" was the artifact — the ring was already ~7-8 s. So the before/after on the **measurement** is: 743 ms (false) → **~7.2 s (true)**. The before/after on the **product** is: off-main flag OFF-by-default → ON-by-default (the deep ring + worklet mix that the docs already validated as correct, now the default).

---

## 3. Why the diagnosis's "optional micro-win" (publish up to `mRingWritePos`) is a NO-OP

The diagnosis suggested publishing availability up to `mRingWritePos` (~8.9 s) instead of `mRingWrittenSpace − consumed` (~7.1 s) to expose ~1.8 s more cushion. **This is a misread — the ~1.8 s is BEHIND the play cursor, not ahead.** Proven from the instrumented producer log (`/tmp/deepring-depth.log`, raw frames):

```
availFrames=317056  writeFrame=172032  readFrame=248192   ringFrames=393216
  (writeFrame - readFrame) mod ringFrames = (172032 - 248192 + 393216) = 317056  == availFrames
```

So `availFrames == (mRingWritePos − mAudioReadPos) mod ring` — the publish **already** reaches the data frontier `mRingWritePos`. The ~1.8 s gap to `mRingWrittenSpace` (8.9 s) is the `consumed` region from `mRingReadPos` (free frontier) up to `mAudioReadPos` (play cursor) — frames **already played**, freed in whole-0xC000 chunks one tick behind. Exposing them would replay consumed audio, not deepen the ahead-cushion. **There is no additional decode-ahead to publish** (`mRingWrittenSpace` is already pinned at `mRingSize`). Hence not implemented.

---

## 4. Verification

- **audio_verify** (`scripts/native/audio_verify.py` on a 22 s worklet-tap capture, flag ON, `20thcenturyboy`): **VERDICT MATCH** — chroma 0.98 (16 s overlap), fp BER 0.14, speed 1.003x (conf 0.89), pitch 1.000x, clip 0.00 %, crest 13.1 dB. Right song, played correctly, no new seam/static.
- **Output latency floor:** `sOffMainFloorMs = 70` (3087 ctx frames) — unchanged, fixed, ≤ 80 ms music floor preserved. My changes deepen only the UPSTREAM stem ring, never the output quantum.
- **Memory:** 16 stem SABs × (0xC0000/2 int16 = 393216 frames = 786 KB) ≈ 12.6 MB — unchanged. `StreamReceiver::mBuffer[0xC0000]` is a static array; filling it deeper is decode work, no new alloc. Confirmed no heap growth.
- **Song-start prime gate:** unchanged (prime 5292 mix frames / ~120 ms; worklet keys on min-across-stems availability). 0 song-start under-runs observed in captures.
- **Match-neutrality:** rb3 `StreamReceiver.cpp` edit is entirely inside `#ifdef HX_NATIVE`; the Wii `#else` (`mBuffer[0x18000]`, `mRingSize=0x18000`) is byte-identical. Engine `AudioDevice_Web.cpp` is `__EMSCRIPTEN__`/web-only. Bench `.mjs` is tooling.

---

## 5. Default-ON recommendation: **YES, flip it (done here)**

- **Correct:** audio_verify MATCH, 0 % clip, right pitch/speed.
- **Deep:** stem ring rides ~7.2 s steady, ~6 s under spaced 800 ms freezes, 0 % dropout on single/spaced stalls.
- **Latency:** fixed 70 ms output music floor (the deepening is upstream-only).
- **Memory:** unchanged (~12.6 MB, no new alloc).
- **Opt-out kept:** `RB3_WEB_OFFMAIN_MIX=0` falls back to the prior main-thread-mix + 8-chunk ring (same env name, now `=0`-to-disable, mirroring the `RB3_*_OFF` opt-out convention).

**Honest caveat (does NOT block default-ON):** the 400/800 ms @ interval-250 **back-to-back** (~100 %-duty) residual still drops audio because the rAF-gated decode pump is starved — this is **path B** (decode off the main thread: a SAB-producer / decode worker + threading flags), explicitly out of MVP-1 / path-A scope and forbidden in this task. The prior shipping path only "passes" 400 ms by riding the output latency to a 500 ms ceiling the user rejected; off-main is strictly better everywhere except the sustained-duty train, where both need path B. This is a documented limitation, not a regression.

---

## 6. Files touched

- rb3 `wt-deepring-benchfix`:
  - `scripts/web/audio-stall-bench.mjs` — seed `minRing` from real stem samples (drop the `bufFrames` clamp); corrected VERDICT text. **Tooling.**
  - `src/system/synth/StreamReceiver.cpp` — deep 16-chunk ring default-ON (`HX_NATIVE`-only; Wii byte-identical). **Match-neutral.**
- engine `wt-deepring-benchfix`:
  - `src/audio/AudioDevice_Web.cpp` — `sOffMainMix` default-ON (`=0` opt-out). **Web-only.**

## 7. Repro

```bash
# worktree wt-deepring-benchfix, paired engine:
export MILO_ENGINE_PATH="$(cat .engine-path)"; export EMSDK=/home/free/emsdk
scripts/web/build.sh --debug
python3 native/web/server.py --port "$(cat .worktree-port)" &   # 8778

# bench (minRing now reports TRUE stem depth, no bufFrames clamp):
node scripts/web/audio-stall-bench.mjs --port 8778 --env RB3_WEB_OFFMAIN_MIX=1 \
     --stalls 0,200,400,800 --step-secs 12 --interval 250  --out /tmp/deepring-bench-on-250
node scripts/web/audio-stall-bench.mjs --port 8778 --env RB3_WEB_OFFMAIN_MIX=1 \
     --stalls 400,800        --step-secs 12 --interval 2000 --out /tmp/deepring-bench-on-spaced2k

# audio identity (worklet-tap = the off-main music output):
node scripts/web/web-worklet-tap-capture.mjs --port 8778 --env RB3_WEB_OFFMAIN_MIX=1 \
     --song 20thcenturyboy --secs 22 --out /tmp/deepring_offmain_cap.wav
python3 scripts/native/audio_verify.py /tmp/deepring_offmain_cap.wav --song 20thcenturyboy
#   -> VERDICT MATCH (chroma 0.98, 1.003x, 0% clip)
```
