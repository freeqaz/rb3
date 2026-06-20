# Web main-thread frame-stall attribution — song-start + steady gameplay (2026-06-20)

Empirical CPU-profile + timeline-trace attribution of the rb3-web main-thread
stalls that starve the single-threaded JSPI audio producer (`PumpAudio` runs
once per `requestAnimationFrame` on the main thread; any main-thread longtask
that overruns the ~90–140 ms buffered audio depth → an audible under-run).

**Bottom line:** the single biggest, most-eliminable song-start stall is **not
engine code** — it is the page's on-DOM console-log mirror in `native/web/index.html`,
which forced a full-document relayout per console line (the engine emits hundreds
of `NOTIFY` lines per frame during song load). It cost **3.72 s of browser Layout
= 37% of the main thread** across the 9 s song-start window and added **~4 s** to
the song-start transition. A page-chrome-only fix (rAF-batched, DOM-capped log)
cuts that to **0.17 s (1%)** and song-start longtask stall from **1176 ms → 173 ms
(−85%)**, with zero engine/wasm/match impact. After that, the residual engine
stalls are characterized below.

## Method (reproducible)

- Harness: `scripts/web/_framestall-songstart.mjs` (Playwright + Chrome DevTools
  Protocol). Drives boot → splash → main_hub → song_select → part_difficulty,
  then arms `Profiler.start` (5 kHz, `interval:200µs`) + `Tracing.start`
  (`disabled-by-default-devtools.timeline` + `v8.execute` + frame categories)
  **right before** the Enter that kicks the song load, and captures the first
  ~9 s. `--steady-delay N` instead settles N s into `game_screen` then profiles
  steady gameplay. `--no-domlog` detaches the `#console` mirror to isolate the
  true engine cost.
- Build profiled: the local `native/web/build/debug/` (`-O0 -g2`, demangled C++
  names in the CPU profile), `?debug=true`. Song `20thcenturyboy`.
- Server: `python3 native/web/server.py --port 8533` (own port; another workflow
  holds 8421).
