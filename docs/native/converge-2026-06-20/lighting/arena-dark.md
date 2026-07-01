# GAP 2 — arena_02 band underlit → dark silhouettes (ROOT-CAUSED)

**Arena-dark agent (Opus). Research only — used the PREBUILT `native/build-native/rb3-native`
(built Jun 21 00:41 against engine HEAD `5cbe855`, which == the pinned
`MILO_ENGINE_PIN`). No code/engine edits, no rebuild, nothing committed.**

## TL;DR

The arena_02 band is **dim/cool-tinted, NOT pure-black** — but it IS materially
underlit vs Wii ground-truth, and the cause is a **single concrete engine bug**:

> **The native point-light distance-attenuation model
> (`standard_wgsl.inc:523-524`, `falloff = saturate(1 - d/range)²`) is a HARD
> cutoff that reaches exactly 0 at `d == range`. The Wii GX model
> (`src/system/rndwii/Lit.cpp:38`, `GXInitLightAttn(.., k0=1, k1=1/mRange, k2=0)`
> = `1/(1 + d/mRange)`) is a gentle long-tail curve that is still 50% at
> `d == mRange` and ~33% at `d == 2·mRange`.**

arena_02's per-performer key spots (`*_silhouette.lit`, type=0 point, **range=55**,
colour white 1,1,1) sit **70–103 units** from the band roots. Under the GX model
each performer gets a **0.59–0.86** white-key diffuse sum; under the native model it
is **0.00–0.02** — i.e. the key light is silently extinguished and the band falls
back to only the 0.01 ambient + two dim directionals (a red `rim.lit` + a near-black
`char_bounce.lit`), which is exactly the dim cool look observed.

The fix is engine-side: **change the native point-light falloff to the GX
`1/(1+d/range)` curve** (or a smoothstep with a far-reaching tail), so range-55
spots still reach the band 70–100u away. This is a 2-line shader change → engine pin
bump. **Do NOT implement here.**

## 1. Repro + measurement (what was actually run)

- Venue override: `{meta_performer set_venue_override arena_02}` via the Group-C
  wrapper `/tmp/bch_override.py` (the harness itself does not set overrides;
  arena_02 requires it). arena_01 crashes — arena_02 used throughout.
- Shots are venue-specific + need `.shot`. Enumerated from
  `orig-assets/extracted/world/venue/arena/arena_02/gen/arena_02.milo_xbox`.
  Band-side / front shots that resolve: `coop_bs_d_c` (behind-drum), `coop_fs_b_c`
  / `coop_fs_b_ch` (front bass closeup/close-head), `coop_fs_all_n00`,
  `coop_bk_fs_all_n00/f00`. (`coop_b_closeup_head`, `coop_front_near` do NOT resolve.)
- Pin determinism: every captured frame `pinned N/N`, `drops_band=0` (this is a
  pure LIGHTING gap, not a dropped-geometry gap — confirms the audit).

### A/B: `RB3_VENUE_LIGHT_OFF=1` vs default, same pinned shots, same `--anchor-ms`

luma over the performer region (upper-centre, where a front-shot torso/head sits):

| shot | VENUE_LIGHT **ON** (default) | VENUE_LIGHT **OFF** |
|---|---|---|
| `coop_fs_b_ch` (front bass close-head) | perf_mean **51.9**, p95 207 | perf_mean **97.8**, p95 231 |
| `coop_fs_b_c`  (front bass close) | perf_mean **43.7**, p95 181 | perf_mean **87.3**, p95 205 |
| `coop_fs_all_n00` (front all) | perf_mean **56.2**, p95 217 | perf_mean **101.9**, p95 230 |
| whole-frame mean (3 behind shots) | **~50** | **~101** |

**The venue-light path is directly responsible for the darkness** — turning it OFF
roughly DOUBLES the band-region brightness. So the bug lives in the venue
`WriteSceneUniforms` light upload + the shader falloff, not in geometry/material/cull.

