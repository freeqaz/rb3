# WHITE-fix — Stage B.S1 STATUS (Wave 9: FORCE-REPRODUCE + DISCRIMINATE, Opus)

Checkpoint id `B-S1`. Lane B. Engine pin `a320f9d` (working tree, `FxSendNative.cpp` audio
edit by a concurrent agent left untouched). Build
`native/build-agent-WHITE-fix/rb3-native` (clang Debug). `RB3_PP_LUMA_CEILING` and the
refuted hands experiments **UNSET in every arm** (A7). Chroma-preserve at its shipped
**default-ON** in every arm. Driver: `white_discriminate.py` (+ `analyze_disc.py`), reusing
`wash-measure.capture_pinned` / `wash_score` / `tonal_band_sat` / `wash_probe_run`.

## Headline verdict: the engaged-venue WHITE is **SCENE-SIDE**

The near-white is **baked into the scene render** (`standard_wgsl.inc` fragment output)
*before* the Stage-2 composite ever samples it. The Wave-8 chroma-preserve composite fix
mitigates it (restores color where the scene kept any chroma) but **cannot** recover a region
the scene render already clamped to zero-chroma white. Per WAVE9_REVIEW A5 the fix therefore
lives in Lane A's `Rnd_Wgpu_RB3.cpp` + the DC3-shared scene shader — **STAGED**, not landed
(see `staged-patch-scene-side.md`). This confirms A4's scene-side prior with measurement.

---

## 1. Reproducers (exact recipe)

Both boot with `RB3_HTTP=1 RB3_FIXED_CLOCK=1 RB3_WASH_PROBE=1`, nav to gameplay on
`song_downs=4` (hard), disable the director + pin a wide/crowd venue shot, race to
**songMs 21000 ± 2000**, screenshot. (`white_discriminate.py --songms 21000 --tol 2000`.)

### (a) DETERMINISTIC reproducer — the venue flood [primary discriminator vehicle]
`RB3_VENUE_LIGHT_OFF=1` (chroma-preserve stays default-ON). Forces the world.cam venue into
the flat-default flood (`1.0 white dir + 0.45 ambient`, `Rnd_Wgpu_RB3.cpp:1607`) → a
deterministically hot scene. **N=5: 2/5 strict WHITE, 5/5 over-exposed** (mean_luma
0.50–0.65, hi_frac 19–30%). This is the exact same UNORM-clamp mechanism as the natural
residual, made deterministic (A4 sanctions the forced-hot probe). Frame: the `small_club`
cork-board venue + band + props blown white (`disc1_venue_light_off_01.png`).

### (b) NATURAL engaged reproducer — the genuine disclosed residual [shipping default]
**No flags** (shipping default: chroma-preserve ON). Boot #3 of the `default` arm read
**WHITE mean_luma 0.755, hi_frac 51.5%, env=chars.env, engaged 40/0** — a real engaged-venue
over-exposure on the default build (`disc1_default_03.png`: the left half of the frame is a
flat zero-chroma white region, the note highway unaffected). Rate here **1/5** (matches the
disclosed ~1/6 stochastic rate; the other 4 boots are dark NEARBLACK/NEUTRAL — song 4's
engaged venue is dark, so the WHITE is director-shot-dependent, not ms-pinned). This is the
condition A4 asked to force; the flood in (a) is the deterministic stand-in.

Note: song_downs=4's venue is dark, so the natural WHITE is **venue/shot-selective** (which is
*why* it is stochastic). A `forced_hot` arm that only cranked the point/dir exposure stayed
dark (dim env lights × 3 is still dim); lifting the **ambient floor** (`eng_hot`, §2) is what
forces the engaged path bright deterministically and reproduces the engaged WHITE (1/4). The
flood (a) removes the venue dependence entirely.

---

## 2. Discriminator: PP_OFF (scene render, no composite) vs default (composite)

`RB3_PP_OFF=1` renders the scene **direct** to the framebuffer with **no intermediate and no
composite grade** (`Rnd_Wgpu_RB3.cpp:1834`). Both the PP-ON intermediate (`RB3PostProc.cpp:155`)
and the PP-OFF framebuffer are **RGBA8Unorm** headless — hot (>1.0) scene values clamp
identically at either write. So PP_OFF isolates "is the white already in the scene render?"

### Continuous metrics (flood reproducer, N=5 each)

