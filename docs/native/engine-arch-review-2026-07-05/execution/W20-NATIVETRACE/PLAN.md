# Lane N — Native load-path divergence trace — PLAN

**Charter (A1, BINDING):** VERIFY + COMPLETE the 2026-06-06 causal chain
(CHAR_SKINNING_DEFORM_INVESTIGATION.md ~:894–1040) on the CURRENT build (engine pin
`6e6387c`, 12 defaults ON). NOT discover-from-scratch. Suspicion order: (b) parse-time
name-resolution share FIRST, (a) FilterSubdir shim SECOND. AUDIT-ONLY: zero fixes, zero
default flips, zero pin bumps. New probes `#ifdef HX_NATIVE` + env-gated default-OFF.
BANNED: the 8 dead offset-bake cells + refuted reskin (VERDICT §5 table).

## What the record already establishes (re-verify, don't rediscover)
- Loader loads FRESH per-member `kInlineCached` skeletons (8–9 distinct; SKEL_LOAD_PROBE,
  now removed from source). Loader is CORRECT.
- Binding decided at PARSE-TIME NAME RESOLUTION: `RndMesh::Load` `bs >> mBones`
  (Mesh.cpp:947) resolves each bone ObjPtr by name via `FindObject` descent into the
  shared preloaded `char/main/skeleton.milo`. (doc ~:536-556, ~:913-920)
- Full shim-off arm did NOT change binding (doc "PROVEN dead-ends").

## Genuine new ground (review Q5)
1. Hit counts on the THREE `BandCharacter::Filter` remap branches:
   - :4182 `sCharSharedDir` → ReplaceRefs :4185
   - :4188 `sInstrumentDir`/`sInstResourceDir` → ReplaceRefs :4196
   - :4202 `sBoneMergeDir` → ReplaceRefs :4207 (VERDICT §1 "never-firing" NEVER instrumented)
2. Which load event establishes the shared-root hand binding under TODAY's build.
3. A Wii-joinable per-slot binding table for native (A10 schema).

## Anchors (re-derived from working tree @ 67187eed)
- `BandCharacter::Filter` :4165–4232; three remap branches at :4182/:4188/:4202 CONFIRMED.
- Shim: HX_NATIVE `FilterSubdir` :4235–4286; override :4280–4282 (kMerge→kReplace when
  `!o1->mStoredFile.empty()`).
- `OnInstallFilter` :4289; resets sBoneMergeDir=0 :4290; sets it :4339 from
  `sOutfitDir->FindObject("bone_pelvis.mesh")->Dir()`; sCharSharedDir :4343 from feet_skin.mat.
- `RebindHeadHandsAtRest` :1254 (Poll :527, pre-Character::Poll); pass-A resolve loop
  :1548; `RebindOutfitBonesToOwnSkeleton` :1102 (Poll :575).
- Parse-time resolve: `RndMesh::Load` `bs >> mBones` Mesh.cpp:947; existing BONE_LOAD_DBG
  probe :1000 (extend for A4 per-bone owning-dir dump).
- Existing probes to reuse: BAND_ANIM_PROBE (Poll :502), SKEL_REBIND_PROBE (:1108),
  HEAD_REBIND_PROBE (:1259), BONE_LOAD_DBG (Mesh.cpp:1000), engine XBONE/XBONE_TRACK
  (Rnd_Wgpu_RB3.cpp — read-only, do not edit engine).

## Work plan
- **Step 0 (lint 9):** confirm probe TUs (BandCharacter.cpp, Mesh.cpp) compile into rb3-native.
- **Probe A (A3):** hit counters on all three remap branches + arg logs (o1 name, o1->Dir(),
  the three static dirs). Env `RB3_LOADBIND_PROBE`. Counters printed at OnInstallFilter and
  end-of-load.
- **Probe B (A3):** per-merge FilterSubdir action log (subdir name → kMerge/kReplace +
  mStoredFile emptiness).
- **Probe C:** OnInstallFilter dump of sBoneMergeDir/sCharSharedDir/sInstrumentDir/
  sInstResourceDir/sResourceDir values+names per install.
- **Probe D (A2):** pristine-binding dump at ENTRY of RebindHeadHandsAtRest AND
  RebindOutfitBonesToOwnSkeleton, per hand/torso mesh, FIRST-TOUCH only + end-of-merge event.
  Per bone slot: trans pointer, Dir() identity+name, owningDirClass.
- **Probe E (A4):** extend Mesh.cpp BONE_LOAD_DBG for hand meshes to dump each bone's
  resolved mBone->Dir() identity+name → answers WHICH dir answers hand-mesh bone names.
- **Boot:** headless RB3_FIXED_CLOCK=1 RB3_HTTP=1, free ports, capture at main_hub AND
  gameplay. Emit A10 native table.
- **Shim reconciliation arm (A1):** control boot (default) + full shim-bypass boot
  (RB3_LOADBIND_NOSHIM disables the :4280 kMerge→kReplace override). Identical probes,
  same lineup+fixed clock. PRE-REGISTERED: topology UNCHANGED ⇒ shim exonerated (a demoted);
  CHANGED ⇒ record superseded (a promoted).
- **Synthesis:** causal chain load-seq → shared-static hand binding; native A10 table;
  three-branch hit-count table (counted 0, lint 8).

## Regression gate
`python3 scripts/native/drawlog-golden.py --fixed-clock --canonical-order` must PASS with
probes OFF (baseline captured before probe edits; re-run after).

## Deliverables
PLAN.md (this), STATUS.md (verdict), evidence/ (probe logs, A10 native table JSON+md,
hit-count table, boot commands), probe code committed default-OFF, checkpoint updated.
