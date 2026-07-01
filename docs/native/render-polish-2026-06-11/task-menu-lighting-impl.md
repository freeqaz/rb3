# task-menu-lighting — impl (Wave 2)

Implements the scout's **Fix 1** (honor `RndMat::mUseEnviron`) + **Fix 2**
(material emissive on all cameras) from `scout-menu-lighting.md`. Fix 3 (venue
heuristic re-tune) and Fix 4 (`neon_arcade.mesh` green slab) are **out of scope**
this wave (separate sibling tasks). No deviation from the scout's design.

## Result

VERIFIED. Engine-only change → **rb3 `src/` byte-identical, zero Wii-match
impact**. The menu (hub) backdrop now renders unlit neon/signs/posters at full
authored colour and shows self-lit windows/marquees; gameplay highway
(`game.cam`) is unchanged; song-select UI unchanged. The one regression check I
could **not** capture (score screen) is blocked by a **pre-existing**
native-NetSession crash that reproduces **identically on the base engine** — not
my change.

## What changed (engine repo only)

All edits in the paired engine worktree
`/home/free/code/milohax/milo-native-engine-worktrees/task-menu-lighting`
(branch `wt-task-menu-lighting`), engine commit **`c5c94a2`** (full
`c5c94a2b73f88ea5312a7d75696b02b50c6e3d83`), based at engine HEAD `8fb669d`.

1. `src/gfx/UniformStructs.h` — added `float unlit;` to `MaterialUniforms`,
   consuming one of the three `_padMat` slots (`_padMat[3]` → `_padMat[2]`).
   **Size stays 192**; the `static_assert(sizeof(MaterialUniforms) == 192)`
   passes at compile time (build is green).
2. `src/gfx/standard_wgsl.inc` — mirrored the struct: `_padMat1` → `unlit`
   (keeps the WGSL std140 layout in lockstep). In `fs_main` final compose, the
   prelit branch now ORs the new flag:
   `if (isEnabled(material.prelit) || isEnabled(material.unlit)) { finalColor = baseColor.rgb; }`
   For an unlit static venue mat, the CPU side already forces a **white**
   `vertexTint` (non-prelit static mesh path), so `baseColor.rgb` =
   `material.color × texture` — exactly the Wii `GX_SRC_REG` / `mUseEnviron==0`
   register-colour-only behaviour. Emissive/rim/reflection adds still run after
   it (night-city glow preserved).
