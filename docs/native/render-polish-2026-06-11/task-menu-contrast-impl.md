# task-menu-contrast — impl (Wave 5)

Completes the deferred wave-3 **Fix 3** (`scout-menu-lighting.md` §3, the `ue=1`
venue-heuristic floor-lighting lever): raise the menu-hub contrast floor toward
retail. After the wave-4 fog fix removed the *wash*, the loop-wide 3×3 contrast
was still ~2.6:1 vs retail ~10:1 because the **dark-cell floor was untouched** —
the unlit-by-design `ue=1` brick / sidewalk / band-outfit geometry was lifted to
a flat grey by the venue-light heuristic's conservatively-bright floors.

## Result

VERIFIED. Engine-only change → **rb3 `src/` byte-identical, zero Wii-match
impact** (the only rb3 commit is the `MILO_ENGINE_PIN` bump). The menu hub now
renders the brick/sidewalk/background dark and warm with the authored point
lights carrying the illumination, so the neon/signs pop against deep blacks —
retail's dark-backdrop + bright-neon-hotspot look. Loop-wide median 3×3 contrast
**2.6:1 → 8.4:1** (retail ~10:1), median dark-cell **0.16 → 0.045** (retail
0.028–0.035). The wave-4 menu-fog (soft-green) and the neon-slab (hard-green)
fixes are NOT regressed; gameplay highway (game.cam) is byte-identical; gameplay
street venues darken to match (more retail-faithful, detail survives); lit club
venues are carried by their point lights and look unchanged.

## Root cause

The venue-light heuristic in `BandRnd::WriteSceneUniforms` (engine
`src/platform/Rnd_Wgpu_RB3.cpp`, the `world.cam` + `sVenueLightEnabled` path)
sets the scene ambient + a fallback grey directional from the current
`RndEnviron`. Three floors were tuned conservatively-bright so nothing crushed
to black:

1. **0.07 ambient floor** (`std::max(ar, 0.07f)` ×3). Most hub environs have a
   *black* raw ambient (`0.00,0.00,0.00` — verified with `RB3_VENUE_PROBE`:
   `back_left`, `buildings_dim`, `theater`, `cityscape`, `street`, `street_slomo_*`,
   `road`, `train`…), so they were all floored to 0.07. Through the lit-mat
   compose (`finalColor = baseColor·softClip(ambient + Σdiffuse)` then
   `linearToSrgb`), a 0.07 ambient floor on a ~0.4-albedo brick reads ~0.18 sRGB
   — exactly the observed dark-cell floor. **This was the dominant lever.**
2. **×0.25 near-white-ambient clamp** (`if (max>0.85) ×0.25`). Fires only on the
   degenerate `env=''` (ambRaw `1,1,1`) that scopes the walking-band outfits →
   0.25 ambient grey-washed the band flat.
3. **0.6 grey no-light key** (the `dl==0 && pl==0` fallback directional). Fires
   for the ambient-only environs (`sky`, `back_left`, `buildings_dim`, `street`,
   `road`, `env=''`) and flat-floods them grey.

The rich authored point lights (`lamppole.lit` color 3.0, `road.lit` 1.0,
`theater*`/`blue_lowlight*` accents, all with quadratic range falloff) were
already present and provide the real illumination — they just couldn't create
contrast because the floors held the unlit zones bright. Unlit neon/sign mats
(`mUseEnviron==0`, wave-2 Fix 1) and emissive maps (wave-2 Fix 2) bypass this
lighting path entirely (register colour / self-illum), so they were unaffected
by the floors and stay bright either way.

## What changed (engine repo only)

All edits in the paired engine worktree
`/home/free/code/milohax/milo-native-engine-worktrees/task-menu-contrast`
(branch `wt-task-menu-contrast`), commit **`3bd4245`**
(`3bd4245d23ce977672372cf092b4354921652804`), based at engine `58254f7`.

`src/platform/Rnd_Wgpu_RB3.cpp` — `WriteSceneUniforms` + a small helper block
just above it (+37/−5), scoped to the existing `world.cam` venue-light path
(so `game.cam` highway + non-venue cams are byte-identical):

