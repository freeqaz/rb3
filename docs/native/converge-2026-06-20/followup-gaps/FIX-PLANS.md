# Converge follow-up gaps — IMPLEMENTABLE FIX PLANS (synthesis)

**Synthesis agent (Opus), 2026-06-21. RESEARCH ONLY — no code/engine change, no
commit.** Synthesizes the three root-cause docs in this dir
(`teal-highway.md`, `bigclub-crowd.md`, `ground-truth.md`). Verified every engine
anchor against `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` and
`src/gfx/standard_wgsl.inc` at engine pin `a360e3c`.

Two genuinely-visible gaps. **Three fix sub-bugs total** (GAP A is one fix; GAP B
splits into a primary engine fix + a route-out). All engine fixes land in
`Rnd_Wgpu_RB3.cpp` — the **RB3-only** GPU backend TU (DC3 compiles
`Rnd_Wgpu.cpp`), so they are **DC3-safe by construction** with zero ifdef/flag
gymnastics. The one shader edit needed for GAP A is shared and MUST default to a
no-op (byte-identical for DC3 + every non-watermark material).

---

## GAP A — native-only TEAL highway watermark (highest-frequency-visible)

### Confirmed root cause

The "teal/ornate pattern" is the **authored highway watermark** — `surface.mat`'s
EMISSIVE map (`watermark_{bass,guitar,drum,keys}.tex`, the clef-scroll filigree in
the shared `ui/track/gen/tracksystem.milo_xbox`). Retail draws the SAME pattern, so
removing it would be WRONG (ground-truth doc A1: identical swirl is present in
retail). It diverges on **value + hue**, two stacked errors confirmed by both
instrumented (LIGHT_PROBE: `mat='surface.mat' emisMul=0.40 emisMap=watermark_bass.tex`)
and pixel measurement:

1. **Too bright** — watermark stroke−bg delta native 81.8 vs retail 24.2 (~3.4×).
   The emissive multiplier is 0.40 over a base darkened to ×0.12, far stronger than
   retail's faint ghost.
2. **Teal-saturated** — stroke teal (g+b−2r) native +92 vs retail −16; G/B native
   0.71 vs retail 0.36. The chroma is intrinsic to the asset (cyan/teal watermark
   texture); it reads neutral when track-light is OFF (bright white base washes the
   add) but dominates teal against the ×0.12 dark base in the shipped path.

Mechanism (verified anchors):
- emissive multiplier set unconditionally for any mat with an emissive map:
  `Rnd_Wgpu_RB3.cpp:5408-5410` (`mu.emissiveMultiplier = emTex ? mat->mEmissiveMultiplier : 0` → surface.mat = 0.40).
- surface base darkened ONLY (game.cam block): `Rnd_Wgpu_RB3.cpp:5557-5559`
  (`mu.color *= 0.12`; does NOT touch the emissive).
- composite: `standard_wgsl.inc:866-868`
  `emissiveTint = mix(white, baseColor, smoothstep(0,0.04,baseLuma))` then
  `finalColor += emissiveTint * material.emissiveMultiplier * emissiveSample.rgb`.
  With base luma 0.12 ≫ 0.04, `emissiveTint = baseColor = (0.12,0.12,0.12)` —
  already value-tinted, but the teal sample's CHROMA passes through unchanged.
  So `finalColor += 0.12 * 0.40 * tealRGB`.

### The fix (engine, RB3-only, DC3-safe)

In the `surface.mat` branch of the game.cam track-light block
(`Rnd_Wgpu_RB3.cpp:5557-5559`), after the existing `mu.color *= 0.12`:

