# task-fret-held — held frets now light the now-bar smasher (impl)

**Issue key:** `fret-held` · **Wave-2 implementer (opus)** · 2026-06-11
**Status:** DONE · verified=true · **engine-repo fix** (no rb3 game-side change)
**Ports used:** 8781-8785 · **Evidence:** `/tmp/rp2-fret-held/evidence/`

---

## TL;DR

Holding a fret now lights that slot's smasher button with its correct per-slot
colour (green/red/yellow/blue/orange) on the now-bar; release turns it off. The
fix is a one-line semantic correction in the engine's standard shader's emissive
term, plus a tiny `RB3_FRET_GLOW_OFF` opt-out in the RB3 backend. **No game-side
(`src/`) change was needed** — the scout's input/anim chain analysis held up; the
bug was purely in how the renderer applies the emissive map.

## What changed (files + why)

Both changes are in **`milo-native-engine`** (engine repo). The rb3 repo only gets
the `MILO_ENGINE_PIN` bump. The shared Wii-matched `src/` tree is **untouched**
(byte-identical Wii build; `git status src/` is clean).

1. **`src/gfx/standard_wgsl.inc`** (the fix). The emissive (self-illumination)
   term was:
   ```wgsl
   finalColor += baseColor.rgb * material.emissiveMultiplier * emissiveSample.rgb;
   ```
   `baseColor.rgb = material.color.rgb * vertexTint`. For `gem_smasher_glow.mat`
   the authored material **color is (0,0,0)** (its per-slot colour lives in the
   emissive MAP, not the base colour), so `baseColor.rgb == 0` zeroed the entire
   emissive term → the glow contributed nothing under additive blend → invisible.
   New code tints emissive by baseColor as before for materials that intend it,
   but falls back to a white tint as the base approaches black:
   ```wgsl
   let emBaseLuma = max(max(baseColor.r, baseColor.g), baseColor.b);
   let emissiveTint = mix(vec3f(1.0), baseColor.rgb, smoothstep(0.0, 0.04, emBaseLuma));
   finalColor += emissiveTint * material.emissiveMultiplier * emissiveSample.rgb;
   ```
   Emissive is self-illumination; a black diffuse base must not collapse it to
   zero.

2. **`src/platform/Rnd_Wgpu_RB3.cpp`** (`BandRnd::DrawMesh`, the gem_smasher_glow
   branch in the `sTrackLight && game.cam` block). Added the `RB3_FRET_GLOW_OFF=1`
   opt-out: when set, the smasher's emissive multiplier is forced to 0 (restoring
   the invisible old behaviour) for clean A/B. Default-on keeps the existing ×2.0
   now-bar emissive boost from `f5ee015`.

3. **rb3 `native/CMakeLists.txt`** — `MILO_ENGINE_PIN` 8fb669d → bcf862f.

## Branches + commit SHAs

- **engine** worktree `wt-task-fret-held` →
  `bcf862f681ad24b86c74c0b7235a7e374e6d6335`
  (`gfx(rb3): held-fret smasher glow — emissive self-illum survives black base`)
- **rb3** worktree `wt-task-fret-held` → `e85d4b97`
  (`build(native): bump MILO_ENGINE_PIN to bcf862f`)
- rb3 worktree: `/home/free/code/milohax/rb3/.claude/worktrees/task-fret-held`
- engine worktree:
  `/home/free/code/milohax/milo-native-engine-worktrees/task-fret-held`

## Diagnosis — corrected the scout's root cause (with evidence)

The scout said the glow was black because the per-slot recolor sets the
**diffuse** texture and that wasn't landing. The probe (FRET_DBG in
`GemSmasher::SetGlowing`, since removed) proved a **different, more precise**
root cause:

- `particle_slot_colors.anim` **exists and runs** (numPropKeys 20/24) and its
  glow-mat key (`key[16]/[20]`, type=kObject) targets `gem_smasher_glow.mat`'s
  prop **`(emissive_map)`** — NOT `diffuse_tex`.
- At the `SetGlowing(true)` rising edge the glow mat already reads
  `color=(0,0,0)`, `blend=2(kBlendAdd)`, **`emMap='square_smasher_bright_<color>.tex'`,
  `emMul=0.90`** — the per-slot bright texture IS correctly bound to the emissive
  map. So the anim→material apply path works; the diffuse tex is (correctly) null.
- The renderer then **zeroed the emissive** via `baseColor.rgb *` (black base).

So this is a **shader emissive-application** bug, not an anim/apply gap and not a
diffuse-texture gap. (The scout's Option A "fix the anim→material apply" and
Option B "additive heuristic" were both unnecessary — the targeted shader fix is
the faithful one.)

