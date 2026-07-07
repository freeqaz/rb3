# W2.8g — Lane B, STAGE B-S1 — STATUS

## B-S1 — done (Opus, Lane B) — VERDICT: **SPACE / composition axis (mesh SHELL); DECODE refuted**

**Charter:** WAVE12_KICKOFF Lane B S1 + ACCEPTANCE A5/A6/A7. BUILD Instrument B (per-vertex shell
invariant) + the A6 space-vs-decode axis discrimination. **No fix this stage.** Engine pin `146fd19`
at start; build dir `native/build-agent-W2.8g` (clang) + engine `build-agent-W2.8g-tests`.

### Verdict in one line
The hands smear is a **rigid rotation of coherent per-bone sub-shells about their bones** — the
**authored-vertex-to-offset composition (SPACE) axis** — proven rest-free: on `hands_naked` the worst-
flung dominant bone's composed palette is a **clean rigid rotation (orthoResid ≈ 0.0002)** and its
sub-shell is transported **isometrically (isoDistort ≈ 0.0000)** — i.e. all verts sharing a dominant
bone move by **ONE rigid rotation, not scatter** (task-4 answer). The **A6 DECODE alternative
(weight / index / V24 decode) is REFUTED**: a decode/weight tear breaks isometry (oracle: isoDistort
0.231), but the hands read isoDistort ≈ 0. The clean-body control (`greaserjacket_resource`) reads
shellMax 1–4u vs hands 20–227u, so the anomaly is specific to the appendage meshes.

### Line ranges re-declared (task 1) — engine HEAD 146fd19, TU 5,775 lines
- wext CPU 4-bone blend (`s(v)` mirror): `Rnd_Wgpu_RB3.cpp:4351-4394`.
- RB3_DUALSKIN_PROBE: `:4453-4735`. RB3_HANDS_ATTACH_PROBE (Tier-1/2, freshness rest): `:4736-4864`.
- **Instrument B inserted `:4872-4966`** (inside the `haMatch` scope, after the Tier-2 log).

### Instrument B (task 2) — built, `RB3_HANDS_INSTR_B`, render-inert, doubly getenv-gated
- `s(v)`   = as-drawn CPU 4-bone blend (`bones.bones[bi] = off_b·liveW_b`, the wext blend).
- `ŝ(v)`   = authored shell transported by ONLY coherent bone motion,
  `Σ_k w_k · v · inverse(restW_bk) · liveW_bk`, restW from Tier-1's pointer-identity freshness capture.
- Reports per-mesh worst/mean `‖s−ŝ‖` (shellMax/shellMean) co-sampled with wext + rest-free
  discriminators (below). Owner-tagged (band members captured separately). Clean-body control runs
  when the selector includes `greaserjacket`.

**A7 result — the design-doc literal shell invariant is CONFOUNDED as a per-frame gate.**
`‖s−ŝ‖` co-varies with wext **+0.89** on the cleaner PARTDIFF settle but **−0.23** on GAMEPLAY, and
reads HUGE at near-rest (128u mean at LO-wext). Root cause: the freshness rest is **not the authored
per-member bind** — the hand smears from **frame 3** (wext 105.8u on the first drawn frame), so **no
clean-rest frame is ever captured**; `ŝ` then measures deviation from an arbitrary smeared pose. The
literal shell invariant therefore JOINS Tier-2 as A7-confounded and is NOT the trustworthy fix gate.
Its **absolute level still discriminates** (hands 20–227u vs body 1–4u). The trustworthy gate is the
**rest-FREE** pair below + wext collapse.

### Rest-free axis discriminators (tasks 3,4) — the decisive readings
Computed on the worst-flung dominant bone (the vertex with max shell error → its dominant bone):
- **orthoResid** = ‖RᵀR − I‖_F of the composed palette 3×3. hands **0.0000/0.0002/0.0004**,
  body 0.0001 → the palette is a **clean rigid rotation** (no shear/scale). Kills "V24 decode emits a
  corrupt/non-rigid matrix" and "weight-normalization distortion".
- **isoDistort** = mean |‖s(vᵢ)−s(vⱼ)‖ − ‖vᵢ−vⱼ‖|/‖vᵢ−vⱼ‖ over w>0.9 worst-bone vert pairs. hands
  **0.0000/0.0000/0.0001**, body 0.0000 → the sub-shell is transported **isometrically** ⇒ verts
  sharing a dominant bone move as **ONE rigid rotation** (task-4: RIGID, not scatter).
