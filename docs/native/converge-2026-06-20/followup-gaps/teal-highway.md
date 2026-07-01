# GAP A — Native-only TEAL highway overlay — ROOT CAUSE + FIX PLAN

**Agent: TEAL-HIGHWAY (Opus), 2026-06-21. RESEARCH ONLY — no code/engine change, no commit.**
Master binary `native/build-native/rb3-native` @ engine pin `a360e3c`.

## TL;DR

The "teal/ornate pattern on the note highway" is **the highway surface
`watermark` emissive** — the bass/guitar-clef filigree authored into
`surface.mat` (`mEmissiveMap = watermark_bass.tex` / `watermark_guitar.tex`, the
shared `ui/track/gen/tracksystem.milo_xbox` track surface). It is the **SAME
pattern retail draws** — RB3's highway *does* have this watermark — but native
renders it **~3-4× too bright AND wrongly saturated teal/cyan**, while retail
draws it as a barely-visible dim, near-neutral ghost over a near-black highway.

It is NOT the scrollbar (already disproven), NOT the gem-bloom (persists with
`RB3_HIGHWAY_BLOOM_OFF=1`), NOT the SP `peakstate` spotlight overlay (that draws
with alpha 0.0 except during 4× streak), and NOT a wrong mesh/texture. It is a
**compositing-intensity** divergence in the RB3-only GPU backend's track-light
path.

**Two coupled defects, both in `surface.mat` compositing under `game.cam`:**
1. The watermark **emissive** is too bright + too chromatic (rendered teal).
2. The highway **base** isn't dark enough (background ~12× brighter than retail).

The fix is RB3-only and **DC3-safe by construction**: it lives in
`Rnd_Wgpu_RB3.cpp`, which is compiled ONLY when `MILO_ENGINE_GPU_BACKEND=rb3`
(RB3). DC3 uses a different backend file (`Rnd_Wgpu.cpp`) and never compiles this.

---

## 1. Reproduction (deterministic)

Driver: `docs/native/converge-2026-06-20/followup-gaps/_teal_drive.py` (a thin
copy of the verified `_sbverify_drive.py` with a corrected repo-root path; boots
master binary, drives boot→song_select→part_difficulty→game_screen, nofail +
autohit, captures deep anchors songMs > 30k). Reproduced in **small_club_01**
(default) and **big_club_01** (`set_venue_override`).

Evidence (existing + new):
- `shots/verify/V1_..._teal_highway.png`, `V2_..._bloomoff_teal_persists.png`
  (existing) — teal filigree on highway, songMs 33-35k, persists w/ bloom OFF.
- `followup-gaps/shots/baseline/anchor_00_ms30309.png` (new, default toggles).
- `followup-gaps/shots/trackoff/anchor_0*.png` (new, `RB3_TRACK_LIGHT_OFF=1`).
- `followup-gaps/shots/bigclub/anchor_00_ms30598.png` (new, big_club override).

### Measured highway color (mid-track watermark band, away from now-bar glow)

| | bg luma | watermark-stroke luma | stroke RGB | teal (g+b−2r) |
|---|---|---|---|---|
| **native** (track ON, bloom OFF) | 33.5 | 115.3 | (88,**124**,144) | **+92** |
| **retail** `yt_qRagnZCIMzk_gameplay_guitar.png` | 2.7 | 26.9 | (36,21,34) | **−16** |

Native bg is **~12× brighter**; native watermark is **~3.4× brighter** (stroke−bg
delta 81.8 vs 24.2) AND **strongly teal** (+92), where retail's watermark is a dim,
near-neutral/slightly-purple ghost (−16). Crop comparison: `/tmp/hwy_native.png`
vs `/tmp/hwy_retail.png` (regen-able) — both clearly show the SAME clef-scroll
filigree; only intensity/hue differ.

---

## 2. Isolation (env A/B)

