# task-venue-blowout — impl (wave 4)

**Status: DONE (engine fix committed, verified) — with an important re-diagnosis.**

The shipped fix is a GX-faithful soft-clip of the unbounded lighting sum in the
shared standard shader. It is correct, low-risk, and verified to tame genuine
lighting over-drive on hot venue moments with NO dimming of normal scenes. BUT
my investigation also proved that the *dominant* "venue wash" in the smoke recipe
(`/tmp/rp-smoke/06_game_screen.png` etc.) is **NOT a lighting blowout** — it is a
**load-transient clear-color background** (venue scene not yet drawn). The
wave-3 `verify-venue-wash.md` root-cause attribution was partly wrong; read the
"Re-diagnosis" section.

---

## Branches / commits

- **Engine** worktree `wt-task-venue-blowout`
  (`/home/free/code/milohax/milo-native-engine-worktrees/task-venue-blowout`),
  based on the pinned `469c550`.
  - commit **`36c8309`** — `gfx(standard): GX-faithful soft-clip of the lighting
    sum (venue blowout)`. Touches ONLY `src/gfx/standard_wgsl.inc`.
- **rb3**: NO source change. Engine-only fix. After landing the engine commit,
  bump `MILO_ENGINE_PIN` in `native/CMakeLists.txt` to the landed engine SHA.
- Wii build unaffected (no `src/` touched). `wiiByteIdentical` = N/A (no shared
  Wii source edited).

## Root cause (the lighting part — real, fixed)

`milo-native-engine/src/gfx/standard_wgsl.inc` `fs_main`, non-prelit/unlit branch:

```wgsl
finalColor = baseColor.rgb * (ambientLight + totalLighting.diffuse * shadowFactor)
           + totalLighting.specular * shadowFactor;
```

The `(ambient + Σ diffuse)` lighting sum is **unbounded**. On Wii GX the rasterised
channel color is clamped to [0,1] **before** the TEV texture multiply, so lighting
can only *tint* a surface — it can never push output past the texture's own
brightness. Our native sum reaches >1 (4+4 point/dir lights at close range, or the
non-venue white-flood fallback `else` branch at ambient 0.45 + dir 1.0 = 1.45×).
The downstream `compressHighlights` shoulder (knee 0.9) then maps a pale texture ×
that sum to ~1.0 → a textureless over-bright wash on hot-lit walls.

## Fix

Restore the GX ceiling as a **soft Reinhard rolloff** of the lighting sum, applied
before it modulates the texture:

```wgsl
fn softClipLighting(lightSum) {
    over = max(lightSum - kLightSumKnee, 0);          // knee = 1.0
    rolled = knee + span * (over / (over + span));    // span = ceiling - knee, ceiling = 1.05
    return select(lightSum, rolled, lightSum > knee); // IDENTITY below the knee
}
...
let litTerm = softClipLighting(ambientLight + totalLighting.diffuse * shadowFactor);
finalColor = baseColor.rgb * litTerm + totalLighting.specular * shadowFactor;
```

Why this shape (chosen over a hard `min(.,1.0)`):
- **IDENTITY for the entire GX-legal range** (sum ≤ 1.0), so the whole diffuse N·L
  falloff (up to N·L ≈ 0.55 under the 0.45-ambient fallback) is untouched → **no
  dimming** of any correctly-lit surface.
- `over/(over+span)` has slope 1 at the knee (**C1-continuous → no banding**) and
  asymptotes to the ceiling, so the runaway multi-light/flood pile-up is pulled
  back toward 1.05 instead of soft-clipping to white.
- **Ceiling 1.05** (not 1.1/1.25) was chosen empirically: it reliably keeps a pale
  club wall (base ≤ 0.92) under the 250 clip while still being a soft rolloff (the
  numerical model + the `pf700` capture both confirm). 1.25/1.1 left near-white
  walls borderline.
- Specular is left additive (unclamped) — it's a small glint term and
  `compressHighlights` handles its tail; clamping it would dull legitimate glints.