- Analysis: `scripts/web/analyze-cpuprofile.mjs` for self-time; ad-hoc node
  scripts (in this doc's commit history / `/tmp/framestall-*`) bin the CPU
  profile by 250 ms windows, reconstruct caller chains from `nodes[].children`,
  and sum trace `RunTask`/`Layout`/`Paint` durations per window.
- Raw artifacts: `/tmp/framestall-out` (original), `/tmp/framestall-fixed`
  (log-fix), `/tmp/framestall-out-nolog` (engine floor), `/tmp/framestall-steady`
  (steady gameplay). Each has `songstart.{cpuprofile,trace.json,meta.json}`.

## Timeline of the song-start transition

`part_difficulty → tv3_*_screen (loading) → game_screen`. With the original
build the handoff took **~9.2 s of wall time**; with the log fix / `--no-domlog`
it settled in **~4–5 s**. The `game_panel_load` engine marker reports the panel
build alone at **~3.0 s** (no-domlog) vs **~7.5 s** (original, inflated by the
layout thrash starving the loader's frame budget).

## Ranked stall attribution

### 1. On-DOM console-log mirror — layout thrash — **3.72 s / 37% of main thread, song-start** — ELIMINABLE (fixed)

`native/web/index.html`'s `log()` appended a `<div>` per console line **and read
`logDiv.scrollHeight/clientHeight/scrollTop`** on every call. The read-after-write
forces a synchronous full-document relayout each line; with the engine emitting
hundreds of `NOTIFY` lines per frame during song load (e.g. `OvershellSlot.cpp:3661`,
`outfit ... included more than once`, char-asset merges) the `#console` DOM grew
**unbounded to 17,817 nodes** and every line relaid out the whole document.

Measured over the 9 s song-start window:

| variant | Layout events | Layout time | % main thread | peak DOM nodes | longtasks>50ms | Σ longtask ms |
|---|---|---|---|---|---|---|
| **original** (per-line append+read) | 913 | **3.72 s** | **37%** | 17,817 | 16 | **1176** |
| **fixed** (rAF-batched + 200-line cap) | 457 | **0.17 s** | **1%** | 433 | 2 | **173** |
| floor (mirror detached) | 0 | 0 s | 0% | 0 | 1 | 102 |

The fix lands essentially at the no-mirror floor: the only remaining song-start
longtask is the genuine engine scene-activation frame (≈100 ms, item 2).

**Why it matters for audio:** the original 185 ms + 106 ms Layout-driven
longtasks each exceed the ~90–140 ms buffered audio depth → guaranteed
under-runs at song start. Removing them removes the largest under-run trigger.

**Caveat / scope:** this is the **debug build** (verbose logging + `#console`
visible). The fix is unconditional page-chrome and also protects the release
build (same `index.html`), where the mirror is present but logging is lighter.
It does *not* touch the wasm — it is the cheapest, safest, highest-magnitude win.

### 2. Engine scene-activation frame — synchronous Poll/draw build — **~100 ms single frame at game_screen handoff** — PARTLY ELIMINABLE

The one residual song-start longtask (≈100–102 ms) is the first real gameplay
frame: `TimerFire → RunMicrotasks` running the wasm `RunOneFrame` that activates
the game panel scene. The trace shows ~1530 `V8.StackGuard` calls inside it — the
JSPI suspend/resume signature of the synchronous async-file reads the panel build
still issues. This is the same async-I/O-bound model documented in
`web-loadperf-findings-2026-06-03.md` (boot App-ctor), now at song-load scope.
Spreading the panel activation across frames (or pre-warming the scene during the
`tv3_*` loading screen, which is already on-screen) would break this single frame
into sub-budget pieces. Lower priority than item 1 (one frame, ~100 ms vs ~1.2 s).

### 3. `BandRnd::DrawMesh` — per-frame mesh render — **643 ms song-start / 846 ms steady (7–8% of every frame), RECURRING** — REDUCIBLE (GPU/draw-count)

The #1 engine self-time hotspot in **both** windows. Caller chains (from the CPU
profile node tree):
- `RndGroup::DrawShowingBudget → RndMesh::DrawShowing → BandRnd::DrawMesh` (venue/scene), and
- `Character::DrawShowing → Character::DrawLod → DrawLodOrShadow → ... → BandRnd::DrawMesh` — **the band characters dominate** (the top two DrawMesh nodes, 144 ms + 100 ms self, are both under `Character::DrawLod`).

Each `DrawMesh` is accompanied by WebGPU upload/bind glue — `writeBuffer`
(169 ms song-start / **326 ms steady**), `setBindGroup` (57/123 ms),
`createBindGroup`, `setVertexBuffer` — i.e. the cost scales with **draw/submesh
count**, not pixels. This is the recurring steady-state per-frame cost that keeps
frames at ~10–24 ms (no headroom for the audio producer). Reducible by reducing
draw/bind calls (batch submeshes, cache bind groups, cull off-screen/occluded
band LODs), not a one-line fix — engine-side, benefits web + native FPS.

### 4. Char-skinning rebind — `dynamic_cast` per ObjectDir entry, every Poll — **~330 ms `__dynamic_cast` + ~318 ms RTTI helpers ≈ 650 ms song-start (7%); still 128 ms 8 s into gameplay** — ELIMINABLE (cache/gate)

The unambiguous #1 `__dynamic_cast` caller chain:
`BandCharacter::Poll() → RebindHeadHandsAtRest() → NativeCollectSkinnedMeshes() →
ObjDirItr<RndMesh>::Advance() → __dynamic_cast` (per directory entry).
`NativeCollectSkinnedMeshes` (`src/system/bandobj/BandCharacter.cpp:722`, all
`#ifdef HX_NATIVE`) walks each member's ObjectDir hashtable **and** recurses the
whole draw tree, doing a `dynamic_cast<RndMesh*>` per drawable plus O(N²)
`std::find` linear scans against `visited`/`targets`. It runs **every Poll, for
every band member**, until a latch flips: `pending==0 && quiet>=30` Polls, or a
**600-Poll (~10 s) give-up** for members with a permanently-unresolvable mesh
(the known 1-residual-mesh-per-member case). So for ~10 s of a song the full
RTTI-heavy walk re-runs ~4× per frame. The steady-state capture still attributes
128 ms (1.2%) to this subtree 8 s in — confirming the give-up hadn't fired.

Eliminable, native-only (`HX_NATIVE`), match-neutral:
- cache the collected skinned-mesh list per member (invalidate on StartLoad/
  SyncObjects) instead of re-walking every Poll;
- replace `ObjDirItr<RndMesh>` `dynamic_cast` filtering + `std::find` with the
  cached vector + a `std::set`/flag for the visited test;
- once a member latches, skip the walk entirely (already intended — but the
  600-Poll give-up keeps it alive far too long; lower the unresolvable-mesh
  give-up, or mark the residual mesh and stop scanning).

### 5. Async asset-fetch completion — `WebAssetsFetchDone` — **184 ms song-start** — PARTLY ELIMINABLE (timing)

`WebAssetsFetchDone(int)` (the JSPI fetch-completion callback) is still firing
during early gameplay because char/venue meshes (`char/main/anim/.../realtime_rocker.milo_xbox`,
~894 KB; `medium_rocker`, ~346 KB) are **still streaming after game_screen
appears**. Each completion does endian-swap parse work — at +6.5 s the profile
shows `ChunkStream::ReadImpl + BinStream::Read/ReadEndian + EndianSwap/SwapData`
(~40 ms in one 250 ms bin) byte-swapping the big-endian `.milo_xbox` assets on the
main thread. Largely an incremental-load-scheduling concern (already an active
workflow); the endian-swap-on-load is a candidate to precompute (ship
little-endian assets) or move off the critical path.

### 6. Transform dirty propagation — `DirtyCache::SetDirty*` / `RndTransformable::SetDirty`/`WorldXfm` — **~170 ms song-start / ~350 ms steady** — REDUCIBLE (low priority)

Steady recurring cost of marking/recomputing transform dirty state across the
scene each frame. Engine-wide; modest per-item but pervasive. Lower priority.

## Steady-state gameplay (after load settles)

10 s capture, 8 s into `game_screen`, log fix active: **idle 35%, busy 5.4 s**,
RunTask **max 24 ms, p99 9 ms, 0 tasks >33 ms**. So settled gameplay rarely drops
a *full* frame — yet `AudioDevice: latency GROW` under-runs still fire ~every 10 s.
That is the **single-threaded producer model**, not a giant longtask: `PumpAudio`
runs once per rAF, and a busy ~10–24 ms frame (vs the 16.7 ms budget) plus the
WebGPU submit leaves no headroom to refill the ring at its current depth.
Shrinking the recurring per-frame engine cost (items 3, 4, 6) restores that
headroom; the complementary fix (make the audio producer survive a stalled
frame — decouple `PumpAudio` from rAF / give it a deeper buffer) is the in-flight
audio workflow's lane.

## What's NOT the problem (refuted here)

- **GC**: 14–74 ms across 9–10 s windows (<1%). Not a stall source.
- **wasm cold-compile / network**: out of scope for *song-start* (that's boot;
  see `web-loadperf-findings-2026-06-03.md`). The song-start window is steady
  wasm execution + the layout thrash, not download/compile.
- **A single giant engine longtask**: there isn't one once the log mirror is
  fixed — the residual engine cost is spread (item 3/4/6 per-frame) plus one
  ~100 ms activation frame (item 2).

## Prototype (proves item 1 is reducible)

`native/web/index.html` `log()` → rAF-batched, `MAX_LOG_LINES=200` capped,
no per-line layout read. Committed to worktree branch **`wt-framestall-domlog`**
(both the source `native/web/index.html` and the deployed
`native/web/build/index.html` copy for live no-rebuild testing). Re-measured
result is the table in item 1 (3.72 s → 0.17 s Layout; 1176 ms → 173 ms longtask
stall). No engine/wasm rebuild required; no Wii-match surface touched.

## Recommended next actions (ranked by ROI)

1. **Land the `log()` fix** (item 1) — done in prototype; biggest win, zero risk.
2. **Cache `NativeCollectSkinnedMeshes` + drop the dynamic_cast/std::find walk**
   (item 4) — ~650 ms song-start + recurring, native-only, clear cache point.
3. **Pre-warm / frame-spread the game-panel scene activation** (item 2) — kills
   the last ~100 ms song-start frame.
4. **Cut DrawMesh draw/bind count** (item 3) — biggest *recurring* cost; restores
   steady-state audio headroom; helps native FPS too. Larger effort.
5. Ship little-endian `.milo` assets / defer endian-swap off the main thread
   (item 5) — overlaps the incremental-load workflow.
