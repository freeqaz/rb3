# PLAN R5 — Hands endgame: true reskin decision

**Item:** ROADMAP.md R5. **Author:** Fable planner, 2026-07-07 (read-only research pass;
every load-bearing claim re-verified against source this session — file:line cited inline).
**Hard gate:** R5 does NOT start until R1 reports (ROADMAP "Ground rules"). §2.4 below is the
binding artifact contract R1 must satisfy for R5 to run at all.
**Consumers:** the flagship remaining visual bug's close-out; the standing rule "no more
native-only hands verdicts."

---

## 0. THE ONE DERIVATION THIS PLAN RESTS ON (read before the branches)

The item asks: does a verts+**weights** TRUE reskin escape the Wave-14 refutation, or fall
to it? The answer is **conditional**, and deriving the condition is what makes this plan a
decision procedure instead of a bet. Notation (char-space, row-vector `p·M` as the engine
composes — `Rnd_Wgpu_RB3.cpp:3299-3305` palette `skin_i = A_i·O_i(t)`):

- Authored (360-ARK) hand mesh: bind positions `p_v`, weights `w_{v,i}` (RndMesh::Vert
  `boneWeights`/`boneIndices[4]`, `src/system/rndobj/Mesh.h:80,86` — mutable band-side,
  read by the GPU vertex build, engine `Rnd_Wgpu_RB3.cpp:2762`), authored inverse binds
  `A_i = meshWorld·inv(Bbind_i)` where `Bbind` = the SHARED embedded bind skeleton's pose.
- Native per-member skeleton `own`: live worlds `O_i(t)`; NO static gender-bind asset
  exists (SKEL/STATUS.md seam-A: the gender bind is a runtime CharClip; its only static
  realization is `own` after SetDeformation — and that seed pose `R` sits **87.2°** off
  both the authored bind `B` and the play basis, HANDS-ADJUDICATION/VERDICT.md §2).
- Wii ground truth: ONE skeleton; composition `x_v(t) = p_v · Σ w_i A_i·L_i(t)`.

**Step 1 — the no-op trap.** "Express each vert in the shared-bind skeleton's bone-local
frames and re-emit against own" with UNCHANGED bone-local coordinates is algebraically a
no-op: bone-local coord `q_{v,i} = p_v·A_i`, re-emission `x(t) = Σ w_i q_{v,i}·O_i(t)
= p_v·Σ w_i A_i·O_i(t)` — **exactly the Wave-16 `RB3_HANDS_AUTHORED_REPOINT` cell**, which
passed every numeric rest gate and was REFUTED by the visual gate (torn spike-fans at
animated poses, HANDS-FIX/STATUS.md). A "true reskin" only differs from the 8th dead cell
if it **changes** the bone-local geometry (and possibly weights) against a *different*
reference pose of `own`.

**Step 2 — what changing it gives.** Pick a reference pose `Ownref_i` for `own`; re-pose
`p'_v = p_v · Σ w_i (A_i·Ownref_i)`, re-emit offsets `A'_i = meshWorld·inv(Ownref_i)`,
runtime `x(t) = p'_v · Σ w_i A'_i·O_i(t)`. Coherent at `O = Ownref` by construction;
coherent under animation iff `O(t)` is a self-consistent animation *of the Ownref
skeleton*. Now the trap the saga hit twice:
- `Ownref = seed R` (SetDeformation capture) → this IS Wave-14 `RB3_HANDS_RESKIN`
  (RESKIN/STATUS.md: `NativeCharSpaceRestXfm(own)` captured at SyncObjects — the seed) —
  the animation factor keeps the 87° `inv(R)·L` conjugation AND the re-posed verts sit at
  larger radius, so the smear is **amplified** (74.8→87.7u measured). W14's refutation is
  an ANCHOR failure, not a proof that all reskins fail — but it kills every seed-anchored
  variant permanently.
- `Ownref ≈ B` (the play basis arm-S measured at 3.1°) → `A_i·Ownref_i ≈ I` → `p' ≈ p`,
  `A' ≈ A` → degenerates back to the Wave-16 cell (Step 1) — **already refuted**.

