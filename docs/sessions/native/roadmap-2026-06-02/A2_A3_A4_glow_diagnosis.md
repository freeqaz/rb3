# A2 / A3 / A4 — gameplay glow/HUD diagnosis (2026-06-02, Opus workflow)

Produced by a 3-agent parallel diagnosis workflow (read-only) comparing fresh native
gameplay captures (`/tmp/trackA-shots/`, BandRnd backend, A1 `after_hide` fix applied)
against retail refs `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_{guitar,drums_starpower}.png`.
Each agent did its own image review (Opus) + code/asset tracing + a runtime probe.

## TL;DR — one shared root cause

**A2 (gem/fret glow), A4 (highway/lane glow), and the A3 HUD glow all share a single
engine gap: `BandRnd::DrawMesh` drops the material EMISSIVE (self-illumination) feature.**
In `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`:

1. The material-uniform fill in `DrawMesh` (~2819-2963) populates color / useTexture /
   intensify / prelit / useAlphaAsRGB / texGen but **never reads `RndMat::mEmissiveMultiplier`
   or `mEmissiveMap`**, so `MaterialUniforms.emissiveMultiplier` stays value-initialized to 0.
2. `MakeMaterialBindGroup` (~1059-1089) **hardcodes the emissive map slot** (group 1
   binding 5 = `emissiveMapTex`) to `mBlackView` — the material's emissive texture is never bound.
3. The WGSL shader **already implements emissive** (`standard_wgsl.inc:799-800`:
   `finalColor += baseColor.rgb * material.emissiveMultiplier * emissiveSample.rgb`, for both
   prelit and lit paths) — so it is inert because `mult==0` **and** the map is black.

The gem/smasher/track milos ship the emissive assets (`prism_gem_emissive.tex`,
`*_ems.tex`, `square_smasher_bright_*.tex`, `spotlight_guitar_track_emmissive.tex`,
`now_bar_glow.mat`, `overdrive_glow.mat`, `track_gems.env`). RB3 `RndMat` exposes the
members (`src/system/rndobj/Mat.h:284,286` — `mEmissiveMultiplier` @0x78, `ObjPtr<RndTex>
mEmissiveMap` @0x7c). The pattern reference (NOT compiled for RB3) is DC3
`MaterialSetup.cpp:86`: `emissiveMultiplier = GetEmissiveMap() ? GetEmissiveMultiplier() : 0`.

### The shared fix (engine, layer (b))
In `Rnd_Wgpu_RB3.cpp`: (1) in `DrawMesh`, set `mu.emissiveMultiplier = mEmissiveMap ?
mat->mEmissiveMultiplier : 0` (the **guard is mandatory** — without it every non-emissive
mesh gets tinted); (2) in `MakeMaterialBindGroup`, resolve `mEmissiveMap` via
`GetRB3TexView`/`UploadRndTexIfNeeded` (same path as diffuse, so it hits the texture cache)
and bind it to `e[5]` instead of `mBlackView`. Effort: **small**. Blast radius: lights every
emissive-mapped mesh engine-wide → A/B a character/venue/menu frame for regressions.

### ⚠️ MANDATORY first step (the A1 lesson — do NOT skip)
All three agents flagged the **same caveat**: it is confirmed that the emissive path is
*unimplemented*, but **NOT** runtime-confirmed that the gem/highway/HUD materials actually
carry `mEmissiveMultiplier != 0` and a non-null `mEmissiveMap` at draw time. Static analysis
gave the wrong A1 answer once. **Before editing material code, add a one-shot env-gated
`fprintf(stderr)` in `DrawMesh`** that, for meshes named `prism_gem*` / `gem_smasher*` /
`smasher_plate*` / `flat_curve*` / `track_rails*` / `*glow*` / `*meter*`, prints
`mat->mEmissiveMultiplier`, `mEmissiveMap ? mEmissiveMap->Name() : "null"`, `mPreLit`,
`mUseEnviron`, blend, and the per-vertex color range. Boot to gameplay (autohit), `grep -a`
the kept log. If the materials request emissive → the fix above is right. If they DON'T, the
glow comes from a separate additive `*_glow.mat` overlay submesh or a pre-brightened diffuse,
and the fix differs. (Build engine with `cmake --build native/build-native` — the `milo-engine`
lib is a separate target that `--target rb3-native` does NOT rebuild.)

