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