**Step 3 — the condition.** Steps 1-2 exhaust every anchor capturable natively at runtime
(this is the W15 §3 invariant extended to reskins). A true reskin therefore has exactly
one non-dead form: **anchor derived from EXTERNAL ground truth**. If R1 shows native
`own`'s animated inter-bone poses diverge from Wii by a per-pair delta that is
**CONSTANT over frames** (a basis error `C_i`), then a consistent target basis exists
(`Ownref_i = Bbind_i·C_i`-class construction, or equivalently remove `C_i` at the
animation source) and a reskin (or the cheaper anim-side correction, §3.3-B1) is
justified and expected to work. If the divergence is ~zero (bones already Wii-faithful)
the reskin is unjustified — it would deform correct geometry to fit correct bones, pure
error. If the divergence is time-varying, no static re-binding can fix it — it is an
animation-decode defect and both reskin and closure are premature.

**That conditional IS the R5 decision procedure.** Everything below operationalizes it.

---

## 1. OBJECTIVE + NON-GOALS

**Objective:** close the hands/fingers bug family with an evidence-based verdict decided
against R1's Dolphin inter-bone ground truth — one of: (a) a landed, gated fix (anim-basis
correction or true reskin, per §3.3's branch table), or (b) documented CLOSURE with a
polished mitigation and an explicitly accepted user-visible residual. Either way the
family's ledger entry closes with the R1 evidence attached and the decision procedure's
pre-registered thresholds shown to have been applied mechanically.

**Non-goals:**
- NO ninth offset-bake cell. The class is exhausted (7 cells VERDICT §3 + Wave-16's 8th);
  any proposal reducible to `off = f(runtime-capturable anchor)` is auto-rejected.
- NO native-only verdicts. Every branch's decisive gate references R1 output (ROADMAP row
  R5: "no more native-only hands verdicts").
- NO reliance on `wext` as a shard oracle (VERDICT §6.4) — it is DESCRIPTIVE only.
- NOT a re-derivation of R1: if R1's artifact misses the §2.4 contract, R5 bounces it back
  rather than improvising ground truth.
- NO default flips inside the decision lane; fix branches are flag-first per campaign SOP.

---

## 2. CURRENT STATE (verified this session)

### 2.1 The record (what is already measured-dead)

| Lever | Verdict | Where |
|---|---|---|
| 7 offset-bake cells (anchor × bone table) | dead, one invariant | HANDS-ADJUDICATION/VERDICT.md §3 |
| 8th cell: authored offsets + repoint to own (`RB3_HANDS_AUTHORED_REPOINT`) | numeric gates PASS both genders, **VISUAL REFUTED** (torn spike-fans at animated poses) | HANDS-FIX/STATUS.md; flag registered not-live (engine `NativeCompatFlags.classification.json:1376`) |
| Positions-only re-pose (`RB3_HANDS_RESKIN`, seed-R anchored) | REFUTED (wext 74.8→87.7u regression; radius-amplifying) | RESKIN/STATUS.md; code kept default-OFF `BandCharacter.cpp:2587` + refutation header `:3239` |
| Seam-A (un-share/re-pose `bound`) | provably degenerate (no third anchor value exists) | SKEL/STATUS.md |
| Seam-B (per-dominant-bone vert re-pose) | mixed-sign ±6-35° per-bone gaps tear knuckle blends — **vindicated by Wave 16** | SKEL/STATUS.md + HANDS-FIX root cause |

### 2.2 Source anchors (all re-read this session)

- `src/system/bandobj/BandCharacter.cpp`: `RebindHeadHandsAtRest` `:1254` (hands writer;
  AUTHORED_REPOINT machinery incl. rebake-skip at `:1844`); `NativeCaptureRestPoseAfterDeform`
  `:974` + `NativeCharSpaceRestXfm` `:933` (char-space rest, root-relative — the placement
  fix); `NativeReskinHandsAtRest` call `:2587`, W14 refutation header `:3230-3262` (incl.
  the R1-W14 KEY FINDING: do NOT route through `RndMeshDeform::Reskin` — `BoneDesc::
  ExportWorldXfm` returns live pose only for `exo_`-prefixed bones); torso precedent
  `RebindOutfitBonesToOwnSkeleton` `:1102`.
- `src/system/rndobj/MeshDeform.cpp:298-399` `RndMeshDeform::Reskin`: **positions+normals
  only** (`v.pos` `:368`, `v.norm` `:369-395`); weights/indices NEVER touched — confirms
  the item's premise that a verts+weights reskin is NEW code.