| toggle | effect on teal | conclusion |
|---|---|---|
| `RB3_HIGHWAY_BLOOM_OFF=1` | **persists** (V2; re-confirmed) | NOT the gem/now-bar bloom |
| scrollbar fix on/off | no effect; 0 scrollbar meshes in 12001 game-active ratio evals (verify doc) | NOT the scrollbar |
| `RB3_TRACK_LIGHT_OFF=1` | watermark becomes **neutral** (stroke (165,180,151), teal **+1**) but the whole highway turns **bright washed grey** — *further* from retail | track-light is the CORRECT direction (dark highway) but it is what makes the watermark read teal against the darkened base |
| venue override (small_club / big_club) | teal present in both (small +92, big +20) | venue-invariant (watermark is in the shared `tracksystem.milo`); the residual venue delta is just backdrop ambient reflected into the frame |

There is **no toggle today that isolates the watermark emissive** — it is set
unconditionally (see §3). The base color and the watermark emissive are the ONLY
two things `surface.mat` can output (`diff=null`), so the teal is necessarily the
watermark emissive interacting with the darkened base.

---

## 3. Exact mesh / material / pass + WHY it diverges

Runtime instrumentation (`GEM_DBG=1`, `RB3_LIGHT_PROBE=1`) on master:

```
[GEM_DBG] GemTrackDir::UpdateSurfaceTexture: mesh='flat_curve.mesh' mat='surface.mat' tex='(null)'
[LIGHT_PROBE] mesh='flat_curve.mesh' cam='game.cam' env='track.env' prelit=1 blend=3
              color=(1,1,1,1) mat='surface.mat' diff=null emisMul=0.40 emisMap=watermark_bass.tex
[LIGHT_PROBE] mesh='peakstate_plane.mesh' ... mat='peakstate_plane.mat'
              diff=spotlight_bass_track.tex emisMul=0.00 emisMap=null   <- alpha 0.0, NOT contributing
```

- **Drawable:** `flat_curve.mesh` (the highway surface plane), material
  `surface.mat`, drawn under `game.cam` (near30/far224), env `track.env`,
  blend=3 (`kBlendSrcAlpha`), prelit, **diffuse = null** (authored: the highway has
  no diffuse texture — `GemTrackDir::mSurfaceTexture` loads null from the milo,
  `src/system/bandobj/GemTrackDir.cpp:151,388-404`; this is correct, not a bug).
- **The teal = `surface.mat`'s EMISSIVE map** `watermark_bass.tex` (per-instrument:
  `watermark_{bass,guitar,drum,keys}.tex`), authored `mEmissiveMultiplier = 0.40`,
  in the shared `ui/track/gen/tracksystem.milo_xbox`.
- **The SP `peakstate` spotlight overlay is NOT the cause** in steady play — its
  alpha is 0.0 until a 4× streak fades it in via PropAnim.

### The compositing math (engine, `milo-native-engine`)

1. `Rnd_Wgpu_RB3.cpp:5408-5411` ("menu-lighting fix 2", commit `7acc22a`) sets,
   for **every** material with an emissive map, on **every** camera:
   `mu.emissiveMultiplier = mat->mEmissiveMultiplier;` → surface.mat = **0.40**.
   (Before `7acc22a` this was set only inside the game.cam block; the original
   intent — commit `f5ee015` — was explicitly "the surface watermark survives via
   the emissive term." So the watermark is *deliberately* re-enabled; it is just
   too strong.)
2. `Rnd_Wgpu_RB3.cpp:5557-5559` (game.cam block) darkens the surface BASE only:
   `mu.color[0..2] *= 0.12f;` → base = (0.12,0.12,0.12). It does **NOT** touch the
   watermark emissive.
3. Shader `src/gfx/standard_wgsl.inc:855-868`:
   ```
   emissiveSample = textureSample(emissiveMapTex, uv);          // = watermark RGB (teal)
   emBaseLuma    = max(baseColor.rgb);                          // = 0.12
   emissiveTint  = mix(white, baseColor.rgb, smoothstep(0,0.04,0.12)=1.0) = (0.12,0.12,0.12)
   finalColor   += emissiveTint * emissiveMultiplier * emissiveSample.rgb;  // += 0.12*0.40*tealRGB
   ```
   `finalColor` for a prelit mat = `baseColor.rgb` (0.12 grey) `+` the watermark add.