- **idxOK** (worst vert's dom bone == its spatially-nearest bone by `inv(off).v`): **0.00 on BOTH
  hands AND the known-correct clean body** ⇒ this metric is **CONFOUNDED** (bind origin `inv(off).v`
  is in the shared-magnet space, not mesh-vert space) → a **NULL discriminator**, reported honestly,
  **not used as evidence**.

Pre-registered branch mapping (task 3): the observed pattern is **shell error large + sub-shells
rigid** = the **SPACE/composition** branch. The "Instrument-B-GREEN-while-wext-RED ⇒ decode" branch
was **not** observed (shell error is large/RED on hands). SPACE is corroborated by the **Tier-1
uniform 87.3°** magnet-vs-own conjugation (`RB3_HANDS_ATTACH_PROBE`, B.S1/B.S2) and the **Tier-2
EXACT joint-attach ≈ 0** (no palette-slot mismap — a slot swap would dislocate joints, which
isoDistort/Tier-2 would catch).

### Composition oracle (task 5) — truth table validated on real+finger-scale verts
`milo-native-engine/tests/test_skin_golden.cpp :: SkinGolden.ShellInvariantAxisOracle` (real
hand-region bind verts, synthetic-finger fallback) + standalone
`evidence/shell_axis_oracle.cpp` (inline-math mirror, always runs). Result (`/tmp/shell_oracle`):

| mode | isoDistort | shellErr vs coherent |
|---|---|---|
| COHERENT (skin=liveW) | **0.00000** | 0 |
| **SPACE** (skin=R87·liveW) | **0.00000** (rigid) | **24.4u ≈ R·2sin(θ/2)=25.0u** |
| DECODE (per-vert wrong bone) | **0.23145** (torn) | — |

TRUTH-TABLE **PASS**: a SPACE conjugation is a single **rigid** rotation (isoDistort ≈ 0) that
**flings** the shell by R·sinθ; a DECODE corruption **tears** it (isoDistort ≫ 0). ⇒ the in-engine
hands reading (**isoDistort ≈ 0 + large shellMax**) is the **SPACE** signature, **not** DECODE.
DC3 source = corroboration only (unused here — the oracle is self-proving).

### Instrument A (GPU-vs-CPU readback) — predicted GREEN, not built (confirmatory only)
Per the B.S2 design, a ~200-LOC wgpu compute readback is confirmatory-only and predicted GREEN: the
CPU `wext` blend reproduces the full 106u smear with **zero GPU**, and the on-screen shard
(`W2.8f/evidence/bs2_gameplay_smear_ms2000.png`) appears exactly where/how the CPU predicts (worst
bone `bone_R-middlefinger03`) ⇒ the GPU faithfully mirrors the CPU. Logged as predicted GREEN; the
axis verdict does not depend on it. A RED readback would be the only thing to overturn SPACE.

### Numbers (gameplay + partdiff, `evidence/bs1_instrb_harness.py`, `/tmp/wave12-bs1c`)
| mesh (owner) | N | wext | shellMax | orthoResid | isoDistort | worst bone |
|---|---|---|---|---|---|---|
| hands_naked GAMEPLAY | 1815 | 31–106u | 20.7–227.8u | 0.0002 | 0.0000 | bone_R-middlefinger03 |
| hands_naked PARTDIFF | 258 | 53–106u | 20.7–37.2u (corr wext +0.89) | 0.0002 | 0.0000 | bone_R-middlefinger03 |
| greaserjacket (control) | 1146 | 12–54u | **1.1–4.0u** | 0.0001 | 0.0000 | bone_L-thumb01 |

### Gates / process
- **drawlog-golden flag-OFF = PASS (792)** (`--fixed-clock --canonical-order`, 275 known-residual
  within bound) on the committed binary — probe render-inert (doubly getenv-gated).
- **Oracle truth-table PASS** (rest-free discriminator validated: SPACE↔rigid, DECODE↔torn).
- Build clean (clang, `build-agent-W2.8g`). Instrument B flag-OFF byte-identical by construction
  (`if(getenv("RB3_HANDS_INSTR_B"))` inside `if(haMatch)` inside `if(RB3_HANDS_ATTACH_PROBE)`).
- classification.json append-only (`RB3_HANDS_INSTR_B`, probe); **NO gen.inc regen** (coordinator).
- All refuted-experiment flags UNSET in all arms; six shipped defaults untouched; left the uncommitted
  `FxSendNative.cpp` engine audio edit intact; staged only my own files under flock.

### The axis for Wave-12 S2 (named, with rest-free numbers)
**SPACE / composition** — the authored-vertex-to-offset composition (the skinned mesh SHELL). Each
per-member finger bone's inverse-bind offset is baked in the shared-magnet basis (~87° off the bone's
OWN rest), so the composed palette rigidly rotates each finger sub-shell about its bone; far-radius
finger verts fling by R·sinθ → the 95–106u wext / starburst-shard. The **S2 fix gate must keep
orthoResid + isoDistort ≈ 0** (palette stays rigid — no fix-by-hiding / no non-rigid clamp) **and
collapse wext** without freezing. **STOP-TRIPWIRE respected: no bind-side bake attempted** (this is
the same 87° own-rest signal Wave-9/10 refuted as a bind-side target — the S2 fix must operate at the
per-vertex shell / composition level, not re-bake off_b).

