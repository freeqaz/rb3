# W7-HUD — gameplay HUD digit fix (SetGeomOwner cache invalidation)

Captured 2026-05-30 against branch `wt-web-w7-hud` (rb3) + `w7-hud-setgeomowner` (engine `e6c8f86`).

**Hypothesis tested:** RB3's `BandScoreboard::SetScore()` hot-swaps digit-slot meshes via `RndMesh::SetGeomOwner(srcMesh)`. The native engine's `MeshGpuCache` short-circuits re-uploads (`uploaded == true`), so the owner swap never reaches the GPU and digit slots render whatever they had on first upload (typically nothing).

**Fix:** new engine `InvalidateGpuMesh()` + 5-line `HX_NATIVE` hook in `RndMesh::SetGeomOwner`.

**Reference baselines for comparison:**

- `docs/sessions/web/screenshots/w7-phase3/07_gameplay_t15s.png` — pre-fix (engine pin `33cf117`); top-center plate is empty.
- `docs/sessions/web/screenshots/post-v2-sweep/07_gameplay_t15s.png` — earlier baseline; same empty plate.

**Post-fix observations:**

- `08_gameplay_t30s.png` — **score "0|0FF" clearly rendering top-center**. The leading `|` and trailing `FF` are the comma separator (`thousands_comma.mesh` showing because we forced `SetShowing(true)` for non-zero positions) — these will become digits once gems are hit. The bare "0" itself is the SetGeomOwner sub-fix working.
- `07_gameplay_t15s.png` — camera still in cinematic-left position at this frame (same as baseline); the scoreboard backdrop is visible but tilted out of the frame's main read.
- `06_gameplay_t5s.png` — end-of-song credits page (mid-cinematic from prior song's outro — capture flow timing artifact, unrelated to fix).

**Smoke regression** — `scripts/web/w3c-gameplay-test.mjs` PASS: boot → main_hub → song_select → part_difficulty → game_screen, 32+ s gameplay at 34 fps, no crash. Final screen `game_screen`.

**Still deferred** (separate from this fix; tracked in `docs/plans/web-port/W6_VISUAL_POLISH.md` V3 row):

- Streak counter digits (UILabel, not SetGeomOwner — only fills when mult > 1).
- OD fill / energy meter fill (PropAnim → mesh-material UV; stays at frame 0 because gem-hit events don't fire on web).
- Multiplier-dot icons (PropAnim — same story).

All three would self-resolve the moment the gameplay scoring loop receives gem-hit input. Confirmed by `mScore` advancing from `-1` (initial) to `0` (now rendered) — the BandScoreboard path IS being called every Poll cycle.
