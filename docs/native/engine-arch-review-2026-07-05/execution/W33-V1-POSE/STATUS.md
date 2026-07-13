# W33-V1-POSE — STATUS

Lane 2, Wave 33. Base SHA `6186706e`, engine pin `2ea8e34`. Outcome: **RECHARTER**
(charter's set_play prime-suspect refuted; real mechanism named; no fix landable
in-fence).

## Acceptance quoted verbatim (kickoff Lane 2 as amended by CA1/CA6/CA8)

> **STEP 0 — discriminators, checkpointed BEFORE any fix:**
> (i) Reproduce V1 at pinned songMs. A/B control per CA1 (verbatim): "The W31
> set_play fix is UNCONDITIONAL (commit a3916764; no flag, no HX_NATIVE gate;
> ledger carries only the read-only probe RB3_SETPLAY_PROBE). A/B control =
> throwaway worktree (tools/setup-worktree.sh) with the 5 SYNC_PROP_SET intensity
> sites in src/system/bandobj/BandDirector.cpp (~:2140-2153;
> {bass,drum,guitar,mic,keyboard}_intensity) locally re-swapped to the pre-W31
> SendMessage(_val.Sym(), '<inst>') order — never committed, worktree deleted
> after. V1 vanishing under the re-swap = set_play-exposed; persisting = other
> layer."
> (ii) Name the offending clip(s)+bones ... a pose census at the bad frame: which
> characters, which clip playing, which bones wrong — wrong DATA (clip content)
> vs wrong TRANSFORM (apply path)?
> (iii) Basis test: if wrong-transform, is the signature the rotation-basis class
> (axis-swap/90°) or something else (bind pose, mirroring, parent chain)?
> THEN one lever at the named layer, default-OFF unless a plain decomp-divergence
> fix (state which). A/B = songMs-matched screenshot pairs on BOTH visual-pass
> songs showing natural poses, PLUS no regression of W31's set_play acceptance
> (rhythm/solo clips still play — rerun its census with RB3_SETPLAY_PROBE).
>
> **FAMILY-STOP (binding):** the SKEL rotation-basis family carries a binding STOP
> ... You may reopen it ONLY if STEP-0(iii) positively names rotation-basis
> (signature: axis-swapped/90°-rotated limb sets matching the family's prior
> evidence), documenting the new-evidence justification in PLAN.md. Otherwise fix
> at whatever layer STEP-0 names and do NOT touch SKEL-family code.
>
> **GATES:** batch_objdiff touched units baseline-exact-or-improved (your owned TU
> baseline per CA6: main/system/bandobj/BandCharacter 99.67018); drawlog per CA2
> (canonical-order 792); rb3-tests per CA3; boot A/B flag-ON if a flag lands.
> Bounds: ≤8 boot runs.

## Self-grade against the quoted text

| Acceptance clause | Verdict | Evidence |
|---|---|---|
| STEP-0(i) reproduce + CA1 A/B re-swap | **DONE** | V1 reproduced in W31 evidence; local render black (env/V4) so used bone-dump. CA1 re-swap built in worktree; performance-clip dispatch ON=64 / OFF=0; **V1 persists** (vignette maxRatio 4.2 == 4.2, world-Y 194.1 == 194.1) → **"persisting = other layer"**. `evidence/raw/ca1_ab_anat_comparison.txt` |
| STEP-0(ii) clips+bones, DATA vs TRANSFORM | **DONE** | Clip DATA correct (authored stand_rhythm/sing). Severe detach = `clipType=vignette` clips player2_f/player1_f/player3_m/player3_f, upperArm ratio 4.2, world-Y +194; performance clips ≤1.5. Parentage intact → **wrong TRANSFORM**. `evidence/raw/run5-anat_*.gz`, `grep_c_tables.txt` |
| STEP-0(iii) basis test | **DONE — positively names rotation-basis** | Distal detach/fling with intact parentage = the seed-R rotation-basis fling signature (hands family's ~87.3° seed-R), now full-body via vignette clips. New-evidence justification (closure premise "band CharBones never animate" now void) in PLAN.md. |
| "THEN one lever at the named layer" | **NO fix (correct)** | Named layer = CharBones pose-pipeline basis = **out of lane fence** (engine) + FAMILY EXHAUSTED (15 waves) + suppressing vignettes/undoing W31 both unfaithful. No in-fence faithful lever exists. Honest RECHARTER > gamed fix. |
| A/B screenshot pairs, both songs, natural poses | **BLOCKED (env)** | Gameplay 3D renders black (`max=0`) in this environment (V4/GPU-fallback, out of scope). Substituted the render-independent bone-anatomy A/B (CA1) — the charter-prescribed discriminator. |
| No regression of W31 set_play census | **HELD** | run1 RB3_SETPLAY_PROBE: 5 SETPLAY_KEYS / 12 SETPLAY_SEND; run5 rhythm/sing clips dispatched (64). set_play untouched. |
| FAMILY-STOP discipline | **HELD** | No SKEL-family code touched. Reopen justified + documented; disposition is recharter, not an in-fence SKEL edit. |
| Gates | **baseline-by-construction** | **Zero source files changed** (CA1 re-swap was worktree-only, deleted). No touched units → batch_objdiff/drawlog/rb3-tests all at HEAD baseline trivially. Owned-TU BandCharacter untouched (dirty BandCharDesc.cpp/rb3_platform_native.cpp in the tree are OTHER lanes', not mine). Bounds: **6 boot runs** used (≤8). |

## Bottom line for the coordinator

- **REFUTED:** V1 is NOT the W31 set_play body-performance path. CA1 A/B proves
  the severe explosion is set_play-independent (identical with performance
  dispatch fully disabled).
- **NAMED:** severe V1 = the on-stage band char playing a **gameplay vignette
  body clip** (`player2_f` etc.) that detonates via the **seed-R rotation-basis**
  error at full-body scale (upperArm 4.2× stretch, world-Y +194), parentage
  intact. Same closed SKEL/CharBones-basis family, new full-body surface.
- **NO FIX** landed — the faithful fix is the closed/exhausted CharBones basis
  pipeline (engine, out of this lane's fence). Recommended Wave-34 recharter:
  re-scope V1 to the CharBones-basis family with this evidence (and, secondarily,
  the V4 gameplay-render blackout that blocks visual gating).

## Files
- `PLAN.md` (STEP-0 record + FAMILY-STOP reopen justification)
- `evidence/lane2-step0.json`, `evidence/lane2-ca1-abswap.json` (checkpoints)
- `evidence/raw/ca1_ab_anat_comparison.txt`, `grep_c_tables.txt`
- `evidence/raw/run1-census / run5-anat_setplayON / run6-reswap_setplayOFF .engine.log.gz`
