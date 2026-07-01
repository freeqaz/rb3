# Frame-degradation FIX — adversarial review (2026-06-21)

Reviewer: independent rebuild + re-run of the 90–135s repro on the fix branch
`wt-memleak-audiofix @ fffa63d1` (engine pin b8f3cfa, unchanged by the fix).

## Verdict: CONFIRM (with one honest residual: the NVIDIA driver pool)

The in-song **application** memory leak is genuinely fixed — not slowed, not
relocated. The fix is correct (audio byte-identical), does not regress gameplay
or frame timing, and is match-neutral (Wii byte-unchanged).

## What I built/ran (independent)

- Force-rebuilt the 3 fix TUs (`rb3_heap_maint_native.cpp`,
  `rb3_sampleinst_native.cpp`, relink) — fresh binary 07:03, fix symbols present
  (`RB3NativeHeapMaintenance`, `TryLoadCached`, `DecodeCacheEnabled`).
- Wrote my own dt+RSS sampler that **splits RSS by /proc/smaps mapping** into
  `[heap]` (glibc arena = application) vs `[anon]` (driver/other), reusing the
  project's own keyboard-to-gameplay nav module. A/B on ONE binary via env
  opt-outs (`RB3_SFX_CACHE_OFF=1 RB3_HEAP_TRIM_OFF=1` = original behavior).
- **Built a standalone unit harness** that drives the *shipped* `TryLoad` (legacy
  owned) vs `TryLoadCached` (fix) against a real `.ogg` SFX sidecar — the cleanest
  isolation of exactly the code the fix changes.

## Result 1 — in-game A/B (135s gameplay, autohit firing note-hit SFX)

| config | app `[heap]` slope | NVIDIA `[anon]` slope | dt / fps | collapse? |
|---|---:|---:|---|---|
| **fix OFF (=original)** | **+74.5 KB/s** (264→272 MB, climbing) | +49 KB/s | 7.9ms / 127fps | none |
| **fix ON (default)** | **−7.0 KB/s** (flat 272 MB) | +83 KB/s | 7.4ms / 136fps | none |

The application heap leak (~75 KB/s, the doc's claimed ~82 KB/s) is **eliminated**
by the fix (heap goes flat). dt never degrades monotonically in either config and
fps is stable past 60s — the malloc_trim cadence does not hitch (fps is if anything
slightly better with the fix on).

> Caveat I verified: the leak is **SFX-trigger-density dependent**. My first
> fix-off run (no autohit) showed a *flat* heap because note-hit SFX weren't
> firing — so the A/B is only meaningful with autohit on (which re-arms the
> per-note SFX). With SFX firing, the original heap leak reproduces clearly.

## Result 2 — unit isolation (shipped decode path, 2000 triggers, deterministic)

```
leak  (original TryLoad, retained per trigger = un-reaped SfxInst):
        +256 KB / trigger  ->  RSS 4.8 MB -> 516 MB   UNBOUNDED, linear
cache (fixed TryLoadCached, borrowed):
          0 KB / trigger   ->  RSS flat at 4.8 MB     BOUNDED
PCM bytes owned-vs-cached: IDENTICAL (memcmp=0; 129466 samples @ 32kHz mono)
cache hit returns the SAME pointer (one buffer reused); owned=false (inst won't free)
```

Each decoded SFX PCM buffer is ~253 KB (>128 KB), which independently matches the
prior profiling's ≥128 KB-filtered interposer that caught
`DecodeOggBuffer→TryLoadOgg→TryLoad→RB3SampleInstNative::StartImpl alloc=10 free=0`.
I re-resolved those backtrace addresses in the prior profiling binary and they
**do** resolve to `DecodeOggBuffer` — the prior attribution is sound.

## Correctness / regression

- **Audio unchanged**: cached PCM is byte-identical to a fresh owned decode.
- **No use-after-free / double-free**: the cache (`std::map`) only grows, never
  erases/frees entries; borrowed pointers are valid for the process lifetime;
  `mOwnedPCM` stays null on a cache hit so the inst dtor doesn't free a borrowed
  buffer. `RenderAudio`/`LerpMono` only READ `mPCMData` (const), and sidecar PCM
  is always little-endian (no in-place byte-swap), so sharing one buffer across
  concurrent/sequential triggers is safe.
- **Gameplay reached + full song played** (~129s) in both configs; gems/render
  path unaffected (the fix touches only SFX PCM ownership + a per-frame trim).

## Match-neutrality (verified)

- `rb3_xma_sidecar.h`, `rb3_sampleinst_native.cpp`, `rb3_heap_maint_native.cpp`,
  `main_native.cpp` are native-only (`#ifdef HX_NATIVE`).
- `src/App.cpp`: the lone call site is at lines 822–823, **inside** the
  `#ifdef HX_NATIVE` block (708) that closes at `#endif` (840); the Wii build's
  `while(true)` loop starts at 844 (the `#else` path) and is byte-unchanged.
- Engine pin unchanged (no engine edits). NOTE: the FIX_FINDINGS doc says pin
  `a360e3c`; the worktree actually pins `b8f3cfa` (inherited from the worktree
  base, not this fix) — a doc nit only; the fix made zero engine changes either way.

## Honest residual (why "CONFIRM with residuals" is the precise truth)

Total native RSS still rises (~29–124 KB/s, run-dependent), now dominated by the
**NVIDIA Vulkan driver `[anon]` pool** — I independently confirmed via
`/proc/pid/smaps` that the largest growing anonymous mappings are sandwiched
between `/dev/nvidiactl` and `/dev/nvidia0` mappings. This is **not application
memory**, not reachable from app malloc, and **absent on web** (no NVIDIA driver
in a browser). It is therefore irrelevant to the user's actual web collapse. The
application leak — the part the user hit (unbounded app growth → throttle) and the
only part addressable in app code — is removed.

## Bottom line

The fix does what it claims for the application-side leak: bounded, not slowed,
not relocated; correct; regression-free; match-neutral. The only remaining RSS
growth is the platform graphics driver, which is out of scope and does not exist
on the target (web) where the catastrophic symptom was reported. CONFIRM.
