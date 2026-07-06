# Wave 8 — Pre-dispatch review (Fable adversarial pass)

**Reviewer:** Fable subagent, 2026-07-06. **Object under review:** `WAVE8_KICKOFF.md` (draft).
**Verdict: DISPATCH-WITH-AMENDMENTS.** The lane structure, fences, and model tiers are sound;
Lane B's preference-1 is verified viable in source. But the kickoff's headline framing for Lane A
("one env-state machine misbehaving in both directions") is contradicted by both the adjudicated
Wave-6/7 evidence and the actual code structure, S2's gate (a) is statistically toothless against
the disclosed 1/8 baseline, and gate (b) names the WRONG color control as its target. Eight
amendments below, each with file:line evidence.

---

## A1 (R-A) — The "one env-state bug, two directions" framing is HALF-WRONG; S1's grey-direction hypothesis was already refuted in Wave 6. Reword S1.

The code has **three distinct env-state decision points**, and the ms3000 grey lives in a
**fourth stage that is not env-state at all**:

1. **Engagement condition** — `Rnd_Wgpu_RB3.cpp:1404`:
   `sVenueLightEnabled() && camNm=="world.cam" && venv && venv->mAmbientFogOwner`. A miss falls
   to the flat-default else (`:1538-1543`, one white directional + 0.45 grey ambient) — the
   appearance the WASH matrix proved IS the pink cast (`venue_light_off` forces this path →
   PINK 8/8). Note `venv->mAmbientFogOwner == nullptr` is a silent per-boot engagement-miss
   candidate the kickoff doesn't name.
2. **Grey-key no-lights fallback** — `:1523-1534`, INSIDE the engaged venue branch, fires only
   when the engaged env yields `dl==0 && pl==0` after the `mColorOwner`/`Showing()`/black-color
   filters.
3. **DrawMesh staleness gate** — `:2382-2410`: per-environ uniform re-write only on
   `camChanged` or (`sVenueLightEnabled` && world.cam && env-POINTER change, `:2404-2406`).
   Pointer-equality staleness (freed env reallocated at the same address → no rewrite) is a
   second engagement-adjacent candidate.