**Why teal:** the watermark texture (`watermark_*.tex`) is itself a **cyan/teal**
image (proven: with track-light OFF the surface base is bright white (1,1,1), which
washes the add toward neutral → stroke neutral +1; with track-light ON the base is
dark grey 0.12, so the colored teal emissive add **dominates** the dark base →
stroke reads teal +92). So the chroma is intrinsic to the asset; native exposes it
because emisMul 0.40 over a darkened base is far stronger than retail's faint
watermark.

**Why too bright:** native bg luma 33.5 vs retail 2.7. The ×0.12 darken (5558)
leaves the prelit surface (sRGB(0.12)≈0.39) noticeably grey rather than near-black,
and the 0.40 watermark add lands ~3.4× retail's stroke delta.

---

## 4. FIX PLAN (engine, RB3-only, DC3-safe)

All edits are in **`/home/free/code/milohax/milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`**
inside the existing `surface.mat` branch of the `game.cam` track-light block
(`:5557-5559`). **DC3-safety is structural:** RB3 sets
`MILO_ENGINE_GPU_BACKEND=rb3` → compiles `Rnd_Wgpu_RB3.cpp`; DC3 sets `dc3` →
compiles `Rnd_Wgpu.cpp`. DC3 never compiles this file (verified
`rb3/native/CMakeLists.txt:188`, `dc3-decomp/native/CMakeLists.txt:257`, engine
`CMakeLists.txt:100-131`). No flag/ifdef gymnastics needed; nonetheless add an
opt-out env for A/B per house style.

### Change (single, localized) — dim + desaturate the watermark emissive for surface.mat under game.cam

In the `if (std::strcmp(mname, "surface.mat") == 0)` block at `:5557`, after the
existing `mu.color *= 0.12` line, add a surface-specific watermark control that
(a) cuts the emissive multiplier and (b) blends the teal toward neutral so it
reads as the dim near-grey ghost retail shows. Pseudo-shape (tune constants
against the measured target below):

```cpp
if (std::strcmp(mname, "surface.mat") == 0) {
    mu.color[0] *= 0.12f; mu.color[1] *= 0.12f; mu.color[2] *= 0.12f;
    // Watermark (clef filigree) emissive: native rendered it ~3.4x too bright and
    // teal-saturated vs retail's faint near-neutral ghost. Dim the multiplier and
    // desaturate the emissive add. RB3-only file (DC3 uses Rnd_Wgpu.cpp).
    static int sWmOff = -1;
    if (sWmOff < 0) { const char* e = getenv("RB3_HIGHWAY_WATERMARK_OFF");
                      sWmOff = (e && e[0] && e[0] != '0') ? 1 : 0; }
    mu.emissiveMultiplier = sWmOff ? 0.0f : mu.emissiveMultiplier * <K_DIM>;  // ~0.30-0.40
}
```

- **(a) Dim:** scale `mu.emissiveMultiplier` (0.40) by `K_DIM`. Target: bring the
  watermark stroke delta from native ~82 down to retail ~24 → `K_DIM ≈ 24/82 ≈
  0.30` as a first estimate (verify by measurement, §"verification").
- **(b) Desaturate (optional, if dimming alone still reads teal):** the simplest
  shader-free way is to add a uniform that mixes the emissive sample toward its own
  luminance before the add. Two options:
  - *No-shader option:* the existing `emissiveTint` already multiplies the sample;
    if the dim alone matches retail luma but the residual hue is still too teal,
    add a small `desat` uniform consumed in the shader's emissive line
    (`standard_wgsl.inc:868`): `let em = mix(emissiveSample.rgb,
    vec3f(luma(emissiveSample.rgb)), material.emissiveDesat);` — guard it so the
    default (desat 0) is byte-identical for every other material (gems, now-bar).
  - *CPU-only fallback (preferred if it suffices):* the dim probably suffices —
    retail's watermark is only *slightly* less teal-saturated than native once
    dimmed (the teal mostly comes from the high multiplier, not pure hue). Do the
    dim first; only add the shader desat if the measurement still shows
    teal > ~+30 after dimming.

### Optionally also darken the base further (background brightness)