Frames saved under `lighting/frames/`:
- `arena02_front_bass_VENUE_ON.png` — bass player rendered **dim purple/violet**,
  fully formed (head/jacket/arms/hands all readable), crowd LED boards bright. NOT
  pure-black: p95 reaches ~207 on spotlit edges.
- `arena02_front_bass_VENUE_OFF.png` — same player bright/grey but **flat & washed**
  (one-white-directional flood); no mood, no colour, scene reads like a grey wire-frame.
- `arena02_behind_drum_VENUE_{ON,OFF}.png` — the over-the-shoulder behind framings
  the audit measured; here the performer is back-lit/small, which is why the audit's
  "near-black silhouette" reading came from these (the band is far dimmer from behind
  than the front spots intend).

### Honesty: dark-but-spotlit, not pure-black

RB3 arenas ARE intentionally dark with spotlit performers — and the front shots
prove the band is **spotlit-dim with form**, not a flat black cut-out. But it is
*under*-lit: the key spots that SHOULD pop the performer out of the dark are being
cut to zero (see §3), so the only illumination is ambient + a red rim + a dim-blue
directional. The result is a too-dark, too-cool/purple band instead of a
white-key-spotlit performer against a dark stage.

## 2. The lights arena_02 actually authors for the band (`RB3_VENUE_PROBE=1`)

The venue scopes ~25 RndEnvirons to mesh groups per world.cam frame. The one that
scopes the BAND CHARACTERS is **`char.env`** (8 lights, all showing):

```
char.env  ambRaw=(0,0,0) ambAdj=(0.01,0.01,0.01)   [ambient floored to 0.01 → near-black]
  rim.lit               type=1 dir   color=(1.00,0.00,0.00) range=800   → RED key directional
  rim_underneath.lit    type=1 dir   color=(0,0,0)                       → SKIPPED (black)
  vocals_silhouette.lit type=0 point color=(1,1,1)  range=55  pos=( 46.7,-532.1,313.5)
  bass_silhouette.lit   type=0 point color=(1,1,1)  range=55  pos=(-141.3,-508.2,311.1)
  drums_silhouette.lit  type=0 point color=(1,1,1)  range=55  pos=( -11.8,  -5.0,382.5)
  guitar_silhouette.lit type=0 point color=(1,1,1)  range=55  pos=(102.3,-559.1,286.1)
  keyboard_silhouette.lit type=0 point color=(0,0,0)                     → SKIPPED (black)
  char_bounce.lit       type=1 dir   color=(0.00,0.03,0.24) range=450    → DIM-BLUE directional
```

Selection in `WriteSceneUniforms` (`Rnd_Wgpu_RB3.cpp:1302-1336`) caps `dl<4` dirs +
`pl<4` points. char.env yields **2 dirs** (rim.lit red, char_bounce dim-blue) and
**4 points** (the 4 non-black silhouette spots) — exactly fills the point slots.
So the band-scoping env IS lit (no grey-fallback fires; `dl||pl > 0`). The bug is
purely that those 4 white key spots don't *reach* the band.

The grey-key fallback (`Rnd_Wgpu_RB3.cpp:1337-1349`, only when `dl==0 && pl==0`) is
**irrelevant here** — char.env has real lights so it never fires. This is NOT the
MEMORY-A4 "unlit env grey fallback" case.

## 3. Why the band gets no key — the falloff mismatch (the smoking gun)

### Band root world positions (live, via `{rb3_pos_dump}` under arena_02)
```
player0 (guitar) root=(116.3,-461.6,256.9)
player1 (bass)   root=(-122.7,-465.6,257.5)
player2 (vocals) root=(  3.7,-509.1,257.1)
player3 (drums)  root=( -5.0, -46.2,320.9)
```
`band_at_origin=0/4`, `crowd_at_origin=0/4700` — placement is correct (this is NOT
the crowd-origin family). Each performer is ~70–103u from its matching
`*_silhouette.lit` spot (which is authored a bit in front of + above the performer).

### Native vs GX point-light contribution at the performer (torso sample, ×0.70 venue exposure)

