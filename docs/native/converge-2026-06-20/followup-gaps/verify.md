# Converge follow-up gaps — VERIFY (adversarial final gate)

**Verify agent (Opus), 2026-06-21. Independent re-measurement of the two landed
engine fixes in the `converge-render2` paired worktree. Engine pin base `a360e3c`.**

Verdict: **LAND.** Both fixes work, are DC3-safe by construction, and the crowd
discriminator is empirically exact (probe-confirmed it dims ONLY crowd/extras +
the impostor billboards, never band or text). One note: the impl **deviated from
the written plan** for GAP B(a) — see §1.

Engine SHAs (push order = oldest first, then bump `MILO_ENGINE_PIN`):
1. `ada6e566403dc3b6310e436ce409d700f8ae0900` — GAP B(a) crowd dim
2. `b8f3cfafc1acf78f80812f1c48255563dd2b5246` — GAP A1 watermark dim

Both touch ONLY `src/platform/Rnd_Wgpu_RB3.cpp` (96 insertions, 0 other files).
No shared shader / `MaterialUniforms` change (A2 correctly NOT applied).

---

## Build provenance (caught a stale-binary trap)

The worktree binary on arrival (04:47) predated the A1 commit. I rebuilt
(`cmake --build native/build-native --target rb3-native`), force-touched the TU to
prove the binary is current, and confirmed all four env strings
(`RB3_CROWD_DIM`, `RB3_CROWD_DIM_OFF`, `RB3_HIGHWAY_WATERMARK_OFF/DIM`) link in.
All measurements below are on the freshly-built binary with BOTH commits.

---

## 1. GAP B(a) — big_club white crowd → ~0  ✅ PASS (with a plan deviation)

### Plan deviation (defensible, measured)
The brief said dim **skinned crowd/extras ONLY**. The impl measured that the
DOMINANT visible white is actually **2D bowl-impostor crowd BILLBOARDS** (empty
mesh name + empty material name + non-skinned, ~9000 quads/frame under world.cam),
NOT the skinned characters. The fix dims BOTH families. I independently confirmed
this with a one-shot draw probe (added, ran, reverted — engine is clean):

- **impostor path fires on exactly ONE family**: `mesh='' mat='' skinned=0
  col=(1,1,1)`. Nothing else.
- **skinnedCrowd path fires ONLY on crowd/extras**: every match was
  `*_crowd_*` / `*_extra*` / crowd-character body parts (clap, fist, lighter,
  eyes, hair, skin, head, tongue) bound to a `char/crowd/` | `char/extras/`
  skeleton. **Zero band matches**, even during a band closeup pin.
- **TEXT SAFETY (decisive)**: the ONE empty-mesh world.cam draw that is NOT a
  crowd billboard is a real font glyph — `mat='Pentatonic_Regular_(5_00)4x_…'
  col=(0,0,0)` — and the `matName[0]=='\0'` gate correctly logged it `NOTDIMMED`.
  Real text under world.cam carries a NAMED font material, so the discriminator
  excludes it. The text exclusion is sound.

### Crowd white% → ~0 (two boots, exposure-strobe peaks)
| boot | region | OFF white% (peak) | ON white% (peak) | crowd luma ON |
|---|---|---|---|---|
| #1 | crowdL | 9.76 | 0.36 | 16–51 |
| #1 | crowdR | 5.10 | 1.32 | 12–58 |
| #1 | topband | 16.30 (strobe) | 3.15 | — |
| #2 | crowdL | 6.72 | 0.31 | 22–48 |
| #2 | crowdR | 5.37 | 0.37 | 12–51 |

Crowd white% drops to ~0; crowd stays **dim-but-present** (luma ~12–51, not
zeroed). Visual A/B at the brightest strobe frame: stark-white audience cut-outs →
dim red figures blended into the venue. Gate met.

### Band NOT dimmed  ✅
`band-closeup-capture.py` pinned 15/15 deterministically in BOTH OFF and ON;
`drops_band=0`, `max_band_ratio=0.00` both passes. Band-member region luma ON ≥ OFF
(cg 44 vs 39, cg01 101 vs 74 — variation is venue strobe, not the fix). The probe
confirmed no band outfit/skin/head matched either path. `skeleton_unshared.milo`
hard-exclusion holds.

### Text/HUD NOT dimmed  ✅
The `topband` (HUD/score/title) region holds a CONSTANT ~3.2% white% across ALL
ON and OFF frames in both boots — unchanged by the fix. Probe proved the only
world.cam text quad (Pentatonic font) is excluded by the empty-material gate.

### Other venues — not over-darkened, no crash  ✅
- **festival_01**: crowd luma preserved (17–94 ON vs 9–110 OFF), crowd present;
  residual white is the authored B&W comic backdrop (NOT a skinned/impostor crowd
  mesh → correctly untouched, per ground-truth). Not over-darkened.