**Step A1 — dim the watermark emissive (the dominant visible error):**
```cpp
if (std::strcmp(mname, "surface.mat") == 0) {
    mu.color[0] *= 0.12f; mu.color[1] *= 0.12f; mu.color[2] *= 0.12f;
    // GAP A: the surface watermark (watermark_*.tex clef filigree, emisMul 0.40)
    // renders ~3.4x too bright AND teal-saturated vs retail's faint near-neutral
    // ghost (retail stroke delta ~24 G/B ~0.36; native ~82 G/B ~0.71). Dim it.
    // RB3-only file (DC3 uses Rnd_Wgpu.cpp). Opt out: RB3_HIGHWAY_WATERMARK_OFF=1.
    static int sWmOff = -1;
    if (sWmOff < 0) { const char* e = getenv("RB3_HIGHWAY_WATERMARK_OFF");
                      sWmOff = (e && e[0] && e[0] != '0') ? 1 : 0; }
    static float sWmDim = -1.0f;
    if (sWmDim < 0.0f) { const char* e = getenv("RB3_HIGHWAY_WATERMARK_DIM");
                         sWmDim = e ? (float)atof(e) : 0.30f; }
    mu.emissiveMultiplier = sWmOff ? 0.0f : mu.emissiveMultiplier * sWmDim;
}
```
Target `K_DIM ≈ 24/82 ≈ 0.30` to bring stroke delta to retail's ~24. Tunable
without rebuild via `RB3_HIGHWAY_WATERMARK_DIM`.

**Step A2 — desaturate the teal (ONLY if dimming alone still reads teal > +25):**
Dimming reduces brightness but does NOT change G/B ratio, so the residual hue may
still read teal. The minimal shader-side desaturate (default no-op → DC3-safe):
- add a `materialEmissiveDesat` field to `MaterialUniforms` (default 0.0);
- in `standard_wgsl.inc:868`, mix the sample toward its luma:
  `let em = mix(emissiveSample.rgb, vec3f(luminance(emissiveSample.rgb)), material.materialEmissiveDesat);`
  then `finalColor += emissiveTint * material.emissiveMultiplier * em;`
- set `mu.materialEmissiveDesat = <0.4-0.6>` ONLY in the surface.mat branch above;
  every other material keeps 0.0 → byte-identical (gems, now-bar, menu neon, DC3).

**Do A1 first; measure; only add A2 if the measured stroke teal stays > +25.** The
teal mostly comes from the high multiplier over a dark base, so dimming alone may
land hue inside the band. Defer A2 unless measurement demands it.

**Optionally** drop the base further (`0.12f → 0.06-0.08f` at :5558) if the bg luma
is still too bright (native 33.5 vs retail 2.7) — but this is shared with the
lit-lanes look and was adversarially tuned (MEMORY a234); leave at 0.12 unless the
watermark dim alone leaves the surface visibly grey. Lower-risk to rely on the
watermark dim, since the dominant visible artifact is the bright teal filigree, not
the base grey.

### Files / lines
- `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:5557-5559` (A1 dim;
  optionally :5558 base). **Pin-bump required** (engine commit → bump
  `MILO_ENGINE_PIN` in `rb3/native/CMakeLists.txt`).
- A2 only: `../milo-native-engine/src/gfx/standard_wgsl.inc:868` +
  `UniformStructs.h`/`MaterialUniforms` (default 0.0 → no-op).
- No `src/` (RB3 game) change. No Wii-match impact.

### DC3 risk + mitigation
- A1: **zero** — `Rnd_Wgpu_RB3.cpp` is RB3-only (DC3 compiles `Rnd_Wgpu.cpp`;
  verified engine `CMakeLists.txt`, `rb3/native/CMakeLists.txt:188`).
- A2 (shared shader): default `materialEmissiveDesat = 0.0` makes the new mix a
  no-op for every material on every backend → DC3 byte-identical. Mitigation =
  the default; only surface.mat (RB3-only set site) ever raises it.

### Blast radius / what must NOT regress
- Keyed on `pc->Name()=="game.cam"` AND `mname=="surface.mat"` → touches ONLY the
  gameplay highway surface, only in RB3. Gems (`prism_mat`), now-bar
  (`gem_smasher_glow`), lit lanes (`rails.mat`), the SP `peakstate` overlay, all
  HUD/menu, and the whole world.cam venue/band/crowd are untouched.
- Must NOT remove the pattern (ground-truth: retail has it) — only dim/desaturate.
- `RB3_HIGHWAY_WATERMARK_OFF=1` must remove it entirely (proves source).

### Objective verification metric + toggle
Re-run `followup-gaps/_teal_drive.py` to a deep anchor (songMs > 30k); measure the
mid-track watermark band (crop `x∈[0.38,0.62] y∈[0.50,0.62]`, top-25%-luma =
stroke, bottom-50% = bg) vs `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png`:

