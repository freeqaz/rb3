# Ground-truth: highway "teal overlay" (GAP A) + big_club white crowd (GAP B)

**Ground-truth agent (Opus). RESEARCH ONLY — no code/engine/build/commit changes.**
Establishes what the note highway and the big_club audience *should* look like in
original RB3 so the two follow-up fix agents target the right appearance (and don't
"fix" something that is actually authored/correct). Evidence = (1) in-repo retail
gameplay frames (`images/retail-screenshots/`), measured quantitatively; (2) the
authored track-texture asset names read out of `orig-assets/extracted/ui/track/gen/*`;
(3) prior converge investigations (`docs/native/converge-2026-06-20/lighting/`); (4)
web search (low yield — the visual details are too niche to be documented externally).

---

## TL;DR verdict (go/no-go + tuning target for the two fix agents)

| gap | native render | RB3 intent | divergence? | what the fix must change |
|---|---|---|---|---|
| **A — highway "teal overlay"** | the ornate filigree swirl on the highway renders as a **bright, saturated CYAN/TEAL glowing pattern** (matched swirl-region luma **83**, G/B **0.71**) | the **SAME** filigree swirl exists in retail but as a **subtle dark-blue watermark** on a deep-navy surface (matched swirl-region luma **20**, G/B **0.36**) | **YES — but NOT "an extra overlay"** | **DARKEN + DE-GREEN the highway surface**, do NOT remove the pattern. It is the authored `background.tex`/`guitar_effects_bg.tex`. Native is ~4× too bright and green-shifted. |
| **B — big_club white crowd** | audience figures lining the stage render as **flat stark-WHITE cut-outs** (crowd-region white% ~9–11%, invariant across venue-light knobs) | audience is **dim/dark + unobtrusive** (cf small_club in-engine crowd white% ~0; retail audiences are never bright-white) | **YES — genuine** | shade the **world.cam char-EXTRAS** crowd dim (it currently flat-whites) AND/OR kill the **mesh-shard white smear**. NOT the impostor-cam path (already fixed by STEP 2). |

Both are genuine divergences. **The single most important new finding is for GAP A:
the teal pattern is NOT a native-only overlay — the identical decorative swirl is present
in retail.** The bug is *exposure + hue* on the highway surface material, not the
presence of a pattern. A fix that removes/hides the swirl would be WRONG (it would
diverge further from retail, which has it). The fix is to make the swirl read as a faint
dark-blue watermark again.

---

## GAP A — highway "teal overlay" is the AUTHORED swirl, over-bright + green-shifted

### A1. The swirl exists in retail (decisive)

Side-by-side high-zoom crops of the matched swirl region prove the pattern is the same
ornate filigree in both:
- retail: `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_drums.png` (drums, swirl
  clearly visible center-track) and `..._gameplay_guitar.png` (guitar, same swirl).
- native: `docs/native/converge-2026-06-20/shots/verify/V1_deep_anchor_guardoff_fixoff_teal_highway.png`.

The shapes are identical (twin mirrored scroll/paisley curls flanking the lane center).
This is the authored RB3 track-background art, not a native artifact. **Therefore the
gap is colour/value, not geometry/overlay.**

### A2. Measured colour divergence (matched swirl-region, same harness)

| region | mean RGB | luma | G/B | saturation | reading |
|---|---|---|---|---|---|
| **RETAIL drums swirl** | (16, 17, 47) | **20.3** | **0.36** | 0.76 | dark, deep-blue, low-luma watermark |
| **NATIVE V1 swirl (teal)** | (63, 86, 121) | **83.0** | **0.71** | 0.55 | ~4× brighter, green-shifted → glowing teal |
| NATIVE V3 swirl ("clean" cam) | (55, 58, 59) | 57.5 | 0.99 | 0.06 | (region includes floor; see A4) |

Wider mid-track means (less tight) corroborate: retail highway G/B ≈ 0.41–0.50,
native ≈ 0.61–0.71; native overall ~2–3× brighter. **Two independent errors stack:**
1. **Over-exposed** — highway surface luma ~4× retail. A subtle dark watermark becomes a
   bright pattern simply because the whole surface is too bright.
2. **Green-shifted** — G/B climbs from retail's ~0.36 to native's ~0.71, turning the
   blue swirl CYAN/TEAL. Even at correct exposure, the hue is wrong.

**Tuning target for the fix agent: drive the highway-surface swirl region back toward
luma ≈ 20–40 and G/B ≈ 0.36–0.50 (deep blue, green suppressed).** Lower exposure alone
won't fully fix the hue; the green channel needs to come down relative to blue.

### A3. WHAT draws the pattern (authored asset identity)

The swirl is authored track-background art in `orig-assets/extracted/ui/track/gen/`:
- `trackpanel.milo_xbox` / `tracksystem.milo_xbox`: `background.mat` + `background.mesh`
  + `background.tex`; plus `guitar_effects_bg.tex` (`guitar_fx_bg.mat`/`.mesh`) and the
  per-instrument `spotlight_*_track.tex` / `$spotlight_*_track_emmissive.tex`.