- `src/system/rndobj/Mesh.h:80,86`: `Vert::boneWeights` (Vector4_16_01) + `boneIndices[4]`
  — the weight surface a true reskin would mutate; engine GPU build reads them
  (`Rnd_Wgpu_RB3.cpp:2762`).
- Engine clamp (`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:3745-3835`):
  **the `RB3_NO_SKIN_CLAMP` clamp does NOT apply to band hand meshes** — rebound meshes
  are skipped (`bool reboundSkip = mesh->mNativeBonesRebound` `:3777`; clamp gate
  `sSkinClamp && !reboundSkip && numBones >= 8` `:3812`), and the hands paths set
  `mNativeBonesRebound=true` (`BandCharacter.cpp:1898`). **Correction to the record's
  shorthand:** "RB3_NO_SKIN_CLAMP remains the shipped mitigation" is literally true only
  for crowd/extras; the band hands' shipped state is the default rebake — the coherent
  displaced "ceiling hand" + wrist spike-webbing at some animated poses (HANDS-FIX
  evidence `matched_zoom_burst08_burst12.png`, flag-OFF arm). Any CLOSURE polish must
  target *that*, not clamp tuning (§3.4).
- Engine pin `51640ff` (verified `git -C ../milo-native-engine log`), flags registry
  entries for `RB3_HANDS_AUTHORED_REPOINT`/`RB3_HANDS_RESKIN`/`RB3_NO_SKIN_CLAMP` present
  (`NativeCompatFlags.classification.json:327,1376,1432`).

### 2.3 The standing tension R1 must break (why the decision cannot be made today)

Arm S measured `own` ≈ B (matrix-relative 3.1°, count(>5°)=0, ALL 1038 freshness blocks)
— yet the Wave-16 cell (whose correctness follows from exactly that premise) tears at
animated poses. These are only jointly consistent if `own`'s **animated inter-bone** poses
leave the ≈B basis in a way static freshness captures don't sample — or if the defect is
downstream of bone worlds entirely (palette inputs, authored-bind platform delta). Native
probes cannot distinguish these (that ambiguity IS the last three waves). R1's
two-adjacent-bone relative-pose diff vs real Wii is the designed discriminator.

### 2.4 THE R1 ARTIFACT CONTRACT (what R5 gates on — binding)

R5 runs only when `execution/R1-DOLPHIN/evidence/` contains, committed:

1. **Joined inter-bone delta table** (PLAN-R1 §3.7 JSON + human table): per
   `(member, clip, frame)` — adjacent-pair deltas `angle(D_wii·inv(D_native))` +
   translation deltas for BOTH hand chains (`wrist→hand`, `hand→finger01`, `01→02`,
   `02→03`), wrist/forearm anchor rows, per-member AND per-gender rows.
2. **Frame coverage across ARTICULATED poses** — ≥5 distinct matched frames per clip
   spanning real finger articulation, and (strongly preferred) R1-M4 gameplay-burst
   frames covering the Wave-16 burst_08/burst_12 pose class. **Hub-idle-only data is a
   degraded input**: the tear is invisible at rest (Wave 16's central lesson — every
   rest-coherence gate passed while the visual failed). If only M2 hub data exists, R5's
   M1 first checks articulation range (max per-pair rotation swing over frames ≥ 15°);
   below that, R5 formally requests R1-M4 and pauses. This is the pre-registered
   "R5 asks" trigger PLAN-R1 M4 names.
3. **Noise band** from R1-G3 (multi-pause static-idle inter-bone delta spread) → `ε_noise`.
4. **G5 separation record**: native default arm vs `RB3_HANDS_AUTHORED_REPOINT=1` arm are
   distinguishable through the joined metric (known-good/known-bad separation, OPTIONS
   §4.3). If G5 showed no separation, the metric may not decide R5 — bounce to R1.
5. **Authored-bind inter-bone tables** both platforms (`Dbind_wii` vs `Dbind_360`) with
   build provenance labels (`bank8_debug_dol` vs `retail_dol`).

Missing any of 1-4 → R5 returns the artifact to R1 with the gap named; it does not
improvise.

---

## 3. DESIGN — the decision procedure and both endgame branches

### 3.1 Pre-registered thresholds (fixed NOW, before R1's numbers exist)

Registered in this plan so the decision is mechanical, not narrative (OPTIONS §4.3/§4.6):

