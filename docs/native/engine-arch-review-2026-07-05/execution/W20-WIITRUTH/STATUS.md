# Lane W — Wii load-truth (static) — STATUS

**Wave 20, Lane W (W20-WIITRUTH). AUDIT-ONLY — zero fixes, zero flips, zero pin bumps.**
Static pointer reads only; **NO animation claims** (A11 — the substrate's band CharClipDrivers=0
is known and irrelevant to binding topology; the frozen band pose IS the Wii-side basis B).

## HEADLINE

> **Do Wii hand meshes bind OWN_MEMBER or SHARED_ROOT?**
>
> **OWN_MEMBER.** On real Wii execution (Bank-8 patched-disc boot), every resolved
> hand/outfit-mesh bone binds to the member's OWN `char/main/skeleton_unshared.milo`
> instance. **SHARED_ROOT = 0 in every reachable state.** The shared `char/main/skeleton.milo`
> magnet EXISTS in the heap (exactly 1 resident instance) but receives **ZERO** bone bindings.

Confirmed across three reachable states + a determinism check, all G2-validated,
pointer-deref only (never name-based nearest-neighbor):

| state | OWN_MEMBER | SHARED_ROOT | UNRESOLVED | hand mesh present |
|---|---|---|---|---|
| gameplay/song-load (richest) | **60** | **0** | 0 | `hands_naked.mesh` ✓ |
| main_hub (clean, stable) | 32 | 0 | 2 | (torso lineup) |
| loading/preview | 32 | 0 | 2 | (torso lineup) |
| determinism (2× same frozen state) | identical | identical | identical | — |

**Topology (identical across states):** 4 BandCharacter instances · **1** shared
`skeleton.milo` heap dir instance (the magnet) · **5** `skeleton_unshared.milo` instances
(1 template/preload + 4 per-member). Bones bind to 3–4 DISTINCT per-member unshared
instances — never the single shared magnet.

## Why this matters (the native join, for the coordinator — Lane N joins row-for-row)

This is the fact native must reproduce. CHAR_SKINNING_DEFORM_INVESTIGATION.md (~:536-556)
records that NATIVE binds outfit meshes to the SHARED preloaded `skeleton.milo` magnet
(the shared skeleton wins the parse-time `FindObject` descent). **Wii does the opposite:
the per-member `skeleton_unshared.milo` is the name winner, so the hand meshes' bone refs
resolve OWN.** The divergence is therefore at the name-resolution/share layer — exactly the
"un-share `char/main/skeleton.milo` for the band" fix class labeled broad/high-risk in
2026-06-06 and shelved (VERDICT §1 (b)-at-load). Lane W supplies the Wii ground truth that
this class of fix is aiming at a REAL target: a per-member instance that both carries the
authored bind basis and is the animation-driven skeleton.

## A7 basis capture (for the A11 synthesis discharge — NOT a Lane W claim)

Per resolved bone slot the census JSONs carry the bound trans's **world + local packed
Transforms** (the Wii-side bind-pose basis B) and the mesh's authored `RndBone.mOffset`
(inverse-bind). These let the coordinator's A11 synthesis test what the Wii-loaded object
has that the VISUAL-refuted 8th cell (`inv(B)·L_own`) lacked — Lane W only DELIVERS the
matrices; it makes no articulation/animation claim about them (the substrate is static).

## Fail-reds (all fired correctly — instruments shown red AND green)

- **UNRESOLVED ≠ SHARED**: `mesh.1.mesh` (generic name, 40 ObjPtr elements whose targets
  carry no `bone_*` RndTransformable) → all 40 UNRESOLVED, no dir class fabricated. It is
  NOT a hand/torso mesh (outside the census filter; surfaced only in exploratory scan).
- **MESH_ABSENT loud**: outfit meshes present-by-name but with empty/unbound mBones at a
  given state (`male_placement_torso_*`, `gloves_resource.mesh` in some boots) are reported
  MESH_ABSENT, distinct from resolved-to-shared.
- **GENDER-GAP (real, reported)**: the Guest-profile lineup on this substrate is all
  male-count (≤39 bones); **no female (40-bone) member mesh was reachable**. The 38-vs-40
  gender census is therefore male-only here — flagged, not faked.

## Priced exhaustion — what was NOT obtained (honest blockers)

1. **Full-gameplay 38/40 per-member hand binding is WALLED.** The Bank-8 patched-disc boot
   stalls at a **black LOADING** screen when starting a song (headless), reproducing the
   V_findings gameplay wall (band performance-load is gated on a subsystem not running
   headless; CharClipDrivers=0 everywhere). Hand meshes appear only partially-bound in the
   menu/song-select preview (`hands_naked.mesh` at 6 bound bones, not the full 38). **This
   does not weaken the headline**: binding is fixed at PARSE-TIME (before gameplay,
   CHAR_SKINNING ~:536), so the OWN-vs-SHARED topology is complete and unambiguous from the
   reachable states — every reachable binding is OWN, none shared, in every state. The
   fully-bound 38-bone case was seen once (`drivinggloves_resource.1.mesh` → OWN) but the
   band-preview lineup varies per boot and is not deterministically reproducible at 38 bones.
2. **Female (40-bone) member**: not in the Guest-profile lineup; would need a saved band
   with a female member or a boot/mode that assembles one.

Neither blocker is a read/instrument gap — the reader G2-validates and is deterministic;
they are content/boot-state limits of the frozen substrate.

## Deliverables

- Tool: `milo-trace tools/wii_mesh_binding.py` (commit `ec5c55b`) — WiiMesh binding reader
  (`derive` / `census` / `determinism`).
- `evidence/wii_binding_{gameplay,main_hub,loading}.json` — A10-schema rows (per-slot,
  incl. A7 basis matrices), topology, class counts, mesh_absent, gender_gap.
- `evidence/wii_binding_table.md` — human-readable aggregate.
- `evidence/offset_derivation.md` — every derived offset + its validation + fail-red proofs.
- `evidence/state_*.png` — main_hub + the black-LOADING gameplay wall.
- `PLAN.md`, this `STATUS.md`. Checkpoint `/tmp/wave20-checkpoints/W.json`.

## Process-lint compliance

Pointer identity on every binding row (dir instance guest addrs quoted); instruments shown
red (UNRESOLVED / MESH_ABSENT / red-team `mesh.1.mesh`) AND green (G2 pass, determinism
identical); no unvalidated oracle promoted (UNRESOLVED never coerced to a class); no name-NN
matching (pointer-deref only); GENDER-GAP flagged not faked; instance-scoped (`-u
/tmp/dolphin-w20`, own Xvfb `:93`, pgid-only teardown — the concurrent `/tmp/r1b-user`
Dolphin untouched); NO default flips, NO pin bumps, NO engine/game-source edits (milo-trace
tool + evidence only). Lane W makes STATIC binding claims ONLY; GT-D closure + its
articulated-capture reopen condition untouched (A11).
