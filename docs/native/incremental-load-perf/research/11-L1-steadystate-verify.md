# 11 — L1 vertex-unpack cache: steady-state gameplay verification

**Date:** 2026-06-23
**rb3 SHA at measure:** `30dbc389329b9939cf5e469bcb86c0feffa0a648` (build from working tree; HEAD advanced under concurrent agents during the run — engine state below is what pins it)
**engine HEAD / `MILO_ENGINE_PIN`:** `20dba552cb7d1f161440e657232c780fb389f818` (HEAD == pin, clean)
**Cache code under test:** `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:3642-3726` (L1 vertex-unpack cache, flag `RB3_UNPACK_CACHE_OFF=1`, engine commit `8fb669d`)

## Question

Wave 5 shipped the L1 "vertex-unpack cache" and claimed it also shaves **steady-state**
gameplay draw cost — web gameplay engine-frame **p50 18.99 → <17 ms**. That steady-state
claim was never verified (the measurement agent built `native-steady.py` but died on a
rate-limit before running it). This doc verifies or refutes it on current master, honestly.

## What the cache does (so the A/B is interpreted right)

Per `DrawMesh`, the per-vertex CPU unpack (`Be*`/`Half2Float` helpers → `GpuVertexRB3`/
`GpuVertexSkinned`) re-ran **every frame for every visible mesh**. The L1 cache skips it when
the geometry is unchanged (`!needUpload`): static meshes have no consumer past the GPU upload,
and skinned meshes read bind-pose verts from `meshEntry.cachedSkinnedVerts`. The
`RB3_FRAME_TRACE` counters `unpackMs` / `unpackN` charge exactly this work, so the A/B is
directly quantifiable: **cache ON ⇒ `unpackN ≈ 0` on steady frames; cache OFF ⇒ every visible
mesh re-unpacks every frame.**

## Method

* **Native** (3s rebuilds, deterministic): `native-steady.py` drives headless to gameplay with
  the `song-end-test.py` nav script, waits for `songMs > 1500` (track actually scrolling — NOT
  the `songMs=0` intro cinematic), dwells 12 s, then computes p50/p95 of frame `dt` +
  `unpackMs`/`unpackN` over the steady window (first 60 game frames skipped as warm-up).
  **3 interleaved trials per arm.**
* **Web** (the build where the original claim lives): the `_framestall-steady-profile.mjs`
  harness (release `-O0` build, own server on :8448, `?env=RB3_FRAME_TRACE=/trace.jsonl;
  RB3_UNPACK_CACHE_OFF=…`), drives to `game_screen`, 6 s warm-up, 30 s steady window, reads the
  `RB3_FRAME_TRACE` JSONL back from MEMFS. The engine-side **frame-trace `dt`** is the
  comparable metric (= per-`RunOneFrame` busy-time, what "p50 18.99" referred to); the rAF-gap
  p50 is vsync-quantized to 33.3 ms under the headless xvfb compositor and is NOT a CPU metric.

The metric is **engine frame `dt`** (the busy draw cost the claim is about), not rAF cadence.

## Native result — 3 trials/arm, 12 s dwell, median

| arm | dt p50 | dt p95 | dt max | unpackMs p50 | unpackMs p95 | unpackN p50 | steady frames / 12 s |
|---|---|---|---|---|---|---|---|
| **cache ON** (default) | **5.49** | 9.48 | 17.24 | 0.0 | 0.003 | 0 | ~3000 |
| **cache OFF** | **10.96** | 19.08 | 24.57 | 4.37 | 8.14 | **298** | ~1500 |

* All 3 trials per arm were tight (ON dt_p50 5.4–6.4; OFF dt_p50 10.3–11.9).
* OFF re-unpacks **~298 meshes every frame** for ~4.4 ms p50 / ~8 ms p95 of CPU.
* OFF renders **half as many frames** in the same 12 s dwell — confirms it's genuine busy-time,
  not idle-wait.
