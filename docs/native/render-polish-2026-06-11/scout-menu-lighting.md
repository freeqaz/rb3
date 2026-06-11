# scout-menu-lighting — Main menu (hub) lighting vs retail

Wave-1 scout, 2026-06-11. Investigation only; no source edits. Engine state
inspected at pin `8fb669d` (= current `../milo-native-engine` HEAD).
Evidence screenshots: `/tmp/rp-menu-lighting/` (key files listed inline).
Helper scripts (reusable): `/tmp/rp-menu-lighting/menu-capture.py`,
`/tmp/rp-menu-lighting/hub-series.py` (frame-locked A/B hub capture).

## 1. SYMPTOM

Repro (headless native):

```bash
python3 /tmp/rp-menu-lighting/hub-series.py --port 8671 --out-prefix /tmp/out/von
# boots with RB3_GAME_INPUT="@10:start,@30:confirm,@120:cancel" → top-level hub,
# then screenshots at fixed frame numbers across the menu camera-shot loop
```

The hub backdrop is the "Rock City" night-street vignette
(`world/vignette/shell/sv3_a.milo`, panel `sv3_panel`, subdir `streetslomo_ao`)
rendered under **world.cam** while the camera loops through authored shots
(tent/tiger wall → walking band → storefronts). vs retail
(`images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png` /
`..._menu_playnow_submenu.png`, 360/PS3):

- **Hue is wrong**: retail street is warm amber (global mean RGB
  0.248/0.191/0.141, R:B = 1.76). Native same-shot frame is neutral-green
  (0.193/0.224/0.175 with the green slab hidden, R:B = 1.10; G is the LARGEST
  channel). Evidence: `green_off.png` vs retail ref (same camera shot — BARBER
  sign top-right, Baboon-Nest tent left).
- **Authored contrast is gone**: retail 3×3-cell luminance spans 0.04→0.38
  (~9:1, bright neon hotspots + deep shadow); native spans 0.12→0.34 (~2.8:1,
  flat). Native is *brightest* in the lower-right cells (0.34, green) where
  retail is *darkest* (0.04–0.07).
- **Lit-sign zone half brightness**: retail upper-centre storefront cell mean
  lum 0.38 (RGB 0.38/0.40/0.33); native 0.19 (0.20/0.19/0.14).
- **Tent/poster wall (unlit mats) half brightness + de-warmed**: retail
  mid-left cell 0.36 (0.42/0.35/0.27); native 0.21 (0.20/0.22/0.22).
- **Giant green slab** across mid-frame in many shots (18–30% of all pixels
  with green-excess > 0.2): `green_on.png`, `von_f01800.png`, `von_f02100.png`.
  Lighting-independent (present in both venue-light ON and OFF runs).
- Menu **UI chrome itself is fine**: white item text 13.9% px @ lum 0.98
  native vs 11.0% @ 0.94 retail (text is force-prelit; UI draws under
  `[ui.cam]`/overshell.cam, not world.cam). This is a *backdrop* issue.

A/B `RB3_VENUE_LIGHT_OFF=1` (frame-locked series `von_f*.png` vs `voff_f*.png`,
same camera pose verified at f1050): OFF is uniformly brighter and *greener*
(mean lum 0.23–0.41 vs ON 0.12–0.28), not closer to retail. Neither path
matches: **the fix is not toggling the venue path**.

## 2. ROOT CAUSE

Three independent causes, ranked by impact. (1) and (2) are the lighting model;
(3) is a geometry/decode artifact that dominates several shots.

### 2a. Native ignores `RndMat::mUseEnviron` — unlit materials get lit (PRIMARY)

Ground truth (Wii GX decomp, `src/system/rndwii/Mat.cpp` `WiiMat::Select`,
~line 261):

```cpp
if (!mUseEnviron && !mPreLit) {
    // GXSetChanCtrl(GX_COLOR0A0, FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, ...)
    // → channel output = material register color. NO ambient, NO lights, NO vertex color.
```

i.e. `use_environ=0, pre_lit=0` ⇒ **full-bright authored color × texture**.
Only `mUseEnviron=1` materials are modulated by the environ's ambient+lights.

