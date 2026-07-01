# 04 — Mesh-cache v2: validate + promote (P2.2)

> **VERIFIER UPDATE (2026-06-09, later): v2 is ALREADY VALIDATED *and* PROMOTED.**
> The strict frame-pinned native A/B gate PASSED ("fixed matches stock at the
> determinism floor; broken `a0f98ad` systematically 2x worse, localized to the
> multi-instance regions") and rb3 commit `91468cd5` ("perf(native): bump engine
> pin …", 2026-06-09 19:38) **already set `MILO_ENGINE_PIN` to `b5309b3`** —
> which is also current engine HEAD. So the "Remaining work" section below
> (especially step 7, the pin bump) is **mostly historical**; the pin bump is
> DONE. What may still be open is the *broader* re-validation (web crashrate
> sweep, gameplay multi-draw capture, perf numbers) called out in steps 3-6 — do
> those as confidence checks, not as a gate to a bump that already landed.
> NOTE: the engine repo's default branch is **`main`**, not `master`.

## Status discovery (2026-06-09): v2 is ALREADY IMPLEMENTED

- v1 = engine `a0f98ad` (per-mesh VB/IB cache + per-mesh persistent uniform buffers).
  FAILED the strict pixel gate: a mesh drawn N×/frame rendered every instance with the
  LAST instance's uniforms (WebGPU `queue.WriteBuffer` executes before submit →
  shared per-mesh objUB/matUB/boneUB hold final values for all draws). Symptom:
  darkened song_select rows, mean Δ ≈ (−7,−11,−12). Record:
  `docs/native/songlib-web-gpu-device-lost-2026-06-09.md` + commit `d03f041e`.
- v2 = engine `b5309b3` (2026-06-09, current engine HEAD on branch `main`): keeps
  the VB/IB cache, adds **per-instance uniform slots** — `RB3MeshEntry.slots`
  (`std::vector<UniformSlot>`) free-list, per-frame lazy reset via global
  `sFrameSeq` (`frameSeen`/`nextSlot`), each draw claims its own
  objUB/boneUB/matUB+bind-groups. Material BG rebuilt only on material/view change.
  Opt-out `RB3_NO_MESH_CACHE=1` restores legacy per-draw RAII (the old leak) for A/B.
  Verified line refs (Rnd_Wgpu_RB3.cpp): `struct RB3MeshEntry` 360-418 + `slots`
  vector at 411 + `frameSeen`/`nextSlot` 416-417; `static …sMeshGpu` 419;
  `static uint64_t sFrameSeq` 434.
- Invalidation: `RndMesh::OnSync(int)` clears `uploaded` (Rnd_Wgpu_RB3.cpp:4406-4416);
  fingerprint (ownerKey/fpVerts/fpFaces/fpSkinned) catches SetGeomOwner swaps.
  `BeginFrame` does `sFrameSeq++` (Rnd_Wgpu_RB3.cpp:1147-1151). Destruction:
  strong `CleanupGpuMesh` (Rnd_Wgpu_RB3.cpp:440-442) erases `sMeshGpu`; rb3
  `~RndMesh` (src/system/rndobj/Mesh.cpp:349-355 — call at :354, HX_NATIVE) calls it.
  Shutdown clears the map (Rnd_Wgpu_RB3.cpp:829-830: `sTexGpu.clear(); sMeshGpu.clear();`).
  Skinned chars animate via bone palette, NOT vert re-upload, so caching bind-pose
  verts is safe; skinned unpack still runs unconditionally for the shard-guard (only
  the GPU upload, gated `if (needUpload)` at :3208, is conditional).
- v1's measured win stands: total GPU buffers ~97k-and-climbing → ~3.4k steady;
  song_select no longer crashes the GPU process.

## Remaining work (this handoff)