| arm | mean_luma (max) | hi_frac (max) | WHITE | low_sat | mid_sat | high_sat |
|---|---|---|---|---|---|---|
| `venue_light_off` (PP **ON**, composite) | 0.565 (0.649) | 24.4 (30.4) | 2/5 | 0.109 | **0.282** | 0.235 |
| `venue_light_off_ppoff` (PP **OFF**, raw scene) | 0.537 (0.732) | 16.8 (**38.5**) | 2/5 | 0.064 | **0.183** | 0.176 |

**Verdict = SCENE-SIDE**, two independent ways:
1. **The raw scene (PP_OFF) is at least as blown as the composite output** — it hits WHITE
   2/5 *by itself* and its single-boot peak over-exposure (hi_frac **38.5%**, mean 0.732)
   **exceeds** anything the composite produces (peak 30.4%, 0.649). Removing the composite does
   not remove the white; it makes it *worse*. The composite is not the gain — it caps the top.
2. **Per-tonal-band saturation goes the wrong way for a composite-gain story.** The composite
   *raises* mid-band saturation (0.183 → **0.282**, +54%): chroma-preserve is *restoring*
   color, not desaturating. On the WHITE-class captures only the gap is starker — raw scene
   **mid_sat 0.045** vs composite **mid_sat 0.283** (6×). The desaturated-white is the raw
   scene; the composite de-washes it as far as the surviving chroma allows.

### Why the residual survives chroma-preserve (the natural WHITE)
The natural `default` WHITE region (`disc1_default_03`) reads **mid_sat 0.045 even WITH the
composite** — because that region is **fully clamped to (1,1,1) in the scene render**, so the
ungraded `sceneColor` chroma-preserve reconstructs from is itself chroma≈0
(`rb3_postproc.wgsl.inc:234`, `cpColor = sceneColor·(gradedLuma/inLuma)`). Nothing to restore
→ the residual WHITE the Wave-8 A.S3 disclosed (luma 0.709 < ppKnee 0.82; the ceiling never
fires). Visual: `montage_discriminator.png`.

