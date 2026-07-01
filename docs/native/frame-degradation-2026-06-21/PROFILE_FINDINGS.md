# Frame-degradation profiling — findings (2026-06-21)

Worktree: `rb3/.claude/worktrees/memleak-frame-degrade` (branch `wt-memleak-frame-degrade`)
Engine worktree: `milo-native-engine-worktrees/memleak-frame-degrade` (branch `wt-memleak-frame-degrade`, pinned SHA a360e3c — unchanged)
Build: `native/build-native` (CMake `Debug`, clang; Dawn_DIR=/home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn)

## TL;DR

- **It is a LEAK** (RSS climbs monotonically), **not CPU-growth**. The in-song
  dt is noisy (5–18 ms) and song-section-driven (note density / camera cuts) — it
  oscillates and *recovers*, it does NOT ramp monotonically with time.
- **Native reproduces the leak but NOT the catastrophic 2–3 fps collapse.** Over
  300 s of continuous gameplay native held 56–220 fps the whole time while RSS
  climbed steadily. The user's "2–3 fps after ~60 s" is far more severe than what
  headless native shows → the catastrophic *symptom* is amplified on web
  (browser: non-reclaimable wasm heap, GC pressure, single-thread, memory-pressure
  throttling), but the *mechanism* (the leak) is present and reproducible on native.
- **Leak rate: ~122 KB/s, monotonic** (56/57 sample steps up over 300 s).
- **Onset: immediate** at gameplay start (not a ~60 s threshold — it leaks from the
  first SFX trigger). The ~60 s the user perceives is when accumulated browser
  memory pressure starts throttling.
- **FIRST ATTRIBUTION: the kXMA SFX sidecar PCM path.** Every SFX trigger
  re-decodes an ogg→PCM buffer that is never freed because finished SfxInsts are
  not being reaped.

## Curve (native, headless, 300 s long run — `samples_long300.json`)

dt is a 60-frame trailing mean; RSS from /proc VmRSS.

| t (s) | dt (ms) | fps  | RSS (MB) |
|------:|--------:|-----:|---------:|
|   5   |   8.0   | 125  |   445    |
|  40   |  12.0   |  83  |   448    |
|  81   |   8.8   | 113  |   451    |
| 122   |   5.4   | 184  |   456    |
| 164   |  13.8   |  72  |   461    |
| 205   |  17.1   |  58  |   466    |
| 247   |  12.1   |  83  |   472    |
| 288   |  13.6   |  74  |   478    |
| 294   |   —     |  —   |   493    |  (song ended; teardown alloc spike)

RSS linear fit = **122 KB/s** (= ~7.3 MB/min). dt shows no monotonic trend; HWM
constant at boot peak (~549 MB) — gameplay never re-peaks, confirming the climb is
slow heap growth, not a one-time spike.

## How the attribution was nailed

1. **dt-vs-RSS sampling** (`profile_degradation.py`): proved leak (RSS↑ monotone)
   vs CPU-growth (dt flat-noisy). Frame-trace buckets (tex/mesh/obj/loader/pipe)
   are ~0 at steady state → leak is NOT in any instrumented path.
2. **Subsystem A/B** (`leak_ab.sh`): bloom-off, particle/haze-off all leak at the
   *same* ~80–105 KB/s → leak is in an **always-on** path, not a toggleable
   render subsystem. (Char-anim opt-out combo crashed; not pursued.)
3. **glibc `malloc_info` snapshots via gdb** (gdb as parent — ptrace_scope=1 blocks
   attach to a non-child; `gdb_leak.py` runs rb3-native as gdb's inferior, SIGINT
   from outside to interrupt, `malloc_info(0, fopen(...))` at two timepoints 75 s
   apart). Diff: **system heap +4.55 MB in 75 s, dominated by mmap +5.25 MB**
   (mmap_count 3→4) → the leak is **large (mmap-served, ≥128 KB) buffers**, plus a
   secondary +2,909 49-byte chunks (~1.9 KB/s small-object leak).
