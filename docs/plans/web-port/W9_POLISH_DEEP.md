# W9 — Post-W8 Deep Polish Handoff

**Status at handoff (2026-05-30):** RB3 boots in browser, plays one song
end-to-end with menus + text + score visible. The W4-W8 polish wave landed
the "easy wins" surface; what remains needs deeper engine/runtime probes.

| | |
|---|---|
| **Master HEAD** | `dca650c9` |
| **Engine HEAD** | `749bf92` (pinned in `native/CMakeLists.txt:74`) |
| **Live smoke** | `w3c-gameplay-test.mjs` PASS — splash → main_hub → song_select → part_difficulty → gameplay, 30+ fps, MOGG decrypts + plays, no crashes |
| **Dev port reserved** | 8421 (user). Worktree agents allocate via `tools/setup-worktree.sh` → `.worktree-port` (hashed from name, range 8500-8999) |

## What works after W4-W8

- Splash → keyboard nav → song select → part select → gameplay loop end-to-end
- Song titles + artists visible in song_select (W6-V1 `plain-UILabel` fallback)
- Main-hub menu items visible (W6-V2 `AnimTask::Poll` `SetFrame` arg-swap)
- Album art loads via server-side fallback to `extracted-xbox-full/` (W7-V4)
- Score digits render in gameplay HUD (W7-HUD `MeshGpuCache::InvalidateGpuMesh`
  on `SetGeomOwner` — also fixes 4 latent sites: Tail / VocalTrack /
  ArpeggioShape / NoteTube)
- Loading overlay before WASM boot, hides on first real frame (W4c)
- Release WASM 2.21MB brotli (W4a), IDB cache 95% warm hit (W4b)
- Worktree tooling: deterministic port hash, engine/node_modules/orig-assets
  symlinks, `bin/` overlay silenced

## Open work — priority order

### P1.1 — Scene-wide brightness (engine post-processing) — HIGHEST LEVERAGE

The W8 Phase 3 reassessment proved every screen is 2.6× darker than Wii reference
(avgLuma 34 vs 90; 95.6% dim on song_select). Not text-specific —
**scene-wide**. Likely a missing post-processing pass (bloom, gamma correction,
tone mapping) or wrong ambient-light setup.

- Hypothesis 1: missing bloom pass — Wii engine does bloom; WebGPU backend
  doesn't have one wired (or one is wired but disabled/broken).
- Hypothesis 2: gamma — content is authored against sRGB → display, but our
  backend writes linear (or vice versa).
- Hypothesis 3: ambient light intensity set wrong — `RndLight::ambient` or
  similar default lower than retail.
- Hypothesis 4: post-processing pipeline missing tone-mapping operator.

**Method:**
1. Compare `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp::WriteSceneUniforms`
   against retail behavior. Check what `mu.lightAmbient`, `mu.diffuseColor`,
   gamma/exposure values are.
2. Look at `milo-native-engine/src/gfx/standard_wgsl.inc` for any
   post-processing flags (sRGB conversion, bloom toggle).
3. Compare with DC3 engine — DC3's `Rnd_Wgpu.cpp` may have bloom or other
   post-processing the rb3 backend skips.
4. Try forcing tone-map / gamma-correction on, screenshot before/after.
5. If pure-color floor is needed (no post-process available), expand the W7
   Tier 1 predicate — but that was scene-wide, not text-specific, so
   pure color lift would affect everything (acceptable trade-off?).

**Files:** engine `src/platform/Rnd_Wgpu_RB3.cpp`, `src/gfx/standard_wgsl.inc`,
DC3's `Rnd_Wgpu.cpp` for comparison.
**Scope:** M (engine investigation + 1-2 commits).
**Risk:** zero decomp-match risk (engine-only).
**Win if fixed:** every screen brightens proportionally — biggest visual
quality jump available right now.

### P1.2 — `Player::SetEnergyAutomatically(0.000)` flood

Per W8-meters: this fires ~30Hz on the active player throughout gameplay,
stomping any accrued band energy → OD/SP meter never fills. Caller is hidden
behind `Player::SetEnergyFromNet` or `set_energy_automatically` DTA action.

**Method:**
1. Probe `Player::SetEnergyFromNet` entry — does it fire 30Hz on web?
2. Probe `Player::HandleType` dispatch for `set_energy_automatically` action
   key — which DataNode source?
3. Check for a `remote_update_energy` message handler that shouldn't fire in
   offline play (V2-style `IsLocal()` gate may be missing — see W6-V1
   pattern).
4. Likely fix: `#ifdef HX_NATIVE` skip the auto-update when offline.

**Files:** `src/band3/game/Player.cpp`, `src/band3/net_band/` or wherever
`SetEnergyFromNet` lives.
**Scope:** S — once caller identified, single HX_NATIVE gate.
**Risk:** zero decomp-match risk.
**Win:** SP/OD meter fills correctly during play.

### P2.1 — V15 main-hub button stacking — draw-time mesh probe

W8 RndCam fix proved fovScale isn't the cause. Buttons are in correct world-Z
positions (29-unit intervals); something between world-Z and screen-Y compresses
the rendered text positions. W7 BTNPROBE only sampled at `MainHubPanel::Enter()`
— need draw-time probe.

