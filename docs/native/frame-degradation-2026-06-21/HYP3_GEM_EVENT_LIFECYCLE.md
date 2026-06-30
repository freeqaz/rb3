# Frame-degradation — Hypothesis 3: gem/note/event/DataNode lifecycle (2026-06-21)

**Verdict: RULED-OUT.** Gem/note visual instances and the engine's small-object
allocations are NOT a source of the in-song memory growth. The gem visual
lifecycle is correctly bounded and recycled on native. Every leaking heap site in
the 33–2048 byte range backtraces to the **GPU driver / Dawn-Vulkan backend**, not
to gameplay object lifecycle.

Worktree: `rb3/.claude/worktrees/memleak-gem-lifecycle` (branch `wt-memleak-gem-lifecycle`)
Engine wt: `milo-native-engine-worktrees/memleak-gem-lifecycle` (unchanged, pin a360e3c)
Build: `native/build-native` (clang, Debug). Reuses `profile_degradation.py`'s driver.

## Hypothesis tested

Do gems/notes/DataNode/message/event objects spawned as the chart scrolls get
freed? The live count of these objects should be bounded by the on-screen window,
not by total song length.

## Method (two independent lines of evidence)

### 1. Direct gem-instance counter (decisive)

Instrumented `MultiMeshWidgetImp` (`src/system/track/TrackWidgetImp.cpp`,
`#ifdef HX_NATIVE`, gated by `RB3_GEM_PROBE=1`): a net live counter bumped +1 in
`PushInstance` and decremented by `(sizeBefore-sizeAfter)` in `RemoveUntil`,
`RemoveAt`, `Clear`. These are the methods that add/remove the visual gem/note
`RndMultiMesh::Instance`s as the chart scrolls. Output written to
`RB3_GEM_PROBE_OUT` (default `/tmp/gem_probe.txt`).

**Result over a full 120 s song (song clock 4 000 → 114 000 ms):**

| sample | liveInst | peakInst | cumPushes |
|-------:|---------:|---------:|----------:|
| first  |   14     |   23     |    36     |
| mid    |  12–16   |   23     |  ~350     |
| last   |   16     |   23     |   606     |

- `liveInst` **oscillates 12–16 the entire song** — flat, bounded, NO trend.
- `peakInst` **capped at 23** (reached early, never exceeded for 120 s).
- `cumPushes` grows monotonically 36 → 606 (gems ARE spawned as the chart
  scrolls) yet `liveInst` stays flat ⇒ they are removed at the same rate they're
  added. Textbook recycled lifecycle.

The scroll-off cleanup path is confirmed live on native (gdb stack):
`App::RunOneFrame → BandUI::Poll → UIManager::Poll → UIScreen::Poll →
TrackPanel::Poll → GemTrackDir::Poll → TrackDir::Poll → PollActiveWidgets →
TrackWidget::Poll → MultiMeshWidgetImp::RemoveUntil` — runs ~every frame.

### 2. Size-windowed LD_PRELOAD backtrace interposer (corroborating)

`/tmp/leak49.so` (the prior agent's interposer, with a deadlock fix in `dump()`:
release the mutex + set `in_hook` before `fopen`, and print RAW addresses resolved
offline via `/proc/<pid>/maps`). Windowed so it's cheap enough to not stall the
engine (verified: song clock advances real-time, RSS climbs 442.9→465.0 MB,
190 KB/s, 87/119 steps monotone-up — leak reproduced under the interposer).

Resolved every top-live (alloc≫free) site over the small/medium object range:

- **33–128 B window:** ALL leaking sites = `libnvidia-eglcore`,
  `libnvidia-gpucomp`, `libvulkan` (GPU driver). Dominant site `live` 107→219.
  ZERO rb3-native / milo-engine frames.
- **129–2048 B window:** dominant grower (`live` 286→408→492→537→547→621,
  alloc/free 5509/4888) backtraces through `rb3-native` into **`dawn::native::
  vulkan::Buffer::Create` / `TextureView::Create` / `InternalTexture::Create`** —
  i.e. the Dawn-Vulkan GPU backend creating GPU resources faster than they're
  freed. Still NOT gameplay-object lifecycle.

No GemPlayer / DataNode / Message / event-queue / TrackWidget allocation appears
in any leaking site across the whole 33–2048 B range.

## Why this rules the hypothesis out

On native, `std::list`/`std::vector` use the host libstdc++ allocator → `malloc`
→ visible to the interposer. A leaking gem-instance list (each
`RndMultiMesh::Instance` ≈ a 64-byte Transform + list node, in window) WOULD show
up. It does not — and the direct counter proves the list stays at 12–16 live.

## Note for the orchestrator: the SECONDARY (non-PCM) leak is the GPU backend

The prior PROFILE_FINDINGS attributed the big mmap leak to the SFX-sidecar PCM
path (hypothesis-1 territory) and noted a "secondary ~49-byte small-object leak"
it didn't attribute. **That secondary small-object leak is the GPU driver /
Dawn-Vulkan backend, not gameplay objects.** Specifically a growing pool of
`dawn::native::vulkan::Buffer`/`TextureView`/`InternalTexture` (live 286→621 over
~75 s). This is a *native headless GPU backend* artifact and would NOT be present
on web (browser WebGPU) — so it is NOT the cross-platform mechanism the user
perceives. It overlaps the bloom/halo capture-and-replay and draw-list
hypotheses (per-draw/per-frame GPU resource creation) — flag for whichever agent
owns those; it is out of scope for gem/note/event lifecycle.

## Artifacts

- Instrumentation (worktree, HX_NATIVE-gated, RB3_GEM_PROBE): `src/system/track/TrackWidgetImp.cpp`
- `/tmp/gem_probe_run.py` — drives to gameplay, plays N s, RB3_GEM_PROBE on
- `/tmp/gem_probe.txt` — the 112-sample bounded-count series above
- `/tmp/leak49.c`/`.so` — fixed windowed interposer; `/tmp/leak_period_run.py`,
  `/tmp/resolve_run.py` — drive + resolve sites against live `/proc/maps`
- `/tmp/leakp_33_128.txt`, `/tmp/leakp_129_2048.txt` — windowed leak dumps