| performer | nearest spot dist | **native** `saturate(1-d/r)²` sum (4 spots) | **GX** `1/(1+d/r)` sum (4 spots) |
|---|---|---|---|
| player0 guitar | ~100u | **0.00** | **0.69** |
| player1 bass   | ~47u  | **0.01** | **0.74** |
| player2 vocals | ~50u  | **0.01** | **0.86** |
| player3 drums  | ~45u  | **0.02** | **0.59** |

(Numbers are the Lambert-pre, distance-attenuated white key sum the shader feeds the
diffuse term; multiply by `N·L` per pixel. The native column is effectively zero
key; the GX column is a strong, dominant white key — exactly the spotlit look.)

The exact lines:
- **Native shader** — `milo-native-engine/src/gfx/standard_wgsl.inc:522-525`:
  ```wgsl
  let lightRange = pointLightRangeAt(lightIndex);
  let rangeAttenuation = saturate(1.0 - lightDistance / max(lightRange, 0.001));
  let falloff = rangeAttenuation * rangeAttenuation;   // == 0 for d >= range
  let lightColor = scene.pointLightColors[lightIndex].rgb * falloff;
  ```
- **Wii GX (ground truth)** — `rb3/src/system/rndwii/Lit.cpp:36-44`:
  ```cpp
  if (mType != kFakeSpot) {
      GXInitLightAttn(&mLight, Intensity(), 0, 0,  1, 1 / mRange, 0);  // k0=1,k1=1/range,k2=0
  } else { // kFakeSpot
      GXInitLightDistAttn(&mLight, mRange / 2, 0.3, GX_DA_STEEP);      // 0.3 brightness at range/2
      GXInitLightSpot(&mLight, GetLightFieldOfView() / 2, GX_SP_FLAT);
  }
  ```
  GX point attenuation = `1 / (k0 + k1·d + k2·d²)` = **`1/(1 + d/mRange)`**:
  0.50 at d=range, 0.33 at d=2·range, 0.20 at d=4·range — a long tail, never a hard
  cutoff. The silhouette spots are `kPoint` (type=0), so they take the
  `GXInitLightAttn` branch (the gentle one), NOT the steep kFakeSpot branch.

The `range` value (55) is identical on both sides — the engine reads `L->Range()`
straight through (`Rnd_Wgpu_RB3.cpp:1333`). The discrepancy is entirely the
**falloff CURVE**: native treats `range` as a hard extinction radius; GX treats it
as the half-brightness reference distance. With band-to-spot distances at 1.3–1.9×
range, that's the difference between 0 and ~0.4 per light.

### Why VENUE_LIGHT_OFF looks brighter
The OFF path (`Rnd_Wgpu_RB3.cpp:1352-1358`) is one white directional (1,1,1) + 0.45
grey ambient — no range falloff at all, so the band is uniformly lit (but flat,
mood-less, wrong). It's brighter only because it sidesteps the broken point falloff,
not because it's correct.

## 4. Compare to a venue that looks RIGHT (festival_01) — same path, why it survives

festival's band region measures bright (audit: frame luma 78–200 vs arena 26–56).
The mechanism is the same `WriteSceneUniforms` venue path, so festival is NOT
immune to the falloff bug — it survives because its band-scoping env relies far less
on tight range-55 point spots and more on **directionals / high ambient / large-range
lights** (directionals have NO distance falloff in either model, and big-range points
keep `d/range` small so even the broken squared curve stays > 0). The
festival-correct reference is in the ground-truth agent's
`refs/native_arena02_black_band.png` + `refs/native_festival_band_correct.png`.

Implication: any venue whose performer key relies on **tight (small-range) point
spots authored slightly off the performer** (the silhouette-spot pattern — common to
arenas) will go dark on native; venues lit by directionals/ambient/wide points won't.
That is the per-environ "fails differently per venue" behaviour the audit predicted,
and it is **the same root cause** as the candidate crowd-lighting gaps (GAP 3): a
falloff/lighting-model mismatch, not a per-venue special case.

## 5. Fix HYPOTHESIS (engine-side — DO NOT IMPLEMENT)