- the highway *surface* itself: `surface.mat` (+ `surface_w_norm.mat`, `surface_keys.mat`).
- PNG sources are referenced inside the milo (`textures/guitar_effects_bg.png`,
  `textures/spotlight_guitar_track.png`, …) but are NOT unpacked to disk, so they can't
  be eyeballed standalone — the in-game retail frames are the ground-truth render.

So "the teal overlay" is most likely the `background`/`guitar_effects_bg` decorative
layer and/or the `surface.mat` highway floor, lit by the game.cam track-lighting path.

### A4. Why this is the track-light path (not bloom, not scrollbar) — corroboration

- Prior verify (`shots/verify/`): the teal **persists with `RB3_HIGHWAY_BLOOM_OFF=1`**
  (V2) → it is NOT the a234-P1 gem bloom-halo. And it is NOT the scrollbar (0 scrollbar
  meshes after game_screen; the `7a6525fc` scrollbar fix is unrelated).
- `V3_deep_anchor_other_camera_clean.png` shows the SAME highway from a different
  camera reading **much darker / blue-dominant**, with the swirl barely visible (G/B
  near-neutral because that crop catches venue floor). This camera-dependent brightness
  is the fingerprint of the **game.cam-scoped track-lighting** path (MEMORY a234:
  game.cam surface.mat ×0.12, re-enabled material emissive, lit lanes, brighter now-bar)
  — i.e. the deep-gameplay anchor camera over-exposes the surface while another cam
  doesn't. **The teal over-bright is a track-lighting / surface-material exposure issue
  on the game.cam highway path, exactly the suspect set in the task brief.**

**GAP A go/no-go: GENUINE divergence. Target = restore the authored swirl as a faint
dark-blue watermark (luma ~20–40, G/B ~0.36–0.50). DO NOT remove the pattern.**
Likely lever: the game.cam track-lighting exposure/emissive on `surface.mat` /
`background.mat` (a234 over-brightened it); bring surface luma down ~4× and pull the
green channel down relative to blue. The fix agent should A/B with `RB3_TRACK_LIGHT_OFF=1`
to confirm the track-light path owns the teal, then tune the surface/background exposure.

---

## GAP B — big_club white crowd: the VISIBLE white is the world.cam char-EXTRAS path
   (+ a mesh-shard smear), NOT the impostor crowd

### B1. Intent: audience is dim/dark, never white (genuine divergence — confirmed)

- Retail RB3 audiences are dim, low-value, unobtrusive — the brightest things on screen
  are the band + the highway, never the crowd. In-repo club frames
  (`yt_qRagnZCIMzk_gameplay_*`, `fandom_gameplay_*`) show dark/dim audiences.
- In-engine ground-truth for "correct dim crowd": **small_club_01 crowd white% ≈ 0–0.1%**
  (prior measurement); big_club is 9–11%. The crowd should be among the *darkest* things
  in frame; native makes it the *brightest*.
- Authored intent (read via `RB3_VENUE_PROBE`, prior lighting/ground-truth.md): the crowd
  environs carry **ambient-only, NO real lights** (`RB3_crowd_mesh.env`
  ambRaw=(0.18,0.18,0.18), `crowd.env`=(0,0,0)). The audience is meant to be carried by
  dim grey ambient + baked vertex shading → dim/dark. White is unambiguously wrong.

Evidence: `docs/native/converge-2026-06-20/refs/native_bigclub_white_crowd.png` (stark
white figures lining the stage). Do NOT mistake authored-white surfaces for this bug:
video_01's white studio backdrop and festival_01's intentional B&W comic crowd backdrop
are CORRECT and must not be "fixed."

### B2. WHICH path owns the visible white (decisive — from STEP-2 verify)

The HELD STEP-2 fix (engine tag `converge-step2-crowd-wip` = `bae1aae`) correctly widened
the venue-light gate so the **unnamed impostor-RT cam** reads its dim crowd env — but it
did **NOT change the visible white%** (invariant ~9–11% on/off/greykey0). The STEP-2
verify agent proved why:

> "the VISIBLE big_club crowd renders via **world.cam** scoping `char_rooftop.env`, plus
> a **mesh-shard white smear** — NOT the impostor path STEP 2 fixes."
> (`docs/native/converge-2026-06-20/lighting/step2-verify.md`)

And the decisive A/B: **`RB3_VENUE_LIGHT_OFF=1` triples crowd luma but leaves white%
unchanged** → the white is a **material/shader** problem, not exposure (a brighter
ambient just makes already-white figures stay white).

So GAP B has **two sub-parts to disentangle (both under world.cam, NOT the impostor cam):**

- **(a) char-EXTRAS shaded flat-WHITE.** The world.cam crowd-extras characters take the
  LIT shader branch with (i) the crowd's high baked vertex luma and (ii) **missing
  crowd skin/cloth diffuse textures** in the extracted asset set (documented in
  `standard_wgsl.inc` and prior lighting/ground-truth.md §3b): with no diffuse,
  `baseColor ≈ material.color × skinnedTint` → high → near-white even under dim ambient.
  This is a *value* (white) failure that exposure won't fix — it needs the crowd-extras
  material/shading clamped or the missing-diffuse fallback darkened.