native bg luma 33.5 vs retail 2.7. If, after dimming the watermark, the highway
*surface* still reads too bright/blue, drop the base further: change `0.12f` →
~`0.06-0.08f` at `:5558`. CAUTION: this is shared with the lit-lanes look and was
adversarially tuned in MEMORY `a234`; change it only with a full track A/B (gems
must still pop, lanes must stay bright). Lower-risk to leave 0.12 and rely on the
watermark dim, since the dominant *visible* artifact is the bright teal watermark,
not the base grey.

### Files / lines
- Engine: `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:5557-5559` (add
  watermark dim/desat); optionally `:5558` (base darken); optionally shader
  `milo-native-engine/src/gfx/standard_wgsl.inc:868` + `UniformStructs.h`
  (`MaterialUniforms`) if the shader desat path is needed.
- No `src/` (RB3 game) change required. No Wii-match impact (engine-only,
  HX_NATIVE render path).

### Blast radius
- **Tiny + RB3-only.** The branch is keyed on `pc->Name()=="game.cam"` AND
  `mname=="surface.mat"`, so it touches ONLY the gameplay highway surface, only in
  RB3. Gems (`prism_mat`), now-bar (`gem_smasher_glow`), lanes (`rails.mat`), the
  SP `peakstate` overlay, the venue/band/crowd (world.cam), and all HUD are
  untouched. If the shader desat uniform is added, default it to 0 so every other
  draw is byte-identical.
- DC3: zero (different backend file).
- Wii match: zero (engine-only).

---

## 5. Objective verification (for the implementer)

A/B with the new `RB3_HIGHWAY_WATERMARK_OFF` env (and the existing
`RB3_TRACK_LIGHT_OFF` / `RB3_HIGHWAY_BLOOM_OFF`). Re-run
`followup-gaps/_teal_drive.py` to a deep anchor (songMs > 30k) and measure the
**mid-track watermark band** (crop `x∈[0.38,0.62], y∈[0.50,0.62]`, top-25%-luma =
"stroke", bottom-50% = "bg"); compare to retail
`images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png`:

| metric | native now | retail target | pass band |
|---|---|---|---|
| watermark stroke−bg delta | 81.8 | 24.2 | ≤ ~35 |
| stroke teal (g+b−2r) | +92 | −16 | ≤ ~+25 |
| bg luma | 33.5 | 2.7 | ≤ ~15 (only if base also darkened) |

Pass = the highway reads as a near-black track with a faint, near-neutral clef
ghost (retail look), gems/lanes/now-bar unchanged. Sanity: `RB3_HIGHWAY_WATERMARK_OFF=1`
must remove the pattern entirely (proves the source), and the default-ON state
must match retail within the bands above. Also confirm in big_club_01 (the venue
delta should shrink to retail-like in both).

The measurement one-liner used in this investigation is in §1/§3 above (PIL crop +
percentile luma/teal); reusable verbatim.

---

## Anchors (every claim)
- Mesh/mat/tex binding: `GEM_DBG` + `RB3_LIGHT_PROBE` runtime logs
  (`/tmp/teal_gemdbg.log`, `/tmp/teal_lightprobe.log`); src
  `GemTrackDir.cpp:388-404` (UpdateSurfaceTexture), `:145-148` (mSurface* decl),
  `:151` (binstream load of null surface_texture).
- Emissive set unconditionally: `Rnd_Wgpu_RB3.cpp:5408-5411` (commit `7acc22a`).
- Surface base darken (game.cam): `Rnd_Wgpu_RB3.cpp:5557-5559` (commit `f5ee015`
  original intent: "surface watermark survives via the emissive term").
- Shader emissive composite: `standard_wgsl.inc:855-868`.
- peakstate NOT contributing: `RB3_LIGHT_PROBE` (alpha 0.0, emisMul 0.0) +
  `Rnd_Wgpu_RB3.cpp:5601-5608`.
- Scrollbar disproven: `docs/native/converge-2026-06-20/scrollbar-fix-verify.md`.
- Bloom disproven: persists w/ `RB3_HIGHWAY_BLOOM_OFF=1` (V2 + re-confirm).
- DC3-safety: engine `CMakeLists.txt:100-131`; `rb3/native/CMakeLists.txt:188`
  (rb3 backend → Rnd_Wgpu_RB3.cpp); `dc3-decomp/native/CMakeLists.txt:257` (dc3 →
  Rnd_Wgpu.cpp).