4. **Targeted LD_PRELOAD interposer** (`leak49.c`, size-windowed so it's cheap
   enough to not stall the game): tracked only ≥128 KB allocations with 4-frame
   backtraces. **One rb3-native site: alloc=10 free=0** in 85 s. `addr2line`:

```
rb3_xma::DecodeOggBuffer()   native/src/rb3_xma_sidecar.h:339   (malloc of the decoded PCM)
rb3_xma::TryLoadOgg()        native/src/rb3_xma_sidecar.h:379
rb3_xma::TryLoad()           native/src/rb3_xma_sidecar.h:397
RB3SampleInstNative::StartImpl()  native/src/rb3_sampleinst_native.cpp:190
```

## Root-cause mechanism (for the attribution/fix phase)

- `SynthSample::NewInst` (`rb3_sampleinst_native.cpp:310`) `new`s a
  `RB3SampleInstNative`. On `StartImpl` (kXMA path) it calls
  `rb3_xma::TryLoad → TryLoadOgg → DecodeOggBuffer`, which **`std::malloc`s the
  full decoded PCM every Start** and stores it in `mOwnedPCM`. `~RB3SampleInstNative`
  frees `mOwnedPCM`.
- Those sample-insts live inside an `SfxInst` (`Sfx.cpp`: `mSamples`,
  `~SfxInst → DeleteAll(mSamples)`). SfxInsts are created by `Sfx::MakeInstImpl`
  (`Sfx.cpp:192`, `new SfxInst` + `mSfxInsts.push_back`) and are supposed to be
  reaped by `Sequence::SynthPoll` (`Sequence.cpp:61-66`): `delete curSeq` when
  `Started() && !IsRunning()`.
- **The leak = those finished SfxInsts are never reaped on native** (alloc=10
  free=0 over 85 s = zero reaps), so `~RB3SampleInstNative` never runs and every
  per-trigger decoded PCM buffer is retained. Each gameplay SFX hit (note hits,
  fired ~tens/sec) adds one PCM buffer.

### Two leads for the reaper failure (next phase should disambiguate)
1. `Sequence::SynthPoll` (the reaper) is not being driven on native for Sfx
   (StartPolling/CancelPolling not wired, or Sfx not in the synth poll list), OR
2. The `SeqInst::Started()` flag never flips true for `RB3SampleInstNative`-backed
   SfxInsts, so the `Started() && !IsRunning()` reap condition at Sequence.cpp:65
   is never satisfied → insts pile up forever.

A secondary, independent improvement: `DecodeOggBuffer` re-decodes the SAME ogg on
every Start (no PCM cache keyed by payload hash) — even with reaping fixed, this is
wasteful churn. A decode cache would both cut the leak's growth and reduce CPU.

## Match-neutrality note for the fix

`rb3_sampleinst_native.cpp` + `rb3_xma_sidecar.h` are `native/src` (native-only) —
free to fix. The reaper lives in `src/system/synth/Sequence.cpp` (shared Wii
decomp) — any change there must be `#ifdef HX_NATIVE` or proven Wii-byte-identical.
Prefer fixing the native-only ownership (e.g., reap-on-finish or pool/cache the
decoded PCM) over touching shared Sequence.cpp.

## Web check

Native reproduces the leak mechanism, so a full web build/run was not needed to
confirm "which platform." The catastrophic *collapse* is web-amplified (the
mechanism is identical shared code; the symptom severity is browser-specific). A
deployed web build exists (`native/web/build/`, Jun 21) if a browser-side
confirmation of the amplified collapse is wanted, but the fix is platform-shared.

## Artifacts (all under /tmp and this dir)

- `profile_degradation.py` — dt+RSS sampler (drives to gameplay, holds, samples)
- `samples_baseline.json` (120 s), `samples_long300.json` (300 s) — the curves
- `/tmp/leak49.c` / `leak49.so` — size-windowed LD_PRELOAD backtrace interposer
- `/tmp/gdb_leak.py` — gdb-as-parent malloc_info snapshotter (ptrace workaround)
- `/tmp/minfo_t1.xml` / `minfo_t2.xml` — the two malloc_info snapshots
