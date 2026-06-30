# Hypothesis 2 — draw-state capture / render caches — RULED OUT

Worktree: `rb3/.claude/worktrees/memleak-rendercap` (branch `wt-memleak-rendercap`)
Engine worktree: `milo-native-engine-worktrees/memleak-rendercap` (branch
`wt-memleak-rendercap`, off pin a360e3c; **MILO_ENGINE_PIN NOT bumped, NOT pushed**)

## Hypothesis tested

Does the bloom/halo capture-and-replay, the mesh GPU cache, the per-instance
UniformSlot vectors, the skinned-mesh collection cache, or any per-draw
bind-group/pipeline state grow each frame and account for the in-song RSS leak?

## Verdict: RULED-OUT

The render-path caches are **bounded and plateauing**; the leak (~85–100 KB/s,
~10 MB over the 113 s window) climbs at the SAME rate with the bloom composite
disabled. Render contributes ≤ ~0.3 MB (bounded), not the ~10 MB monotonic leak.

## Evidence

### 1. Direct cache-size instrumentation (new probe)

Added `RB3_RENDERCAP_PROBE=N` to `BandRnd::EndFrame()` (engine
`src/platform/Rnd_Wgpu_RB3.cpp`, native-only diagnostic, gated, zero cost by
default). Every N render frames it dumps the SIZE of every render container that
could leak. Curve over a full 113 s gameplay run (`rcap2`, frames 100→16500, the
entire window the RSS leak was climbing):

| frame | meshCacheEntries | totalSlots | tex | geomSyncGen | haloDraws(cap) | bufCreates | bgCreates |
|------:|-----------------:|-----------:|----:|------------:|:--------------:|-----------:|----------:|
| 100   | 363 | 1252 | 1270 | 4100 | 0(0)  | 0 | 0 |
| 2900  | 279 | 1183 | 1001 | 3633 | 0(0)  | 0 | 0 |
| 5900  | 328 | 1292 | 1001 | 3638 | 0(16) | 0 | 0 |
| 8900  | 338 | 1305 | 1001 | 3640 | 0(16) | 0 | 0 |
| 11900 | 357 | 1335 | 1001 | 3640 | 0(16) | 0 | 0 |
| 16500 | 365 | 1358 | 1001 | 3640 | 0(16) | 0 | 0 |

