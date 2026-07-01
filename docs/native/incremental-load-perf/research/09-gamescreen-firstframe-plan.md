# 09 — game_screen first-frame plan (Wave 5)

**Date:** 2026-06-11. Planning doc for the game_screen FIRST-FRAME hitch wave
(P2). Fresh measurements taken against rb3 `f1ca1af4` + engine `f75339a`
(deployed web release+debug builds of 2026-06-11 04:00, both post-pin-bump).
All numbers below are from THIS session's runs, not wave-3/M1 carry-overs.

Artifacts: native `/tmp/w5-native-trace2.jsonl` (frame_profiler.py
`--into-song`, headless, lock-serialized); web release `/tmp/w5-web-c0/`
(`_netmatrix.mjs --mbps 200 --rtt 5` ≈ localhost, cold IDB, full journey,
song=combatbaby, vignette=tv3_b); web debug `/tmp/w5-prof/`
(`scripts/web/_w5-firstframe-profile.mjs`, **uncommitted probe on disk** — CDP
V8 sampling profiler armed at part_difficulty, 200 µs interval, cold IDB,
vignette=tv3_c). Reveal frame = the first frame whose `scr` flips to
`game_screen` in the RB3_FRAME_TRACE JSONL.

---

## 1. Fresh measured decomposition (the hitch TODAY)

### 1.1 The reveal frame

| run | reveal dt | lp | lpu | objMs (worst obj) | texMs / texN | meshMs / meshN | pipeMs / pipeN | residue (dt − counters) |
|---|--:|--:|--:|--:|--:|--:|--:|--:|
| native (`-O2` host) | **95.4 ms** | 0 | **48.3** | 36.1 (RndTex:floor_wood02_NORM.tex) | 17.3 / **97** | 3.2 / **113** | 0 / **0** | ~27 ms (objMs runs inside the lpu drain) |
| web release (200 Mbps, cold IDB) | **1010.4 ms** | 0\* | 0\* | 54.5 (same RndTex) | 31.0 / **97** | 1.7 / **113** | 0 / **0** | **~923 ms** |
| web debug (-g2) | **658.9 ms** | 0\* | 0\* | 180.1 (RndTex:wall_wainscoat_plaster_norm.tex) | 96.2 / **97** | 6.0 / **113** | 0 / **0** | **~377 ms** |

\* **lp/lpu are not wired on web** — every web frame records 0 while native
records real drains (the N4 recorder wire-in passes zeros for the two
caller-computed args). The web reveal frame demonstrably runs the same
PostLoad drain (objMs 54–180 with the same worst-object) without attributing
it. Fixing this is T3.

Identical texN=97 / meshN=113 / worst-object class on all three runs: the
reveal frame's work is **structural**, not run noise — (a) a final synchronous
dir drain whose per-object PostLoad is venue textures (native: `lpu=48 ms` via
PollUntilLoaded — i.e. a `ForceGetLoader`-shaped drain ON the reveal frame;
`pend` flips 0→1 that exact frame), then (b) the venue/track/char world's
FIRST DRAW: 97 `UploadRndTexIfNeeded` texture uploads + 113 mesh VB/IB
creates/writes + the **uncounted** full CPU vertex unpack of all 113 meshes +
~300 draw encodes.

**dt vs rAF caveat (user-visible meaning):** in the same web release run the
longest rAF gap in the whole start-song window was **102.9 ms** (over100=1,
frozenSum=0). The 1010 ms is the *wall time of one logical RunOneFrame* —
JSPI yields inside the in-frame drain keep the tab compositing. The user sees
a ~0.7–1.0 s **game-content freeze** (vignette lingers / first venue frame
stalls) plus one ~100 ms rAF gap, not a 1 s dead tab. Both are the same work
and both go away together; gates below check BOTH (reveal dt and over100).

### 1.2 The residue: PROVEN not pipeline compile, not GC, not fetch

- **Pipeline compile is ruled out**: `pipeN`/`pipeMs` = **0 on the reveal
  frame AND run-total 0** across native, web release, and web debug runs.
  A5's PreWarm sweep (engine `PipelineManager::PreWarm`, 240-key full sweep of
  blend×zMode×layout×alphaCut×pass) is a verified superset of the gameplay
  venue's keys — the "extend A5 to the gameplay venue" option has nothing
  left to cover. (PipelineManager.cpp:505 sweeps all 8 blends / 5 zModes /
  both layouts / both alphaCut / 3 pass variants; gameplay introduces no new
  field values — confirmed empirically by zero draw-time cache misses.)