### Commits
- engine `<eng-sha>` (flock): `src/platform/Rnd_Wgpu_RB3.cpp` (Instrument B `:4872-4966`) +
  `tests/test_skin_golden.cpp` (ShellInvariantAxisOracle) +
  `src/platform/NativeCompatFlags.classification.json` (append-only `RB3_HANDS_INSTR_B`; NO gen.inc).
- rb3 `<rb3-sha>` (flock): `W2.8g/PLAN.md` + `W2.8g/STATUS.md` + `W2.8g/evidence/`.

Checkpoint: `/tmp/wave12-checkpoints/B-S1.json` (`verdict: SPACE_AXIS`).

---

## B-S2 — done (Opus, Lane B) — VERDICT: **BLOCKED** (measured 6th dead cell; NO fix landed)

**Charter:** WAVE12_KICKOFF Lane B S2 — implement the fix for the B-S1 NAMED axis (SPACE/
composition), flag-first default-OFF, under the BINDING STOP-TRIPWIRE (five bind-side bake classes
are dead; a 6th bake = STOP + report BLOCKED). Engine pin `146fd19`. Build `native/build-agent-W2.8g`.

### One-line verdict
The SPACE axis is real and correctly named, but its fix is **NOT reachable from Lane B's renderer/
composition + no-bake charter**: the ~87° smear is the **magnet-vs-per-member bone rest-basis gap**,
which is **irreducible with any single live bone**. The one untried single-bone cell was implemented,
measured, and **REGRESSES** (shard-at-rest) — the predicted 6th dead class. **No fix landed.**

### The mechanism (B-S1 SPACE verdict × W2.8e ground truth)
Each finger sub-shell is a clean **rigid** conjugation ~87° about its bone (B-S1: isoDistort≈0,
orthoResid≈0). W2.8e `RB3_APD_DIAG` established WHY, pose-independently:
- `own` = `Find(boneName)` = the **SHARED MAGNET** instance (`invOff` **IDENTICAL 106°** across two
  members with **distinct** 38/40-bone skeletons) — this is what **ANIMATES**.
- `bound` = `mesh->BoneTransAt(b)` = the **PER-MEMBER static** bone (char-rest **129° / 119°**) —
  correct authored basis, but **does not animate**.
- Gap `angle(off·restW)` = **87.3° / 68.8°**, pose-INDEPENDENT and asset-derivable.

Two single-bone cells were already dead before this stage:
| cell | live bone | rest basis | result |
|---|---|---|---|
| DEFAULT | own (magnet, animates) | own 106° | coherent@rest, **106°-basis fling SHARD** |
| RB3_APPENDAGE_ASSET_REBAKE (W2.8e, 5th dead) | bound (static) | bound 129° | correct basis but **FREEZE** |

### The one untried cell — implemented + measured (`RB3_HANDS_SHELL_FIX`, default-OFF)
The last permutation: **own-live (animates, no freeze) + bound-rest (129° authored basis)** — bind
appendage meshes to the animating magnet bone but bake `off = meshWorld·inv(NativeCharSpaceRestXfm(bound))`.
Implemented in `src/system/bandobj/BandCharacter.cpp` (`RebindHeadHandsAtRest`, new branch before the
asset-rebake block; flag read next to the other `sApd*` flags; added to `sApdAny`). Default-OFF,
settle-guarded, getenv-cached, HX_NATIVE.

**Prediction (from the 87° pose-independent gap):** at own's rest (106°),
`skin = meshWorld·inv(129°)·106° = meshWorld·R₈₇` → the shell is rotated 87° about the bone **at rest**
(shard-AT-rest). **CONFIRMED by A/B** (`evidence/bs2_shellfix_ab.py`, gameplay ceiling-forearm
sighting, `RB3_HANDS_ATTACH_PROBE=1`, N≈1400/arm, `/tmp/wave12-bs2ab`):

