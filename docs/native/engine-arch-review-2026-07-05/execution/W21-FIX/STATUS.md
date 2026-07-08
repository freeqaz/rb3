# Lane FIX — STATUS (Wave 21 hands flagship, two-part fix)

**Owner:** Lane FIX (sole rb3 `BandCharacter.cpp` writer). Commit: Part 1 `24c2ac1c`
(default-OFF). Charter: `WAVE21_KICKOFF.md` + `WAVE21_REVIEW.md` (A1–A10). Checkpoint
`/tmp/wave21-checkpoints/FIX.json`.

## HEADLINE VERDICT — the third outcome: **L2-b R5-WALL (males + females)**

**Does the faithful-remap draw regime fix MALE hands? NO.** And there is **no distinct
female-only fix.** Part 1 is a real, clean, faithful Layer-1 restore (retail per-member
bone remap + textures, no crash, no white-texture regression) — but it does **NOT** fix
the hands, and DISCRIM's measured reading is **L2-b for BOTH genders** (basis-coherent at
draw yet visually torn; the tear is invisible to the inter-bone joint metric = the R5-walled
*animated* question). This is the pre-authorized honest-wall outcome (A8 third outcome /
decisive-question "Torn" branch), reported plainly — **not** an L2-a success story.

Three-outcome selector (kickoff §"decisive question"):
- ~~completable now (males coherent under the faithful regime)~~ — REFUTED.
- ~~female-only per-gender ship~~ — REFUTED (DISCRIM: female 28.9° rest-gap DISSOLVES
  under authored-repoint; no L2-a distinct from males).
- **L2-b R5-wall (males basis-coherent-only under the BANNED 8th cell, and torn there
  too)** — THIS.

## Why Part 1 does not fix the hands (the mechanism, measured)

Part 1 (`RB3_HANDS_BINDFIX`) restores the retail `sBoneMergeDir` bone remap: br2/br3 fire
**31,488×/member** (== full shim-off), br0=320, matching the Wave-20 noshim arm exactly.
But the hand MESHES stay bound to the **SHARED_ROOT** magnet:

- `LOADBIND_SLOT` under BINDFIX: **199/199 hand-mesh slots read
  `owningDirClass=SHARED_ROOT distinct=0`** (`ownFindPtr == boundPtr`) — hands_naked,
  fingernails, gloves all still on the shared root, per-member instances NOT bound.
- The remap `ReplaceRefs(o1, found)` re-points OTHER references to the shared bone, but the
  hand meshes' `mBones` array is bound at **PARSE** (`RndMesh::Load` `bs >> mBones` →
  `FindObject` descent into the shared preloaded `skeleton.milo` root — Lane N, W20-NATIVETRACE),
  UPSTREAM of the merge. The merge remap does not rewrite an already-parsed mesh bone array.

So the faithful remap is genuinely restored, yet the hand draw regime is **identical to
flag-OFF**: the engine seed-R rebake still runs at draw (`rebound=1`, Tier-1 own-vs-B = 87.3°
uniform — DISCRIM measured this directly and byte-for-byte reproduced the flag-OFF numbers),
producing the shipped ceiling-hand. The A2 draw-composition dump (`RB3_BINDFIX_DUMP`,
`BINDFIX_COMPOSE`) confirms the regime at rest: `own==bound`, `off·ownWorld = 87.2°`
(the §2 seed-R signature), authored offset survives, `mNativeBonesRebound=0` at
`RebindHeadHandsAtRest` (the boundRebakeOff MISS) — but the engine draw-side SKEL_REBAKE
then rebakes to seed-R and flags rebound anyway. The hoped "own-bound + clamp-active +
authored-offset never drawn" regime (kickoff A2) does not persist to draw: it is preempted
by the engine seed-R rebake, and even if it did, the hands are on the shared root, not
per-member.

## DISCRIM's L2-b decision (A6 gate — consumed, not overridden)

DISCRIM (`/tmp/wave21-checkpoints/DISCRIM.json`, COMPLETE, commits `c774657d`/`f1e81060`)
measured, matched-frame, gender-split:

| regime | rebound | Tier-1 own-vs-B | Tier-2 exact-joint | visual |
|---|---|---|---|---|
| flag-OFF shipped | 1 | 87.3° uniform | med 0.1u | ceiling-hand + spike-web |
| **BINDFIX Part-1** | 1 | **87.3° (IDENTICAL to flag-OFF)** | med 0.1u | same spike-fan, textures OK |
| raw shim-off | 0 | 3.1° rest → 167-180° animated | med 50–150u (TEAR) | catastrophic (arm/forearm slot-mixing, E2 artifact) |
| authored-repoint (BANNED 8th cell) | 1 | 3.1° count=0 EVERY frame | med 0.0u (max 8.4u distal) | **finger-spike fan TORN** |

The only regime that reaches a coherent basis (3.1°) is the **BANNED** 8th-cell
authored-repoint, and it is **VISUALLY REFUTED** (reproduces HANDS-FIX). Coherent-basis is
reachable but does not fix the visual → **L2-b: coherent basis + tear invisible to the
inter-bone joint metric** = the animated question R5 CLOSED without articulated Wii GT.
Per A6/E12 the Part-2 gender-rest-pose is the **wrong shape** — it targets an L2-a rest-basis
gap that does not survive as the operative defect. **Part 2 was therefore NOT implemented.**