**Method:**
1. Engine-side probe in `BandRnd::DrawMesh` — log mesh name + world position
   matrix when name matches `mb_*_label*` or similar.
2. Compare to the per-button `.btn` group's world Z (104.4 / 75.4 / 46.7).
3. Two likely causes:
   - `LabelShrinkWrapper` re-positions text relative to a parent that
     collapses height.
   - The text-rendering camera is different from the geometry camera (separate
     RndCam for UI overlay vs scene), with different projection.
4. Once identified: surface the right cam or fix the wrapper.

**Files:** engine `BandRnd::DrawMesh`, `src/system/ui/LabelShrinkWrapper.cpp`,
`src/system/ui/UILabel.cpp`.
**Scope:** M — investigation likely surfaces a clean fix.
**Risk:** low (HX_NATIVE-gateable).
**Win:** main-hub menu fully usable (all 5 items at correct positions).

### P2.2 — Streak `×N` label render refresh

Per W8-meters: `StreakMeter::UpdateMultiplierText(mult)` fires with correct
values (2, 3, 4); `mMultiplierLabel`/`mXLabel` are non-null; widget `showing=1`
— label data writes but render never refreshes. Same class as W7-HUD's
`SetGeomOwner` cache bug, applied to **UILabel-text-mesh** instead of digit
sprite-quad.

**Method:**
1. Read `UILabel::SetDisplayText` / `SetTokenFmt` chain.
2. Find where it touches `RndText::SetText` or equivalent.
3. Add an engine-side cache invalidation hook similar to W7-HUD's
   `InvalidateGpuMesh` — when label text changes, the glyph mesh upload must
   re-fire.
4. Test with multiplier streak ramp during gameplay.

**Files:** engine `MeshGpuCache.{h,cpp}` (extend W7-HUD's pattern),
`src/system/ui/UILabel.cpp` (the `SetDisplayText` hook).
**Scope:** S (mirror W7-HUD's pattern).
**Risk:** zero decomp-match risk.
**Win:** streak `×N` text appears + updates live during play.

### P3 — Multiplier 5-dot icons (defer or skip)

`mMultiMeterAnim->SetFrame(mult)` has zero callers in source. Retail behavior
is DTA-trigger-driven or milo-internal. Either:
- DTA action missing from extracted assets (data issue, not code).
- We need to call `SetFrame` ourselves on multiplier change (rb3-side patch).

Defer until P2.2 is resolved — if `UILabel` refresh fixes both label families,
this may resolve incidentally.

### P3 — Song-title brightness within scene-wide context

After P1.1 ships, re-measure song-title brightness. Likely resolved as part
of the scene-wide lift; if STILL specifically dim relative to the rest of the
scene, fall back to the W5/W7 text-predicate path.

## Reference docs (deeper context per item)

- `docs/plans/web-port/W6_VISUAL_POLISH.md` — full visual backlog with V-numbered table
- `docs/plans/web-port/W7_CURSOR_HUDBARS.md` — W7-cursor-hud negative result
- `docs/plans/web-port/W8_MWCC_ARG_SWAP.md` — arg-swap class verdicts (all 9 RULED-OUT)
- `docs/plans/web-port/W5_TEXT_RENDERING.md` — Phase 1/2/3 text history (Phase 3 STILL DIM verdict)
- `docs/sessions/web/screenshots/w8-rndcam-fix/README.md` — V15 draw-time-probe handoff
- `docs/sessions/web/screenshots/w8-meters/README.md` — HUD-meter 3-gap analysis

## Session-wrap memory hits

The orchestration memory entry (`project_web_port_orchestration.md`) captures:
- Subagent model tiering (Opus complex, Sonnet straightforward, Haiku read-only)
- Worktree-isolation pattern (`tools/setup-worktree.sh <name>` → deterministic port via `.worktree-port`)
- Doc-as-handoff convention (each phase doc self-contained; planning agents update status)
- "Dim ≠ not-drawn" diagnosis pattern — verify with paint-% per stripe + RGB sampling, not visual impression
- V2 arg-swap class is genuinely narrow — don't go hunting more without symptom evidence

## Orchestration patterns that worked this session

- **Fan-out by file territory** — 3-5 parallel Opus agents per wave, scoped to
  disjoint dirs (e.g. `src/system/char/` vs `src/band3/bandtrack/`); explicit
  "watch out" lines in dispatch prompts to avoid stomping.
- **Dual-worktree for engine fixes** — `git worktree add /tmp/milo-engine-<x>`
  + rb3 `setup-worktree.sh <name>` + `-DMILO_ENGINE_PATH` override; agent
  reports both diffs; orchestrator ff-merges engine then bumps pin.
- **Negative results are valuable** — RULED-OUT verdicts go in the roadmap so
  future agents don't re-investigate. W8 RGTrainerPanel TODO comment is the
  template for "real bug, not worth fixing right now."
- **Verify before believing** — multiple "dim text" + "missing widget" claims
  turned out to be wrong on inspection. Always run a fresh screenshot capture
  + numeric measurement, not visual impression.
- **Investigation-first** — every "obvious bug" this session was misdiagnosed
  initially. ~30min of static analysis saved hours of failed fix attempts.
