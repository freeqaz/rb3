# Lane FIX — PLAN (Wave 21 hands flagship, two-part flag-first fix)

**Owner:** Lane FIX (SOLE rb3 `BandCharacter.cpp` writer this wave). Charter:
`WAVE21_KICKOFF.md` + `WAVE21_REVIEW.md` (A1–A10 binding). Checkpoint
`/tmp/wave21-checkpoints/FIX.json`.

## Goal
Implement the faithful Layer-1 fix (restore retail per-member bone binding by
scoping the white-texture FilterSubdir shim) as `RB3_HANDS_BINDFIX` (default-OFF,
HX_NATIVE), plus a CONDITIONAL Layer-2 gender-pose (`RB3_HANDS_GENDERPOSE`,
default-OFF), and MEASURE against all gates gender-split + mitten-controlled. Ship
nothing (coordinator decides). Report which of three outcomes: completable /
female-only-ship / L2-b-wall.

## Mechanism analysis (done before writing code — resolves R-A/R-B)

The shim at `BandCharacter.cpp:4433` converts `kMerge -> kReplace` for EVERY subdir
with a non-empty stored file. Wave-20 control log enumerates the shimmed subdirs;
under retail (noshim) the ones that get `kMerge` are: `char_shared.milo`,
`colorpalettes.milo`, `skeleton.milo`, `skeleton_unshared.milo`, and the
`*_resource.milo` outfits.

**Where the bone remap actually fires (from `loadbind_noshim_shimOFF.log`):** the
`sBoneMergeDir` remap (br2, :4337) fires on `bone_*.mesh` objects whose
`o1->Dir() == sBoneMergeDir` (= the SHARED_ROOT skeleton dir). The merge sequence
per install is: outfit_resource -> char_shared -> colorpalettes -> skeleton ->
skeleton_unshared -> **then** the 20k+ BR2 bone-remap burst (incl. hand bones
`bone_R-pinky01.mesh`, `bone_L-ringfinger03.mesh`). The bones being remapped live
in `skeleton.milo`/`skeleton_unshared.milo` (reached as nested subdirs of
char_shared via `MergeObjectsRecurse`). `MergeObjectsRecurse` only iterates a
subdir's objects (and recurses its child subdirs) when `FilterSubdir` returns
kMerge; a kReplace short-circuits (append-as-subdir, return).

**Therefore** char_shared-alone kMerge is INSUFFICIENT: its nested `../skeleton.milo`
subdir gets its OWN `FilterSubdir` call, and if that still returns kReplace the
recursion stops before the bone objects are iterated. To restore the remap I must
allow retail kMerge for the **skeleton-bearing subdirs**: `char_shared.milo`,
`skeleton.milo`, `skeleton_unshared.milo`.

**White-texture safety (E11):** verified from the Wii assets — `skeleton.milo` and
`skeleton_unshared.milo` carry ONLY bone objects (316 bones), ZERO `.tex`/`.mat`.
`char_shared.milo` carries `feet_skin.mat`/`torso_naked.mat`/`dummy_*.tex` +
`colorpalettes.milo` subdir + `../skeleton.milo` subdir. But char_shared's own
texture/material objects hit the `o1->Dir() == sCharSharedDir` branch in `Filter`
(:4311) -> `ReplaceRefs` + **kIgnore** (ref-swapped, never MOVED), so kMerge on
char_shared does not DRAIN its palette. And `colorpalettes.milo` (the actual base
texture palette) is its OWN stored-file subdir -> it STILL gets kReplace under the
scoped shim -> the white-tex fix stays intact. This is the mechanically-mitigated
E11 story the review (Q2) blessed.

## Part 1 — `RB3_HANDS_BINDFIX` (default-OFF)
Scope the :4433 override: when the flag is ON and the subdir's stored file names a
skeleton-bearing milo (`char_shared.milo` | `skeleton.milo` | `skeleton_unshared.milo`),
DO NOT override -> retail kMerge stands. All other subdirs (colorpalettes + outfits)
keep the shim's kReplace. Verify via `LOADBIND_SUBDIR` that these three flip to
kMerge and colorpalettes stays kReplace, and via `LOADBIND_COUNTERS` that br2/br3
go non-zero (the remap fires).

**A1 pointer-verify:** confirm the landing instance == Poll-`Find()` instance
(distinctFromOwnFind expected False — same instance; the win is the DRAW REGIME not
a different instance).

**A2 draw regime + composition dump:** under Part 1 hand meshes become own==bound ->
`RebindHeadHandsAtRest` :1831 `own==bound` -> `sNoBoundRebake` default-ON ->
`boundRebakeOff` MISS -> mesh NEVER flagged `mNativeBonesRebound` -> no seed-R
rebake, authored offsets survive, fling-clamp/V24 guard STAYS ACTIVE. This is the
untested regime (NOT dead cell #1 seed-R rebake, NOT 8th cell repoint-clamp-exempt,
NOT FULL-rebind). MUST NOT reuse `RB3_SKEL_REBIND_FULL`. Add a `RB3_BINDFIX_DUMP`
draw-time composition dump per slot (offsets + mNativeBonesRebound flag + regime
label) so the flag-ON composition is NAMED (discharges E4).

**A4:** same commit, update the stale :4406-4428 NOTE per errata E10.

## Part 2 — `RB3_HANDS_GENDERPOSE` (default-OFF, CONDITIONAL)
Only implement/enable if Part-1 matched-frame still shards (males) or flings
(females) AND DISCRIM's `measured_L2_reading` (/tmp/wave21-checkpoints/DISCRIM.json)
is consistent with L2-a for that population (A6). If DISCRIM says L2-b, Part 2 is the
wrong shape — report the wall, do NOT force it.
Shape: pose each per-member hand skeleton so `L_own(rest)==B`, studying the shipping
`RebindOutfitBonesToOwnSkeleton` torso path (:1211, SetBone(own,false), sTorsoOnly
:1239). Writes ONLY the per-member skeleton's local/rest transform — BoneOffsetAt/
verts/weights BYTE-IDENTICAL (dump to prove, A5). Address absolute-SET clip semantics;
define B per-asset (hands vs gloves 60-69 vs nails ~170) w/ per-asset Tier-1 ~0.

## Gates (ALL, gender-split nb=38/40, matched fixed-clock burst_08/burst_12)
- G-FIX-E1 (DECISIVE visual): ceiling-hand AND spike-fan GONE both genders,
  gloves+fingernails non-regressing. mitten-OFF pairs (RB3_HANDS_MITTEN_OFF)
  ALONGSIDE mitten-ON (A8). Per-member attribution.
- A3 crash: zero MILO_ASSERT 0xAB8; full boot-to-gameplay all 4 members flag-ON.
- E11 texture-integrity: no white-tex regression across ALL members; lineup capture.
- batch_objdiff == baseline on touched src/system units.
- flag-OFF drawlog-golden 792 byte-identical.
- guard-DROP census 0; crowd oracle untouched.

## Landing order
Part-1 flag lands first (DISCRIM consumes `RB3_HANDS_BINDFIX` as substrate). Part-2
gated on DISCRIM's measured reading.
