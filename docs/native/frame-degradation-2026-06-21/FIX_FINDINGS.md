# Frame-degradation FIX — findings + result (2026-06-21)

Worktree: `rb3/.claude/worktrees/memleak-audiofix` (branch `wt-memleak-audiofix`)
Engine wt: `milo-native-engine-worktrees/memleak-audiofix` (branch `wt-memleak-audiofix`, pin **a360e3c — unchanged, NOT bumped**: no engine edits were needed)
Build: `native/build-native`, clang Debug, `Dawn_DIR=dc3-decomp-deps/dawn`.

## TL;DR

The in-song RSS climb has **two independent sources**, only one of which is
application-fixable. I bounded the application one; the other is the NVIDIA
graphics driver and is not reachable from app code (and does not exist on web).

| RSS sub-component | Owner | Before (orig) | After (this fix) | Status |
|---|---|---:|---:|---|
| `[heap]` (glibc main arena) | **application** | 82 KB/s | **28 KB/s, flat after warmup** | **FIXED** |
| `[anon]` near `/dev/nvidiactl` | NVIDIA Vulkan driver | 45 KB/s | 45–51 KB/s | not app-fixable; absent on web |
| **total RSS** | | **109–128 KB/s** | **35–65 KB/s** | 40–72% reduction |

Headline back-to-back A/B (150 s gameplay each, same machine state, cleaned binary):

```
A: BOTH OFF (= original)   RSS 109.1 KB/s   heap 82.4 KB/s   anon 44.6 KB/s
B: FIX (cache+trim)        RSS  65.4 KB/s   heap 28.4 KB/s   anon 51.1 KB/s
```

Long run (260 s, fix default) — **the heap leak is bounded**: heap warms to
~272 MB by t≈30 s then stays flat (late-window slope **8 KB/s**, +1 MB over the
last 137 s). dt never declines monotonically (oscillates 27–217 fps, note-density
driven); fps is stable past 60 s. The frame trace confirms the periodic
`malloc_trim` does **not** cause hitches (trim-frame mean dt = 9.9 ms ≈ non-trim
9.3 ms; the early dt spikes are song-section asset loads at f≈5–7k, uncorrelated
with the 240-frame trim cadence).

## How the prior "audio = 90% of the leak" reconciles

The prior alloc-audit (HYP4) was correct that **`DecodeOggBuffer` is the single
largest malloc-level leak** (90% of tracked *net malloc bytes*). But "90% of
malloc bytes" ≠ "90% of RSS slope". This investigation closed that gap:

1. **Decode cache landed → DecodeOggBuffer plateaus.** A per-call-site malloc
   interposer (allnet.so) on the fixed build shows `DecodeOggBuffer` go to **61
   distinct keys / ~6.5 MB and STOP** (dumps 3 & 4 byte-identical) — the cache
   bounds it. Tracked malloc `totalLive` also plateaus (+2.75 MB then flat).
2. **…yet RSS kept climbing.** With the cache on, RSS still grew ~12 MB/100 s.
   Tracked live malloc only grew 2.75 MB. The ~9 MB gap is **not** in tracked
   malloc.
3. **It is not anonymous mmap either.** An mmap-syscall interposer showed
   `mmapLive` FLAT at 1.38 MB the whole run.