Runtime dump of all 98 materials in the hub vignette dir (live DTA, port 8674:
`{do ($s "") {{{sv3_panel loaded_dir} find "streetslomo_ao"} iterate Mat $m ...}}`):
**the scene-defining materials are nearly all `ue=0,pl=0` = UNLIT**: every neon
mat (`red_neon`, `white_neon`, `lt_blue_neon`, `green_neon`, `park_neon0*`,
`tiger_trim_glow`), the palace/sign faces (`palace_a/c/e/l`, `tiki_bar_name`,
`thebaboonnest_line_logo`), fog/atmosphere (`fog_thin`, `cool_cloud`,
`street_fog_system`), `city_backdrop`, `car_headlights`, bokeh billboards
(`red/green/orange_bokeh`), tent walls (`flat_tent_1/2`), tattoo murals
(`tattoo_t2/t3`). The `ue=1` (lit) set is the mundane geometry: `sidewalk`,
`grey_pain_wall`, `doors_windows_vents`, `store_boxes`, `trash(can)`, `metal`,
`awning`, `graffiti_brick_wall`, `neonsigns_*` (sign *backboards*, em=0.1).

The engine (`../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` `DrawMesh`
~4296-4360) never reads `mUseEnviron`; `src/gfx/standard_wgsl.inc` `fs_main`
lights every non-prelit mesh with `ambient + Σdir + Σpoint`. On the menu that
scene-light set is the venue path's per-environ heuristics (see 2c) — so unlit
neon/posters/fog get arbitrarily darkened (0.07 ambient floor + ≤4 point
lights) or washed (grey 0.6 key), instead of full-bright. This single semantic
explains BOTH symptom directions (too-dark wide shots, washed close-ups).

LIGHT_PROBE corroboration (`/tmp/rp-menu-lighting/von.log`, 453 distinct mesh
draws probed on the menu): 451/453 are `prelit=0` → in the current shader the
*entire* menu venue takes the dynamic-lighting branch.

### 2b. Material emissive is zeroed outside game.cam — night-city glow missing