1. **Confirm tree state**: engine `main` HEAD is `b5309b3` (VERIFIED — working tree
   CLEAN, not dirty); rb3 `MILO_ENGINE_PIN` already `b5309b3` (committed in rb3
   `91468cd5`). Nothing newer/dirty. (This was the halt-on-dirty guard; it's clear.)
2. **Strict visual gate** (the one that failed v1):
   ```
   python3 scripts/analysis/visual_diff_capture.py \
     --a native:0 --a-bin <baseline rb3-native built at current pin> \
     --b native:0 --b-bin <candidate rb3-native built at b5309b3> \
     --mode strict --screens main_hub,song_select --threshold 0.10
   ```
   (Read `docs/native/visual-diff-tooling.md` for exact flags/canonical screens.)
   Must PASS. Also eyeball the heatmaps for the song-row signature specifically.
3. **Steady-state resource check**: `RENDER_DBG`-style per-frame log shows
   `buf_creates=0 bg_creates=0` after geometry settles on song_select + in-song.
4. **Multi-draw correctness beyond song rows**: capture gameplay (gems are multi-drawn
   meshes) — `scripts/native/gameplay-depth-capture.py` or `_framepin_capture.py`
   game screen; compare vs baseline.
5. **Web validation**: one web build; `_songlib-crashrate.mjs` (N=5) through
   song_select; confirm no device-lost regression and buffer counts stay flat
   (`_songlib-wgpu-interpose.mjs` exists for buffer counting).
6. **Perf numbers**: native `frame_profiler.py --scroll 10 --into-song` cache-on vs
   `RB3_NO_MESH_CACHE=1`; web `audio-jitter-profile.mjs --play-secs 50` — record rAF
   p50/p95 and underruns/s delta. This is the first real evidence for whether render
   perf was the audio-jitter root cause (feeds roadmap P0.2/P2 decision).
7. **Promote**: ~~bump `MILO_ENGINE_PIN`~~ — **ALREADY DONE** (rb3 `91468cd5` set the
   pin to `b5309b3`, the validated engine SHA, after the strict gate passed). A
   later 05/06 bump can move it forward again. Remaining: update the songlib/STATE
   docs with v2-validated status + numbers if not already reflected.

## Risk notes

- Slot vector growth is bounded by max instances-per-frame of one mesh; sanity-check
  `slots.size()` for the worst mesh in the RENDER_DBG log (expect ≤ ~100s, not 1000s).
- Halo-bloom capture path holds slot handles until EndFrame — verified in v2 design,
  but re-test with bloom ON (it's default-on; RB3_HIGHWAY_BLOOM_OFF to A/B).

## IMPLEMENTED (2026-06-09) — native confidence checks DONE (measurement-only)

Status: steps 3, 4 and the native half of 6 are COMPLETE — full numbers +
verdicts in **[04-results.md](04-results.md)**. No code changed, no commits made
(measurement-only pass); pin bump was already done (`91468cd5`). Step 5 (web
crashrate sweep) + web half of 6 SKIPPED per instruction (web build dirs
contended) — still open.

Measured (clean isolated build: rb3 `4c6b8500` + engine `b5309b3`; box load
avg ~6–12, relative deltas only):
- Steady state: `buf_creates=0 bg_creates=0` on song_select; in-song residual
  0–6 buf-creates/frame (one dynamic mesh VB+IB), bg=0; cached_meshes bounded
  183–218 (menus) / 342–346 (in-song). OFF-arm contrast: ~1.2k–4.1k buf +
  ~0.7k–1k bind-group creates PER FRAME.
- Multi-draw correctness: PASS at determinism floor. song_select cross-arm
  diff inside same-arm floor, signed means flip sign (no v1 darkening
  signature). Gameplay highway (songMs-pinned 3500/6000/9000): gems
  individually colored both arms; best-aligned pair mean Δ≈(−0.4,−0.4,−0.4).
- Perf: p50 **9.96 → 23.25 ms** (song_select) and **9.57 → 21.97 ms**
  (gameplay) ON→OFF — cache is a ≈2.3× p50 win, 2.8× at gameplay p99.
- Verdict: plausibly THE web 25 fps lever (≈40 ms/frame OFF-class → ≈50+ fps
  ON-class if web bottleneck matches native profile); feeds P0.2/P2.

Deviations / gotchas for the next agent:
- `slots.size()` is not logged by the pinned build; bounded indirectly
  (steady `bg_creates=0` ⇒ slot pools stopped growing).
- Shared `native/build-native/rb3-native` was rebuilt mid-session by a
  concurrent agent (uncommitted Rnd_Wgpu_RB3.cpp/Tex.cpp edits) — all numbers
  came from an isolated pinned build at `/tmp/meshcache-eval/build/`.
- NEW pre-existing bug (not cache-related): into-song crash on some songs
  (`beencaughtstealing`, `breakonthrough`) — TrackInfo OOB assert,
  `NewTrackWatcherImpl` TrackWatcher.cpp:35 ← `Game::PostLoad`. Deterministic,
  reproduces with cache ON and OFF. Needs its own ticket.