| metric | native now | retail | pass band |
|---|---|---|---|
| stroke−bg delta | 81.8 | 24.2 | ≤ ~35 |
| stroke teal (g+b−2r) | +92 | −16 | ≤ ~+25 |
| stroke G/B | 0.71 | 0.36 | ≤ ~0.55 |

Toggles: `RB3_HIGHWAY_WATERMARK_OFF=1` (off), `RB3_HIGHWAY_WATERMARK_DIM=<f>`
(tune), existing `RB3_TRACK_LIGHT_OFF` / `RB3_HIGHWAY_BLOOM_OFF`. Confirm in
big_club_01 too. Sanity: gems/lanes/now-bar visually unchanged in same anchor.

### Confidence: **HIGH** (instrumented mesh/mat/tex binding + measured value+hue;
single localized branch; toggle to prove source). A2 desaturate is a contingency,
not a certainty — flagged as measure-gated.

---

## GAP B — visible big_club_01 WHITE crowd (TWO sub-bugs)

Decisive A/B across both docs: crowd white% is **INVARIANT** to every lighting knob
(VENUE_LIGHT_OFF triples crowd luma 53→131 but white% 17.5→17.6; POINT_EXPOSURE
0.70→0.15 leaves it ~16-17%). So it is a **material/asset value** problem, NOT
lighting/exposure. STEP-1 GX falloff is EXONERATED; STEP-2 impostor gate did not
move it. The visible white is two sub-bugs, quantified:

### Sub-bug B(a) — DOMINANT (~85-100% of visible white): crowd/extras shaded flat-WHITE — FIX HERE

**Confirmed root cause:** world.cam crowd/extras non-prelit skinned characters have
`material.color=(1,1,1)` × ~white vertexTint × ~white diffuse, so any lit term
saturates to white. Measured: bright crowd pixels are **98% near-white, mean
(219,219,219), saturation 0.000** — pure achromatic. The native crowd LOD diffuse
is effectively flat-white in the extracted asset set, and the crowd LOD baked vertex
colors are ~white too (unlike the band, which carries real dark baked AO: band
region sat 0.437, white% 0.01). `kSkinnedVtxChroma=0.25` desaturate can't help —
there's no value variation to keep. The shader already documents this exact failure
(`standard_wgsl.inc:719-723`). LIT branch: `baseColor.rgb * softClipLighting(...)`
(`:843-852`), `baseColor = color × vertexTint × diffuse` (`:751-759`).

**Fix (engine, RB3-only, DC3-safe):** crowd-base color-multiply in the material
setup block (`Rnd_Wgpu_RB3.cpp:5352-5410`, where `mat`/`mu`/`owner`/`mesh` are all
in scope — verified). Because the saturation is downstream of lighting, the fix MUST
attack `baseColor`, not the lit term (which is why every lighting knob failed):
```cpp
// GAP B(a): crowd/extras LOD characters have ~white diffuse + ~white vtx AO in the
// native asset set (no faithful skin/cloth texture), so the LIT term saturates them
// to flat white. Retail audiences are dim/dark. Darken ONLY non-band skinned
// crowd/extras (NOT band, NOT static venue, NOT UI/text). RB3-only file.
// Opt out RB3_CROWD_DIM_OFF=1; tune RB3_CROWD_DIM (default 0.30).
if (skinned && isCrowdOrExtras(mesh, owner) && !bandMember
        && !isTextMeshHeur && !isLikelyUiText && !sCrowdDimOff()) {
    float k = sCrowdDim();  // default 0.30
    mu.color[0] *= k; mu.color[1] *= k; mu.color[2] *= k;
}
```
- **Discriminator `isCrowdOrExtras`:** invert the existing band detector — mesh-name
  contains `crowd`/`extra` OR the owner's bone-skeleton dir is `char/crowd/*` |
  `char/extras/*` (mirror the `bandMember` skeleton walk at `:5154-5162`, inverted).
  Use BOTH (name OR skeleton) so a name-only LOD without the skeleton signal is
  caught. `bandMember` (skeleton_unshared.milo) is the hard exclusion that protects
  the band.
- **Default factor 0.30:** crowd luma 53 → ~16 (into retail-dim 20s, below the
  white-clip threshold). Tunable via `RB3_CROWD_DIM` (no rebuild to A/B).

**DC3 risk:** zero — RB3-only TU.