Over the whole run (165 steady-state samples):
- `meshCacheEntries`: **+2** (363→365) — flat.
- `tex` (sTexGpu): **−269** (1270→1001) — NET DECREASING (entries freed).
- `geomSyncGen`: **−460** (4100→3640) — NET DECREASING.
- `totalSlots`: **+106** (1252→1358) — bounded by max-instances-per-frame,
  recycled across frames (the slot vector grows only when a frame's instance
  count exceeds any prior frame's). ~298 KB of GPU buffers TOTAL, mostly in the
  first ~20 s; second half added only +55 slots, then plateaus.
- `bufCreates`/`bgCreates` (per-frame GPU resource creation): **0 in 155/165 and
  163/165 samples**; the handful of nonzero hits (max 6) are when new distinct
  meshes first enter the scene — transient, not per-frame. **At steady state,
  ZERO per-frame GPU resource creation.**
- `haloDraws`: stayed **0** the whole run (cap=16). The bloom capture-and-replay
  never even fired on this gameplay path in headless — it cannot be the leak.

Render path's total contribution: ≤ ~0.3 MB, bounded/plateauing, vs a measured
~10 MB monotonic RSS leak over the same window.

### 2. Subsystem A/B (bloom composite disabled)

`RB3_HIGHWAY_BLOOM_BLEND=0` short-circuits `CompositeHaloBloom()` to a TRUE no-op
(no replay pass, no BloomPass, no composite, no per-frame `CreateBindGroup`).

| arm                         | RSS start→end (MB) | window (s) | slope (KB/s) |
|-----------------------------|--------------------|-----------:|-------------:|
| baseline (rcap2)            | 447.1 → 457.0      | 113        | ~88          |
| **bloom no-op** (BLEND=0)   | 446.1 → 454.5      | 101        | ~85          |
| SFX_OGG_OFF (raw-PCM arm)\* | 444.2 → 454.5      | 101        | ~102         |

Disabling the bloom composite did **not** flatten the slope. \*`RB3_SFX_OGG_OFF`
is NOT a clean "disable the leak" control — it only swaps the ogg path for the
legacy raw-PCM path, which still `malloc`s a per-trigger PCM buffer in the
un-reaped SfxInst (the actual root cause per PROFILE_FINDINGS.md), so it leaks too.

### 3. Measurement-sensitivity control (proves the null result is real)

Held the engine on the splash/menu screen (no gameplay SFX firing) for 80 s while
sampling RSS. RSS 417.7 → 423.1 MB but **plateaus** in the second half
(422.1→423.1 over the last 40 s ≈ 25 KB/s, decaying — one-time asset settle, not a
leak). The render path renders every frame here too. → the apparatus correctly
detects flat/plateauing curves; the sustained ~85–100 KB/s climb is
gameplay/SFX-tied, not background drift; the bloom-off null result is valid.

## Static confirmation (why the caches can't leak per-frame)

- `mHaloDraws` — `clear()`ed every BeginFrame and every CompositeHaloBloom exit;
  `push_back` only inside the frame with a `reserve(16)`. Bounded per-frame.
- `sMeshGpu` (`unordered_map<RndMesh*, RB3MeshEntry>`) — keyed by mesh pointer,
  erased in `CleanupGpuMesh` (RndMesh dtor). Grows with distinct meshes, not time.
- `RB3MeshEntry::slots` — per-frame `nextSlot` reset via `frameSeen != sFrameSeq`;
  vector grows only to the mesh's max-instances-per-frame, then recycles. The
  per-slot uniform buffer + bind group are created once (`if (!slot.objUB)`) and
  reused — this code REPLACED an older per-draw ring-offset bind group that DID
  accumulate under web backpressure (that earlier leak is already fixed).
- `mNativeSkinnedMeshCache` (BandCharacter) — `clear()`ed on every
  StartLoad/SyncObjects invalidate, rebuilt; bounded by char's skinned-mesh count.
- `mQuadPipelines`/`mPartPipelines`/`PipelineManager` — keyed by pipeline config
  hash; bounded by distinct pipeline variants (stayed 2–4 the whole run).

## Reproduce

```bash
# build (worktree)
cd rb3/.claude/worktrees/memleak-rendercap
cmake --build native/build-native --target rb3-native -j   # ~5s incremental

# render-cache curve over 120s gameplay
RB3_RENDERCAP_PROBE=100 python3 \
  docs/native/frame-degradation-2026-06-21/profile_degradation.py \
  --bin .../memleak-rendercap/native/build-native/rb3-native \
  --seconds 120 --interval 8 --label rcap2 --out /tmp/rb3-rcap2
# probe lines go to the per-run log; strip \r first:
tr -d '\r' < /tmp/rb3-degrade-rcap2-*.log | grep RENDERCAP_PROBE

# bloom-off A/B
... --extra-env RB3_HIGHWAY_BLOOM_BLEND=0 --label bloomoff
```

## Code change (diagnostic only, native-only, NOT for landing as-is)

`milo-native-engine .../src/platform/Rnd_Wgpu_RB3.cpp` — added an
`RB3_RENDERCAP_PROBE`-gated cache-size dump in `EndFrame()`. Gated, off by
default, native-only. Harmless to keep for future investigation; not required for
the fix.

## Pointer to the real source

Leak source is hypothesis 1 (kXMA SFX sidecar / un-reaped SfxInst), already
attributed in `PROFILE_FINDINGS.md`. This rules the render/draw-state-capture
path OUT as a contributor.
