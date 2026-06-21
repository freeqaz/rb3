# GAP B — visible big_club_01 WHITE crowd: root cause + fix plan

**BIGCLUB-CROWD agent (Opus). RESEARCH ONLY — no code/engine changes, no commit, no
unnecessary rebuild.** Ran the prebuilt `native/build-native/rb3-native` (engine pin
`a360e3c` = STEP-1 GX point falloff landed; STEP-2 impostor-crowd env gate is NOT in
this binary — it was held in the worktree branch `bae1aae`). All A/B passes via
`/tmp/bch_override.py` (`{meta_performer set_venue_override big_club_01}` + force-shot
pin), shots under `docs/native/converge-2026-06-20/followup-gaps/shots/`.

---

## TL;DR — root cause found, two sub-parts disentangled and QUANTIFIED

The visible white big_club audience is **TWO** effects, and the dominant one is NOT
lighting-path-fixable:

- **(a) DOMINANT (~7–17% crowd white%): the crowd/extras characters render as
  ACHROMATIC FLAT WHITE because their diffuse content + vertex tint + material color are
  ALL ~white, so any lighting ≥ ~0.45 saturates them.** Measured: the bright crowd
  pixels are **98% near-white, mean RGB (219,219,219), saturation 0.000** — pure grey,
  zero chroma. This is the SAME failure the shader already documents
  (`standard_wgsl.inc:719-723`): RB3 character skin/cloth diffuse is effectively white in
  the extracted/native asset set, so the baked per-vertex AO is supposed to carry the
  value — but the crowd LOD characters have ~white vertex colors too, leaving NOTHING to
  darken them. The band performers do NOT have this problem (their region is colored,
  sat 0.437, white% 0.01) because they carry real baked vertex AO.
- **(b) MINOR (~0–3% crowd white%): a few small accessory MESH-SHARDS** (eyebrows / hair
  / shoe-skin / scrollbar) explode and are CORRECTLY DROPPED by the V24 SHARD_GUARD by
  default, so they are mostly invisible in the shipped build. This is the
  char-skinning/servo-skeleton family — route to that workstream, not GAP B.

**Decisive A/B: the white% is INVARIANT across every lighting knob** — VENUE_LIGHT_OFF
(triples luma 53→131 but white% 17.5→17.6), GX-vs-legacy point falloff (17.5 vs 17.3),
and even POINT_EXPOSURE 0.70→0.15 (17.5 vs 15.7). It is NOT a lighting/exposure bug and
**STEP 1's GX falloff is EXONERATED** (does not cause or worsen it). The white is a
**material/asset issue** (white diffuse × white vtx-tint × white color), exactly as the
prior step2 verify/impl docs concluded ("world.cam char-extras + mesh-shard").

---

## 1. Repro + measurement (matches prior bigclub-white.md numbers)

`set_venue_override big_club_01`, crowd shots `coop_dir_crowd.shot` + `coop_all_n00.shot`,
pinned 4/4 deterministic, anchor-ms 6000. "white%" = fraction of pixels all channels >200.
Crowd regions = left strip [0,0.22w] and right strip [0.78w,1.0w], mid-height [0.30h,0.75h].

| mode | crowdR luma | crowdR white% | crowdL white% | frame luma |
|---|---|---|---|---|
| **baseline** (default, guard ON, GX falloff) | 53.4 | **17.5** | 7.2 | 36.1 |
| `RB3_VENUE_LIGHT_OFF=1` | 131.3 | **17.6** | 7.4 | 108.8 |
| `RB3_VENUE_POINT_FALLOFF_LEGACY=1` | 52.6 | **17.3** | 7.8 | — |
| `RB3_VENUE_POINT_EXPOSURE=0.15` | 49.4 | **15.7** | 8.6 | — |
| `SHARD_GUARD_OFF=1` (shards drawn) | 16.9 (dir) / +2.7 (n00) | **16.9** | 6.6 | — |
| small_club_01 override (crowd off-frame) | 17.1 | **0.01** | 3.25 | 30.5 |