Constants `kLightSumKnee = 1.0`, `kLightSumCeiling = 1.05`. High blast radius
(every lit material) — that's why the design is identity-below-1.0.

## Re-diagnosis (the part the verify doc got wrong)

I instrumented the shader (temporary debug binaries, now reverted) to tag every
pixel by its compose path: prelit=RED, unlit=BLUE, lit-overdrive(sum>1)=GREEN,
lit-normal=YELLOW, untagged=clear-color background.

**The dominant smoke-recipe "wash" frames (`06_game_screen`, early `07_playing`)
are ~80% UNTAGGED clear-color background.** The worst frame (`dbg3/run5/06`,
near-white 80.1%) has lit-overdrive GREEN = 0.2%, prelit RED = 3.5%, unlit = 1.2%.
There is essentially **no geometry there at all** — the venue scene has not finished
drawing in that first game frame, and the framebuffer clear color (`mClearColor`,
`WgpuRnd::Clear`, written directly with no shader sRGB encode) is near-white, so it
reads as a white/pink wash. This is a **scene-load / clear-color** issue, not a
lighting blowout, and the shader clamp correctly does nothing to it.

The genuine lit-path over-drive my fix tames is real but localized: the now-bar &
lit lanes (gameplay), and the walls/smoke near the stage lights in *populated*
venue frames (e.g. the CORK club crowd room). Those ARE > 1.0 lit and DO blow out
without the clamp.

### What this means for the campaign
- Shipping the soft-clip is still right: it's GX-faithful, defensive against hotter
  venues than small_club, and it visibly improves the populated-club moment.
- The smoke-frame white "venue wash" the orchestrator flagged is a SEPARATE bug:
  **the first ~1-2 game frames render before the venue is drawn, over a near-white
  clear color.** Follow-up: either (a) clear to black during venue load, or (b)
  defer the first game-screen present until the venue scene's first draw completes.
  Out of scope for a shader change; filed below.

## Verification (strict, controlled)

Two binaries built from the SAME source except the shader — `rb3-native-BASELINE`
(no fix) and `rb3-native-FIXED` (md5 `fe96827e`, == the committed-state rebuild).
The wash is camera-shot-random, so batch-vs-batch is invalid (an early mistake I
caught); all conclusions below use **`_framepin_capture.py`** which pins a FIXED
frame number on a target screen → deterministic, frame-locked content across both
binaries.

| pin-frame / scene | BASELINE | FIXED | verdict |
|---|---|---|---|
| `game` pf700 (CORK crowd club, HOT walls) | clipW 4.0%, clipAny 8.8%, lum .388 | clipW **0.3%**, clipAny 0.3%, lum .268 | **blowout tamed, detail preserved** (visual: walls/smoke readable) |
| `game` pf720 (dark-red venue) | lum .157, clipW 1.0% | lum .156, clipW 0.9% | **no-op** (sum ≤ 1.0) |
| `game` pf900 (dark venue) | lum .156, clipW 0.9% | lum .156, clipW 0.9% | **no-op** (byte-stable; meanAbsDiff 1.8) |
| `game` pf900 highway/now-bar region | lum .136 | lum .136 | **no regression** (maxDiff 71 = now-bar rolloff only) |
| `main_hub` pf120 | lum .414, clipW 1.5% | lum .414, clipW 1.5% | **no change** (B-vs-F diff 9.8 < self-noise 13.0) |
| `song_select` pf120 | lum .295, clipW 0.4% | lum .293, clipW 0.4% | **no change** (B-vs-F diff 8.7 < self-noise 18.4) |

Self-noise control (BASELINE vs BASELINE, same pin-frame) = meanAbsDiff 13–18 on
menus (pure animation-phase jitter); the BASELINE-vs-FIXED menu diff is SMALLER
than that floor → the fix introduces no systematic menu change, i.e. **no dimming**.

Sweep coverage: gameplay (highway), venue (hot + dark), menu hub, song-select.
Score screen not separately captured (UI overlay = prelit, bypasses the lit path
entirely per shader line `prelit || unlit ⇒ register color only`, so unaffected by
construction; same path as song-select which is verified unchanged).