**The grey at ms3000 is none of these.** W3.3/STATUS.md (Wave-6 D.S2) already adjudicated and
REFUTED exactly the hypothesis the kickoff assigns S1 ("fallback fires while the env is actually
lit → grey at song start"): under `RB3_PP_OFF` the ms3000 render is PINK/colored — the lighting
output is colored, so the grey-key fallback (which is composite-independent; it would render grey
under `RB3_PP_OFF` too) is NOT firing. Wave-7 A.S2 then located the grey precisely: a **sub-knee
MID/LOW-tone desaturation created by the composite** (default mid sat 0.026 vs pp_off 0.389),
upstream of the ceiling guard. The two-control requirement (BOTH `RB3_PP_OFF` AND
`RB3_VENUE_LIGHT_OFF` restore color at ms3000) is therefore not a paradox to instrument away —
it is the signature of an **interaction**: the venue path supplies over-hot colored input
(`sVenueDirExposure`/`sVenuePointExposure`, `:1481/:1490`) and the composite destroys its chroma
in the mid-tones. Removing either side removes the grey.

**Amendment:** reword S1 as two hypotheses, both in-fence (Lane A owns `Rnd_Wgpu_RB3.cpp` AND
`RB3PostProc.*` + WGSL, so no fence change is needed): (H-PINK) per-boot engagement
miss/staleness at decision points 1 and 3 → flat-default pink base at ms21000 — instrument which
env is `sCurrent`, `mAmbientFogOwner` nullity, which of the three decision points was taken, and
`mLastSceneEnv` pointer-aliasing, per boot; (H-GREY) hot-venue-input × composite mid-tone
desaturation at ms3000 — do NOT re-instrument the grey-key fallback (refuted); instead
instrument the composite stage with A.S2's per-tonal-band method. Concrete composite lead worth
one probe: the intermediate texture is created at the framebuffer format
(`RB3PostProc.cpp:155`, `td.format = mTargetFmt` — unorm), so hot >1.0 saturated lighting is
per-channel clamped at the intermediate WRITE, before any grade or ceiling can act; also the
grade's contrast/brightness remap (`rb3_postproc.wgsl.inc:92-104`) then pulls the flattened
channels down into the mid-tones where they read grey. (This candidate must explain why
`RB3_PP_OFF` — also a unorm target — stays colored; likely the composite's remap darkening is
the differencing term. S1's job, not mine.)

## A2 — Split S2 into two separately-flagged, separately-gated fixes; gate (b) currently names the WRONG target control.

As written, S2 is one fix ("deterministic engagement + correct fallback") that must pass gate (b)
("song-start sweep ms2000-6000 in color"). Per A1, an env-state-only fix architecturally cannot
pass gate (b) — deterministic engagement would keep feeding the hot reveal into the composite that
desaturates it. That is a re-run of the Wave-7 W3.3-fix failure mode (one lever, wrong stage),
except this time it's knowable pre-dispatch. **Amendment:** authorize S2 to land TWO default-OFF
flags — WASH-fix (engagement determinism + non-pink unlit fallback; gates a/d-pink/e) and
W3.3b-fix (sub-knee-chroma-preserving composite change; gates b/d-grey/e) — with per-fix
attribution, or explicitly instruct S2 that gate (b) may require the composite fix and budget for
it.

**Gate (b) error:** "matching the venue_light_off control's hue" is wrong. `venue_light_off` at
ms3000 renders the **flat-default warm look** (W3.3/STATUS.md flag matrix: "warm/colored") — i.e.
the very pink-base/unlit appearance Lane A is trying to ELIMINATE at ms21000. A fix could pass
gate (b) by regressing venue lighting to the flat default. The faithful target is the **`RB3_PP_OFF`
control** (D.S2: "pink/colored"; A.S1 hue 324.9; A.S2 mid-tone sat 0.389 / low-tone 0.779).
Amend gate (b) to: hue within a few degrees of the pp_off control AND mid-tone sat recovering
toward 0.389 (A.S2's per-tonal-band protocol), with the venue-light path still ENGAGED (verify
via the S1 instrumentation, so the gate can't be passed by disengaging the venue path).

## A3 — Gate (a) cannot demonstrate the fix at n=8: 0/8 vs a 1/8 baseline is luck-indistinguishable. Gate on the instrumented mechanism instead.

The pure-default wash rate is 1/8 in the Wave-7 matrix and was 2/7–4/7 in the Wave-6 S2
measurement — boot-stochastic AND session-unstable. At a true rate of 1/8, a **no-op fix passes
"wash 0/8" with probability (7/8)^8 ≈ 34%**; at 2/7, ≈ 7%. The gate can fail red, but it cannot
distinguish a real fix from luck at n=8, and its baseline moves between sessions. **Amendment:**
the primary gate must be the S1-delivered mechanism counter: (i) engagement-miss/staleness events
= 0/N across N≥16 instrumented boots at the pinned shot (each boot informative regardless of wash
class), and (ii) a **deterministic forced-miss arm** — force the unlit/fallback path (the
venue_light_off-style condition) and require the corrected fallback to render non-pink
(fail-red: revert fallback → pink 8/8, which IS deterministic per the matrix). Keep the wash-rate
matrix as secondary confirmation, disclosed as low-power at n=8.

## A4 (R-B) — Game-side per-frame correction IS expressible; preference (1) verified in source. Two hazards the S1 plan must address.

Verified: the skinning palette is composed **engine-side per draw** but from **game-writable
state** — `Multiply(owner->BoneOffsetAt(b), bt->WorldXfm(), skin)` at `Rnd_Wgpu_RB3.cpp:3529`
(bone world read at `:3486`), and `RndMesh::BoneOffsetAt` returns a **mutable `Transform&`**
(rb3 `src/system/rndobj/Mesh.h:257`; `SetBone` at `Mesh.cpp:328` writes `mBones[idx].mOffset`).
`BandCharacter::Poll` already has the correct post-pose seam: `NativeRepinHandsRigid()` is called
at `BandCharacter.cpp:581`, after `Character::Poll()` (`:527`) and after
`RebindOutfitBonesToOwnSkeleton()` (`:572`). The existing rebinds are **latched one-shot**
(`mNativeHandsRigidOnce` early-return at `:1709`, re-armed at `:136/:1944/:2396`) but the call
site runs EVERY Poll — an unlatched per-frame offset rewrite drops into the same seam with no
engine edit. So preference (1) stands; the engine palette-build hook (preference 2) is only
needed for the dual-skin gate probe (see A5), not for the fix itself.

Two hazards the plan must state: **(a) palette source is the OWNER mesh** — the draw path reads
`owner->BoneOffsetAt`, not the drawn mesh's, when they differ (`:3183-3200` documents the
owner-vs-own distinction); prior hands rebinds via `mesh->SetBone` measurably reached the palette
(B.S3's 106→205u shift proves it), but the plan must write the palette-source mesh explicitly.
**(b) draw-time world recompute** — the default-ON `RB3_NO_SKEL_WORLDFIX` pass
(`Rnd_Wgpu_RB3.cpp:3421-3446`) force-recomputes every referenced bone's world AFTER Poll, exactly
because Poll-time `WorldXfm()` reads can be stale (`:3400-3411`). A per-frame offset computed in
Poll from a stale `wristLiveWorld` will disagree with the palette's post-force world. The plan
must force the anchor chain (`DirtyLocalXfm`+`WorldXfm_Force`, same pattern) before sampling, or
prove coherence.

## A5 — Lane B S3's gate must name the operative metric: IK_SHARD_VERT wext A/B, not the RealPathFixture gtest.

"BL-A2 oracle flag-ON GREEN (<20u from the 79-107u RED baseline)" conflates two instruments. The
79-107u RED baseline is the **in-engine `IK_SHARD_VERT` far-vertex probe** (B.S2/B.S4 tables);
the BL-A2 gtest's `RealPathFixture` arm is a **SKIP** without `live_pose.txt`, and populating it
requires the dual-skin engine probe that B.S2 explicitly scoped as an engine edit in
Lane-A-owned `Rnd_Wgpu_RB3.cpp`. **Amendment:** the hard S3 exit is the IK_SHARD_VERT wext A/B
under B.S4's same-binary/same-members protocol — flag-ON worst appendage wext < 20u vs flag-OFF
~106u; FLING=0; zero added drops; nothing in the 200-460u STOP band. RealPathFixture population
is optional and, if pursued, is a staged Lane-A-tail patch (coordinator-sequenced), matching the
kickoff's own preference-2 rule. Additionally: both A/B arms must run with `RB3_HANDS_POSEAWARE`
unset — it overwrites the same meshes' binds at the same seam (`:574-581`) and would confound the
measurement; the new flag and `RB3_HANDS_POSEAWARE` should be mutually exclusive in code or the
plan must state which wins.

## A6 (R-C) — YES: require the W2.7 census probe as Lane C's mandatory step 1, add exit gates (the brief has none), and pre-authorize the staged-patch outcome.

`RB3_HEADMAT_DBG` (engine `RB3MaterialBinder.cpp:166`, per W2.7/STATUS.md) already emits
`diffuse=... hasTex=...` per drawn material — zero new code, and it directly discriminates the
three hypotheses (null diffuse → flat lit color = W2.7 family; authored shadow decal; alpha bug).
Step 1 = capture the part_difficulty backdrop + a gameplay venue shot with `RB3_HEADMAT_DBG=1`
and census the poster/decal materials for `diffuse='<null>' hasTex=0` before ANY new diagnosis.
Two corrections to the brief: (i) W2.7's shipped fix lives in `OutfitConfig::SetSkinTextures`
(head-scoped, character skin path) — venue poster quads never pass through it, so "generalization
of the shipped head fix" means a NEW call site in venue/world game code, not an extension of the
existing one; (ii) if the diagnosis points at an engine-side lever (e.g. a binder-level
null-diffuse fallback in `RB3MaterialBinder.cpp` — an engine file, forbidden by Lane C's fence
and NOT in Lane A's declared ownership either), the sanctioned outcome is a staged patch for
Wave 9, and the brief should say so. Add explicit exit gates: census A/B (opt-out null / default
bound), visual A/B against `images/retail-screenshots/`, lineup PASS, fail-red via the opt-out
flag. Model tier: Opus→Sonnet is fine ONLY if the census confirms the W2.7 family; a non-W2.7
diagnosis escalates the fix to Opus.

## A7 (R-D) — Baseline effects beyond the kickoff's notes: three additions.

The kickoff correctly carries the text-floor/hub-quad flips and the pure-default-arm disclosure.
Add: **(a)** `RB3_PP_LUMA_CEILING` must be UNSET in every Wave-8 arm — WASH matrix configs 1-4
all carried it (WASH/STATUS.md configs table); it is sub-knee-identity (A.S2) but the 4/8-vs-1/8
delta it sat next to is exactly the noise a clean protocol should drop. **(b)** the WASH matrix
ran on engine HEAD `af4a22a`, ahead of pin `a94762f` — S1 should reproduce the default wash rate
(and the venue_light_off 8/8 PINK) on the current pin before attributing any change to its own
instrumentation build. **(c)** any menu-screen visual baseline captured pre-Wave-7 is stale for
hub/text content (quad now hidden, focused text darker) — Lane C must capture fresh part_difficulty
baselines, not diff against Wave-6 captures. Drawlog golden 792 (gate e) is already correct
post-contract-flip.

## A8 — Missing standard gates + fence confirmations (minor).

S2's gate list omits the standing engine gates: `milo-engine-tests` 198/0/2, DC3 zero-blast for
any shared-WGSL/`UniformStructs` change (`rb3_postproc.wgsl.inc` is RB3-only so composite work is
structurally zero-blast — state it, as A.S2 did), classification.json append-only + single
coordinator regen. Fence audit result: **no missed collision.** Lane B is the sole
`BandCharacter.cpp` writer this wave (W2.6 foot/shoe is deferred; Lane C's game-side surface is
OutfitConfig/venue-world code; Lane A is engine-only); Lane C's fence should explicitly name
`BandCharacter.cpp` and `RB3MaterialBinder.cpp` as forbidden to make that stick. Lane A's
sequential S1→S2→S3 is right (the Wave-7 A.S1-vs-A.S2 divergence is the argument for the
independent S3). Lane B S1(plan)→S2(impl)→S3(verify) with all-Opus tiers is justified by two
prior empirically-killed fix classes. Checkpoints/process rules are consistent with Wave 7.

---

## What I checked in source (appendix)

- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`:
  - `:1404` venue-branch engagement condition (incl. the `mAmbientFogOwner` guard);
    `:1523-1534` grey-key dl==0&&pl==0 fallback (inside the engaged branch);
    `:1538-1543` flat-default else (white dir + 0.45 ambient — the venue_light_off look);
    `:1481-1494` `sVenueDirExposure`/`sVenuePointExposure` hot-input scalars;
    `:1059` `sVenueLightEnabled` (RB3_VENUE_LIGHT_OFF); `:1430-1441` VENUE_PROBE.
  - `:2382-2410` DrawMesh scene-uniform rewrite gates: camChanged path (`:2390-2397`) vs
    per-environ pointer-change path (`:2404-2409`, `mLastSceneEnv`).
  - `:3421-3446` draw-time `WorldXfm_Force` bone-chain recompute (RB3_NO_SKEL_WORLDFIX) and its
    stale-Poll-read rationale `:3400-3411`; `:3472-3529` palette compose
    `owner->BoneOffsetAt(b) * bt->WorldXfm()`; `:3183-3200` owner-vs-own palette-source probes.
- `milo-native-engine/src/platform/RB3PostProc.cpp`: `:155` intermediate format = `mTargetFmt`
  (framebuffer unorm); `:38-52` mid-frame flush/composite structure.
- `milo-native-engine/src/gfx/Shaders/rb3_postproc.wgsl.inc`: `:88-107` levels/contrast/
  brightness/saturation grade (all per-channel or luma-mix, upstream of the ceiling);
  `:175-199` ceiling-guard + W3.3-fix comment block (identity below the knee).
- rb3 `src/system/bandobj/BandCharacter.cpp`: `:517-598` Poll seam ordering
  (RebindHeadHandsAtRest pre-`Character::Poll`, RebindOutfitBonesToOwnSkeleton `:572`,
  NativeRepinHandsRigid `:581`, RebindInstStringsToRestBasis `:598`); `:1709` once-latch;
  `:136/:1944/:2396` latch re-arms.
- rb3 `src/system/rndobj/Mesh.h:257` mutable `BoneOffsetAt`; `Mesh.cpp:328-335` `SetBone`
  offset write semantics.
- Documents: WAVE8_KICKOFF.md; execution/README.md Waves 6-7 + Wave-8 menu; WASH/STATUS.md
  (5-config matrix + disclosures); W3.3/STATUS.md (D.S2 flag matrix + hypothesis-ii refutation;
  A.S2 per-tonal-band refutation); W2.8/STATUS.md (BL-A2 oracle, step-0 inert-flag finding,
  B.S3/B.S4 rigid-anchor negative); W2.7/STATUS.md (census probe + fix mechanism).