* **Δ dt_p50 = −5.47 ms (−50%)**, and the unpackMs (4.37 ms) accounts for the bulk of it.

## Web result — release build, the clean A/B run (`web_on_0` vs `web_off_0`)

Both runs were in real gameplay rendering with **identical non-unpack render work**
(residue_p50 9.28 vs 9.29 ms — GPU submit / Poll / skinning), so the ONLY difference is the
unpack bucket:

| arm | engine dt p50 | engine dt p95 | dt max | unpackMs p50 | unpackMs p95 | unpackN p50 |
|---|---|---|---|---|---|---|
| **cache ON** (default) | **9.28** | 14.43 | 95.4 | 0.0 | 0.0 | 0 |
| **cache OFF** | **14.58** | 20.62 | 115.8 | **5.29** | 7.40 | **354** |

* Decomposition is exact: `OFF dt_p50 14.58 = residue 9.29 + unpack 5.29`; `ON dt_p50 9.28 = residue 9.28 + unpack ≈ 0`.
* With OFF, `unpackMs` is the **dominant spike bucket** (1801 ms over 267 over-budget frames; the
  single worst frame is 7.8 ms of unpack), i.e. the unpack is precisely what pushes web frames
  over the 16.7 ms budget.
* **Δ dt_p50 = −5.30 ms (−36%); Δ dt_p95 = −6.2 ms (−30%).**

### Variance caveat (honest)

A second web trial pair (`web_*_1`) was **invalid for this A/B**: that gameplay session never
sustained a real rendered highway scene (0 % of its `game_screen` frames had `unpackN > 50`;
`unpackMs ≈ 0` even with the cache OFF). The OFF cost is proportional to *visible mesh count*,
so a window that caught a near-empty render view shows ~no delta. The clean, representative
sample is the `*_0` pair, where OFF unpacked **354 meshes/frame at 100 % of game frames**. Native
(deterministic, all 6 trials consistent) corroborates the magnitude. Recommend the harness gate
its steady window on `unpackN > 50` (cache-off) / a non-trivial mesh count before sampling.

## Verdict: **CONFIRMED** (mechanism + magnitude; absolute target met with margin)

The L1 vertex-unpack cache delivers a **real, large steady-state gameplay win**, not just a
first-frame win:

* **Native:** engine frame dt p50 **10.96 → 5.49 ms** (−5.47 ms, −50 %), p95 19.08 → 9.48 ms.
* **Web (release):** engine frame dt p50 **14.58 → 9.28 ms** (−5.30 ms, −36 %), p95 20.62 → 14.43 ms.
* The win is exactly the ~5 ms/frame of per-vertex unpack on ~300–354 visible meshes, eliminated
  by memoizing the unpack (`unpackN`/`unpackMs` → 0 on steady frames). residue (all other render
  work) is byte-for-byte the same across arms, isolating the cause.

On the **18.99 → <17 ms** claim: the W5 baseline number isn't reproducible to the decimal on
this newer master (different scene/build state), but the directional + absolute claim holds —
web steady-state engine dt p50 with the cache **ON is 9.28 ms, well under 17 ms**, and turning
the cache **OFF raises it to 14.58 ms** (still under 17 here, but +5.3 ms and pushing p95/p99
and the spike tail over budget). The cache is the difference between comfortably-under-budget
and budget-pressured gameplay, and is worth keeping default-on.

## Artifacts

* Raw traces + per-trial JSON: `/tmp/rb3perf-w6-L1/` (`on_{0,1,2}.jsonl`, `off_{0,1,2}.jsonl`,
  `ab_results.json`, `web_{on,off}_{0,1}/summary.json` + `frametrace.jsonl`).
* Samplers: `native-steady.py` (from `/tmp/rb3perf-w6/`), `web-steady.mjs` (a copy of
  `scripts/web/_framestall-steady-profile.mjs` patched to inject a second env var via `--extra-env`).