## Gate table (Part 1, RB3_HANDS_BINDFIX, default-OFF)

| gate | result | evidence |
|---|---|---|
| **br2/br3 bone remap fires** (mechanism) | **PASS** 31,488/member (==noshim); br0=320 | `evidence/loadbind_subdir_and_counters_bindfix.log` |
| **A3 crash — MILO_ASSERT 0xAB8** | **PASS** 0 crashes; all 4 members boot to gameplay flag-ON | 4 gate-arm harness logs, PASS=1 each |
| **E11 texture-integrity** (no white-tex regression) | **PASS** 0 dummy/skin/naked cascade OFF and ON; skin+cloth textures resolve | `evidence/e11_texture_{OFF,ON}.txt`, matched PNGs |
| **flag-OFF drawlog-golden 792** | **PASS** byte-identical (263 known-residual within bound) | drawlog-golden `--fixed-clock --canonical-order` |
| **batch_objdiff == baseline** (G3 Wii-match) | **PASS** FilterSubdir 100%==base, OnInstallFilter 99.1%==base, Filter 95.6%==base (edit is HX_NATIVE-only) | batch_objdiff + report.json baseline |
| **guard-DROP census** | **PASS** 0 band drops (draw regime unchanged vs OFF; all 4 members render) | gate-arm burst frames |
| **crowd oracle** | **untouched** (BandCharacter band-scoped; crowd renders) | gate-arm wide frames |
| **G-FIX-E1 visual** (ceiling-hand AND spike-fan GONE, both genders, mitten-OFF+ON) | **FAIL-to-fix** (expected): hands NOT fixed; ceiling-hand/spike-fan persist (arguably worse mitten-OFF), textures intact | `evidence/{OFF,BINDFIX}_mitten{ON,OFF}_burst*.png` |

G-FIX-E1 is a FAIL in the sense that BINDFIX does not remove the morphology — this is the
CORRECT, honest result given the mechanism (hands stay on the shared root). It is **not** a
regression to ship: flag is default-OFF; flag-ON preserves textures and does not crash. Run
with matched fixed-clock burst frames, both genders (player1 female), mitten-OFF pairs
(`RB3_HANDS_MITTEN_OFF`) alongside mitten-ON (A8). Camera-cut variance across arms limits
pixel-matching; the morphology conclusion (persist, not fixed) is robust and corroborated
by DISCRIM's numeric BINDFIX==flag-OFF.

## What Part 1 IS worth (for the coordinator's default decision)

Part 1 is a genuine, faithful **Layer-1 topology + texture** restore: it makes the native
loader run the retail merge remap (br2 fires, matching Wii's counted mechanism) and keeps
the white-texture fix intact (colorpalettes.milo stays kReplace; char_shared's textures are
ref-swapped via the `sCharSharedDir` kIgnore branch, 0 dummy_torso cascade). It removes the
native-introduced remap-suppression regression at the load-topology level. But it is **not
landable as a hands fix** — the hand meshes bind at parse, upstream of the merge, so
per-member HAND binding is not achieved and the seed-R ceiling-hand persists. **Recommend
keeping default-OFF** (coordinator decides): flag-ON is faithful and non-crashing but
delivers no visual hand improvement, so there is no reason to flip it, and every reason
(this bug's history) not to without extraordinary evidence.

## Banned-cell hygiene

No dead cell re-attempted. Part 1 is a load-path merge-scope change (not an offset bake,
not a reskin). Part 2 not implemented (would have been the SKELETON rest write, byte-identical
offsets/verts/weights — but DISCRIM's L2-b reading makes it the wrong shape, so it was
correctly NOT dispatched). wext cited nowhere as a gate (A9). No default flips, no pin bumps.

## Evidence (`execution/W21-FIX/evidence/`)
- `OFF_mittenON_burst12.png` / `BINDFIX_mittenON_burst12.png` — matched-ish shipped vs flag-ON
  (textures intact; hand tearing persists/worse).
- `OFF_mittenOFF_burst08.png` / `BINDFIX_mittenOFF_burst08.png` — mitten-OFF pair (A8, finger tear unmasked).
- `loadbind_subdir_and_counters_bindfix.log` — char_shared→kMerge, colorpalettes→kReplace,
  br2/br3=31488 proof.
- `bindfix_compose_dump.log` — A2 draw-composition (own==bound, off·ownWorld=87.2°, rebound=0
  at rebind entry, authored offset survives) — discharges E4.
- `hand_mesh_slots_sharedroot.log` — the SHARED_ROOT distinct=0 finding (hands never per-member).
- `e11_texture_{OFF,ON}.txt` — texture-integrity summaries (0 white cascade both arms).
- Full run log: `/tmp/rb3-kbd2game-33911.log`.
- Checkpoint: `/tmp/wave21-checkpoints/FIX.json`.