4. **smaps per-mapping RSS diff localised it exactly:**
   ```
   +8.94 MB  [heap]                       <- glibc main arena (brk), application
   +5.92 MB  [anon]  (size 10 MB, rw-p)   <- sandwiched between two /dev/nvidiactl
   ```
   - `[heap]` grows **8.94 MB of RSS while live malloc grew only 2.75 MB** =
     classic **glibc arena fragmentation/retention**: the heavy per-frame churn
     of short-lived small allocations (the un-reaped SFX `SfxInst`/`SampleInst`
     objects + their decode buffers + DataNode/message/vector temporaries) bumps
     the arena's brk high-water mark, and glibc doesn't return those pages on
     `free`.
   - The `[anon]` grower is **NVIDIA driver memory** (its neighbours in
     `/proc/pid/smaps` are both `/dev/nvidiactl`). `MALLOC_ARENA_MAX=1` did not
     change it (proving it's not a glibc secondary arena), and an app-side
     malloc/mmap interposer never sees it. It is the Vulkan driver's internal
     pools faulting in as draw state accumulates — not fixable from app code.

## The fix (two native-only parts, both opt-out-able)

### 1. Payload-keyed PCM decode cache — `native/src/rb3_xma_sidecar.h`
`TryLoadCached()` decodes each distinct SFX (keyed by the existing 64-bit
`PayloadKey`) **at most once** and keeps the buffer for the run; every re-trigger
borrows it (`SidecarPCM.owned=false` → the inst never frees it). Bounds total PCM
to the distinct-SFX set (~61 buffers / ~6.5 MB here) instead of growing per
trigger, and removes the per-trigger re-decode CPU. `RB3SampleInstNative::StartImpl`
now uses it and only frees `mOwnedPCM` when it actually owns a buffer (legacy
path). Opt-out: `RB3_SFX_CACHE_OFF=1`.

### 2. Periodic glibc heap trim — `native/src/rb3_heap_maint_native.cpp` (new)
`RB3NativeHeapMaintenance(frame)`, called once per frame from `App::Run`'s
`#ifdef HX_NATIVE` loop, calls `malloc_trim(0)` every `RB3_HEAP_TRIM_FRAMES`
(default 240 ≈ a few seconds). This `madvise(DONTNEED)`s the arena's
fragmentation-retained free pages back to the OS, capping the `[heap]` ratchet
that the cache alone doesn't (the cache cuts the *churn*; the trim reclaims what
churn already fragmented). Cheap at this cadence (a single arena walk, sub-ms;
verified non-hitching). Opt-out: `RB3_HEAP_TRIM_OFF=1`. No-op on web (the wasm
`dlmalloc` heap can't return memory to the host).

The two are complementary: cache OFF + trim ON still leaks more (less reclaim
because more fragmentation); cache ON + trim ON is the flat-heap default.

## Match-neutrality

- `rb3_xma_sidecar.h`, `rb3_sampleinst_native.cpp`, `rb3_heap_maint_native.cpp`,
  `main_native.cpp` are all `native/src` / native-only (`#ifdef HX_NATIVE`).
- `src/App.cpp`: the one call site is **inside the existing `#ifdef HX_NATIVE`
  block** (lines 708–840) of `RunWithoutDebugging()`; the Wii build takes the
  `#else`/`while(true)` loop below `#endif` and is byte-unchanged.

## Web relevance

The catastrophic 2–3 fps collapse is web-amplified. On web there is **no NVIDIA
driver** (`[anon]` grower absent) and the wasm heap is the analogue of `[heap]` —
so the decode cache (which bounds the wasm-heap PCM directly, the dominant
churn) is the web-relevant half. `malloc_trim` is a no-op on wasm but harmless.
The fix is platform-shared; iterate/confirm in native (done), then web.

## Residual / honesty

- **Bounded, not zero.** The application `[heap]` is now flat-after-warmup. Total
  native RSS still rises ~35–65 KB/s, dominated by the **NVIDIA `[anon]` driver
  pool** I cannot fix from app code. Over a 5–6 min song that's a few MB — well
  short of the OOM/throttle that caused the user's collapse, and the *mechanism*
  the user hit (unbounded application growth) is removed.
- A fuller fix for the small object-churn would be to also make finished native
  `SfxInst`s reap reliably (today they reap slowly — alloc≫free), which would cut
  the fragmentation *source* further. The trim already reclaims the consequence,
  so this is optional polish, not required to bound RSS.

## Tools (this dir + fix-tools/)

- `profile_degradation.py` — original dt+RSS sampler (inherited).
- `fix-tools/final_verify.py` / `rss_ab2.py` — RSS **split by `[heap]`/`[anon]`**
  via /proc/smaps, with dt; the instrument that separated the two leaks.
- `fix-tools/mmaptrack.c` — LD_PRELOAD mmap/munmap net tracker (showed mmapLive
  flat → ruled out anon-mmap).
- `fix-tools/diff_dumps.py` — diff the allnet.so per-call-site dumps (showed the
  DecodeOggBuffer plateau under the cache).
- (allnet.so per-call-site malloc interposer reused from the alloc-audit worktree.)
