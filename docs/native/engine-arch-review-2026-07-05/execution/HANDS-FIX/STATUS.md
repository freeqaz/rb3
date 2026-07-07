# Lane HANDS-FIX (Wave-16 Lane F) — STATUS — VERDICT: **REFUTED by the decisive VISUAL gate**

Implemented the adjudicated never-measured cell (HANDS-ADJUDICATION/VERDICT.md §4–5)
flag-first, default-OFF (`RB3_HANDS_AUTHORED_REPOINT`), rb3 `BandCharacter.cpp` ONLY,
zero engine edits (pin `84ccb9e` untouched). **All numeric rest-coherence gates PASS
— including the female gate that killed SHELL_FIX — but the pre-registered VISUAL E1
gate FAILS: flag-ON produces WORSE finger-spike shards at animated poses than the
flag-OFF coherent "ceiling hand."** The cell is now MEASURED. Kept in-tree default-OFF
as the definitive 8th dead cell (campaign precedent: RESKIN / APPENDAGE_*); the
offset-bake fix class is now FULLY EXHAUSTED.

Built + measured in the isolated worktree `.claude/worktrees/wave16-handsfix`
(main `build-native` had concurrent uncommitted edits from Lane T / session-trace).

---

## What was built (implementation is CORRECT — the cell is what failed)

`BandCharacter::RebindHeadHandsAtRest`, appendage (`apdMesh`) scope only, flag-gated:
- **pass A** (`:1543`): resolve `own = Find(bound->Name())` as today; `owns[b]=own`,
  `apply[b]=1`, NO rest capture. own==bound → no-op repoint, still `apply=1` (matches
  default). clipPlaying → NO guard (nothing to poison — we capture no pose).
- **pass B** (`:1817`): `SetBone(b, own, false)` repoint, then **SKIP** the `:1775`
  `Multiply(meshWorld, invRest, BoneOffsetAt(b))` overwrite — KEEP the authored
  inverse-bind. `mNativeBonesRebound=true` (clamp/ SKEL_REBAKE exempt) unchanged.

The flag is added to `sApdAny` so `apdMesh` scope opens for it. flag-OFF: `sApdAny`
unchanged → `apdMesh=false` → both arms inert → **byte-identical** (drawlog-792 PASS).

Also **A7**: corrected the INVERTED bound/own comment block (old `:1558-1572`, now the
`W2.8e MATCH-path appendage asset rebake` header + the two inline labels): `bound` =
mesh->BoneTransAt is the SHARED, STATIC embedded-bind instance (authored basis B,
~129°); `own` = Find(name) is the PER-MEMBER animating bone (seed rest R ~106°, but
sits ≈B at play). The original text had these swapped — one of the saga's
scalar-vs-matrix premise slips (VERDICT §0). Comment-only, no code behavior change.

## Gates (protocol: `keyboard-to-gameplay.py --song-downs 3 --game-burst 15
## --burst-interval 0.3`, `RB3_FIXED_CLOCK=1 RB3_HANDS_ATTACH_PROBE=1 IK_SHARD_VERT='*'`,
## gender-split nb=38 male / nb=40 female — arm-C control reproduced the adjudication)

| gate | flag-OFF | flag-ON | verdict |
|---|---|---|---|
| **Tier-1 count(>5°)==0 — male hands_naked** | 87.3° count=34 | **3.1° count=0 / 992 blk** | **PASS** |
| **Tier-1 count(>5°)==0 — female hands_naked** (A6) | 42.6° count=36 | **3.1° count=0 / 502 blk** | **PASS** |
| Tier-2 EXACT ≤1u | 0.33/0.04u | 0.34u male / 0.03u female | PASS |
| gloves_resource (female) | 68.8° count=40 | 3.1° count=0 | PASS (improved) |
| gloves.1 / gloves_skin.2 | 60.2°/38.9° | 1.1°/3.1° count=0 | PASS (improved) |
| fingernails | ~170° Tier2 0.0u | ~178° Tier2 0.0u | non-regressing (0u = no shard) |
| drawlog-792 flag-OFF byte-identical | — | 792 draws == golden | PASS |
| A4 offset provenance | — | off·own_rest=87.2° (§2 pristine sig); rebound by frame 3 | PASS |
| crowd oracle + guard-DROP census | — | untouched (band appendages only; 100% rebound) | unchanged |
| wext (DESCRIPTIVE only) | M 75.1 / F 60.5 | M 87.9 / F 65.0 | rose as predicted — NOT a gate |
| **VISUAL E1 — ceiling-hand / spike-web GONE** | coherent ceiling-hand | **torn finger-spikes (WORSE)** | **FAIL** |

### A6 female — the SHELL_FIX confound is BROKEN
28.9° is the SHELL_FIX FAILURE signature (it forced the SHARED male-bind B onto the
female). This cell keeps the female mesh's OWN authored offset → female Tier-1 reads
**3.1° count(>5°)==0**, IDENTICAL to male. `evidence/offset_basis_derivation.py`
(female-extended) records the axis. **A6 female NUMERIC gate: PASS.** So the fix is
strictly BETTER than SHELL_FIX on per-gender/per-asset basis — yet the shard persists.