## Verification (commands + results)

Deterministic A/B (probe `RB3_FORCE_GLOW=<slot>` forced each smasher to glow
every frame, isolating the render fix from input timing; probe was reverted, not
committed). Capture: `/tmp/rp2-fret-held/capture_forced.py <port> <out> [bin]`.

- **BEFORE** (`RB3_FRET_GLOW_OFF=1`, green forced): green smasher flat/unlit —
  `evidence/01_BEFORE_green_held_no_glow.png`.
- **AFTER** (fix on, per slot forced):
  - green → `evidence/02_AFTER_green_held_glow.png` (solid green glow)
  - red → `03_AFTER_red_held_glow.png`
  - yellow → `04_AFTER_yellow_held_glow.png`
  - blue → `05_AFTER_blue_held_glow.png`
  - orange → `06_AFTER_orange_held_glow.png`
  Each slot lights with its **correct per-slot colour**; others stay unlit.
- Also confirmed via real **held-fret input** (the campaign repro,
  `/tmp/rp2-fret-held/drive_hold.py <port> <bit> <out>`): holding green produced a
  bright green smasher glow that vanished with `RB3_FRET_GLOW_OFF=1`.

**Regression proof (no venue/other-emissive change).** The shader change only
affects materials whose base is essentially black. From a runtime `RB3_LIGHT_PROBE`
dump of every emissive material in gameplay, the **only** black-base one is
`gem_smasher_glow.mat`. For every other emissive material the base ≥ 0.32
(traffic streaks), so `smoothstep(0,0.04,maxBase) == 1.0` exactly →
`emissiveTint == baseColor` → **byte-identical** to the old formula. Buildings,
signs, neon, traffic streaks, prism gems, surface watermark, amps: unchanged.
And any (hypothetical, in another scene) black-base emissive material emits ZERO
today, so the change can only **add** a missing glow, never break a visible one.
Visually confirmed: full gameplay scene (`07_full_scene_fix_on_no_regression.png`)
and a fix-vs-revert venue comparison both render the venue/prism-gems/hit-FX
identically. Hit-FX/autohit smasher glows still render (same emissive path).

## LANDING NOTES (for the orchestrator)

- **Order:** land engine `bcf862f` first (make its SHA reachable on engine main),
  then the rb3 pin-bump commit `e85d4b97`. Standard one-way-dep rule.
- **rb3 conflict surface:** rb3 commit touches ONLY
  `native/CMakeLists.txt:MILO_ENGINE_PIN`. If a sibling task also bumps the pin,
  resolve by pointing the pin at whichever engine commit is landed last
  (cumulative); there is no other rb3 conflict (no `src/` edits).
- **Engine conflict surface:** two files.
  - `src/gfx/standard_wgsl.inc` — single emissive line in `fs_main` (line ~800).
    Low collision risk unless a sibling also edits the emissive/final-color block.
  - `src/platform/Rnd_Wgpu_RB3.cpp` — the `gem_smasher_glow.mat` branch inside the
    `sTrackLight && game.cam` block in `BandRnd::DrawMesh` (~line 4498). This is
    the same hot block other render-polish tasks (track lighting / bloom / SP
    overlay) edit — **watch for context conflicts** there; my hunk only changes
    the `gem_smasher_glow.mat` `if`. The pre-existing ×2.0 now-bar boost is kept.
- **Coupling with the track-light gate:** the smasher emissive multiplier is set
  only inside the `sTrackLight && game.cam` block, so `RB3_TRACK_LIGHT_OFF=1`
  also suppresses this glow (pre-existing behaviour — that env is the master
  gameplay-look opt-out). The dedicated `RB3_FRET_GLOW_OFF=1` is the per-feature
  opt-out.
- **Cache-var caveat (cosmetic):** in my worktree the configured build dir still
  shows a stale `MILO_ENGINE_PIN` (it's a `CACHE STRING`, so the new source
  default doesn't override an existing cache entry → a benign configure WARNING).
  A fresh configure / the orchestrator's clean build picks up bcf862f. The
  committed source value is correct; build is green.

## Harness / probe artifacts (not committed)

- `/tmp/rp2-fret-held/capture_forced.py` — nav-to-gameplay + capture (forced-glow
  probe driven via env; no input-timing dependence).
- `/tmp/rp2-fret-held/drive_hold.py`, `hold_clean.py` — real held-fret input
  drivers (kept for re-verification).
- The two probes (FRET_DBG dump in `GemSmasher::SetGlowing`; `RB3_FORCE_GLOW` in
  `NowBar::Poll`) were used for diagnosis/verification and **reverted** —
  `git status src/` is clean.