1. Three tunable floor getters (read env once, cached):
   - `RB3_VENUE_AMBIENT_FLOOR` (default **0.008**, was hard-coded 0.07)
   - `RB3_VENUE_AMBIENT_CLAMP` (default **0.09**, was hard-coded 0.25)
   - `RB3_VENUE_GREY_KEY` (default **0.22**, was hard-coded 0.6)
2. Replaced the three hard-coded constants in the ambient-floor / near-white-clamp
   / no-light grey-key lines with the getters. No structural change — same code
   path, just lower floors.

Defaults were chosen by a controlled env-var sweep on the 60-frame hub loop
(floor 0.018/0.010/0.008/0.005 × matched clamp/grey): **0.008/0.09/0.22** lands
median contrast 8.4:1 with dark-cell 0.045 and min dark-cell 0.023 (no crush);
0.005 over-darkened some wide shots (min dark-cell 0.016, foreground figures
crushing). Each value is runtime-tunable for future re-tune without a rebuild.

## Branches + commits

| repo | branch | commit | what |
|---|---|---|---|
| milo-native-engine | `wt-task-menu-contrast` | `3bd4245` (`3bd4245d23ce977672372cf092b4354921652804`) | venue-light floor lowering |
| rb3 | `wt-task-menu-contrast` | `d16a9b60` | `MILO_ENGINE_PIN` 58254f7 → 3bd4245 |

Engine wt: `/home/free/code/milohax/milo-native-engine-worktrees/task-menu-contrast`.
rb3 wt: `/home/free/code/milohax/rb3/.claude/worktrees/task-menu-contrast`.

## Verification

Evidence dir `/tmp/rp5-menu-contrast/` (harnesses `hubcap.py`, `measure.py`;
key frames kept, bulk loop frames trimmed for the tmpfs quota). Ports 9231–9239.
`measure.py` reproduces the wave-3 method exactly (3×3-cell luminance contrast,
mean RGB, R:B, soft/hard-green); it measures the retail refs at the published
baselines (hub 10.24:1 / dark-cell 0.035; playnow 13.48:1 / 0.028).

### Menu hub (PRIMARY) — 60-frame loop, loop-wide summary

| metric | retail | BASELINE (composed wave-4) | FIX (default) | target met |
|---|---|---|---|---|
| 3×3 contrast (median) | 10–13:1 | **2.59:1** | **8.42:1** | ✓ toward retail (3.2×) |
| 3×3 contrast (max) | 13.5:1 | 5.75:1 | 18.42:1 | ✓ |
| dark-cell (median) | 0.028–0.035 | 0.156 | **0.045** | ✓ near retail |
| dark-cell (min) | — | 0.096 | 0.025 | ✓ reaches retail black, no crush |
| mean-lum (median) | 0.185 | 0.284 | 0.232 | ✓ toward retail |
| soft-green (median) | 3.4–3.8% | 2.36% | 2.64% | ✓ in-band (wave-4 fog OK) |
| soft-green (max) | — | 6.37% | 5.75% | ✓ no wash regression |
| hard-green (median) | 0% | 0.01% | 0.01% | ✓ no neon slab |

**Clean A/B control (proves the tunables are the sole driver, no confound):** the
FIX binary run with the OLD env values (`RB3_VENUE_AMBIENT_FLOOR=0.07
_CLAMP=0.25 _GREY_KEY=0.6`) reproduces the baseline exactly — median contrast
2.67:1, dark-cell 0.156. So the entire metric delta is the three values.

**Visual (decisive):** same PALACE/BARBER camera shot —
`/tmp/rp5-menu-contrast/baseline/hub_f0010.png` (washed grey brick, flat-lit
ghost figures, no contrast) vs `…/final_default/hub_f0010.png` (dark warm brick,
glowing PALACE/BARBER neon, figures read as dark forms against deep blacks) =
the retail look (`images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png`). Band
close-up `…/final_default/hub_f0048.png`: band detailed + readable, neon pops,
NOT crushed. `visual_diff.py --perceptual` vs the retail hub ref on the matched
PALACE shot: score **47.0 → 50.1**, struct 33.0→36.3, ssim 56.4→59.6,
block-color 79.4→81.8 — every sub-metric moves toward retail.

### Regression sweeps

- **Gameplay highway (game.cam)** — byte-identical (untouched code path:
  `07_playing.png` track/gems/lanes unchanged in both A/B runs).