Shots: `shots/bigclub_baseline/`, `shots/bigclub_venueoff/`, `shots/bigclub_legacyfalloff/`,
`shots/bigclub_lowexp/`, `shots/bigclub_guardoff/`.

The white figures are clearly humanoid crowd/extras characters lining BOTH edges of the
highway (see `shots/bigclub_baseline/bcbase_coop_dir_crowd_0.png`). With VENUE_LIGHT_OFF
the whole scene floods bright but the crowd STILL reads stark white relative to a now-bright
background — the crowd is saturated either way.

---

## 2. DISENTANGLE — which sub-part dominates the visible white

### (a) char-EXTRAS / crowd bodies shaded WHITE — DOMINANT
- `RB3_LIGHT_PROBE` (per-mesh cam/env/material): every crowd + extras body/skin/head mesh
  is drawn under **`world.cam`** scoping **`char_rooftop.env`** (the extras) and
  **`rooftop_foreground.env`** (the crowd bodies) — **NOT** the impostor cam STEP 2
  fixes. `prelit=0`, `blend=1`, material `color=(1,1,1)` for the bodies/skin.
- `CHAR_DBG` confirms the diffuse textures DO resolve (`hasTexView=1 type=0x1`,
  e.g. `male_crowd_body01_lod2_diff.tex`, `male_extras_skin_naked_lod01_diff.tex`) — this
  is **NOT** a missing-texture → white-default fallback at the bind level.
- **Pixel analysis (the clincher):** the bright crowd pixels are **98% near-white,
  mean (219,219,219), saturation 0.000**. The crowd is ACHROMATIC. So whatever the
  diffuse SAMPLES to, the visible crowd is `≈ white × white × white × lighting` with no
  dark/chroma term. The crowd LODs simplify to a flat white texture + white vertex color.
- `SHARD_RATIO_DBG`: the crowd BODIES are well-posed (ratio ~1.0–1.3: `male_crowd_body0*`,
  `female_crowd_body0*`, all extras `*_skin*`/`*_body*` 1.00–1.29). They are NOT shards —
  they are correctly-posed characters that are simply lit white.

### (b) MESH-SHARD white smear — MINOR, already guarded
- The only exploding skinned meshes (ratio >2, dropped by the guard): `lowtopsneaks_skin.2`
  (4.88), `male_extras_eyebrows11` (4.70), `scrollbar_bg` (4.01), `male_extras_hair02`
  (2.56). All tiny accessories. The guard DROPS them by default → not visible in ship.
- `SHARD_GUARD_OFF=1` A/B: drawing the shards changes crowd white% by only **0 to +3%**
  (`coop_dir_crowd` crowdR 17.5→16.9 = noise; `coop_all_n00` crowdR 1.5→4.2 = +2.7). So
  the shards are a small additive contributor; the guard is doing its job.
- This is the char-skinning / servo-skeleton family (cf MEMORY `crowd_origin_inststrings`,
  `char_skinning_deform`): crowd/extras bind separate `char/crowd/*` & `char/extras/*`
  skeletons whose bind doesn't match the meshes' inverse-bind under native load.

**Conclusion: sub-part (a) is ~85–100% of the visible white; (b) is the remaining ~0–15%,
already mitigated.** Fixing (a) is the GAP-B win; (b) belongs to the crowd/servo rebake
workstream.

---

## 3. WHY white — the exact pipeline (engine trace)