- **Not GC / not network**: garbage collector = 0.5% of the 12.5 s profiled
  start-song window; `fetchMs=0`, `inflMs=0`, `primeMs=0` on the reveal frame
  (audio priming completes during the vignette's kWaitingForAudio state).
- **What the CPU profile shows** (debug build, profiler armed
  part_difficulty→game+3 s, 49 286 samples): wasm self-time is dominated by
  the per-draw mesh path — `BandRnd::DrawMesh` self **1216 ms (9.7%)** plus
  the unpack family `BeDec4n`/`BeColor`/`BeFloat`/`BeUDec4n`/`Half2Float`
  (~585 ms), `Vector4_16_01::Get*` (~290 ms), `GpuVertexSkinned` vector
  construct/copy (~380 ms), `__dynamic_cast` 274 ms, `(program)` 14.9%.
  Every >200 ms busy stretch at game-entry time has the same signature.
- **Why the web residue is huge while native's is ~27 ms**: the unpack runs
  per-draw, per-vertex through tiny helper calls; the web build is `-O0`
  (both release and debug — `--opt` is broken since W4a), a ~20–40×
  multiplier on exactly this code shape. Engine `f75339a` (BE-float unpack
  fix) changed correctness of this path, not its cost; RB3's `DrawMesh` has
  its own inline unpack (Rnd_Wgpu_RB3.cpp:3203-3300) that runs
  **unconditionally** — only the GPU write is gated on `needUpload`
  (the in-source comment marks caching it as "the tracked follow-up perf
  win"). On the reveal frame all 113 meshes unpack + upload; on every
  steady frame the skinned (character) meshes re-unpack bind verts for the
  shard guard.
- Second-order web residue: the reveal-frame drain yields via
  `emscripten_sleep(0)` (16 ms-throttled), each suspend a 4–16 ms event-loop
  turn — a ~200 ms drain accrues O(100 ms) of suspend wall inside dt. Moving
  the drain off the reveal frame (T2/L3) removes these too.

### 1.3 Where the work can move: the vignette dwell is ~idle

The tv3_* "vignette" (loading) screen sits between part_difficulty and
game_screen and polls the pending game screen every frame
(`GamePanel::PollForLoading`/`IsLoaded`, GamePanel.cpp:156/:197 — the
GAME_DBG arms already trace this loop):

| run | vignette frames | p50 dt | frames w/ pend>0 | idle budget (Σ max(0, 16.6−dt)) |
|---|--:|--:|--:|--:|
| native | 1710 | 4.2 ms | 17 | **~21 s** |
| web release (200 Mbps) | 236 | 5.7 ms | 51 | **~2.2 s** |

The song+venue milos finish loading in the first ~20–50 vignette frames at
high bandwidth; the rest of the dwell is idle. At 4–8 Mbps the dwell extends
to tens of seconds (research/07: maxSingleMilo 17–35 s — the venue milo
downloads during the vignette), so headroom GROWS exactly where the wall is
worst. The reveal-frame work needs ≈0.1 s (native) to ≈0.9 s (web -O0) of
CPU+upload — it fits in the dwell with large margin at every condition.

---

## 2. Chosen design (3 levers, 3 tasks)

**L1 — per-mesh vertex-unpack cache (engine, steady-state + reveal).**
Stop re-unpacking per draw: static meshes skip the unpack entirely when
`!needUpload` (their unpacked verts have no other consumer); skinned meshes
cache the unpacked bind-pose verts in `RB3MeshEntry`, invalidated by exactly
the conditions that already set `needUpload` (owner/fingerprint/dirty
OnSync), because the shard guard/rebake needs them every frame. Add a
`gVertUnpackMsThisFrame`/count trace counter FIRST and re-measure, so the
residue attribution is pinned by a counter, not only by the profile. Bonus:
steady-state gameplay (web release p50 dt currently **18.99 ms** — under 60
fps) gets the per-frame skinned re-unpack back.

**L2 — loading-dwell GPU warm sweep (engine API + rb3 driver).**
`BandRnd::WarmGpuForDir(ObjectDir* root, float budgetMs)` walks
`ObjDirItr<RndTex>` / `ObjDirItr<RndMesh>` (incl. subdirs) and pushes each
not-yet-resident resource through the SAME upload paths DrawMesh uses
(`UploadRndTexIfNeeded`, the extracted mesh-upload helper) until the
per-call budget is spent; returns #uploaded (0 = nothing left to warm).
The rb3 driver calls it from the vignette's per-frame poll
(GamePanel::PollForLoading / IsLoaded HX_NATIVE arm) once `UIPanel::IsLoaded()`
is true, ≤8 ms/frame, and holds `IsLoaded()==false` until the sweep drains
(bounded by a ~2 s max-hold safety). Because the sweep reuses the draw-path
upload code and cache keys, the reveal frame's lookups all hit. Uploading is
visual-no-op by construction (no draws, no state changes — same buffers the
first draw would create one frame later).

**L3 — reveal-frame drain pre-kick (rb3).**
The reveal frame still synchronously creates+drains one loader (native
`lpu=48 ms`, `pend` 0→1, venue-texture PostLoads). First identify the exact
call site (candidates, in likelihood order: `SyncSubDir`→`ForceGetLoader`
Dir.cpp:903 from the world/venue proxy sync at Enter; `UIPanel::OnLoad` sync
arm UIPanel.cpp:373; `RndTex::SetBitmap`→ForceGetLoader Tex.cpp:222) — a
one-line MILO_LOG in `LoadMgr::ForceGetLoader` + current-screen print, or a
gdb breakpoint on the native run, settles it in minutes. Then kick that same
FilePath as an async `kLoadFront` load during the dwell so Enter's
`GetLoader(fp)` finds it already loaded and the drain no-ops (the designed
backstop behavior). If the path turns out to be Enter-state-dependent,
fall back to accepting the residual (~20–50 ms native-scale) and document.

Flags (house style: default-ON, getenv-once static, `?env=` bridge works):
`RB3_UNPACK_CACHE_OFF` (L1), `RB3_GAMEWARM_OFF` (L2+L3 driver). Engine
web-only code `#ifdef __EMSCRIPTEN__` only; rb3 matched-TU lines all inside
`#ifdef HX_NATIVE`.

### Expected impact (arithmetic)

- **Web release reveal frame**: 1010 ms = 87 ms counters (obj 54.5 + tex 31.0
  + mesh 1.7) + ~923 ms residue. L2 moves the 87 ms + the upload-tied unpack
  residue into ≥2.2 s of measured dwell headroom; L1 makes the moved unpack
  ~one-time; L3 removes the in-frame drain and its JSPI suspends. Remaining:
  steady-frame draw cost (p50 19 ms) + Enter logic → **target ≤120 ms
  (≥88% cut)**, and start-song window rAF over100 = 0 (today 1).
- **Native reveal frame**: 95.4 ms − (48.3 lpu + 17.3 tex + 3.2 mesh moved)
  → **target ≤40 ms**.
- **Steady-state bonus (L1)**: unpack family ≈15–25% of busy draw-path time
  in the profile → web release gameplay p50 18.99 → **target ≤17 ms**.
- Loading wall: unchanged (bytes-bound per research/07) — the dwell absorbs
  the warm work in idle frames; vignette p95 dt must stay <25 ms (budget 8 ms
  on ~5 ms frames).

---

## 3. REJECTED alternatives (and why)

1. **Extend A5 PreWarm key sweep to gameplay materials** (the task's prime
   suspect): REFUTED by measurement — `pipeN` run-total = 0 on native AND
   both web runs; the 240-key sweep is already a verified superset of
   gameplay's draw keys. There is no pipeline compile left to move.
2. **Generic A3 loader-completion slicing (budget Clear()/MergeDirs/PostLoad
   inside Loader.cpp's completion path)**: the loading phase is already ~idle
   (vignette lp Σ=161 ms native over 1710 frames; pend>0 on only 17/1710) —
   the hitch work is NOT on the loader completion path except the single
   Enter drain, which L3 pre-kicks via existing machinery. General slicing
   would touch the invariant-heavy loader core (PLAN §3 invariants 1–7,
   exclusivity/kLoadFront/mAccessed) for no measured win. Narrow > general.
3. **Hidden full pre-render of the venue during the vignette** (one
   draw-without-present warm frame would warm everything through the real
   path): the world cams/state aren't configured until Enter (two-camera
   setup, BandCam, track lighting state) — high crash/visual risk in return
   for the same uploads L2 does deterministically. The dc3-style screen
   prewarm (Q10) covers UIScreens, not the in-song world.
4. **A4 offline pre-converted assets (pre-unpacked verts / LE milos at
   extraction)**: same end-state as L1 for ~10× the tooling surface (asset
   pipeline + cache invalidation + divergence-from-360-truth risk), and it
   does nothing for the drain or texture-upload halves. Revisit only if L1's
   cached-unpack first-fill (one-time, now off the reveal frame) still shows
   up at the 1.5 Mbps DNF condition's CPU floor.
5. **Wire the warm sweep into LoadMgr/DirLoader completion** (upload at
   PostLoad time instead of a dwell sweep): couples the renderer to the
   loader for every dir ever loaded (menus would eagerly upload too,
   regressing boot/hub memory + time); the dwell driver warms exactly the
   one transition that measures hot, with a screen-state gate already polled
   per frame.
6. **"Do nothing — rAF over100 is only 1"**: the 0.7–1.0 s game-content
   freeze at reveal is the single biggest remaining visible hitch in the
   post-Wave-4 journey (everything else over100=0 at 200 Mbps), and it sits
   right where players look. Also the native 95 ms frame is 3× the 33 ms
   budget on a `-O2` host — the same structural work, worth fixing once for
   both.

---

## 4. Implementation tasks

> Sequencing: **T1 (engine) lands first** — T2 calls its API (engine is
> consumed as a working tree; integration does the single pin bump). T3 is
> independent. Files are disjoint across tasks. NEVER touch src/App.cpp.
> Wii gate for any rb3 matched-TU change: `tools/ninja-locked
> build/SZBE69_B8/report.json` match% == baseline (62.88%, 31951 funcs).

### T1 (opus) — engine: vertex-unpack cache + GPU warm API
**Files:** `engine:src/platform/Rnd_Wgpu_RB3.cpp`,
`engine:src/platform/Rnd_Wgpu_RB3.h`.
**Brief:** (1) add `gVertUnpackMsThisFrame`/`gVertUnpackCountThisFrame`
(weak-def counter pattern already used in the TU) charging the unpack block
at :3203-3300, and CONFIRM the reveal/steady attribution before changing
behavior; (2) gate the static-mesh unpack on `needUpload` (no other
consumer); cache skinned bind-pose `GpuVertexSkinned` vectors in
`RB3MeshEntry`, invalidated by exactly the existing `needUpload` fingerprint
conditions (owner ptr, fpVerts/fpFaces/fpSkinned, OnSync dirty) so the shard
guard/rebake reads the cache (memory: skinned-only ≈ a few MB; log under a
DBG env); (3) extract DrawMesh's mesh-upload block into an idempotent helper
and add `BandRnd::WarmGpuForDir(ObjectDir* root, float budgetMs)` → walk
`ObjDirItr<RndTex>`/`ObjDirItr<RndMesh>` incl. subdirs, upload
not-yet-resident textures via `UploadRndTexIfNeeded` (so material bind-time
lookups hit the same per-RndTex cache) and meshes via the helper
(unpack+VB/IB, same `sMeshGpu` entries), stop when budgetMs spent; return
#uploaded, 0 when fully warm. API inert unless called.
**Flags:** `RB3_UNPACK_CACHE_OFF` (default ON = cache active).
**Verification:** rb3-tests 13/13 (gtest, native); new unpackMs field visible
in native+web traces and ≈0 on steady frames with cache ON; native
gameplay screenshots at a fixed songMs>2000 point (poll `/api/health` songMs
— NEVER screenshot at songMs=0, that's the intro cinematic) cache ON vs
`RB3_UNPACK_CACHE_OFF=1` pass `visual_diff.py` below the animation-phase
noise floor, band characters unsharded (SKIN_PROBE clean); web release
game_screen steady p50 dt ≤ baseline (18.99 ms). Engine repo only — no Wii
gate; commit in ../milo-native-engine, do NOT bump MILO_ENGINE_PIN.

### T2 (opus) — rb3: dwell warm driver + reveal-drain pre-kick
**Files:** `src/band3/game/GamePanel.cpp` (HX_NATIVE arms),
`src/band3/game/Game.cpp` (HX_NATIVE, only if the hook needs it),
`native/src/rb3_gamewarm_native.cpp` (new shim),
`native/CMakeLists.txt` (wire the shim TU).
**Brief:** during the vignette, from the per-frame
`GamePanel::PollForLoading`/`IsLoaded` poll (GAME_DBG arms at
GamePanel.cpp:156-220 show the loop), once `UIPanel::IsLoaded()`: call
`WarmGpuForDir` ≤8 ms/frame over the gameplay dir roots until drained, then
report loaded; hold `IsLoaded()` false while sweeping with a ~2 s max-hold
safety. Start with GamePanel's own dir + TrackPanel's dir + the
BandDirector/world dir and verify coverage empirically (reveal-frame
texN 97→≤5, meshN 113→≤5 is the acceptance signal — add roots until true).
Then identify the reveal-frame drain call site (MILO_LOG probe in
`LoadMgr::ForceGetLoader`/`PollUntilLoaded` printing the FilePath + current
screen on the native run; candidates ranked in §2-L3) and pre-kick that
FilePath async (`TheLoadMgr.AddLoader(fp, kLoadFront)`) during the dwell so
Enter's ForceGetLoader finds it loaded; reveal-frame objMs ≤5 ms is the
acceptance signal.
**Flags:** `RB3_GAMEWARM_OFF` (default ON, native+web).
**Verification:** (a) native `frame_profiler.py --into-song`: reveal frame
dt ≤40 ms with texN≤5/meshN≤5/objMs≤5, vignette p95 dt <25 ms; (b) web
release rebuilt+REDEPLOYED then `_netmatrix.mjs` at **8 Mbps/80 ms and
4 Mbps/150 ms** (wave-4 gate policy): journey completes, reveal dt ≤120 ms,
start-song window over100=0, no regression of wave-4 signals
(peakConcurrent≥6, chunkReDownloads=0); (c) visual no-op: native screenshots
at matched songMs>2000 warm-ON vs `RB3_GAMEWARM_OFF=1`, `visual_diff.py`
below noise floor (respect the songMs=0 cinematic gotcha); (d) **audio
MATCH**: the IsLoaded-hold sits on the audio-start path → run
`/audio-verify` on a fresh native gameplay capture (`--rank`, expect MATCH,
pitch 1.000×, clip ~0%); (e) song-end-test.py passes; (f) Wii byte-identical:
`tools/ninja-locked` report == 62.88%/31951 baseline (all edits HX_NATIVE).

### T3 (sonnet) — measurement: web lp/lpu wiring + first-frame gate
**Files:** `src/system/utl/Loader.cpp` (timing wrap inside the existing
`#ifdef HX_WEB` PollUntilLoaded arm + whatever web path feeds the recorder's
lp/lpu args — today every web frame records lp=lpu=0 while objMs>0),
`scripts/web/firstframe-gate.mjs` (new, committed cleanup of the on-disk
probe `scripts/web/_w5-firstframe-profile.mjs`: boot→gameplay, pulls
`/trace.jsonl`, finds the reveal frame, asserts dt/counter/over100
thresholds, optional `--profile` arms the CDP profiler).
**Brief:** make the web reveal frame self-attributing (dt ≈ lp+lpu+counters+
residue on web like it already does on native) and land the regression gate
T1/T2 use, wired for the two canonical conditions (8 Mbps/80 ms,
4 Mbps/150 ms) plus unthrottled.
**Flags:** none (diagnostics under the existing `gFrameTraceActive` gate).
**Verification:** (a) gate harness detects today's reveal frame (exit≠0 at
target thresholds against the current deployed build; exits 0 with
`--baseline` thresholds); (b) after the wrap, a web trace of a cold
transition shows lpu>0 on drain frames; (c) Wii gate: HX_WEB block is not
compiled for Wii, but run `tools/ninja-locked` report == baseline anyway;
(d) native build still links (`flock /tmp/rb3-native-build.lock cmake --build
native/build-native -j$(nproc)`).

---

## 5. Measurement gotchas for this wave (cost prior waves real time)

- **Rebuild + redeploy before ANY web measurement** (stale-deploy false
  bugs); pre-js edits need a forced relink. Native runs under
  `flock /tmp/rb3-native-run.lock`; builds under the build locks.
- **songMs=0 is the intro cinematic** — gameplay screenshots/visual gates
  must poll `/api/health` songMs (native) past 2000 before judging pixels.
- **dt ≠ rAF gap on web** (JSPI suspends inside a logical frame keep the tab
  compositing): gate BOTH reveal-frame dt and the start-song window's
  over100 count.
- **lp/lpu are 0 on web traces until T3 lands** — don't read a web frame's
  drain share from them; objMs/texMs/meshMs/fetchMs are wired and real.
- The reveal-frame dt varies with journey shape (measured 659–1010 ms across
  builds/runs) — compare like-for-like journeys (the gate harness fixes the
  journey), and treat 0.6–1.0 s as the baseline band, not a point.