### Evidence paths (all under `/tmp/rp4-venue-blowout/`)
- Hot-venue A/B (the win): `fpAB/BASELINE_pf700.png` vs `fpAB/FIXED_pf700.png`
- No-op dark venue: `fpAB/{BASELINE,FIXED}_pf900.png`, `..._pf720.png`
- Menu no-change: `menuAB/{BASELINE,FIXED}_{main_hub,song_select}.png` + self-noise
  `menuAB/BASELINE2_*.png`
- Settled venue detail (fixed): `hwAB/FIXED_hw.png`
- Re-diagnosis path-tag captures: `dbg/`, `dbg2/`, `dbg3/` (debug binaries
  `rb3-native-DBG{,2,3}`; shader debug reverted — production shader is clean)
- Binaries: `rb3-native-BASELINE`, `rb3-native-FIXED`
- Metric tools: `/tmp/rp3-venue-wash/measure.py` (venue-region clipW),
  `/tmp/rp4-venue-blowout/fullmeas.py` (full-frame)

`verified = true` for the lighting-over-drive fix (the thing the shader can fix).
`verified = false` would be dishonest for "kills the smoke-frame white wash" — that
wash is the clear-color load transient, which this fix is not designed to and does
not address (and proving why is a primary deliverable here).

## landingNotes

- **Cherry-pick** engine `36c8309` (branch `wt-task-venue-blowout`, base `469c550`)
  then bump `MILO_ENGINE_PIN` in rb3 `native/CMakeLists.txt`.
- **Engine file + EXACT regions touched** — `src/gfx/standard_wgsl.inc` ONLY:
  1. after `const kAlphaFringeFadeStart = 0.1;` (≈ line 22): add the
     `kLightSumKnee = 1.0` / `kLightSumCeiling = 1.05` constants + comment block.
  2. immediately before `fn compressHighlights(` (≈ line 590): add
     `fn softClipLighting(lightSum: vec3f) -> vec3f { … }`.
  3. in `fs_main`, the `} else {` lit-compose branch (≈ line 825–831): wrap the
     `(ambientLight + totalLighting.diffuse * shadowFactor)` term in
     `softClipLighting(...)` (replaces 1 line, adds the `let litTerm` line).
- **Sibling collision watch:** the menu-fog/fret-sphere siblings ALSO touch
  `standard_wgsl.inc` and `Rnd_Wgpu_RB3.cpp`. My change is confined to the three
  regions above (none overlap the lighting-sum/emissive `fog`/particle areas), so a
  sequential cherry-pick should apply cleanly; if a sibling adds shader constants in
  the same const block, the constants hunk may need a trivial context reroll.
- **DO NOT** fold in the main-engine-repo working-tree `Rnd_Wgpu_RB3.cpp` change —
  that is a DIFFERENT agent's uncommitted menu-fog/haze work (I left it untouched;
  my branch is based on clean `469c550` and does not contain it).
- No rb3 src, no Wii impact.

## Follow-ups (filed)

1. **First-game-frame white wash (the actual smoke-frame symptom).** Venue scene
   isn't drawn for the first ~1-2 game frames; the near-white `mClearColor` shows
   through. Fix candidates: clear to black during venue load, or defer the first
   game-screen present until the venue's first draw lands. Separate task; not a
   shader issue.
2. **Per-env venue exposure tuning** (pre-existing open item) folds in here — with
   the soft-clip, the CPU-side per-light clamps (1.5 dir / 1.8 point in
   `Rnd_Wgpu_RB3.cpp` `WriteSceneUniforms`) are now backstopped, so they could be
   revisited/loosened if a venue wants more punch.
3. The gameplay venue lighting currently always uses the **fallback** `else` branch
   (white 1.0 dir + 0.45 ambient) — the `world.cam` venue-light path
   (`RB3_VENUE_PROBE`) never fires during gameplay (camera is `game.cam`/closeup,
   not `world.cam`). Worth confirming whether the authored venue lights are meant to
   reach gameplay geometry; if so, that's why colors are flatter than retail.