### A4 provenance (SKEL_REBAKE mutation window)
`[HANDS_REPOINT]` reads off·own_rest = **87.2°** (male mid03) = the §2 B-vs-seed-R
relative rotation — the signature of an UN-rebaked authored offset (a
SKEL_REBAKE-mutated `off = meshWorld·inv(ownWorld)` would read ~0 here). The mesh is
flagged `mNativeBonesRebound` by frame 3, after which SKEL_REBAKE (`Rnd_Wgpu_RB3.cpp:3545`,
gated on `!mNativeBonesRebound`) skips it. Pristine provenance confirmed IN-SESSION.

## The decisive VISUAL failure (matched-frame zoom — `evidence/matched_zoom_burst08_burst12.png`)

At MATCHED fixed-clock frames (identical body stance + score):
- **burst_08:** flag-OFF = a COHERENT hand (5 natural fingers, the displaced
  "ceiling hand"); flag-ON = fingers TORN into thin elongated strands.
- **burst_12:** flag-OFF = guitarist standing coherently; flag-ON = the right hand
  EXPLODES into a green triangular spike-fan (degenerate torn-triangle mesh).

flag-ON is a **regression**, not a fix, at animated poses — despite Tier-1 = 3.1°.

## Root cause (empirically confirmed — vindicates SKEL seam-B)

Tier-1 rest-coherence (`off·restW ≈ I`, restW captured at the repoint frame where
own ≈ B) PASSES both genders, but it is a STATIC per-bone check — it does NOT capture
ANIMATED multi-bone blend integrity. The authored verts+weights encode the
**SHARED-bind INTER-bone geometry**; repointing each bone to the per-member `own`
(whose ANIMATED inter-bone poses differ — the APD mixed-sign ±6–35° per-bone gaps)
**tears the multi-bone finger blends during animation** — exactly the SKEL/STATUS.md
seam-B "knuckle blend tear" the VERDICT §6 partially dismissed. The DEFAULT rebake
(off = meshWorld·inv(own_seed_R)) instead applies a *rigid per-bone conjugation* →
"coherent hand, wrong place" (ceiling hand), which at least preserves the hand SHAPE;
this cell trades that rigid displacement for a torn blend, which is visually worse.

**Consequence:** the band-side offset-bake fix class is FULLY EXHAUSTED — 7 prior dead
cells (VERDICT §3 table) + this 8th (the never-measured authored-offset repoint). The
genuine fix is an **engine per-member RESKIN** of `hands_naked`/`gloves`/`fingernails`
verts+weights onto `own`'s skeleton via `RndMeshDeform::Reskin` (the exact SKEL/RESKIN
conclusion), NOT any `RebindHeadHandsAtRest` offset/repoint. `RB3_NO_SKIN_CLAMP`
remains the shipped mitigation.

## Pre-registered Dolphin fallback (VERDICT §5) — NOT executable in-lane

The fallback (capture `bone_R-middlefinger03` / `bone_R-hand` `WorldXfm` on real Wii
via Dolphin + `../milo-trace`, diff vs native `own`) is **blocked**: `../milo-trace`
is a project SKELETON (its README: "almost everything is a documented stub with TODO
markers"); there is no turnkey Wii bone-world capture. **It is also mis-targeted for
THIS failure:** the observed shard is a MULTI-bone blend TEAR — a single-bone WorldXfm
diff cannot capture the tear (it lives between adjacent finger bones whose RELATIVE
animated poses diverge from the shared-bind rest). Handoff to coordinator: if pursued,
the diagnostic instrument is a **two-adjacent-finger-bone RELATIVE-pose** comparison
(`own` inter-bone delta vs shared-bind inter-bone delta at matched clip time), not the
single-bone capture — and it will confirm the reskin conclusion, not salvage the cell.

## Disposition
Flag kept in-tree **default-OFF, do-NOT-flip**, refutation documented in the function
comment + `classification.json` (`RB3_HANDS_AUTHORED_REPOINT`, class workaround, not-live
REFUTED-VISUAL). NO default flips, NO pin bumps; refuted flags stay UNSET; TEN defaults
untouched; engine `FxSendNative.cpp` not touched. Staged ONLY my own files under flock.

## Evidence (`execution/HANDS-FIX/evidence/`)
- `matched_zoom_burst08_burst12.png` — DECISIVE: OFF coherent hand vs ON torn spikes.
- `matched_contact_OFF_vs_ON.png`, `OFF/ON_burst08.png`, `OFF/ON_burst12.png`.
- `parsed_arm_summaries.txt` — Tier-1/Tier-2/wext both arms, gender-split (all meshes).
- `parse_hands_attach.py` — the parser (gate verdict built in).
- `armON_provenance_repoint.log` — A4 `[HANDS_REPOINT]` dump.
- `arm{OFF,ON}_hands_attach.log` — raw `[HANDS_ATTACH]` blocks.
- `offset_basis_derivation.py` — §2 male closure + A6 female-axis extension.
- Regen: `RB3_HANDS_AUTHORED_REPOINT=1 RB3_FIXED_CLOCK=1 RB3_HANDS_ATTACH_PROBE=1
  IK_SHARD_VERT='*' HEAD_REBIND_PROBE=1 python3 scripts/native/keyboard-to-gameplay.py
  --bin .claude/worktrees/wave16-handsfix/native/build-native/rb3-native --song-downs 3
  --game-burst 15 --burst-interval 0.3 --out /tmp/wave16-ON` (drop the flag for OFF).
- Checkpoint: `/tmp/wave16-checkpoints/F.json`.