- **(b) a MESH-SHARD white smear** — the V24 SHARD_GUARD / skin-deform family
  (cf [[project_char_skinning_deform]], [[project_crowd_origin_inststrings]]): an
  exploding/sheared crowd-extras servo-skeleton mesh that smears white. The SHARD_GUARD
  (`Rnd_Wgpu_RB3.cpp` ~4924–5141) drops some of these but a residual smear remains;
  the white% is the SAME guard ON vs OFF for the bulk of it (so most of the visible white
  is (a), not a masked shard), but a smear component is shard-family.

**Which dominates:** guard ON ≈ guard OFF white% (prior: 9.4 vs 7.3, 14.4 vs 12.0) → the
**majority of the visible white is (a) the flat-white char-extras shading**, with a
smaller (b) shard-smear contribution. The fix agent should target (a) first (clamp the
char-extras crowd material value / fix the missing-diffuse → white fallback), then
re-measure to see how much (b) shard-smear remains.

### B3. Engine pointers for the fix agent (not a fix)

- world.cam char-extras lighting + the LIT/skinnedTint branch:
  `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` `WriteSceneUniforms`
  (~1265–1358) + `DrawMesh` material/lighting; shader `src/gfx/standard_wgsl.inc`
  fs_main (~692–873, crowd skinnedTint ~703–734).
- SHARD_GUARD (the (b) smear): `Rnd_Wgpu_RB3.cpp` ~4924–5141 (toggles `SHARD_GUARD_OFF`,
  `SHARD_DBG`, `SHARD_RATIO_DBG`).
- crowd-extras source: `src/system/world/` (Crowd, WorldDir extras) + `src/system/char`
  (extras/servo skeletons).
- **DC3 safety:** `Rnd_Wgpu_RB3.cpp` is RB3-only (DC3 compiles `Rnd_Wgpu.cpp`), but any
  shared-shader (`standard_wgsl.inc`) or `src/system`/`src/system/char` change is shared
  with DC3 — gate behind an RB3 path/flag or prove DC3 byte-identical (cf the STEP-1
  SceneUniforms-flag pattern).

**GAP B go/no-go: GENUINE divergence. Target = dim/dark audience (crowd-region white% →
~0, toward small_club). Fix the world.cam char-EXTRAS flat-white shading (the dominant
component) first, then the residual mesh-shard smear. NOT the impostor path (STEP 2
already covers that; it did not move the visible white).**

---

## What I did NOT find / open caveats

- **No external doc** confirms RB3's exact highway swirl or big_club crowd brightness
  (web search yield is poor for this niche). The in-repo retail frames + the authored
  asset names are the authoritative ground-truth; that is sufficient and stronger than a
  third-party description.
- **No retail *big_club* or *arena* wide gameplay frame** is in-repo or locatable online
  (galleries 403). The crowd-brightness intent transfers from the universal RB3 rule
  (audiences are dim, never white) + the in-engine small_club crowd as the "correct dim"
  proxy. This is the same caveat as the prior lighting/ground-truth.md and does not change
  the verdict (white is wrong under any RB3 venue).
- The extracted asset set is **missing crowd skin/cloth diffuse textures**, which is part
  of the (a) char-extras root cause — flagged so the fix agent treats the white as a
  missing-texture/fallback issue, not only a lighting one.

---

## Sources / evidence anchors

- Highway swirl present in retail: `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_{drums,guitar}.png`
  (matched-region crops `/tmp/swirl_retail.png` vs `/tmp/swirl_native.png`).
- Native teal: `docs/native/converge-2026-06-20/shots/verify/V1_deep_anchor_guardoff_fixoff_teal_highway.png`
  (`V2` = bloom-off persists, `V3` = other-cam clean/darker).
- Authored track textures: `orig-assets/extracted/ui/track/gen/{trackpanel,tracksystem,track_shared}.milo_xbox`
  (`background.tex`, `guitar_effects_bg.tex`, `spotlight_*_track.tex`, `surface.mat`).
- GAP B path identity: `docs/native/converge-2026-06-20/lighting/step2-verify.md`
  ("visible big_club crowd renders via world.cam + mesh-shard smear, NOT the impostor
  path"); white-is-material A/B (`RB3_VENUE_LIGHT_OFF` triples luma, white% unchanged) +
  authored crowd env (ambient-only, 0 lights) in
  `docs/native/converge-2026-06-20/lighting/{bigclub-white,ground-truth}.md`.
- Native crowd gap frame: `docs/native/converge-2026-06-20/refs/native_bigclub_white_crowd.png`.
- Retail audiences dim / never white (universal RB3 rule): in-repo club frames + prior
  lighting/ground-truth.md §1.
- web search (low yield): RB Wiki Solo Tour (venue progression), Wikipedia "Rock Band 3"
  (note-highway feature). No external swirl/crowd-brightness documentation surfaced.