**Blast radius / what must NOT regress:**
- MUST NOT darken the band — `!bandMember` (skeleton_unshared.milo) gate excludes
  it; verify band region stays luma-unchanged + colored (sat 0.437).
- MUST NOT touch highway/game.cam — crowd/extras only draw under world.cam; gem/
  smasher/HUD aren't crowd-named/skeleton'd. Plus the text/UI exclusions above.
- MUST NOT regress other venues — change is crowd-scoped + uniform; festival/arena
  crowds get the same desired dim. small_club crowd is off-frame (no visible change).
- Do NOT confuse with authored-white surfaces: video_01 studio backdrop +
  festival_01 B&W comic crowd backdrop are CORRECT and are NOT crowd characters
  (not skinned crowd/extras meshes) → untouched by this gate.

**Objective verification metric + toggle:** A/B `big_club_01 coop_dir_crowd.shot` /
`coop_all_n00.shot` pin (anchor-ms 6000):
- crowd white% (channels >200) → ~0 (from 17.5 / 7.2); crowd luma into ~20s.
- band region UNCHANGED (luma + colored, white% ~0.01) — proves band not darkened.
- game.cam (highway) frame byte-identical.
- re-check arena_02 / festival_01 crowds dim + no band regress; menu hub unaffected.
Toggle: `RB3_CROWD_DIM_OFF=1`, `RB3_CROWD_DIM=<f>`.

**Confidence: HIGH** (98%-achromatic-white measured; exposure-invariant proven;
band/crowd discriminator already exists in-file; baseColor-multiply is the only
lever that works downstream of the saturating lighting).

### Sub-bug B(b) — MINOR (~0-15%): accessory mesh-shards — ROUTE OUT (do NOT fix in GAP B)

A few tiny accessories explode and are already CORRECTLY DROPPED by the V24
SHARD_GUARD by default (`lowtopsneaks_skin.2` 4.88, `male_extras_eyebrows11` 4.70,
`scrollbar_bg` 4.01, `male_extras_hair02` 2.56). `SHARD_GUARD_OFF=1` A/B shows
forcing them on changes crowd white% by only 0 to +3% → the guard is doing its job;
they are mostly invisible in ship. This is the char-skinning/servo-skeleton family
(MEMORY `crowd_origin_inststrings`, `char_skinning_deform`). **Do NOT change the
shard guard for GAP B** — it's correctly suppressing these. The faithful fix is the
crowd/extras servo-skeleton rest-rebake (same family as
`RebindOutfitBonesToOwnSkeleton` / `RebindInstStringsToRestBasis`), its own
decomp/engine workstream. Note for that workstream: `lowtopsneaks_skin.2.mesh` is
misclassified as "band" by the drop classifier (ratio 4.88 hits the relaxed band
cap) — classification-only artifact, still dropped, no render impact.

**DC3 risk:** N/A (no change). **Verification:** none needed in GAP B; route the
shard root-cause to the crowd/servo rebake batch.

---

## RECOMMENDED IMPLEMENTATION BATCH ORDERING

1. **GAP B(a) crowd-dim** FIRST. Highest confidence, single localized engine edit,
   discriminator already exists in-file, exposure-invariance already proves the
   lever. Land + verify (crowd white%→0, band unchanged, game.cam byte-identical),
   commit engine, bump pin.

2. **GAP A(1) watermark dim** SECOND. Also single localized engine edit in the same
   file — batch into the SAME engine commit + pin-bump as B(a) to avoid two
   rebuilds/pin-bumps (both are `Rnd_Wgpu_RB3.cpp`, both RB3-only, both default-on
   with an opt-out env). Verify stroke delta + teal against retail.

3. **GAP A(2) watermark desaturate** ONLY IF A(1) verification still shows stroke
   teal > +25 / G/B > 0.55. This is the only edit that touches a shared file
   (`standard_wgsl.inc`) — keep it default-no-op + DC3 byte-identical, and verify
   DC3 build is unchanged. Defer until measurement demands it; do not pre-emptively
   add a shared-shader uniform.

4. **GAP B(b) mesh-shards** — NOT in this batch. Route to the crowd/servo-skeleton
   rebake workstream (already mitigated by the guard).

Net: ONE engine commit + ONE pin-bump covers B(a) + A(1) (the visible wins). A(2) +
B(b) are conditional/routed-out follow-ups.