`Rnd_Wgpu_RB3.cpp:4494`: `mu.emissiveMultiplier` is only set inside the
`game.cam` track-light block; `MaterialUniforms mu{}` zero-inits it for every
other camera, including world.cam. The comment at ~4466 admits it: *"Re-enable
material EMISSIVE (this backend's DrawMesh dropped it)"* — but only did so for
the highway.

The menu venue is exactly the content that needs it: 48 of the 453 probed menu
draws carry real emissive maps — `building_02/03_ill.tex` (em **1.5**),
`building_04/05_ill.tex`, `building_misc_01_ill.tex`, `sign_tv/cash/check/pawn`
(self-emis), `theater01_illum.tex` (em **1.25**), `subwaytrain_emis.tex`,
`window_01` (em 0.2), `neon_red`/`neon_white` (em **2.0**), traffic streaks.
These are the lit windows / marquees / signage of the night city. All currently
render with emissive = 0. The emissive texture view is *already resolved and
bound* (binding 5, `MakeMaterialBindGroup` ~1533-1557) — only the multiplier is
zeroed.

### 2c. Venue-light heuristics (tuned for gameplay venues) fire on the menu

Confirmed via `RB3_VENUE_PROBE=1` (`default.log` lines 586-748): the menu IS the
venue path — world.cam + per-environ SceneUniforms rewrites for ~20 environs
(`sky.env`, `street_slomo_geom.env` (5 point lights), `street_slomo_char.env`
(8 — engine caps at 4, `pl < 4` @ ~1190), `road.env`, `streaks_red.env`,
`theater.env`, `cityscape.env`, an unnamed `env=''` …). The gameplay-tuned
heuristics in `WriteSceneUniforms` (~1154-1228) all fire on menu content:

- ambient ×0.25 clamp when max>0.85: hits `env=''` (ambRaw 1,1,1 → 0.25) which
  scopes the band-outfit resource meshes;
- 0.07 ambient floor on the black-ambient envs;
- grey 0.6 directional key when an env has no lights: fires for `sky.env`,
  `back_left.env`, `buildings_dim.env`, `street.env`, `road.env`,
  `streaks_red.env`, `env=''` — i.e. most of the menu's environs, since on Wii
  their content is mostly *unlit by design* (2a) and the envs therefore carry
  few/no approx lights.

Once 2a+2b are fixed these heuristics only touch the `ue=1` minority; their
re-tune is a follow-up, not part of this fix.

### 2d. SEPARATE BUG: `neon_arcade.mesh` renders as a giant green slab

- Identification (live hide-test, same camera shot ±0.3 s): hiding ONE mesh —
  `neon_arcade.mesh` (mat `green_neon.mat`: color (0,1,0), `diff=NULL`,
  `ue=0,pl=0`, blend=1 opaque) — drops mean green-excess 0.168 → 0.035.
  Evidence: `green_on.png` / `green_off.png`.
- The slab spans x 0-979, y 190-664 (18-30% of frame). In retail this is thin
  storefront neon tubing (the green neon visible mid-left of the retail hub
  ref). The mesh is ~19 KB (`estimated_size_kb`=19 ≈ hundreds of verts) — real
  tube geometry, not a quad.
- NOT the W5 vertex-unpack cache: slab identical with
  `RB3_UNPACK_CACHE_OFF=1 RB3_NO_MESH_CACHE=1` (run `nocache.log`, capture
  `green_nocache.png`). Lighting-independent (present in ON and OFF runs).
- Hypotheses for impl: native decode of this mesh's faces/strips fills between
  tube segments (strip-vs-list or degenerate-bridge handling), or a kept-faces
  /multi-part grouping bug. CPU `mVerts` is empty natively (DTA `get_vert_pos`
  segfaults) so probe at the decode site, not via DTA. Residual smaller green
  fog remains after hiding (likely `green_bokeh`/fog billboards — plausibly
  authored, re-check after 2a fix).

## 3. FIX DESIGN

**Needs engine-repo change: YES** (`../milo-native-engine`). No rb3 `src/`
changes → zero Wii-match impact. rb3 side only needs the usual
`MILO_ENGINE_PIN` bump.

### Fix 1 — honor `mUseEnviron` (unlit materials)

- `src/gfx/UniformStructs.h` + `standard_wgsl.inc`: add `float unlit;` to
  `MaterialUniforms` using one of the three `_padMat` slots (size stays 192 —
  keep the static_assert green).
- `Rnd_Wgpu_RB3.cpp` `DrawMesh` material setup (~4330, next to `mu.prelit`):
  `mu.unlit = (mat && !mat->mUseEnviron && !mat->mPreLit) ? 1.f : 0.f;`
  (`mUseEnviron` is already on the engine's RndMat mirror — verify; it is
  loaded by `system/rndobj/Mat.cpp` `LOAD_BITFIELD(bool, mUseEnviron)`).
- `standard_wgsl.inc` `fs_main` final compose (~790): treat unlit like prelit
  but WITHOUT vertex tint (Wii: `GX_SRC_REG`, register color only):
  `if (isEnabled(material.prelit) || isEnabled(material.unlit)) { finalColor = baseColor.rgb; }`
  — with `vertexTint` already forced white for non-prelit static meshes, the
  existing prelit branch expression is exactly right; just OR-in the new flag.
  Keep emissive/rim/reflection adds after it (unchanged).
- Optional fidelity nit (skip in v1): Wii prelit+useEnviron modulates vertex
  color by ambient only (`WiiMat::Select` prelit branch). Current native prelit
  ignores ambient; fine for text/UI, revisit only if venues regress.

### Fix 2 — enable material emissive on all cameras

- `Rnd_Wgpu_RB3.cpp`: move the
  `mu.emissiveMultiplier = emTex ? mat->mEmissiveMultiplier : 0.0f;` out of the
  `game.cam`-only `sTrackLight` block (~4494) to the general material setup;
  keep the game.cam-only boosts (`gem_smasher_glow` ×2, peakstate) where they
  are. The emissive view is already bound for every draw, so no bind-group or
  ring-buffer changes.
- Watch-outs: `IsHaloSourceMat` (bloom capture) already gates on game.cam —
  unaffected. Song-select/overshell mats with emisMaps will light up — that's
  the retail look, but include song_select + score screens in verification.
  Native shader composes `baseColor.rgb * mult * emisSample` (Xbox-style
  modulate); Wii TEV may differ, but our visual ground truth (the refs) is
  360/PS3, which uses emissive — keep as-is.

### Fix 3 — re-tune venue heuristics on the menu (FOLLOW-UP, after 1+2)

Re-measure first; expected leftovers: the 0.07 floor and grey key now only
touch `ue=1` mats (sidewalk/brick/props) — likely acceptable or even correct.
If `env=''`'s ×0.25 ambient clamp dims the walking band's outfits, scope the
clamp to gameplay (or drop it — its rationale was based on misattributing
unlit materials' look). Keep `RB3_VENUE_LIGHT_OFF` semantics unchanged.

### Fix 4 — `neon_arcade.mesh` slab (separate task, mesh decode)

Reproduce headless (it's periodic in the hub loop: poll screenshots for mean
green-excess > 0.1, see the polling snippet in this dir's scout work or
`hub-series.py`), then instrument the native mesh decode for this one mesh
(name-gated, like XBONE_TRACK) and dump face/strip counts + bbox of decoded
positions. Compare against the milo data. Likely shared with other tube-neon
meshes (`neon_jupiter_club`, `neon_music`, `neon_arrow_*`) whose slabs are just
off-camera/smaller — check them in the same pass.

Recommended order: Fix 1 + Fix 2 in one engine commit (they're the lighting
model and verify together), Fix 4 independent, Fix 3 after re-measurement.

## 4. VERIFICATION

```bash
# build engine + rb3-native (in YOUR worktree pair), then:
python3 /tmp/rp-menu-lighting/hub-series.py --port <P> --out-prefix /tmp/out/fixed
# (copy of the script also embeddable from this doc's evidence dir)
```

Pass criteria (camera-shot-matched frames — match by content not wall-clock;
find the retail-ref shot = BARBER sign top-right, tent left):

1. Warmth restored: global R:B ratio on the retail-matched shot moves from
   ~1.10 toward ≥1.4 (retail 1.76).
2. Contrast restored: 3×3 cell lum range ≥ 5:1 (currently 2.8:1; retail 9:1);
   lower-right cells darker than upper-centre sign cells (sign zone ≥ 1.5× the
   corner cells; currently inverted).
3. Lit-sign zone (upper-centre cell) mean lum ≥ 0.30 (currently 0.19, retail 0.38).
4. Neon/glow visible: `building_*_ill` windows + theatre marquee + sign_tv
   visibly self-lit in the buildings shots (qualitative screenshot diff vs the
   `von_f*` baselines in /tmp/rp-menu-lighting/).
5. No green slab ≥ 5% of frame across one full hub loop (poll green-excess;
   currently 18-30% in 3+ shots) — Fix 4.
6. Regression gates: song-select (`scripts/native/song-select-capture.py`),
   gameplay (`scripts/native/keyboard-to-gameplay.py --game-burst 24`) — the
   highway look (game.cam) must be byte-identical-ish (track-light block
   untouched); venue backdrop during gameplay should improve (same unlit fix
   applies) but verify no washout vs `docs/native/...a234` glow baselines;
   UI text screens unaffected (text is prelit-forced).
7. A/B sanity: `RB3_VENUE_LIGHT_OFF=1` should now make a much SMALLER
   difference on the menu (only ue=1 mats change), confirming the unlit
   majority no longer depends on scene lights.

## 5. REFERENCE SCREENSHOTS NEEDED

- **Wii hub captures** (any shot of the loop, ideally several: walking-band
  close-up, storefront/ARCADE-neon shot, tent/tiger wall) — current hub refs
  are 360/PS3; Wii may not apply emissive maps at all (rndwii has no emissive
  TEV reference in the decomp), so a Wii capture would settle the
  emissive-strength target for OUR (Wii-derived) port. YouTube `qSRJ8HHPXzM`
  (Wii longplay) likely contains the hub loop.
- **360/PS3 close-up of the green ARCADE storefront neon + green fog moment**
  (the shot at `von_f01800.png`) — to confirm intended tube thinness and
  whether the residual green fog billboards are authored.
- Nice-to-have: a clean capture of the hub's `theater.env` / `cityscape.env`
  camera shots (theatre marquee, skyline) for emissive-level tuning.

## Evidence index (`/tmp/rp-menu-lighting/`)

| file | what |
|---|---|
| `green_on.png` / `green_off.png` | retail-matched shot, default lighting, slab visible/hidden (hide-test) |
| `von_f*.png` / `voff_f*.png` | frame-locked series, venue light ON vs OFF |
| `von.log` | LIGHT_PROBE per-mesh dump (453 meshes: cam/env/prelit/emisMap) |
| `default.log` | VENUE_PROBE per-environ dump (ambients + lights, lines 586-748) |
| `probe_meshes.txt` | grep'd LIGHT_PROBE mesh rows |
| `green_nocache.png`, `nocache.log` | slab persists with unpack+mesh caches off |
| `default_hub_f420.png`, `default_playnow_f560.png` | first captures (tiger wall washout, dark street) |
| `menu-capture.py`, `hub-series.py` | reusable capture harnesses |