For a non-prelit, `mUseEnviron` crowd material the shader takes the LIT branch
(`standard_wgsl.inc:843-852`):
```
finalColor = baseColor.rgb * softClipLighting(ambient + diffuse) + specular
```
where `baseColor = material.color × vertexTint × diffuseSample` (`:751-759`), and for a
non-prelit SKINNED mesh `vertexTint = mix(vtxLuma, rawVtx, kSkinnedVtxChroma=0.25)`
(`:744-749`). For the crowd:
- `material.color = (1,1,1)` (CHAR_DBG / LIGHT_PROBE).
- `diffuseSample` resolves to ~white/grey (the visible result is sat 0.000 white — the
  native crowd LOD diffuse is effectively flat-white).
- `vertexTint ≈ white` (crowd LOD baked vertex colors are ~white; the band's are dark AO).
- `softClipLighting(ambient + diffuse)`: under the venue path the extras' env
  `char_rooftop.env` carries a bright point `foregroundred_char01 (2.0,1.91,0.89) r250`,
  the crowd bodies' `rooftop_foreground.env` carries `foregroundred (2.0,0.64,0.14) r500`
  (VENUE_PROBE). Under VENUE_OFF it's a white directional + 0.45 ambient. EITHER way the
  lit term lands ≥ ~0.9 on a crowd-facing surface → softClip ≈ near 1.0.

`white × white × white × ~1.0 = white`. There is no dark term anywhere in the crowd's
pipeline, which is why **no lighting knob moves the white%** — the energy comes from the
material/asset side, and it's already at the ceiling.

Note the shader comment (`:719-723`) already names this exact failure:
> "with the RB3 skin/cloth textures absent from the extracted asset set it is their
> ENTIRE visible colour. Suppressing it [vertex colour] there ... rendered them as
> flat-WHITE silhouettes."

The current `kSkinnedVtxChroma=0.25` desaturate keeps the band readable (band has real AO
vtx colors), but it does NOT help the crowd because the crowd's vtx colors are themselves
~white. There is no value variation to keep.

### Anchors
| claim | anchor |
|---|---|
| crowd extras drawn under world.cam scoping char_rooftop.env / rooftop_foreground.env | `RB3_LIGHT_PROBE` (this run, /tmp/bch-ov-bcbase-*.log) |
| char_rooftop.env: 2 pts incl. foregroundred_char01 (2.0,1.91,0.89) r250 | `RB3_VENUE_PROBE` (this run, /tmp/bch-ov-bcvp-*.log) |
| rooftop_foreground.env: 1 pt foregroundred (2.0,0.64,0.14) r500 | `RB3_VENUE_PROBE` |
| crowd diffuse textures DO load (not missing) | `CHAR_DBG` hasTexView=1 (this run) |
| visible crowd is 98% achromatic white sat 0.000 | pixel analysis of `bcbase_coop_dir_crowd_0.png` |
| band region is colored (sat 0.437, white% 0.01) | pixel analysis (same shot, center-top) |
| white% invariant to VENUE_LIGHT_OFF / falloff / exposure | A/B table §1 |
| LIT branch = baseColor × softClipLighting(amb+diff) | `standard_wgsl.inc:843-852` |
| vertexTint = mix(luma, raw, 0.25) for skinned | `standard_wgsl.inc:744-749`, const `:46` |
| shader already documents the white-silhouette cause | `standard_wgsl.inc:719-723` |
| crowd bodies well-posed (ratio ~1.0–1.3, NOT shards) | `SHARD_RATIO_DBG` (this run) |
| only tiny accessories explode + are dropped | `SHARD_GUARD` drop list (this run) |
| RB3 renderer (Rnd_Wgpu_RB3.cpp) is RB3-only; DC3 uses Rnd_Wgpu.cpp | engine `CMakeLists.txt:303-366` |

---

## 4. small_club (OK) vs big_club (white) — why they differ

NOT an env/material difference. `set_venue_override` reuses the SAME default-boot venue
crowd/extras meshes + the SAME `char_rooftop.env`/`rooftop_foreground.env` lights for both
overrides (VENUE_PROBE shows identical light data for small_club_01 and big_club_01). The
difference is purely **camera framing + which crowd characters are in-frame**: the
small_club camera shots frame the band/venue geometry and the crowd characters fall
off-screen / behind geometry (small_club override crowdR white% 0.01), whereas big_club's
wide crowd shots place the white crowd characters prominently at both edges. The white is
intrinsic to the crowd characters; it just isn't visible when they aren't framed.

This also means **the in-engine "ground-truth" small_club crowd at white% ~0 is not proof
the crowd is correctly shaded there — it's proof it isn't on-camera.** Retail ground-truth
(retail audiences are dim/dark, never bright-white — `ground-truth.md`, retail club frames
`images/retail-screenshots/yt_qRagnZCIMzk_gameplay_*.png`) is the real target.

---

## 5. FIX PLAN

The visible white is a **material/asset achromatic-white** problem, not lighting. There is
no faithful texture to restore (the crowd LOD diffuse is effectively white in this asset
set), so the fix must **inject value/darkening into the crowd render path** so the crowd
reads dim/dark like retail, WITHOUT darkening the band (which renders correctly).

### Sub-part (a) — PRIMARY: dim the crowd/extras character render (engine, DC3-safe)

**Root cause:** crowd/extras non-prelit skinned characters have `color=(1,1,1)` ×
~white vertexTint × ~white diffuse, so any lighting saturates them to white.

**File:** `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (RB3-only TU → DC3 cannot
compile it; DC3 uses `Rnd_Wgpu.cpp`. Byte-identical for DC3 by construction).

**Discriminator (already exists in this file):** distinguish band members from
crowd/extras. The band binds `char/char/main/skeleton_unshared.milo` (the `bandStatic` /
`bandMember` test at `Rnd_Wgpu_RB3.cpp:4236` and `:5154-5162` walks `owner->BoneTransAt`
→ `Dir()->mStoredFile` for `skeleton_unshared.milo`); crowd/extras bind `char/crowd/*` /
`char/extras/*`. Mesh-name patterns (`crowd`, `extra`/`extras`) are a cheaper secondary
signal (every white mesh's name in the LIGHT_PROBE contained `crowd` or `extra`).

**Concrete change — apply a crowd-only darkening multiplier in the material-uniform setup**
(near the existing `mu.color`/`mu.prelit`/`mu.unlit` block, `Rnd_Wgpu_RB3.cpp:5360-5410`),
gated to skinned crowd/extras meshes (NOT band, NOT static venue, NOT UI/text):
```
// GAP B: crowd/extras LOD characters have ~white diffuse + ~white vertex AO in
// the native asset set (no faithful skin/cloth texture), so the LIT term
// saturates them to flat white. Retail audiences are dim/dark (ground-truth).
// Darken ONLY non-band skinned crowd/extras characters so they read as a dim
// audience. Band members (skeleton_unshared.milo) are untouched — they have
// real baked AO and render correctly. Opt out: RB3_CROWD_DIM_OFF=1; tune the
// factor with RB3_CROWD_DIM (default ~0.30).
if (skinned && isCrowdOrExtras(mesh, owner) && !bandMember && !sCrowdDimOff()) {
    float k = sCrowdDim();  // default ~0.30
    mu.color[0] *= k; mu.color[1] *= k; mu.color[2] *= k;
}
```
This multiplies the crowd base toward dark while leaving the lighting model intact, so the
crowd reads as a dim mass (retail look) instead of saturating to white. Because it scales
`baseColor`, it works regardless of the lighting path (which is why it succeeds where every
lighting knob failed).

- **Why color-multiply (not a lighting change):** white% is exposure-invariant because the
  saturation is downstream of lighting; you must attack `baseColor`, not the lit term.
- **Default factor:** start ~0.30 (crowd luma 53 → ~16, into the retail-dim 20s and below
  the white-clip threshold). Tune with the A/B in §6.
- **`isCrowdOrExtras`:** mesh-name contains `crowd`/`extra` OR owner skeleton dir is
  `char/crowd/*`|`char/extras/*` (mirror the `bandMember` skeleton walk, inverted). Use
  BOTH (name OR skeleton) so a name-only LOD without the skeleton signal is still caught.

**Blast radius / safety:**
- **MUST NOT darken the band** — the `!bandMember` gate (skeleton_unshared.milo) excludes
  it; verify band-region luma unchanged in the A/B (band region is colored sat 0.437 today).
- **MUST NOT touch the highway/game.cam** — crowd/extras only draw under world.cam; gem /
  smasher / HUD are not crowd-named and not crowd-skeleton'd. Verify game.cam frame
  unchanged.
- **MUST NOT regress other venues** — the change is crowd-character-scoped and applies
  uniformly to every venue's crowd. festival/arena crowds (also white-prone, the GAP-4
  stress case) get the SAME dim treatment, which is the desired retail look there too.
  small_club crowd is already off-frame, so no visible change there. Confirm each venue's
  band stays lit and the crowd dims toward retail.
- **DC3-safe:** RB3-only TU. No DC3 path.
- **Runtime opt-out:** `RB3_CROWD_DIM_OFF=1` + `RB3_CROWD_DIM=<f>` tuning knob (no rebuild
  to A/B).

**Alternative considered + REJECTED:** lowering `sVenuePointExposure` / venue ambient —
REJECTED because white% is exposure-invariant (POINT_EXPOSURE 0.15 left it white) and it
would over-darken the band + venue. The fix has to be crowd-base-scoped.

### Sub-part (b) — SECONDARY: the accessory mesh-shards — ROUTE OUT of GAP B

The exploding `*_eyebrows*`/`*_hair*`/`lowtopsneaks_skin`/`scrollbar_bg` meshes are already
DROPPED by the V24 SHARD_GUARD by default (invisible in ship; +0–3% white when forced on).
The faithful fix is the crowd/extras servo-skeleton rest-rebake (the same family as
`BandCharacter::RebindOutfitBonesToOwnSkeleton` / `RebindInstStringsToRestBasis`), which is
its own decomp/engine workstream (cf MEMORY `crowd_origin_inststrings`,
`char_skinning_deform`). **Recommendation: do NOT change the shard guard for GAP B** — it's
correctly suppressing these. Route the shard root-cause to the crowd/servo rebake batch.
Note: `lowtopsneaks_skin.2.mesh` is MISCLASSIFIED as "band" by the drop classifier (ratio
4.88, hits the relaxed band cap) — a minor classification-only artifact, not a render bug;
worth a note for that workstream but it does not affect the visible result (still dropped).

### Objective verification (research handoff to impl)
A/B the same `big_club_01 coop_dir_crowd.shot` / `coop_all_n00.shot` pin (anchor-ms 6000):
- **crowd white% → ~0** (from 17.5 / 7.2) and crowd luma into retail-dim ~20s (from ~53).
- **band region UNCHANGED** (luma + colored, white% stays ~0.01) — proves band not darkened.
- **game.cam (highway) frame byte-identical** — crowd-scoped change can't touch it.
- Re-check **arena_02 / festival_01** crowds for the same dim improvement and confirm no
  band regression; confirm **menu hub** unaffected (no crowd characters there).
- Pixel-saturation check: bright crowd pixels should drop well below the 98%-white / 17%
  white-clip; or simply the crowd should no longer be the brightest thing in frame.

### DEFER
- Exact crowd dim factor (`RB3_CROWD_DIM`) is a tuning judgment — leave it as the last knob,
  default ~0.30, tune against retail club crowd dimness once a retail big_club/arena wide
  frame is located (none found yet; `ground-truth.md` §1).
- A more faithful long-term fix would inject a real baked-AO/value term for the crowd LODs
  (so they have form, not just uniform dimness), but no faithful crowd diffuse exists in the
  asset set, so the color-multiply dim is the pragmatic convergence win now.