### Honest handling of the natural-arm confound
`analyze_disc.py`'s automated rule flags the *natural* arm "COMPOSITE-GAIN" — this is a
**director-RNG sampling artifact, not a real result**: the natural WHITE was one stochastic
hot-shot boot in the `default` arm; the 5 `default_ppoff` boots all happened to pin *dark*
shots (mean 0.08–0.17, 0/5 WHITE), so the arms are **unpaired on the shot**. It is the same
confound the WASH-fix stages flagged repeatedly. The **valid, paired discriminator is the
flood** (both arms deterministically hot) → SCENE-SIDE, corroborated by the natural WHITE
image (a scene-baked zero-chroma region the composite can't touch) and by source.

### Engaged-path confirmation (forced-hot engaged, `disc2`, NOT the flood)
To address the "engaged venue" framing directly (the flood is a broken-env stand-in), a
**forced-hot ENGAGED** arm (`RB3_VENUE_AMBIENT_FLOOR=0.55 RB3_VENUE_AMBIENT_CLAMP=1.0
RB3_VENUE_{POINT,DIR}_EXPOSURE=2.5`, real env, `engaged=40/0` every boot, chroma-preserve
default-ON) reproduces WHITE on the **genuine engaged path**:

| arm | mean_luma mean (max) | hi_frac (max) | WHITE | mid_sat (WHITE-only) |
|---|---|---|---|---|
| `eng_hot` (PP **ON**) | 0.555 (0.711) | (49.3) | 1/4 | 0.256 |
| `eng_hot_ppoff` (PP **OFF**, raw) | 0.558 (0.734) | (47.1) | 1/4 | 0.083 |

Same signature as the flood: the raw engaged scene (PP_OFF) whites **independently** (peak
0.734 / hi 47.1 ≈ PP_ON's 0.711 / hi 49.3; mean_luma **identical** 0.558 vs 0.555 — the
composite is exposure-neutral on average) and its WHITE is **more desaturated** (mid_sat 0.083
vs the composite's 0.256) — the composite restores color, the scene bakes the white. Also note the
engaged WHITE keeps more chroma than the flood (mid_sat 0.256 vs 0.045) because the venue's
real lights are colored — so on the engaged path the luma-preserving fix (§4) has real chroma
to save. Frame `disc2_eng_hot_04.png`.

---

## 3. Mechanism (source-derived — why it is scene-side and un-composite-fixable)

`standard_wgsl.inc` `fs_main` bakes the white before the composite:
1. `softClipLighting(ambient + Σ diffuse)` — **per-channel** Reinhard, knee 1.0 / ceil 1.05
   (`:609`).
2. `baseColor.rgb * litTerm + specular` (`:850`) — pale/white surface × hot lighting.
3. `compressHighlights(finalColor)` — **per-channel** tanh → 1.0, knee 0.9 (`:621`, `:878`).
4. `linearToSrgb` (`:886`) — pushes mid-highs up (0.85→0.94).

Steps 1+3 are **per-channel**: each channel independently approaches 1.0, so a bright
*colored* pixel loses chroma → white. On the hottest venue moments all three channels clamp
near 1.0 and the region is flat white with **no chroma left for any downstream stage**. The
RGBA8Unorm intermediate then hard-clamps it. A float intermediate would **not** help — the
scene is already tonemapped to [0,1] by step 3, so there is no HDR headroom to recover; and no
grade acting on a chroma≈0 pixel can re-introduce color. This is the wall `RB3_PP_LUMA_CEILING`
(Wave-7) and chroma-preserve (Wave-8) both hit.

---

## 4. Fix design (SCENE-SIDE → STAGED for Lane A)

**Root fix (recommended, `staged-patch-scene-side.md`):** a **luminance-preserving** highlight
compression on the venue path (`compressHighlightsLuma` — compress the luminance, scale RGB
uniformly, so hue/sat survive the rolloff), gated by a new `SceneUniforms.venueHighlightLumaMode`
(a repurposed pad, mirrors `pointFalloffMode`; size stays 656) that RB3's world.cam venue upload
sets under a **default-OFF** `RB3_VENUE_WHITE_GUARD`. A hot venue moment then stays
**bright-but-colored** (retail/GX look) instead of clipping white. Three files, all Lane-A /
DC3-shared: `Rnd_Wgpu_RB3.cpp` + `standard_wgsl.inc` + `UniformStructs.h`. DC3 leaves the gate 0
→ per-channel branch → **behaviorally byte-identical** (but the shared-file diff is non-empty —
the coordinator must accept behavioral-zero-blast or re-home; flagged in the patch).

**Rejected:** (i) any composite-side / float-intermediate fix (my fence) — the scene tonemaps &
clamps pre-composite, nothing to recover (REFUTED above). (ii) blunt venue exposure cut in
`Rnd_Wgpu_RB3.cpp` — dims the whole venue to fix a localized blowout, regresses the A2/A3/A4
glow tuning.

**FENCE respected:** I did **not** edit `Rnd_Wgpu_RB3.cpp` (Lane A's single-writer TU) nor any
shared shader; my only engine reads were analysis. The fix is delivered as a staged patch file.

## 5. Gates / limits
- flag-OFF byte-identical is a *design property* of the staged patch (gate field 0 →
  per-channel), to be verified at land (drawlog 792).
- The clean deterministic discriminator is the **flood**; the natural engaged WHITE is
  reproduced (1/5) and imaged but is stochastic (shot-selective) — a fully-paired natural A/B
  needs the specific hot shot pinned (Wave-10 fail-red will use the flood + a re-captured
  natural hot boot).
- Artifacts: `white_discriminate.py`, `analyze_disc.py`, `measure/disc1.json`,
  `measure/disc2.json`, `montage_discriminator.png`, raws under `/tmp/whitefix-caps/`.

---

# WHITE-fix — Stage B.S2 STATUS (Wave 9: FINALIZE staged patch, Opus) — NO-CODE

Checkpoint id `B-S2`. Lane B. Verdict from S1 (**SCENE-SIDE**) upheld; staged patch
**finalized** for Wave-10 land by Lane A. **Honest no-code outcome** — nothing applied this
wave (A5: `Rnd_Wgpu_RB3.cpp` is Lane A's single-writer TU; the shader/uniform edits are
DC3-shared and coordinator-sequenced). My composite-side fence has nothing to change.

## What S2 did
1. **Re-verified every patch anchor** against the CURRENT engine working tree (HEAD `30d4f00`,
   advanced from S1's `a320f9d` by a concurrent FxSendNative audio edit). All 8 hunk anchors
   still resolve exactly — the patch did not rot. Table in `staged-patch-scene-side.md` §
   "S2 FINALIZATION".
2. **Verified the field-offset alignment** (the one thing that could silently corrupt the
   uniform buffer): C++ `_padPL[0]` ↔ WGSL `_padPL2`; the patch renames both to
   `venueHighlightLumaMode` at the same offset, `_padPL[1]`/`_padPL3` stay pad. Size 656
   unchanged, `static_assert` unaffected. Pure pad-rename, no layout risk.
3. **Verified the shader math**: `compressHighlightsLuma` runs on linear `finalColor`
   (pre-sRGB at :886); Rec.709 luma weights correct for linear RGB; below-knee identity;
   gate threshold `> 0.5` robust vs the 0.0f/1.0f upload writes. `softClipLighting` confirmed
   per-channel (mechanism doc accurate).
4. **Isolated the single coordinator decision**: the fix is intrinsically in DC3-shared
   `standard_wgsl.inc` + `UniformStructs.h` (no RB3-only fragment path; per-pixel tonemap
   can't move to CPU). Mitigation is behavioral zero-blast (gate defaults 0 → per-channel
   `else` for DC3). Coordinator accepts behavioral-zero-blast (recommended, matches how the
   sibling `pointFalloffMode` venue gate already ships shared) or re-homes (not recommended).

## Gates
- No behavior changed this wave → flag-OFF byte-identical / drawlog 792 / lineup / DC3
  zero-blast are all trivially satisfied (no code touched). They become Wave-10's land gates,
  restated in the patch's "Wave-10 land order".
- Fence honored: no edits to `Rnd_Wgpu_RB3.cpp` or any shared shader; engine reads
  analysis-only.

## Deliverable
`staged-patch-scene-side.md` (finalized, 3-file hunks + verified anchors + Wave-10 land order
+ coordinator decision). Ready for Lane A to land in Wave 10.

---

# WHITE-fix — Stage STEP-0 STATUS (Wave 10: PRE-LAND staged engine patch, Opus) — LANDED

Checkpoint id `STEP0`. Serialized STEP 0 (WAVE10_KICKOFF COORDINATOR ACCEPTANCE A1). Engine
pin base `10a9ca6`; concurrent `FxSendNative.cpp` audio edit left untouched (never staged).
Build `milo-native-engine/build-agent-STEP0` + `rb3/native/build-agent-STEP0` (clang Debug,
Ninja). This stage is **LAND + inertness proof only** — the WHITE behavioral gates are Lane B's.

## Outcome: staged scene-side patch APPLIED to the engine, default-OFF, inert. Engine commit `2998e78`.

Applied `execution/WHITE-fix/staged-patch-scene-side.md` verbatim (all 8 anchors re-verified
against the current tree — they had NOT rotted from the S2 verification at `30d4f00`):

1. **`src/gfx/UniformStructs.h`** — repurposed `_padPL[0]` -> `float venueHighlightLumaMode`
   (+ `_padPL1` trailing pad). Pure pad rename; `static_assert(sizeof(SceneUniforms)==656)`
   unchanged.
2. **`src/gfx/standard_wgsl.inc`** — mirrored the field (`_padPL2` -> `venueHighlightLumaMode`);
   added `fn compressHighlightsLuma` (compress luminance, scale RGB uniformly so hue/sat survive);
   gated the `fs_main` `compressHighlights(finalColor)` call on `scene.venueHighlightLumaMode > 0.5`.
   The **optional `softClipLighting` luma variant was intentionally NOT landed** (staged patch land
   order: land `compressHighlights` first, re-measure; that decision is Lane B's).
3. **`src/platform/Rnd_Wgpu_RB3.cpp`** — added `sVenueWhiteGuard()` accessor (default-OFF,
   presence-truthy opt-in `RB3_VENUE_WHITE_GUARD`) next to `sVenuePointFalloffGx()`; set
   `s.venueHighlightLumaMode = sVenueWhiteGuard() ? 1.0f : 0.0f;` in the ENGAGED venue branch
   right after the `s.pointFalloffMode` write (`WriteSceneUniforms`).

Flag registered in `NativeCompatFlags.classification.json` (append-only under
`flock /tmp/milo-engine-classjson.lock`, class `workaround`, owner `render/lighting`,
default off, read presence). **No `gen.inc` regen** (coordinator-only).

## Inertness gates (all GREEN)

| Gate | Result |
|---|---|
| flag-OFF byte-identical — `drawlog-golden.py --fixed-clock --canonical-order` | **792 PASS** (306 known-residual eye-jitter divergences within bound, non-blocking) |
| WGSL validity — `rb3-tests WgslValidation.AllRB3ShadersCompile` | **PASS** — `standard_wgsl.inc OK` via real Dawn/Tint (RTX 3090); `HarnessCatchesBadShader` fail-red green |
| DC3 zero-blast + size — `milo-engine-tests` (ctest -j1, DC3_RUNTIME_ROOT=dc3-decomp) | **200/200 pass, 0 fail** (2 by-design skips); compiles `UniformStructs.h` -> `static_assert 656` held on the DC3 build |
| build — `rb3-native` + `rb3-tests` (clang) | clean link |

DC3 zero-blast mechanism (WAVE10_REVIEW A5, now confirmed behaviorally): both `SceneUniforms`
constructions value-initialize; nothing writes the repurposed pad on the DC3 path; WGSL gate
`> 0.5` selects the unchanged per-channel `else` branch -> DC3 rendered scene byte-identical.
The file diff on the two shared files (`UniformStructs.h`, `standard_wgsl.inc`) is non-empty but
behaviorally inert — the accepted behavioral-zero-blast (pointFalloffMode precedent).

## Single-writer handoff

After this commit (`2998e78`), per WAVE10_REVIEW A1, `Rnd_Wgpu_RB3.cpp`'s ONLY writer for the
rest of Wave 10 is **Lane A (W2.8e)**. Lane B is **engine-read-only** (reproducers, WHITE
behavioral gates, flip recommendation). I did **not** run the WHITE behavioral reproducers/deltas
(`RB3_VENUE_LIGHT_OFF` flood, eng_hot, hi_frac/mid_sat paired deltas) — those are Lane B's per the
kickoff. Coordinator must NOT bump the pin or flip the default here (default-OFF stays until Lane B
earns the flip).

Checkpoint: `/tmp/wave10-checkpoints/STEP0.json`.

---

# WHITE-fix — Stage B.S1 STATUS (Wave 10: BEHAVIORAL GATES, Opus) — PRE-REGISTRATION

Checkpoint id `B-S1`. Lane B (**engine-read-only** — STEP-0 already landed the patch,
engine `2998e78`; `Rnd_Wgpu_RB3.cpp`'s sole writer for the rest of Wave 10 is Lane A).
Build `native/build-agent-WHITE-fix-BS1/rb3-native` (clang Debug, against engine working
tree HEAD `2998e78`; concurrent `FxSendNative.cpp` audio edit untouched). All refuted-experiment
flags (`RB3_PP_LUMA_CEILING`, `RB3_HANDS_POSEAWARE`, `RB3_HANDS_PERFRAME_CONJ`,
`RB3_APPENDAGE_REST_ROT`) **UNSET in every arm**; chroma-preserve at its shipped **default-ON**
in every arm. Arms differ ONLY by `RB3_VENUE_WHITE_GUARD` (unset = OFF vs `=1` = ON).

## Mechanism correction adopted BEFORE measuring (A7 gate-validity, file:line evidence)

The guard field is written at **exactly one site** —
`Rnd_Wgpu_RB3.cpp:1474` `s.venueHighlightLumaMode = sVenueWhiteGuard() ? 1.0f : 0.0f;` —
which sits **inside the ENGAGED venue branch** (`:1466`
`if (sVenueLightEnabled() && camNm=="world.cam" && venv && venv->mAmbientFogOwner)`).
`SceneUniforms s{}` value-initializes the field to 0 (`:1334`); there is **no other write**
(`grep venueHighlightLumaMode Rnd_Wgpu_RB3.cpp` → the comment + `:1474` only). Therefore:

- The **flood** reproducer (`RB3_VENUE_LIGHT_OFF=1`) forces `sVenueLightEnabled()=false` → the
  flat-default **else** branch (`:1611`) → the field stays 0 → **the guard is provably INERT on
  the flood**. The flood **cannot** show a flag-ON improvement (flag-ON == flag-OFF there); the
  kickoff's "paired deltas on the flood" is mechanically impossible as an *improvement* gate.
- The **eng_hot** arm keeps `sVenueLightEnabled()` default-true with a real env → engaged branch
  taken (`final_engaged=1` every boot, Wave-9 §2) → the guard **fires**. It is the **only** valid
  improvement vehicle. Its WHITE keeps `mid_sat 0.256` (real chroma to save), the case the fix is
  designed for.

Consequently the flood is repurposed here as the **negative-control + fail-red** (a change that
improved the flood would be a *scope-leak defect*), and **eng_hot carries the PRIMARY paired
deltas**. This is a mechanism-grounded correction of the kickoff wording, recorded before any
capture so the gate can still fail.

## PRE-REGISTERED numeric thresholds (written before measuring; N≥6 per arm)

Metrics: `wash_score.py` → `hi_frac` (% pixels luma>0.90), `mean_luma`, `wash_class`;
`tonal_band_sat.py` → `mid_sat` (per-image), arm-mean over N≥6 boots. songMs pinned 21000±2000,
`song_downs=4`, `RB3_WASH_PROBE=1`, `RB3_FIXED_CLOCK=1`. Because per-boot shots are director-RNG
(not pairable per-boot), gates compare **arm distributions** (mean + one-sided Mann-Whitney color).

### G1 — PRIMARY (eng_hot engaged arm; the fix's real effect). BOTH must hold:
- **G1a over-exposure DOWN:** `mean(hi_frac | ON) ≤ mean(hi_frac | OFF) − 3.0` pp **and** directional `ON<OFF`.
- **G1b chroma UP:** `mean(mid_sat | ON) ≥ mean(mid_sat | OFF) + 0.02`.
- Secondary color (not a hard gate): one-sided Mann-Whitney on `hi_frac` (ON<OFF) p<0.10;
  strict-WHITE count ON ≤ OFF.

### G2 — BRIGHT MOMENTS PRESERVED (must be able to fail):
- **G2a venue not over-dimmed (eng_hot):** `mean(mean_luma | ON) ≥ mean(mean_luma | OFF) − 0.08`
  (the fix caps zero-chroma whites; it must NOT crush overall venue brightness → colored-bright).
  FAILS if the venue craters dark.
- **G2b intended-bright non-venue elements untouched (game.cam highway / HUD):** capture a normal
  gameplay highway frame (game.cam, guard field provably 0 off world.cam) flag-ON vs OFF; the
  full-frame `|Δmean_luma| < 0.01` and `|Δhi_frac| < 1.0` (expected ~identical). FAILS if the
  scoped fix leaks onto the highway/HUD bright FX. (If an SP-overlay/strobe frame is reliably
  reachable headless it is added as G2c; otherwise documented as not-headless-reachable and G2b
  stands as the falsifiable preservation gate.)

### G3 — FAIL-RED (the WHITE we're fixing must exist with the flag OFF):
- **G3 flood OFF reproduces over-exposure:** `RB3_VENUE_LIGHT_OFF=1` flag-OFF → `≥1/6` strict-WHITE
  **or** `mean(hi_frac|OFF) ≥ 15`. Proves the reproducer still produces the target defect.

### G4 — SCOPE / NEGATIVE CONTROL (flood, where the guard is inert):
- **G4 flood flag-invariance:** `|mean(hi_frac|ON) − mean(hi_frac|OFF)| < 3.0` **and**
  `|mean(mid_sat|ON) − mean(mid_sat|OFF)| < 0.02`. Proves the fix is venue-engaged-scoped and does
  NOT touch the non-engaged path (a flood *improvement* would be a scope-leak defect, FAIL).

### G5 — REGRESSION:
- **G5a** `drawlog-golden.py --fixed-clock --canonical-order` = **792** flag-OFF (re-confirm on this build).
- **G5b** `lineup-gate.py` PASS (default build).
- **G5c** 2-venue flag-ON vs OFF spot captures render sane (no new artifacts; engaged venue stays
  bright-but-colored, second venue unaffected/consistent).

### FLIP RECOMMENDATION RULE (pre-registered):
Recommend **flip default-ON** iff **G1(a+b) PASS AND G2(a+b) PASS AND G3 PASS AND G4 PASS AND
G5(a,b,c) PASS**. Any PRIMARY (G1) or preservation (G2) failure → **hold**, documented honestly.
"WHITE class 0/N" is secondary color only (Fisher-trap, A7).
