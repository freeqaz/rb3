# W31-HUBWALKER-SHARDS — PLAN (Lane D, diagnosis-first)

**KEY:** W31-HUBWALKER-SHARDS  **Base SHA (rb3):** fd119705  **Engine pin:** b36bcfc
**Mandate:** DIAGNOSIS-ONLY per the three-supersessions rule. NO fix code before the
(iii) verdict. Owned: read-only probes + (conditionally) a narrow default-OFF
walk-clip/prop-track scoping fix ONLY on the (iii)=undriven-track branch.
READ-ONLY: BandPatchMesh (F5 not this lane), CharDriver/CharClip write paths,
BandCharacter.cpp / BandCamShot.cpp (Lane A exclusive-write, A2 — incl. RELOAD_PROBE
at BandCharacter.cpp:50).

## Target family

Hub street walkers = `player0..3` (CharCache; `world/shared/chars.milo` proxies to
`char/main/main.milo`, same body machinery as the gameplay band —
`src/band3/meta_band/CharCache.cpp:47-63`). W29 census-trap Q(b) BINDING: measure
player0-3, key by object POINTER (char_addr / trans_addr), state the name-key caveat (E7).

Symptoms (W31-REPRO evidence, on disk):
- forehead flesh cone — `crop_hub_center_face.png`, `crop_hub_right_face.png`
- waist stick-fans — W30-F1 (`W30-VISUAL-PASS/FINDINGS.md` F1)
- crumpled boots / hip-high dangling foot — `crop_hub_legs.png`

Morphology (all three) = the point-radial R·sin(θ) shard family = the SKEL
rotation-basis signature described in `R5-HANDS-ENDGAME/CLOSURE.md` (87.2° seed-R
rebake). R5 explicitly lists FOREARM-FLOAT (top-center floating flesh structure) as
an OUT-OF-SCOPE residual, forearm/prop level, NOT finger-level — so the hub-walker
face/waist/boot shards are NOT pre-adjudicated by the closed hands-finger family and
must be discriminated on their own bones.

## STEP-0 discriminators (checkpointed, NO fix code)

- **(i) NAME the shard meshes + bound bones** — matrix-relative + pointer-verified
  (lint 1), per-walker rows (lint 2). Method: read-only probe walks each `player%d`
  dir for skinned `RndMesh`; per mesh dump name / NumVerts / NumBones and per bound
  bone {name, `BoneTransAt` pointer, `WorldXfm`, `BoneOffsetAt` (inverse bind)}.
  Suspect meshes: head/scalp/hair/face, waist/belt/prop, boot/shoe/foot.
- **(ii) are those bones driven by `playerN_{f,m}` walk clips?** — static track
  enumeration (W27(b) method): the body `CharDriver`'s playing `CharClip`, call
  `CharClip::ListBones()` → the driven bone-track name set. A shard-mesh bone whose
  channel name is NOT in that set = an undriven-track candidate.
- **(iii) SKEL 87°-family vs distinct undriven-track gap** — NOT decidable by track
  enumeration alone (A12). Pre-authorized READ-ONLY live-bone transform probe
  (distinct TU `native/src/rb3_shardprobe_native.cpp`, NOT BandCharacter/BandCamShot,
  A2). For each shard-mesh bone compute the deviation of `WorldXfm · BoneOffset` from
  identity and its rotation angle:
  - a coherent ~87° seed-R rotation on a *driven* bone ⇒ SKEL-family;
  - a bone at pure parent-propagated bind (no own animation) whose track is ABSENT
    from ListBones ⇒ undriven-track gap.

## Verdict gate

- **(iii)=SKEL-family ⇒ STOP.** Write a memo binding to `R5-HANDS-ENDGAME/CLOSURE.md`.
  status=done verdict=SKEL_FAMILY_STOP. No fix.
- **(iii)=undriven-track ⇒** a scoped default-OFF fix MAY land (walk-clip prop-track
  scoping class) with before/after hub crops + flag hit-count (lint 8) + class.json
  append under `/tmp/milo-engine-classjson.lock`. Any write toward CharClip/CharDriver
  ⇒ coordinator arbitration → return engineAckNeeded=true instead.

## Lint-4 check

`RB3_PROP_POSE_FULL` (15th default ON) does NOT retire these (W30 flip retest: hub-walker
fans survive). Re-verify the named meshes against its ledger row; record no-contradiction.

## Deliverable

Per-mesh per-bone discriminator table (per-walker, pointer-keyed) + the (iii) verdict.
Fix only on the undriven-track branch.

## Steps

1. [done] Read kickoff, R5 closure, W31-REPRO NOTES, CharCache, RndMesh/CharClip headers.
2. Write `rb3_shardprobe_native.cpp` (read-only, env-gated `RB3_SHARD_PROBE_OUT`), register in CMake like `rb3_bonedump_native.cpp`.
3. Build in `native/build-agent-W31-HUBWALKER-SHARDS`.
4. Boot hub headless (RB3_HTTP=1 RB3_FIXED_CLOCK=1, char preview ON), settle, POST probe, capture JSON.
5. Analyse → per-mesh per-bone table + classify (i)/(ii)/(iii).
6. Verdict + memo/fix per gate. Checkpoint + STATUS at every milestone.