**Primary (highest-confidence, 2 lines): match the GX point falloff curve.**
`milo-native-engine/src/gfx/standard_wgsl.inc:523-524`, replace the squared-saturate
hard cutoff with the GX inverse-linear law:
```wgsl
// GX-faithful point falloff: GXInitLightAttn(.., k0=1, k1=1/range, k2=0)
//   => 1 / (1 + d/range).  Half-bright at d=range, long tail (not a hard cutoff).
let falloff = 1.0 / (1.0 + lightDistance / max(lightRange, 0.001));
```
This makes the range-55 silhouette spots reach the band 70–100u away (~0.35–0.55
each → a real white key), restoring the spotlit-dim look while keeping the dark
stage. Because directionals and the off-path are untouched, festival/club stay put;
verify no regression there.

Notes / guards for whoever implements:
- The GX inverse law never reaches 0, so very-distant points retain a tiny tail
  (e.g. 0.10 at d=4·range). With only 4 point slots this is negligible, but if it
  bleeds onto far geometry, add a soft far-cut at e.g. `4·range` via a smoothstep
  window rather than reverting to the squared cutoff.
- Keep the existing `RB3_VENUE_POINT_EXPOSURE` (0.70) lever — after the falloff fix
  the band may read slightly hot; tune exposure on the A/B, don't re-cripple falloff.
- Consider distinguishing `kFakeSpot` (type=2) lights, which GX gives the *steeper*
  `GXInitLightDistAttn(range/2, 0.3, STEEP)` + a spot cone — the engine currently
  treats every point identically. arena_02's char.env spots are type=0 (kPoint) so
  the inverse-linear law is correct for THIS gap; a fuller fidelity pass could honor
  the kFakeSpot cone separately, but it is not needed to close GAP 2.

**Secondary (only if the curve change regresses other venues): raise the band-scoping
env's effective range.** Detect the `*_silhouette.lit` / `char.env` family and bump
their effective range (e.g. ×2) in `WriteSceneUniforms` before writing
`s.pointLightRanges`. Hackier, venue-pattern-coupled — prefer the curve fix.

## Localization summary (anchors)

| claim | anchor |
|---|---|
| native point falloff (the bug) | `milo-native-engine/src/gfx/standard_wgsl.inc:522-525` |
| Wii GX point falloff (ground truth) | `rb3/src/system/rndwii/Lit.cpp:36-44` (`GXInitLightAttn(..,1,1/mRange,0)`) |
| venue light upload + per-env rewrite | `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:1266-1358`, `:3505-3511` |
| range passed straight through | `Rnd_Wgpu_RB3.cpp:1333` (`s.pointLightRanges[pl] = L->Range()`) |
| lit-branch (band depends on scene lights) | `standard_wgsl.inc:825-837` (`unlit==0` → `base × softClip(amb+diffuse)`) |
| grey fallback (NOT firing here) | `Rnd_Wgpu_RB3.cpp:1337-1349` (only when `dl==0 && pl==0`) |
| band roots (live) | `{rb3_pos_dump}` → player0..3 at §3; `band_at_origin=0/4` |
| char.env light dump | `RB3_VENUE_PROBE=1` log, §2 |
| A/B frames | `lighting/frames/arena02_front_bass_VENUE_{ON,OFF}.png` |
| harness | `/tmp/bch_override.py` (Group-C wrapper) + `scripts/native/band-closeup-capture.py` |

## Relationship to GAP 3 (big_club white crowd) — likely ONE root cause

Both are the same lighting-model mismatch in the venue path, manifesting per-environ:
- arena too DARK = tight range-55 point key spots cut to 0 by the squared-saturate
  cutoff (this doc).
- big_club too WHITE is the inverse symptom — likely crowd chars either taking the
  `unlit` full-bright branch under big_club's environ, OR an over-bright/over-reaching
  light. The crowd agent should check (a) the crowd char material's `mUseEnviron`
  (unlit→white) and (b) whether the GX-faithful falloff change above also tames a
  too-strong big_club crowd light. Fix the falloff curve FIRST, then re-measure
  big_club — it may move both gaps.