- `ε = max(3°, 2·ε_noise)` — per-pair "agrees with Wii" bound (3° floor = arm-S coherence
  scale; ε_noise from contract item 3).
- **DIVERGENT** pair: median-over-frames delta > 5° AND > 3·ε_noise on that pair.
- **CONSTANT** divergence: per-pair delta std-dev over the matched frames
  < max(2°, ε_noise) while the median stays DIVERGENT; else **TIME-VARYING**.
- Population rule: verdict per gender per hand chain; ≥2 finger pairs must agree to call
  a branch (single-pair signals are flagged, not decisive); wrist/forearm anchor rows must
  read ≤ ε for the finger verdict to be interpretable (else the divergence is upstream of
  the hand — different work item, report as GT-U upstream).

### 3.2 The branch table (the deliverable of the decision lane)

| R1 shows (finger pairs, anchors ≤ ε) | Verdict | Endgame |
|---|---|---|
| **GT-A**: all pairs ≤ ε across articulated frames — native bones ARE Wii-faithful | Tear is DOWNSTREAM of bone worlds | Reskin UNJUSTIFIED (it would deform correct geometry against correct bones). One bounded forensic step (§3.5); if that names no concrete fixable input in-budget → CLOSURE (§3.4) |
| **GT-B**: ≥2 pairs DIVERGENT and CONSTANT | Native animates a differently-based skeleton, consistently — the §0 Step-3 condition HOLDS | FIX branch: B1 anim-basis source correction (preferred) with B2 true reskin as fallback (§3.3) |
| **GT-C**: pairs DIVERGENT and TIME-VARYING | Animation/channel decode defect | Neither reskin nor closure: open a NEW scoped item (channel-decode forensics vs R1 timeline); R5 re-prices and reports. Reskin would yield coherent-but-wrongly-posed hands; closure is premature with a root cause now instrument-visible |
| **GT-U**: anchors (wrist/forearm) themselves DIVERGENT | Defect upstream of hands | Redirect to arm/torso chain item; hands verdict deferred (likely resolves for free) |
| **GT-D**: R1 NO-GO / contract unmet after bounce | No ground truth obtainable in budget | CLOSURE (§3.4) — the evidence-based variant, honestly labeled "closed without ground truth" |

Bind-delta side-table (contract item 5): if `Dbind_wii ≠ Dbind_360` beyond ε on hand
pairs, record it in the verdict — under GT-A it becomes the PRIME suspect for §3.5; under
GT-B it feeds the B1 correction derivation.

### 3.3 FIX branch (GT-B): two designs, priced, in preference order

**B1 — anim-basis source correction (preferred; root-cause side).**
The CONSTANT per-pair divergence means `own`'s bones carry a constant per-bone LOCAL basis
error `C_i` (constant inter-bone world deltas ⇒ constant local error — a world-constant
error would not survive chain propagation). Correct it where poses are applied: fold
`C_i^{-1}` (derived from R1's measured deltas, hand chain only, per gender) into the
bone's local rest at pose-apply time (`CharBonesMeshes::PoseMeshes` level or the bone's
local rest seed — exact seam chosen in-lane; candidates are the deform-clip pose path
`src/system/char/CharBonesMeshes.cpp` and the `own` bone Trans local). Properties:
fixes ALL meshes bound to those bones (hands_naked, gloves, fingernails, per gender) with
zero geometry mutation, zero weight machinery; decisive gate = R1 re-run shows the
divergent pairs collapse ≤ ε. Flag-first `RB3_HANDS_ANIMBASIS`, default-OFF; data table
(per-bone `C_i`) committed as evidence with its derivation script.
Failure mode: the error is NOT expressible per-bone-local (e.g. hierarchy/parenting
differs between native `own` and Wii skeleton) — detectable in-lane because the derived
`C_i` won't reproduce the measured world deltas when re-composed (self-check before any
engine edit); then fall to B2.