| metric | OFF (default) | ON (`RB3_HANDS_SHELL_FIX`) | gate |
|---|---|---|---|
| wext **min** | 34.8u | **51.0u ↑ (rest now sharded)** | — |
| wext mean | 68.9u | **82.4u ↑** | — |
| wext **max** | 105.8u | 106.6u (unchanged) | ≤60u → **FAIL** |
| Tier-2 exact max | 0.33u | **0.81u ↑ (worse)** | ≤1u |
| distinct wext (freeze) | 467 | 435 (not frozen) | ON>3 |

The ON screenshot (`evidence/ON_ms12000.png`) is a **catastrophic flesh-spike starburst** far worse
than OFF (`evidence/OFF_ms12000.png`) — the 87° now flung at rest AND animated. The cell is the
**measured 6th dead class**; it stays default-OFF, `class:workaround`, not-live, do-NOT-flip.

### Why every composition formulation reduces to this (the no-bake wall)
Any frame-consistent skin = `meshWorld·inverse(restW)·liveW`, coherent **iff** `restW` is the
animating bone's TRUE rest. But the animating bone IS the magnet (106°), whose rest genuinely differs
from the per-member authored rest (129°) by exactly the 87° that IS the smear. Closing it needs
either (a) **conjugation** — retarget own's motion into bound's frame (`= W2.8c`, dead, freezes), or
(b) a per-member **ANIMATING** instance carrying the authored rest — i.e. make `Find(boneName)`
resolve the per-member bone from **`skeleton_unshared.milo`** rather than the shared magnet. (b) is a
**loader / skeleton-merge / asset** fix, OUTSIDE Lane B's renderer-composition + no-off_b-bake scope.
No draw-time-available or rest-capture-available correction is per-bone-constant AND non-conjugating.

### RE-LANE recommendation (for the coordinator)
The faithful fix = **skeleton instancing**: the appendage meshes' bones must resolve to a per-member
animating instance whose rest == the authored `skeleton_unshared.milo` basis (129°), so the DEFAULT
composition (`own-live + own-rest`) becomes coherent with zero bind bake. That belongs in a
loader/`BandCharacter` skeleton-merge lane (cf. the `char-skinning-deform` torso rebind that fixed the
analogous **body** case by repointing outfit bones to the member's OWN animated skeleton) — NOT Lane B.

### Gates / process (B-S2)
- **wext ≤60u WITHOUT freezing:** **FAIL** (ON max 106.6u; min ROSE 34.8→51.0u = shard-at-rest). No
  fix landed → the fix gate is not claimed; this is a BLOCKED, not a pass.
- **Tier-2 joint-attach ≤1u:** OFF 0.33u / ON 0.81u (both ≤1u; ON worse — consistent with a rigid
  conjugation that keeps joints attached while flinging far verts).
- **drawlog-golden flag-OFF = PASS 792** (`--fixed-clock --canonical-order`, `build-agent-W2.8g`, 284
  known-residual within bound). Flag-OFF byte-identical by construction (getenv default-OFF; the
  `sApdAny` term is inert when the flag is unset).
- **No fix-by-hiding / no vertex clamp:** none used — the cell is a bind composition, not a clamp.
- **STOP-TRIPWIRE respected:** the cell degenerated into a (regressing) bind bake → STOPPED, reported
  BLOCKED, did NOT land it as anything but a documented default-OFF diagnostic.
- All refuted-experiment flags UNSET in all arms; six shipped defaults untouched; uncommitted
  `FxSendNative.cpp` engine audio edit left intact; staged only my own files under flock.
- classification.json append-only (`RB3_HANDS_SHELL_FIX`, class:workaround not-live); **NO gen.inc
  regen** (coordinator).

### E1 review artifacts (band screenshots)
`evidence/OFF_ms02000.png`, `evidence/OFF_ms12000.png` (before = current default, hand smear),
`evidence/ON_ms02000.png`, `evidence/ON_ms12000.png` (after = the regressing cell, flesh-spike
starburst). Full set + probe logs in `/tmp/wave12-bs2ab/`.

### Commits
- rb3 `<rb3-sha>` (flock): `src/system/bandobj/BandCharacter.cpp` (`RB3_HANDS_SHELL_FIX` branch,
  default-OFF) + `W2.8g/STATUS.md` + `W2.8g/PLAN.md` + `W2.8g/evidence/`.
- engine `<eng-sha>` (flock, classjson-lock): `NativeCompatFlags.classification.json` (append-only
  `RB3_HANDS_SHELL_FIX`; NO gen.inc regen).

Checkpoint: `/tmp/wave12-checkpoints/B-S2.json` (`verdict: BLOCKED`).