- **arena_02**: renders clean, band + highway intact, crowd dim-not-white.
- **small_club_01**: no crash, highway/gems clean, crowd off-frame (no change).
  (arena_01 was not tested — it CRASHES per the brief, unrelated to this fix.)

### Default factor note
Default `RB3_CROWD_DIM=0.10` (impl), not the plan's 0.30 — because the impostor
diffuse is near-white and needs a smaller multiplier to hit retail-dim. Verified
0.10 lands crowd luma in the 12–51 band (dim, present). Tunable; opt-out
`RB3_CROWD_DIM_OFF=1` confirmed working (it's the OFF baseline above).

---

## 2. GAP A1 — highway watermark dim  ✅ PASS

Captured the game.cam highway surface watermark in THREE modes (FULL `DIM=1.0`,
DIMMED default `0.30`, OFF) and cropped/upscaled the bare-surface clef-filigree:

- **FULL**: clef-scroll swirls render BRIGHT and prominent (the over-bright bug).
- **DIM (0.30, default)**: swirls are a FAINT ghost — still clearly visible as a
  watermark, much dimmer. This is the retail-faint intent.
- **OFF**: swirls COMPLETELY gone (pure dark surface) → proves `surface.mat`
  emissive is the source.

Pattern still present (not removed) ✓. Dimmed vs full ✓. OFF removes it ✓.

**Gems / now-bar / lane separators / note bars: VISUALLY IDENTICAL across all
three modes** — keyed on `game.cam && surface.mat` only, so `prism_mat` (gems),
`gem_smasher_glow` (now-bar), `rails.mat` (lanes) are untouched. ✓

**A2 (shader desaturate) correctly NOT applied.** The impl measured the
watermark's OWN teal contribution at +0.9 (the residual highway teal is the
rails/surface base, adversarially tuned per MEMORY a234, NOT the watermark), so
the A2 measure-gate (stroke teal > +25 FROM the watermark) was not met. I confirmed
`materialEmissiveDesat` is ABSENT from the engine and `standard_wgsl.inc` is
untouched. Keeping A1-only keeps the whole change in the RB3-only TU.

Caveat on numeric teal/delta gate: a tight watermark-band crop is contaminated by
moving gems (delta stays 80–114 even with watermark OFF), so the
stroke-delta/teal table is not a clean isolation across non-time-locked frames.
The decisive evidence is the bare-surface visual A/B (full→dim→off), which clearly
shows the watermark dimmed to a faint ghost while gems/now-bar are unchanged.

---

## 3. DC3 safety  ✅ PASS

- Engine `CMakeLists.txt:363–366`: `dc3` flavor compiles
  `MILO_ENGINE_GPU_PLATFORM_SOURCES` (`Rnd_Wgpu.cpp`); `rb3` flavor compiles
  `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` (`Rnd_Wgpu_RB3.cpp` ONLY). DC3 builds with
  `dc3` and **never compiles `Rnd_Wgpu_RB3.cpp`**.
- Both commits touch ONLY `Rnd_Wgpu_RB3.cpp`. No shared shader/uniform change → DC3
  byte-identical. A2 not present.

---

## 4. Discriminator robustness audit

- **Impostor billboard** (`worldCam && !skinned && isTextMeshHeur (empty mesh) &&
  matName[0]=='\0'`): probe shows it fires on exactly the one empty/empty/white
  billboard family. A future named-material crowd billboard would be missed
  (conservative under-dim, NOT a false-positive). Cannot fire under ui/overshell
  cams (gated on `world.cam`), cannot fire on skinned meshes, cannot fire on named
  text (font material name present). The single world.cam font quad is excluded.
- **skinnedCrowd** (`skinned && !text && !uiText && isCrowdOrExtras && !bandMember`):
  name-OR-skeleton detector; `skeleton_unshared.milo` is the hard band exclusion.
  Probe: zero band matches across normal play AND a band closeup pin. Shared
  head/eyes/hair materials only match when the OWNER skeleton is crowd/extras.
- **Watermark dim**: keyed on `mname=="surface.mat"` inside the
  `game.cam` track-light block → cannot affect any other material or camera.

## What I did
Rebuilt the worktree (binary was stale, missing A1); 2× big_club A/B + band
closeup A/B + festival/arena/small_club ON; added+ran+reverted a one-shot draw
probe to enumerate exactly what each path dims (engine left clean). Drive script
+ /tmp scratch are not in the repo.

## Residual / out of scope
GAP B(b) accessory mesh-shards: correctly guard-dropped, NOT touched (per brief).
Festival's residual white is the authored comic backdrop (correct, not a bug).
arena_01 crash is pre-existing and unrelated.