---

## Per-item detail

### A2 — Gem / fret-button glow  · confidence HIGH · effort SMALL · layer (b)
Native gems are flat matte rounded-rects with hard edges, zero halo; retail gems have a
self-lit glossy core + soft bloom halo. Runtime `GEM_VTX` probe confirmed the prism gem
material is otherwise correct: `color(1,1,1,1) blend=3(SrcAlpha) zmode=1 intensify=0 prelit=1
hasDiffuse=1` — draws, prelit+blend honored, diffuse binds. So flatness is **not** blend/prelit/
geometry — it's the missing emissive boost (the core never reaches the ~0.9-luma bloom onset
at `Rnd_Wgpu_RB3.cpp:1717`, so it also gets no halo). The hit-time additive overlay
(`gem_smasher_glow.mesh`/`*_glow.mat`) is a SEPARATE thing that already draws with correct
additive blend — not this bug, and distinct from A1's particle FX. **The shared emissive fix
is A2's fix.** Follow-up for full parity: `track_gems.env` environ-cube reflection (`mUseEnviron`)
— a larger, deferred sub-feature.

### A3 — Star-power / multiplier HUD gauge  · confidence MEDIUM · effort MEDIUM · layer (b)+verify(a)
Three distinct deltas vs retail: (1) the glowing multiplier number + cyan streak-meter
glow/glass ring at the deploy disc is missing (only a partial OverdriveMeter radial pie draws);
(2) the star-power gauge bar is missing; (3) the `BandScoreboard` shows 1 star disc instead of
retail's discrete 5-star row. The **data feed works** (score "1,424" accumulates with autohit;
`StreakMeter::Reset` has an existing `HX_NATIVE` block proving the meter runs natively). The
streak/overdrive meters are emissive/additive **glow/glass** materials (`streak_meter_blue_glow.mat`,
`overdrive_glow.mat`, `streak_meter_glass.mat`, `multiplier_meter_glow`) — i.e. the **same
emissive/additive-glow family** as A2/A4. Two item-specific unknowns to confirm by probe
(do NOT edit blindly): **(i)** do the glow/glass/`multiplier.lbl` meshes reach `DrawMesh`
showing-and-in-a-drawn-group (vs parent group `Showing()==false`)? **(ii)** is the per-track
`StreakMeter::SetMultiplier` fed natively (mult rises >1 so `UpdateMultiplierText` re-shows
`multiplier.lbl`) — the existing force-hide in `StreakMeter.cpp:161-177` keeps it hidden until
then. The 5-star scoreboard delta is a separate, lower-confidence sub-issue (discrete
`star0..star4` meshes possibly hidden by a non-running anim) and is entangled with the
out-of-scope top-center camera-frame positioning (`SCORE_HUD.md` item #1). **Note:** an
`/api/dta/eval` call SIGABRT-wedged the eval queue this session — use `fprintf` probes, not
DTA object-walking.

### A4 — Highway lane lighting / glow  · confidence MEDIUM · effort MEDIUM · layer (b)
Native highway is a uniformly mid-gray matte plane with faint lane lines; retail is a DARK
surface with bright saturated glowing colored lane edges + a luminous now-bar. Geometry renders
fine (`flat_curve.mesh` 284×, `track_rails_guitar.mesh` 284×). **Three compounding causes:**
(1) the **shared emissive gap** (above) — `now_bar_glow`/`overdrive_glow`/`spotlight_guitar_track_emmissive.tex`
contribute nothing; (2) **scene lighting ignored** — `WriteSceneUniforms` (`Rnd_Wgpu_RB3.cpp:877-885`)
hardcodes a single white directional + flat 0.45 grey ambient and **never reads
`RndEnviron::Current()`/`RndLight`** (DC3 `Rnd_Wgpu.cpp:1386-1540` does), flooding the dark
surface to gray and killing the dark-surface/bright-lane contrast; (3) possible **vertex-color
suppression** — `standard_wgsl.inc:699-704` sets vertexTint=white for non-prelit static meshes,
discarding baked lane/AO color if the surface mat isn't `mPreLit`. The emissive fix is shared
with A2; the **scene-lighting port is A4-specific and RISKY** (regresses character/crowd/venue
look; the DC3 light-gather can hang under WASM via `ObjDirItr<RndLight>` — must use
`env->LightsApprox()` only, no venue walk). Gate behind an env canary (e.g. `RB3_SCENE_LIGHTS`)
and A/B the whole scene. The widened-probe vertex-color dump decides whether the shader/prelit
change is needed at all.

---

## 2026-06-02 — implementation attempt + 4-agent verification (REVERTED)
The emissive fix was applied and adversarially reviewed (gem/highway/HUD/regression Opus panel):
- **Mechanism VALIDATED** by runtime probe: gems carry `prism_mat.mat` mult=1.0
  map=`prism_gem_emissive.tex`; `surface.mat` ×0.4; `gem_smasher_glow` ×0.9. The map-presence
  **guard is essential** (many HUD/rails/plate mats have mult=1.0 but a null map → would
  self-illuminate). Regression reviewer **PASSED** (no leak to menu/venue/characters).
- **But standalone NET-NEGATIVE → reverted.** Gem lift marginal (already glossy), no bloom
  halo, and a **regression**: prelit sustain **note trails white-clip/blow out**
  (finalColor = baseColor + baseColor·mult·emissive ≈ 2× on prelit surfaces). HUD/highway glow
  not delivered.
- **KEY: glow is GATED ON SCENE-LIGHTING.** The 0.45 gray ambient flood washes out emissive
  contrast — emissive can't pop until the scene is dark. **So A2/A3/A4 glow is a COUPLED,
  scene-lighting-LED unit:** do A4 scene-lighting FIRST, THEN re-add emissive (validated) + a
  hue-preserving blowout CLAMP on prelit surfaces + get bloom firing on gem cores (halo).
  Verify FRAME-LOCKED (add an emissive on/off toggle + capture the SAME frame both ways — the
  songMs captures drift 44-58%, so pixel A/B was unreliable). See memory
  `project-a234-emissive-glow-shared-rootcause`.

## Recommended implementation order (REVISED 2026-06-02 post-verification)
**Glow is now scene-lighting-led & coupled.** Order: (1) A1 particle renderer [independent,
safe, very visible]; (2) A4 scene-lighting [dark scene — big win AND unblocks glow]; (3) glow
stack on top of lighting = re-add validated emissive + blowout clamp + bloom-halo; (4) A3 HUD
feed/showing. Original order below kept for reference.

### Original order
1. **Probe-confirm** the emissive hypothesis (mandatory, above) — widen the existing
   `GEM_VTX` dump to gem/smasher/highway/glow/meter mesh names.
2. **Shared emissive fix** in `Rnd_Wgpu_RB3.cpp` (DrawMesh + MakeMaterialBindGroup) → A2 +
   A2/A4 glow + likely A3 glow at once. A/B gems, highway, HUD, and a character/venue frame.
3. **A3 feed/showing checks** (probe `StreakMeter::SetMultiplier` + group showing) → fix the
   smaller of {render glow, re-show label, multiplier feed} that the probe identifies.
4. **A4 scene lighting** (port `RndEnviron`/`LightsApprox` into `WriteSceneUniforms`, env-gated)
   — only if step 2 doesn't restore enough lit-lane contrast; whole-scene regression sweep.
5. Deferred: `track_gems.env` environ-cube reflection (A2/A4 polish), the 5-star scoreboard
   indicator + top-center HUD camera-frame (A3, entangled with out-of-scope positioning).

See memory `project-a234-emissive-glow-shared-rootcause` and the A1 doc
`N8_HIT_FLAME_FX_PLAN.md` (⚠️ correction header) for the BandRnd-vs-WgpuRnd architecture.
