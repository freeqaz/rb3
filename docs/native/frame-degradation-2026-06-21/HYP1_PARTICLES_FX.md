# Hypothesis 1 — PARTICLES / FX as the leak source

**Verdict: RULED-OUT.** The particle / FX render path is NOT a (or the) source
of the in-song RSS leak. Bounding it entirely left the leak slope unchanged.

Worktree: `rb3/.claude/worktrees/memleak-particles-fx` (branch `wt-memleak-particles-fx`, from `c6b6955f`)
Engine worktree: `milo-native-engine-worktrees/memleak-particles-fx` (branch `wt-memleak-particles-fx`, base `a360e3c`)
Build: clang, `native/build-native`, `-DMILO_ENGINE_PATH=<engine wt>` `-DDawn_DIR=.../dawn/lib/cmake/Dawn`

## Method

Reused `profile_degradation.py` (dt+RSS sampler that drives to gameplay via
`keyboard-to-gameplay.py`, holds, samples every 5 s). 120 s gameplay window per
arm; RSS slope = least-squares fit over t≥10 s (skip warmup). Same song/diff/seed
across arms.

## A/B arms (RSS leak slope)

| arm | env | RSS slope | total over ~107 s |
|---|---|---:|---:|
| **baseline** | — | **135.3 KB/s** | 14.4 MB |
| bloomoff | `RB3_HIGHWAY_BLOOM_OFF=1 RB3_BLOOM_OFF=1` | 116.1 KB/s | 10.0 MB |
| **partdrawoff** | `RB3_PART_DRAW_OFF=1` | **135.5 KB/s** | 13.8 MB |
| nosfx (control) | `RB3_NO_SFX=1` | 141.9 KB/s | 13.9 MB |

dt was noisy (5–13 ms, no monotonic trend) in every arm — consistent with the
prior profile's "it's a LEAK, not CPU-growth" finding. Native never collapsed to
2–3 fps (held 75–230 fps); the catastrophic symptom is web-amplified.

## What `RB3_PART_DRAW_OFF` does (probe added this round)

Engine `BandRnd::DrawParticles` (`src/platform/Rnd_Wgpu_RB3.cpp:6077`) early-returns
when `RB3_PART_DRAW_OFF` is set, **bounding the entire particle FX render path**:
the per-particle vertex build, the per-call `wgpu::CreateBindGroup`, the pipeline
lookup, and the draw. This is the only per-frame heap/GPU work in the FX path.

`DrawParticlesBillboard(RndParticleSys*)` (the matched-fork `RndParticleSys::DrawShowing`
hook, HX_NATIVE) forwards to this, so the gate covers all RB3 particle drawing.

The probe is **uncommitted** in the engine worktree (a diagnostic, not a fix).
Drop it or keep it gated if useful; it is harmless (off by default).

## Why particles can't be the leak (code reading that the A/B confirmed)

1. **Particle simulation is a bounded fixed-size pool.** `Part.cpp`
   `InitParticleSystem` allocates `gParticlePool->mPoolParticles = new RndFancyParticle[size]`
   **once**, threaded as a free-list. `AllocParticle` pops from `mPoolFreeParticles`,
   `FreeParticle` pushes back; `mNumActiveParticles` is capped by pool size with a
   `mHighWaterMark`. Per-system `mPersistentParticles = new RndParticle[i1]` is a
   one-time per-system allocation. Nothing here grows per frame or per emit — it
   recycles. (`src/system/rndobj/Part.cpp:191–243`.)
2. **The draw builds into a `static std::vector` reused across calls** (`sVerts.clear()`
   each frame — capacity retained, not freed, not growing) and grows the dynamic
   VB/IB grow-to-fit (capacity-tracked, bounded). (`Rnd_Wgpu_RB3.cpp:6145+`.)
3. **Pipelines are cached** in `mPartPipelines` keyed by `(fmt,blend,hasDepth)` — a
   tiny bounded set, NOT created per frame. The only per-frame GPU object is the
   `texBG` bind group, which is Dawn-refcounted and reclaimed when the local goes
   out of scope after the pass — it is not retained in any container.

## Bloom / halo capture-replay also ruled out (secondary)

`mHaloDraws` (the highway-halo capture-and-replay list) is `clear()`ed at the top
of **every** frame (`Rnd_Wgpu_RB3.cpp:1474–1478`), so it is bounded per-frame, not
accumulating. The `bloomoff` arm (both highway-halo and full-frame bloom off) leaked
at the same rate → confirmed not the source.

## Particles ARE exercised (so the A/B had a real signal to flatten)

`PART_PROBE=1` showed **1046** `DrawParticles` invocations in a 25 s gameplay window
across many active systems (`background_red_fog`, `cavern_smoke01`, `crowd_smoke_a/b`,
`broken_glass_squares`, `clouds`, etc.). Fully disabling all of that draw work
changed the leak slope by **0.2 KB/s** (135.3 → 135.5) — i.e. nothing. This is not a
"the path was never active" false negative.

## Cross-cutting note for the orchestrator (important)

**`RB3_NO_SFX=1` did NOT flatten the leak either** (141.9 KB/s, same as baseline).
The prior profile correctly *found* a real leaking malloc site in the SFX/XMA
sidecar (alloc=10/free=0 over 85 s for ≥128 KB `DecodeOggBuffer` PCM buffers), but
the dominant ~130 KB/s RSS climb is an **always-on path that survives both the SFX
disable and the particle/bloom disables**. The SFX PCM leak is real but appears to
be a *minority* of the total RSS growth here (or `RB3_NO_SFX` doesn't fully close
that specific path under these gameplay conditions). Whatever the primary leak is,
it is in code that runs with SFX off, particles off, and bloom off — i.e. a
base-render / per-frame / event-or-message path, not the FX subsystem. Recommend the
next hypotheses target the always-on per-frame paths (draw-list / DataNode / message
/ event queues, or per-frame allocations in the base mesh/char render) rather than
any toggleable render subsystem.

## Artifacts

- `/tmp/rb3-part-ab/samples_{baseline,bloomoff,partdrawoff,nosfx}.json` — the curves
- `/tmp/rb3-degrade-{label}-<port>.log` — per-arm engine logs
- Engine probe (uncommitted): `RB3_PART_DRAW_OFF` early-return in
  `BandRnd::DrawParticles` (`Rnd_Wgpu_RB3.cpp:~6082`)