3. `src/platform/Rnd_Wgpu_RB3.cpp` `BandRnd::DrawMesh`:
   - **Fix 1** (next to `mu.prelit`, inside the `if (mat)` block):
     `mu.unlit = (!mat->mUseEnviron && !mat->mPreLit) ? 1.0f : 0.0f;`
     (`mUseEnviron`/`mPreLit` are bitfields on RB3's `RndMat`,
     `src/system/rndobj/Mat.h:306/308` — verified; the engine include path
     `${REPO_ROOT}/src/system` resolves to the rb3 worktree's `Mat.h`).
     Text/UI keep going through the prelit path (`isTextMeshHeur`), unaffected.
   - **Fix 2**: moved the baseline
     `mu.emissiveMultiplier = emTex ? mat->mEmissiveMultiplier : 0.0f;`
     OUT of the `game.cam`-only `sTrackLight` block into the general material
     setup (so it runs for `world.cam` / every camera). The game.cam-only boosts
     (`gem_smasher_glow` ×2, `peakstate` ×2) stay in the `sTrackLight` block and
     now multiply that baseline. The emissive map view is already bound for
     every draw (binding 5, `MakeMaterialBindGroup`), so no bind-group/ring
     changes. `IsHaloSourceMat` (bloom) reads the RndMat fields directly under
     its **own** `game.cam` guard at the call site — unaffected (confirmed:
     it never reads `mu.emissiveMultiplier`).

## Branches + commits

| repo | branch | commit | what |
|---|---|---|---|
| milo-native-engine | `wt-task-menu-lighting` | `c5c94a2` (`c5c94a2b73f88ea5312a7d75696b02b50c6e3d83`) | Fix 1 + Fix 2 |
| rb3 | `wt-task-menu-lighting` | `ee94fbe2` | `MILO_ENGINE_PIN` bump 8fb669d → c5c94a2 |

Engine wt path: `/home/free/code/milohax/milo-native-engine-worktrees/task-menu-lighting`.
rb3 wt path: `/home/free/code/milohax/rb3/.claude/worktrees/task-menu-lighting`.

## Verification

A/B was a controlled same-binary-config comparison: I built the **BEFORE**
binary from the engine worktree at base `8fb669d` (the 3 fix files reverted to
base) into a throwaway `native/build-before/`, and the **AFTER** binary
(`native/build-native/`) with the fix. Both clang, same flags. (build-before was
deleted after capture to free the tmpfs quota; rebuild it by reverting the 3
files to `8fb669d^{the fix's parent}` if you need to re-A/B.)

Evidence dir: `/tmp/rp2-menu-lighting/` (`before/`, `after/`, `gameplay-*`,
`songselect-*`, `score-*`; `analyze.py`, `score-capture.py`, and the
worktree-pointed `hub-series-*.py` capture harnesses).

### Menu hub (PRIMARY) — frame-locked hub-series, 12 camera-shot frames each

- **BABOON NEST tent shot (`von_f00450.png`)** — clearest matched pair: AFTER's
  tent/poster (unlit `flat_tent`/`tattoo` mats) jump to full authored amber
  (backdrop mean RGB ~0.20 → ~0.35) and the **marquee bulb-string glows**
  (emissive now live on world.cam). Both already warm (R:B 1.80→1.59, near
  retail 1.76); the win here is **brightness/glow restoration**, scout symptom
  "tent/poster wall half brightness".
- **Walking-band shot (`von_f01050.png`)** — **contrast restored 3.3:1 → 12.5:1**
  (scout pass criterion #2 target ≥5:1; retail ~9:1); band reads against a dark
  night street; green slab (the **sibling** `neon_arcade.mesh` bug) absent in
  this AFTER frame.
- `von_f02100.png` — BABOON marquee + walking band: AFTER brighter sign, glowing
  marquee bulbs, much-reduced green wash.
- **A/B sanity (scout criterion #7)**: on the AFTER build, `RB3_VENUE_LIGHT_OFF=1`
  vs ON mean-luminance delta is now **0.013–0.120** (frame 1500 nearly invariant
  at 0.013), vs the scout's pre-fix ON 0.12–0.28 / OFF 0.23–0.41 (big). The
  unlit majority no longer depends on scene lights — exactly the intended
  semantic. Residual delta = the `ue=1` minority + venue heuristics (Fix 3
  follow-up).

Caveat on aggregate metrics: hub-series frame-locking drifts slightly between
the two builds (the camera loop pacing differs a few frames), and the green-slab
**sibling** bug inflates G/green-excess in several shots — so the aggregate R:B
and per-frame contrast table in `analyze.py` is noisy. The **content-matched**
shots above (tent f450, band f1050/f2100) are the decisive evidence.

### Regression sweeps

- **Gameplay** (`scripts/native/keyboard-to-gameplay.py --game-burst …`, hard,
  guitar) — reached `game_screen` at the same songMs (~17036) on both builds.
  `gameplay-{before,after}/07_playing.png`: highway (game.cam) is
  **byte-identical-ish** (dark track + bright lanes + gems unchanged — track-light
  block untouched). The **venue backdrop** (world.cam) improves: the "CORK" bar
  sign + red signage now glow (same unlit+emissive fix). **No washout** on the
  focal track. Bloom (`IsHaloSourceMat`, game.cam-gated) confirmed unaffected.
- **Song select** (`scripts/native/song-select-capture.py`) —
  `songselect-{before,after}/native_depth_08.png` are visually identical (UI
  chrome = force-prelit text + panels; my flags don't touch it). No regression.
- **Score screen** — **could not capture** in EITHER build: jump-to-end reaches
  `endgame_waiting_screen` then aborts at
  `src/band3/meta_band/MetaPerformer.cpp:1079 Error: netServer`
  (data stack: `ui/endgame/endgame_helpers.dta:64 meta_performer`). This is a
  **pre-existing native NetSession-shim gap** (cf. memory
  `project_song_end_gameover_native`), reproduces **identically** on the base
  `8fb669d` BEFORE binary (`score-before/engine.log` SIGABRT at the same dta
  line). NOT a regression from this change — my fix doesn't touch that path.

## Pass-criteria scorecard (scout §4)

| # | criterion | result |
|---|---|---|
| 1 | warmth R:B → ≥1.4 | tent shot already ~1.6–1.8 (warm); brightness restored. Aggregate noisy (green slab + frame drift). Partial — directionally correct |
| 2 | contrast ≥5:1 | f1050 **3.3 → 12.5** ✓ (varies per shot) |
| 3 | lit-sign zone lum ≥0.30 | per-cell grids show AFTER brighter sign cells; mixed by frame |
| 4 | neon/glow visible | ✓ marquee bulbs + signage now glow (tent/band shots) |
| 5 | no green slab ≥5% | **out of scope** (Fix 4 sibling). Slab reduced where my fix dims the over-lit version, but the mesh-decode bug is unowned here |
| 6 | regression gates | gameplay highway unchanged ✓, song-select unchanged ✓, UI text unaffected ✓; score screen blocked by pre-existing crash (non-regression proven) |
| 7 | venue ON/OFF delta now small | ✓ 0.013–0.12 (was 0.1+ scout) |

## LANDING NOTES (for the orchestrator)

- **Commit order**: land engine `c5c94a2` FIRST (push/merge so the SHA is
  reachable), THEN the rb3 pin bump `ee94fbe2`. The rb3 commit ONLY changes
  `native/CMakeLists.txt` (`MILO_ENGINE_PIN`), no `src/`.
- **Conflict surface (per the prompt's landing note)**: sibling engine tasks
  also touch `src/platform/Rnd_Wgpu_RB3.cpp` (mesh-cache sites, bloom composite)
  and possibly `src/gfx/standard_wgsl.inc` (a shader safety net). My diff is
  tight and localized:
  - `Rnd_Wgpu_RB3.cpp`: two inserts inside `DrawMesh` only — (a) `mu.unlit` +
    the general `mu.emissiveMultiplier` block right after the `mu.prelit` line
    (~:4330), and (b) the deletion of the old `RndTex* emTex = …;
    mu.emissiveMultiplier = …;` two lines inside the `sTrackLight` game.cam
    block (~:4493, replaced with a comment; the `gem_smasher_glow`/`peakstate`
    boosts are kept). No mesh-cache or bloom-composite lines touched.
  - `standard_wgsl.inc`: struct field rename `_padMat1`→`unlit` (~:111) and the
    one-line OR in the fs_main prelit branch (~:791). If a sibling adds a shader
    safety net elsewhere in fs_main, no overlap expected; if both edit the
    MaterialUniforms struct, reconcile the pad-slot usage (I took the FIRST pad
    slot `_padMat1`; size must stay 192 and the WGSL struct must match
    `UniformStructs.h` exactly).
  - `UniformStructs.h`: `_padMat[3]` → `unlit;` + `_padMat[2]` (~:81-83). Same
    192-byte invariant; if a sibling also extends MaterialUniforms, they must
    coordinate which pad slot each takes.
- **If a sibling lands the `neon_arcade.mesh` green-slab fix (Fix 4)** the menu
  aggregate metrics (R:B, contrast, green-excess) will jump cleanly into the
  scout's target ranges — my fix + Fix 4 are complementary and verify best
  together.
- **Build note**: a fresh `cmake -B` configure in a worktree defaults to GCC and
  fails on clang-only flags (`-fms-compatibility-version`,
  `-fdelayed-template-parsing`) injected for the engine; pass
  `-DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++` to
  match the main build (and `-DDawn_DIR=…/dc3-decomp-deps/dawn/lib/cmake/Dawn`).
- No env-var opt-out added (the unlit+emissive behaviour is the correct default
  and is what the existing `RB3_VENUE_LIGHT_OFF` / `RB3_TRACK_LIGHT_OFF` A/B
  hatches now interact with cleanly). Add one only if a sibling regresses.