- **Gameplay street venues (tv3_b / tv3_c, the same Rock-City street content as
  the hub)** — darken with the same fix: burst mean-lum FIX avg 0.133 vs BASE
  0.188 on tv3_b. This is the *intended* improvement (retail's street venue is
  dark too); detail survives — `gp_sd2/burst_03.png`/`burst_08.png` show the CORK
  bar + crowd + neon clearly, moody not crushed.
- **Lit club venues** — the CORK bar (`gp_fix/burst_15.png`) is well-lit, brick /
  crowd / chairs all visible: its authored point lights carry it, the floor
  reduction barely touches it (gameplay burst-loop avg lum FIX 0.213 vs BASE
  0.200 — within boot-to-boot noise, FIX slightly *brighter* on the lit shots).
- **Song select** (`ss_fix/native_depth_08.png`) — unchanged. It draws under
  overshell/ui cams, not world.cam, so the venue path never touches it. (The
  FRIEND RANKINGS overlay + grey album box + header garbage digits are
  pre-existing, already in PLAN.)
- **No crashes / asserts** in any hub or song-select run. (Two songs SIGABRT on a
  `SongData::TrackInfo` vector OOB during track load — pre-existing, song-specific,
  reproduces with the old env values too; unrelated to lighting.)

## Pass-criteria scorecard

| criterion | result |
|---|---|
| 50+ frame hub loop | ✓ 60 frames |
| contrast toward ~10:1 (measure.py method) | ✓ median 2.6→8.4:1 (3.2× toward retail) |
| neon/signs still correct (wave-4 not regressed) | ✓ hard-green 0% (no slab), soft-green in-band (no fog wash); neon visibly pops |
| gameplay venues unchanged | ✓ highway byte-identical; lit club carried by point lights; street venues improve (retail-faithful) |
| no blow-out (don't lean on softClipLighting) | ✓ this LOWERS the floor; bright-cell median 0.44→0.40, no clip introduced |

## LANDING NOTES (orchestrator)

- **Commit order**: land engine `3bd4245` FIRST (so the SHA is reachable), THEN
  the rb3 pin bump `d16a9b60`. The rb3 commit ONLY changes `native/CMakeLists.txt`
  (`MILO_ENGINE_PIN`), no `src/` → Wii byte-identical by construction.
- **Engine conflict surface**: TIGHT, single region. My diff is in
  `src/platform/Rnd_Wgpu_RB3.cpp` only:
  - the new `sVenueEnvFloat` + three `sVenue*` getters inserted right after
    `sVenueLightEnabled` (~:1091, just before `WriteSceneUniforms`);
  - inside `WriteSceneUniforms` the ambient-floor / near-white-clamp lines
    (~:1223–1235) and the no-light grey-key line (~:1290–1294).
  This is the `WriteSceneUniforms` function ONLY. Sibling render-polish engine
  tasks touched **`DrawMesh`** (menu-lighting `mUseEnviron`/emissive, fret-sphere
  bloom), **`DrawParticles`** (menu-fog haze), and **`standard_wgsl.inc`** (venue
  soft-clip, unlit field) — none overlap `WriteSceneUniforms`. No shader / struct
  / bind-group change.
- **Tunables / opt-outs** (all default to the fix being ON): `RB3_VENUE_AMBIENT_FLOOR`
  (0.008), `RB3_VENUE_AMBIENT_CLAMP` (0.09), `RB3_VENUE_GREY_KEY` (0.22). Set all
  three to the old `0.07/0.25/0.6` to fully revert to pre-fix behaviour
  (`RB3_VENUE_LIGHT_OFF=1` still disables the whole venue path as before). If a
  specific *lit* gameplay venue ever regresses (it shouldn't — its point lights
  carry it), the cleanest follow-up is to scope the lower floors to the shell /
  street environs rather than all venue environs; gameplay venues checked clean
  here so this was not needed.
- **Residual (out of scope, NOT a regression)**: contrast lands ~8.4:1 vs retail
  ~10–13:1. The last gap is bright-side, not dark-side: retail's neon hotspots
  are a touch brighter than the native unlit register colour / emissive strength
  (the wave-2 Fix 1/2 levers, already landed). R:B warmth stays ~1.19 (retail
  1.75) — dominated by the white UI-text overlay + green neon in-frame; the
  backdrop brick is now visibly warmer. Both are independent of this floor lever.