**B2 — TRUE reskin (fallback; the item's title option).**
New code (confirmed: `RndMeshDeform::Reskin` is positions-only and exo_-gated —
§2.2), band-side in `BandCharacter.cpp` next to the W14 artifact (same surface: mutate
`Vert::pos/norm` and now also `boneWeights/boneIndices`, then `Sync`), flag-first
`RB3_HANDS_TRUE_RESKIN`, default-OFF, appendage scope, gender-split by construction.

Math (per §0 Step 2, with the ground-truth anchor):
```
Ownref_i = Bbind_i · C_i            (C_i from R1's measured constant divergence;
                                     NEVER a runtime capture — §0 Step-3 condition)
M_i  = A_i · Ownref_i               (bind → ownref map, char space)
p'_v = p_v · (Σ_i w_{v,i} M_i)/(Σ w)     (blended re-pose; normals per MeshDeform.cpp:369-395)
A'_i = meshWorld · inv(Ownref_i)    (new offsets; must SKIP the :1775-class rebake —
                                     reuse the Wave-16 rebake-skip machinery :1844)
weights: KEEP authored w_{v,i} when both skeletons expose the same named bone with joint
origin within 1u (Tier-2 EXACT measured 0.33/0.04u ⇒ expected for hands_naked);
RE-DERIVE only for verts whose 4-bone support differs:
  - referenced bone name absent in own OR joint origin moved > 1u:
      reassign that weight share to the nearest present ancestor in the chain,
      renormalize to Σw=1 (the 1/255-quantized format tolerance ±1 LSB), log per-vert;
      gate: reassigned-vert count == 0 for hands_naked (same 38/40 name sets expected;
      a nonzero count is a finding, not a silent fallback)
```
Spelled-out failure modes (each with its in-lane detection):
1. **Anchor poisoning** (the saga's recurring death): `Ownref` must come from the R1
   table, never `NativeCharSpaceRestXfm` at SyncObjects (that is seed-R = W14) and never
   a play-time capture (≈B = Wave-16 cell). Guard: assert
   `angle(Bbind_i·inv(Ownref_i))` equals the R1-measured `C_i` per bone (loud abort on
   ~87° = seed signature).
2. **Support mismatch** (the item's named concern): covered by the weight rule above;
   female (40-bone) vs male (38-bone) meshes each re-bind against their OWN gender's
   `own` skeleton — never a shared anchor (the SHELL_FIX lesson).
3. **Shape drift**: re-binding bends the authored hand by up to the ~6-35° joint deltas —
   visible pose change vs authored art, coherent by construction. Under GT-B this is the
   CORRECT shape (it is what animates); the E1 visual gate adjudicates.
4. **Mesh churn**: outfit changes and progressive-sharpen churn-recreate meshes — latch by
   mesh pointer set + re-apply on recreation (W14 RISK-2 precedent; add a sharpen-churn
   test since SHRP recreation postdates W14).
5. **LBS residual**: candy-wrapper-class blend error remains at extreme articulation —
   ordinary, accepted, distinguishable from tears in E1.

Decisive gates for either fix (§5 G-F*): ground-truth first, visual second, native
numerics third — inverting the saga's failed ordering.

### 3.4 CLOSURE branch (GT-A-unfixable or GT-D): what "polished" means

Facts to design against (verified §2.2): band hands are NOT clamped today (rebound-skip);
the shipped visual residual is the coherent displaced "ceiling hand" + wrist spike-webbing
at some animated poses; `RB3_NO_SKIN_CLAMP` tuning is therefore NOT the closure lever for
hands (it never touches them — the record's shorthand corrected).

Closure package (0.5 lane-wave, all-or-nothing per sub-item):
1. **Optional mitigation polish — "mitten fallback" (flag-first `RB3_HANDS_MITTEN`,
   default decided by E1):** render-layer, hands-scoped: per finger bone, compare the
   composed skin `A_i·O_i(t)` against the wrist bone's rigid transform in the wrist frame;
   past a threshold (the displacement/tear signature — calibrated from the Wave-16
   evidence frames), blend that bone's palette entry toward wrist-rigid. Degrades finger
   articulation at exactly the poses that today tear/displace; hand stays attached and
   moving. Honest classification: workaround. Explicitly NOT an offset bake (render-side
   palette blend, no anchor capture). E1 gate on the burst_08/12 frames + a
   no-regression check on coherent frames.
2. **Accepted-residual statement** in the ledger + `classification.json`: "hands may
   articulate wrongly / appear displaced at extreme animated poses [or: degrade to
   rigid-hand under MITTEN]; most visible in close-up cams; no mesh tearing" — with the
   R1 evidence (or the GT-D no-ground-truth label) attached.
3. **Record hygiene:** family ledger closed; the two default-OFF dead-cell flags'
   registry entries updated to point at the closing verdict; the stale
   "milo-trace is a stub" claim in HANDS-FIX/STATUS.md annotated (corrected by PLAN-R1 §0).

### 3.5 GT-A bounded forensic step (one lane, hard-boxed)

If bones match Wii, the defect is in palette INPUTS. R1's dump contains everything needed
offline: recompute native palettes `A_i·O_i(t)` and Wii palettes `Aw_i·Lw_i(t)` from the
committed dumps, diff per bone per frame. Localizes to: authored-bind platform delta
(then: data fix — use/derive Wii-side binds), meshWorld factor, or per-bone offset
corruption in-flight (then: targeted code fix). Offline analysis only, no new captures; if
it does not name a concrete fixable input in one lane → CLOSURE per §3.2. This step
exists so GT-A does not silently become an unbounded hunt.

---

## 4. MILESTONES

**M0 — contract check (hours, in the decision lane's first step).** Verify §2.4 items 1-5
against `execution/R1-DOLPHIN/evidence/`. Articulation-range check (contract item 2). Exit:
proceed / bounce-to-R1 (named gap) / formally request R1-M4.

**M1 — the decision (cheapest decisive step; ≤0.5 lane-wave, offline).** Apply §3.1
thresholds to R1's table mechanically (a ~100-line script over the JSON, committed with the
verdict); produce `execution/R5-ENDGAME/VERDICT.md` naming GT-A/B/C/U/D with the
per-pair/per-gender evidence rows inlined. Go/no-go exit: a branch is named by the
pre-registered rules alone. If the numbers land ambiguous (between thresholds), the verdict
is "GT-C-indeterminate → re-price," NOT a judgment call — that is the fail-safe direction.
*This is the whole risk-retirement: every prior wave died on diagnosis, and M1 makes the
diagnosis a table lookup against external ground truth.*

**M2 — branch execution.**
- GT-B → B1 lane (derive `C_i`, self-check recomposition, flag-first fix, gates G-F1..4);
  B2 only on B1's named failure mode. Exit: gates green → flip package per campaign SOP,
  or refutation documented (and then CLOSURE per §3.2 — GT-B refuted-in-implementation
  falls back to closure, not to a new diagnosis loop).
- GT-A → §3.5 forensic lane (one lane hard box) → targeted fix lane or CLOSURE.
- GT-C/GT-U → re-price memo + new scoped item; R5 itself closes as "redirected."
- GT-D → CLOSURE package §3.4.

**M3 — close-out.** Ledger + registry + ROADMAP row updated; evidence committed under
`execution/R5-ENDGAME/evidence/`; MEMORY.md hands entries updated to the terminal state.

---

## 5. GATES (each with its fail-red demonstration)

| Gate | PASS criterion | Fail-red demo (shown RED once, committed) |
|---|---|---|
| G-D1 decision mechanicalness | M1 verdict reproduced by the committed script from the R1 JSON alone | run the script on a frame-shuffled/pair-permuted copy of the table → must output GT-C-indeterminate, never a confident branch |
| G-D2 metric validity inherited | R1's G5 separation record present and positive | (inherited fail-red: R1-G5's own known-bad arm; R5 refuses to run without it) |
| G-F1 ground-truth collapse (fix branches) | re-run R1 diff with flag-ON: previously DIVERGENT pairs read ≤ ε, anchors unchanged | run the same diff on the Wave-16 `RB3_HANDS_AUTHORED_REPOINT=1` arm (known-bad): must stay DIVERGENT/tear-signature |
| G-F2 visual E1 | matched fixed-clock burst_08/burst_12 frames: ceiling-hand AND spike-fan morphology GONE, both genders, gloves+nails non-regressing | the flag-OFF arm at the same frames IS the red baseline (HANDS-FIX evidence protocol) |
| G-F3 native numerics (tertiary) | Tier-1 count(>5°)==0 gender-split; Tier-2 EXACT ≤1u; guard-DROP census 0; crowd oracle untouched | `RB3_HANDS_ATTACH_PERTURB` shipped fail-red pattern (0.15 rad → ≈8.6° reading) |
| G-F4 faithfulness hygiene | flag-OFF byte-identical (drawlog-792); Wii build untouched; B2-only: reassigned-vert count == 0 for hands_naked | drawlog diff with flag forced ON is the red demo; B2: point the weight-rederivation at a mesh with a known-absent bone name → count > 0 reported loudly, not silently reassigned |
| G-C1 mitten (closure, if pursued) | E1 on burst frames: no tear/displacement flag-ON; no articulation loss on known-coherent frames | set threshold to 0 → all fingers rigid → articulation-loss visible in E1 + probe (the red demo), restore calibrated value |
| G-C2 closure honesty | residual statement + evidence links land in ledger/registry; no "fixed" language | n/a (doc gate; reviewer checks the residual is stated in user-visible terms) |

Process lints riding along (OPTIONS §4): matrix-relative + pointer-verified everywhere
(§4.1); gender/mesh split on every table (§4.2); no unvalidated oracle gates (§4.3 — wext
stays descriptive); evidence committed under `execution/R5-ENDGAME/evidence/` (§4.7); flag
hit-counts on any negative fix result (§4.8).

---

## 6. RISKS (honest)

1. **R1 under-delivers articulation** (hub idles only, fingers barely move). Mitigation:
   contract item 2 + the M0 articulation check + the pre-registered M4 request path. This
   is the likeliest schedule risk, not a validity risk.
2. **The GT branches are not exhaustive in practice** — e.g. divergence constant per CLIP
   but different across clips (per-clip basis error). Handled: that is GT-B with per-clip
   `C_i` tables; B1 still applies if the per-clip constants agree (else it degrades to
   GT-C). The M1 script computes both groupings.
3. **Wii-vs-360 content divergence breaks the join** (R1 risk 7 inherited): few matched
   clips → thin frame coverage → thresholds under-powered. Mitigation: M1 reports
   statistical power (frames × pairs per verdict row); below floor (≥5 frames × ≥2 pairs)
   → verdict is indeterminate, not forced.
4. **B1's seam is wrong-level** (error lives in skeleton hierarchy, not local rest).
   Detection is designed-in (recomposition self-check BEFORE edits); fallback B2 is fully
   specified. Residual: B2's shape drift is user-visible and could fail E1 on aesthetic
   grounds even while ground-truth-correct — then closure inherits a better mitigation
   (the reskin flag itself, default-OFF, documented).
5. **Retail-DOL ground truth** (R1 M1-B fallback): retail body ≠ Bank-8 — a behavioral
   hands-anim delta between banks is conceivable. Accepted with labeling (PLAN-R1 risk 1);
   R5's verdict records the provenance and the residual doubt.
6. **Plan-invalidating premise:** if R1's G5 shows the inter-bone metric CANNOT separate
   the known-good/known-bad native arms, the entire decision procedure has no instrument;
   R5 must not proceed on it (G-D2). The fallback is honest GT-D closure — stated now so
   nobody "finds a way" later.
7. **Two-repo coupling** (B1/B2 may touch engine + rb3): standard pin-bump discipline
   (engine commit first, `MILO_ENGINE_PIN` bump in matching rb3 commit); B2 is designed
   band-side-only to avoid this where possible.

---

## 7. COST (agent-waves) + WHAT IT UNBLOCKS

- **M0+M1 (decision):** 0.5 Opus lane-wave, offline, after R1-M3 (+M4 if requested).
  Matches the ROADMAP row "decision ≤1 wave after R1."
- **M2 by branch:** GT-B/B1 ≈ 1 lane-wave; B2 fallback +0.5-1 (weights machinery + churn
  tests); GT-A forensic ≈ 0.5-1 hard-boxed; GT-C/GT-U re-price memo ≈ 0.25 (fix itself is
  a new priced item); GT-D closure ≈ 0.5 (0.25 without the mitten).
- **Prior-weighted expectation** (stated as priors, not promises — GT-B 40% / GT-A 25% /
  GT-C+U 20% / GT-D 15%): ≈ 1.3-1.6 lane-waves total beyond R1.
- **Unblocks:** the flagship visual bug's terminal state either way; frees the E1/visual
  review budget the hands family has consumed for ~9 waves; B1 (if taken) hardens the
  whole skeleton-animation basis for every future char item (W2.4 BandPatchMesh, 4→8
  lights char lighting interplay); the decision-procedure pattern (pre-registered
  thresholds against external ground truth) becomes the template R2's oracle-validation
  harness enforces going forward.
