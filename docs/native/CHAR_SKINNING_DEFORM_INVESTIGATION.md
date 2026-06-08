# Band-Character Skinning Deformation — Investigation

---
## 2026-06-08 UPDATE — gender deform enabled + IsExoBone LP64 fix LANDED (`a5999979`), with a premise correction

Two HX_NATIVE-guarded changes landed on `master` (Wii path byte-identical, default-on with opt-outs):

1. **`RndMeshDeform::IsExoBone` (src/system/rndobj/MeshDeform.cpp)** — the Wii path reads the
   bone's name through a hard-coded vtable-slot offset (`((const char**)((void**)(*(void***)t))[0])[3]`),
   an ABI trick that aliases `Hmx::Object::Name()` on the Wii image. Under clang LP64 the vtable
   layout differs → the raw read returns garbage and faults. Now uses the portable `t->Name()`
   accessor under `HX_NATIVE`; the Wii branch is preserved verbatim under `#else`.

2. **`BandCharDesc::Init` (src/system/bandobj/BandCharDesc.cpp)** — stop deferring `gDeforms`
   (`char/main/shared/deform.milo`). Opt-out: `RB3_NO_DEFORM_LOAD=1`. With (1) fixed it loads
   cleanly — **the feared `CharBonesSamples.cpp:457` version-desync assert does NOT occur**
   (that fear, held by both this investigation and the concurrent wave-08 session, was unfounded).
   `BandCharacter::SetDeformation` then applies the per-gender `CharClip` ("male"/"female") as a
   **vertex morph** (gender body/face/cuff shape).

**PREMISE CORRECTION (important).** Earlier notes claimed the female member *shatters* without
`gDeforms` because her skeleton is never gender-posed. That is **wrong** on the current tree. The
gross fling/shatter is fixed by the **`acd9c19a` skeleton rebind + engine fling-clamp ALONE**:
the `REBIND_DRAW_SKINPOS` probe (engine, prints draw-time `|skin.v − boneWorld.v|`) reads a clean
**~50–65u** for every band mesh (trackjacket/plaidshirt/shred) — *with `gDeforms` ON and OFF
alike*. So `gDeforms` is a **faithfulness / hack-removal** refinement (gender silhouette), not the
fling fix; its visible gain at typical gameplay cameras is subtle. It was still landed because
`gDeforms=0` is itself a native-only hack and enabling it converges toward the original game
(matches the "minimize hacks / converge with Xbox" goal). Corroboration: DC3 (the working
reference) has **no `gDeforms` symbol at all** — it achieved working characters without this path.

**Verification (headless, `scripts/native/char-burst-capture.py`):** reaches gameplay; no crash
over thousands of frames; characters render coherently. A `RB3_NO_DEFORM_LOAD=1` negative control
gives the same clean `skinToBoneDelta`, confirming the rebind (not the deform) owns the fling fix.
A one-off overhead-intro-cam frame showed band members as a "flattened blob" — this is overlapping
members + camera angle, NOT corruption (singer-cam closeups in the same run are clean).

Everything below predates this update and is the historical investigation trail.

---

### wave-08 VERIFY (independent, run-only — NOT REFUTED) — 2026-06-06. Tried to refute "the band animates correctly + female no longer flings"; could not. Ran the BUILT binary (did NOT build) through gameplay with the in-tree diagnostics; every claimed metric reproduced on my own measurement.

**VERDICT: refuted=false.** The wave-08 rebind claim holds on independent measurement.

**(a) THE KEY — does the OUTFIT-bound bone actually MOVE? YES.** With default rebind ON,
`XBONE_TRACK=bone_R-upperArm` on the `trackjacket` outfit mesh: across 2358 draws the bone
worldPos took **1198 DISTINCT values** (run 52201), and across 1901 draws **967 distinct
values + 965 distinct rotations** (run 58485), max frame-to-frame delta 231-352u. The band
ANIMATES (translation AND rotation vary). NEGATIVE CONTROL `RB3_NO_SKEL_REBIND=1` (run 50785):
the SAME bone took **exactly 1 distinct value** `(7.4020,-0.8298,57.5293)` over 1152 draws,
maxDelta **0.000000u** — the static magnet, byte-identical to the documented BEFORE. So the
motion is real and is produced by the rebind.

**It is NOT silently re-binding a static bone (point e).** `SKEL_REBIND_PROBE` per-bone lines:
all **32 rebind slots had magnet != own** (32 diff / 0 same); the rebind resolves a DIFFERENT
instance than the static magnet, on all 4 outfit meshes (trackjacket/vestdenim/plaidshirt/shred)
across all 4 members (player0-3). And the negative control proves the rebake/clamp do NOT mask
this: with rebind OFF, `SKEL_REBAKE` fires (meshLocal 20.2u → rebakes 10 bones) and `SKIN_CLAMP`
fires 4797× on the band torso, yet the bone stays frozen — so the visible motion is the rebind's
doing alone.

**(b) trackjacket clean WITH animation? YES.** `REBIND_DRAW_SKINPOS` (authoritative draw-time
|skinWorld−boneWorld|): trackjacket 50-65u, max across ALL band meshes **65.42u** (clean limb
extent). `REBIND_DRAW_FLING` (>120u) fired **0 times**. No band torso mesh appears in SKIN_CLAMP
(0 occurrences). The female does not fling and is not re-flung mid-animation.

**(c) 3 males clean AND animating? YES.** vestdenim/plaidshirt/shred all rebound (all 4 members
in the magnet→own probe) and all show 50-65u skinpos — same clean range as the female, now
animating off the same per-member animated skeleton.

**(d) no regression? CONFIRMED.** Crowd/extras still clamped IDENTICALLY with rebind ON (clap,
male/female_crowd_body, fist, lighter, extras, facehair — same mesh set as rebind OFF; band-only
scan never touches them). No crash across 4 runs (reached frames 4830-6790, clean exit, no
assert/segfault). HUD/highway/venue render normally in screenshots (no shard artifacts anywhere
in frame). Source edits are `#ifdef HX_NATIVE`-gated (method 675-916, Poll call site 448-492 in
BandCharacter.cpp). Wii match% in report.json = **62.883026% code / 77.44461% fns**, BandCharacter
**77.31311%** — exactly the claimed values (report predates the edits, but the HX_NATIVE gating
guarantees the Wii build is byte-identical).

**Caveats (honest, non-refuting):** (1) the SKEL_REBIND probe summary that prints during the
scan window is the SyncObjects-site probe (`skinMeshes=0 reboundDiff=0`) — that site genuinely
finds nothing, exactly as the doc says; the WORKING probe is the Poll-site per-bone magnet→own
lines, which fire and show magnet!=own. (2) Burst-capture camera framing is non-deterministic;
the band members were not prominently framed in the captured shots, so the visual confirmation
is "no shard artifacts in frame" + the decisive per-bone MEASUREMENT, not a hero closeup. (3)
The shipped scope is TORSO-ONLY by design; head/hands stay coherent-static (the wave-08 finding's
own documented limitation) — that is the claimed behavior, not a refutation. (4) Capture logs
carry interleaved null bytes from binary stderr — use `grep -a` to parse them (plain grep
silently drops matches).
Runs: /tmp/rb3-charburst-{52201(after),50785(no-rebind ctrl),58485(probe),53091(clamp)}.log.

---
### wave-08 IMPLEMENT (band rebind, measured) — 2026-06-06, BUILT + MEASURED. The band TORSO now ANIMATES + the female no longer flings. Default-ON, TORSO-SCOPED. (Partly CONFIRMS, partly REFUTES wave-07: the rebind animates, but rebinding head/hands SHARDS thin geometry — a rotation-basis mismatch wave-07 didn't anticipate.)

**TL;DR.** Implemented the wave-07 rebind: in `BandCharacter::Poll` (after `Character::Poll()`
poses the per-member skeleton), repoint the outfit skin meshes' bones from the static shared
`char/main/skeleton.milo` magnet onto the member's OWN animated per-member bone, resolved by
name via `Find<RndTransformable>(boneName)` (which at Poll time returns the LIVE moving instance,
exactly as the wave-07 BAND_ANIM probe found), keeping the authored gender-correct offset
(`SetBone(b, own, /*calcOffset=*/false)`). **Result: the OUTFIT-bound bone now MOVES (the band
animates) and the female trackjacket no longer flings — BOTH fixed, as wave-07 predicted — BUT
ONLY for the TORSO CLOTHING.** Rebinding the high-bone head/hands/face meshes shards their
long-thin geometry (hair/fingers) into thin radiating spikes, because the animated per-member
bone's rotation BASIS differs from the static magnet the authored offsets were baked against (a
subtlety wave-07's translation-only reasoning missed). So the shipped scope is **torso clothing
only** (the compact geometry that rebinds cleanly); head/hands stay coherent-static via the
wave-06 rebake. **Default-ON, opt-out `RB3_NO_SKEL_REBIND=1`; full-body study mode
`RB3_SKEL_REBIND_FULL=1` (animates everything but shards thin geo).**

**THE FIX (files + entry point):**
- `src/system/bandobj/BandCharacter.cpp` / `.h` (HX_NATIVE-gated, Wii byte-identical —
  match% unchanged 62.8830% code / 77.4446% fns; BandCharacter 77.3131%, 275/290):
  - New method `BandCharacter::RebindOutfitBonesToOwnSkeleton()`, **called from
    `BandCharacter::Poll()` right after `Character::Poll()`** (skeleton freshly posed/animated
    for this frame) and before the outfit meshes draw. This is the wave-07 TIMING fix: at Poll
    `Find` returns the ANIMATED per-member bone (`0x..429c00`-class, moves 100-200u/frame),
    whereas at SyncObjects (the wave-06 SKEL_REBIND probe site) `Find` returned the static magnet
    (`reboundDiff=0 same=4`). Same `Find` call, different call site/timing.
  - Mesh collection: the multi-bone outfit skin meshes are NOT in any hashtable (`meshDir=''`,
    merged resources). They live in the per-LOD draw groups. The collector walks `this` +
    `mOutfitDir`: (1) `ObjDirItr` (face/hands in the hashtable) + (2) each dir's `mDraws` + (3)
    **each `Character::mLods[i].Group()/TransGroup()`** (this is where the BODY CLOTHING lives —
    `Character::DrawLodOrShadow` draws `curLod->Group()`), recursing the draw tree via
    `RndDrawable::ListDrawChildren` (groups → nested clothing meshes). `mInstDir` (guitar/mic)
    is EXCLUDED (instruments attach to prop bones, not the gender skeleton).
  - Per bone: `own = Find<RndTransformable>(BoneTransAt(b)->Name(), false)`; if `own != bound`,
    `SetBone(b, own, false)`. Sets `mesh->mNativeBonesRebound = true`.
  - SCOPE FILTER (the shard fix): default rebinds only torso clothing meshes
    (`trackjacket|vestdenim|plaidshirt|shred`); `RB3_SKEL_REBIND_FULL=1` rebinds all.
  - LATCH: re-scans each Poll (skipping already-rebound meshes — bounded cost) until a torso
    mesh is caught AND ~90 quiet Polls pass (late LOD streaming), then `mNativeReboundOnce`
    latches and Poll stops scanning. Members: 4 rebind events over 8559 frames (efficient).
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`: NO functional change needed — the
  wave-06 SKEL_REBAKE pre-pass already guards on `!mesh->mNativeBonesRebound`, and the
  fling-clamp already skips rebound meshes (`reboundSkip`). Added only a diagnostic
  (`REBIND_DRAW_SKINPOS`/`REBIND_DRAW_FLING`, default-OFF) reporting draw-time skin-to-bone
  delta. The clamp + rebake stay LIVE as the backstop for crowd/extras + the non-rebound
  head/hands.

**BEFORE → AFTER (XBONE / XBONE_TRACK on the OUTFIT mesh `trackjacket`):**
| metric | BEFORE (wave-06 / `RB3_NO_SKEL_REBIND=1`) | AFTER (default rebind) |
|---|---|---|
| outfit `bone_R-upperArm` ptr | static magnet `0x..924ec0`-class | the animated per-member ptr `0x..429c00`-class |
| outfit bone worldPos | **1 distinct value** = byte-identical static `(7.40,-0.83,57.53)` | **1750 distinct values**, max frame-to-frame delta **209.8u** — THE BAND ANIMATES |
| trackjacket fling (skin-to-bone delta) | ~20u static bind mismatch (rebaked to coherent-static) | **50-65u** (limb/joint extent — CLEAN, tracks the bone) |
| 3 males (vestdenim/plaidshirt/shred) | coherent-static | coherent + animating, 50-65u (unchanged-clean) |

**THE WAVE-07 REFUTATION (why head/hands can't be rebound):** the per-bone skin-to-bone
TRANSLATION delta is clean (<65u, no bone >120u) for ALL rebound meshes, yet the full-body
rebind SHARDS visibly (thin radiating spikes from head/hands). Root cause: the animated
per-member bone's rotation BASIS differs from the static magnet's (e.g. `bone_R-upperArm`
worldRot.x sign-flips: magnet `(0.73,-0.07,-0.68)` vs animated `(-0.73,0.09,-0.68)`), and the
authored offset was baked against the magnet basis. A bone ORIGIN maps correctly (translation
delta tiny) but a vertex at radius R from the bone with a rotation error θ flings by ~R·sin(θ) —
so LONG-THIN geometry (hair strands, fingers, clothing edges) shards while compact torso
geometry survives. `calcOffset=true` shards too (the skeleton is already animating when first
reachable → no rest frame to bake; and it drifts as the bone moves away from the rebind pose).
The COMPACT torso clothing has no long-thin geometry, so it rebinds cleanly. **NOTE on a
measurement trap:** a skinned mesh's own `WorldXfm()` is IDENTITY (renderer convention — the
palette already carries world space), so a "mesh-local" (skin·inv(meshWorld)) metric just reads
back the character's far-from-origin world position (~300-500u) and FALSELY looks like a fling;
the authoritative metric is `|skinWorld − boneWorld|` (limb extent, ~50-65u clean).

**SUPERSEDING wave-06 SKEL_REBAKE:** with the rebind active, `SKEL_REBAKE_PROBE` shows the
rebake fires **0 times** on rebound torso meshes (the `!mNativeBonesRebound` guard skips them) —
no conflict. With `RB3_NO_SKEL_REBIND=1`, the rebake fires as before (trackjacket meshLocal=20.2u
→ rebakes 10 bones) — the wave-06 coherent-static path is fully intact as the opt-out. The
fling-clamp stays the backstop for crowd/extras (`clap`/`*_crowd_body*`/`fist`/`lighter` still
clamped) + the non-rebound head/hands.

**REGRESSION:** males animate + clean (50-65u, same as female); crowd/extras unchanged (still
clamped, band-only scope never touches them); no crash over 8559+ frames; Wii byte-identical
(match% unchanged); DC3 inert (engine change is a default-OFF probe only); menus/HUD/highway
unchanged (the rebind only runs in the band Poll active branch during gameplay).

**DIAGNOSTICS LEFT IN-TREE (HX_NATIVE, default-OFF unless noted):** `SKEL_REBIND_PROBE=1`
(rebind summary: meshes/slots/reboundBones/body/quiet/latched + per-bone magnet→own),
`SKEL_REBIND_SKINPOS=1` (Poll-time skin-to-bone delta), engine `REBIND_DRAW_SKINPOS=1`
(draw-time skin-to-bone delta, the authoritative AFTER) + `REBIND_DRAW_FLING=1` (>120u
shards + skinDet). Study knobs: `RB3_SKEL_REBIND_FULL=1` (rebind head/hands too — shards),
`RB3_SKEL_REBIND_CALCOFF=1` (recompute offset — also shards). Opt-out the fix:
`RB3_NO_SKEL_REBIND=1`.

**OPEN (future work):** to animate head/hands cleanly, capture the animated per-member
skeleton's BIND-pose orientation (the rest frame, before the idle clip plays) and rebake each
outfit offset against it — i.e. `offset' = meshBindWorld · inverse(perMemberBoneBindWorld)`.
That bind frame is not available at the current rebind site (the skeleton is already animating
when reachable); it would need a hook at per-member skeleton load/first-pose, before the
director assigns a clip. Until then, torso-only is the clean shippable scope.

---

### wave-07 PROBE band-static (2026-06-06, BUILT + MEASURED) — the chain DRIVES the per-member skeleton fine; the break is the BIND (outfit meshes skin the STATIC shared magnet, not the animated per-member skeleton). SAME root cause as the wave-06 fling.

**TL;DR (this OVERTURNS the wave-06 "HARD FACT" and the wave-07 DC3-COMPARE hypothesis on
empirical, built-probe grounds).** The on-stage band animation chain is NOT broken at the
drive: a clip IS playing, `mFirst` is NON-null, weight is 1.0, `ScaleAdd(*mBones)` runs, and
the per-member skeleton bone the char pipeline poses MOVES 100-165u/frame across the whole
song. The band looks static ONLY because the **outfit skin meshes are bound to a DIFFERENT,
STATIC skeleton instance** (the shared `char/main/skeleton.milo` magnet at worldPos
(7.4,-0.8,57.5)) than the one the animation drives. The wave-06 "byte-identical across 1424
draws" fact was a MEASUREMENT ARTIFACT — `XBONE_TRACK` reads `owner->BoneTransAt` = the
OUTFIT-mesh's bound bone (the static magnet), which is genuinely static; it NEVER measured
the per-member skeleton the drive animates. **=> The "band doesn't animate" gap and the
wave-06 female-fling gap are ONE AND THE SAME bind/un-share problem.** Fixing the bind
(DESIGN-A band-scoped un-share so outfits bind the animated per-member skeleton) fixes BOTH
the static AND the fling at once. There is NO unported BandDirector / choreography / clip-
library gap — all of that machinery WORKS on native.

**THE CHAIN, traced stage-by-stage with a built probe (all HX_NATIVE, env-gated, default OFF):**

| stage | probe | result |
|---|---|---|
| (d) director assigns a clip to the band | `BAND_ANIM_PROBE` (BandCharacter::Poll) | **YES** — `mDriver` non-null, real venue idle clips: player0 `stand_realtime_idle_d_04`, player2 `ms_idle_adjust_rt`. NOT the empty fallback. |
| (a) `BandCharacter::Poll → Character::Poll → CharDriver::Poll` called per frame | `CHARDRV_PROBE` (CharDriver::Poll top) | **YES** — body driver `clipType='guitar_body'/'mic_body'` Polls every frame. |
| (b) a CharClip is PLAYING (`FirstPlaying`/`mFirst` non-null) | `CHARDRV` + `BAND_ANIM` | **YES** — body `mDriver->FirstPlaying()` non-null, `mFirst=0x..91a0` non-null. (Instrument drivers `kick`/`fret_left` have `mFirst=nil` — irrelevant, those are finger/foot sub-drivers.) |
| `mFirst->ScaleAdd(*mBones, weight)` reached | `CHARDRV_APPLY` (CharDriver::Poll, inside `if(mFirst)`) | **YES** — `weight=1.000` (NOT a zero-bootstrap deadlock — refutes DC3-COMPARE hypothesis #2), `apply=0` (kApplyBlend), `mBones` non-null. |
| (c) the playing clip UPDATES the skeleton bone WorldXfm frame-to-frame | `BAND_ANIM` pre/post around `Character::Poll()` | **YES — the per-member skeleton bone MOVES.** player0 bone_R-upperArm worldPos `moved` >1u in 30/35 samples (rest are idle-repeat frames); across a song run **662/666 samples moved≠0** (100-165u). The per-member skeleton is LIVE-animated. |
| **THE BREAK: outfit meshes bind a DIFFERENT, STATIC skeleton** | `BAND_ANIM bonePtr` vs engine `XBONE` outfit `bonePtr` | **animated per-member bone = `0x..ca28700` (moves). Outfit-bound bone = `0x..924ec0` (static at (7.4,-0.8,57.5)).** The animated pointer NEVER appears in the outfit XBONE list (count=0). ALL FOUR outfits (trackjacket/vestdenim/plaidshirt/shred + their `_skin.N` meshes) share the ONE static magnet pointer. |

**The decisive line (engine `XBONE=bone_R-upperArm`, band outfit meshes only):**
```
trackjacket_resource.mesh  bone='bone_R-upperArm.mesh' bonePtr=0x..924ec0 worldPos=(7.4,-0.8,57.5)
vestdenim_resource.mesh    bone='bone_R-upperArm.mesh' bonePtr=0x..924ec0 worldPos=(7.4,-0.8,57.5)
plaidshirt_resource.mesh   bone='bone_R-upperArm.mesh' bonePtr=0x..924ec0 worldPos=(7.4,-0.8,57.5)
shred_resource.mesh        bone='bone_R-upperArm.mesh' bonePtr=0x..924ec0 worldPos=(7.4,-0.8,57.5)
   (...all _skin.N share 0x..924ec0 too. The CharPipeline animates a SEPARATE 0x..ca28700.)
```
Contrast: crowd/extras meshes (`female_extras_skin02`, `male_extras_skin*`) bind DISTINCT
per-character bones at DISTINCT moving worldPos — they animate because their outfit IS bound
to their own animated skeleton. The band outfits are not.

**WHERE the chain breaks (file:line):** NOT in the drive — in the bind, established at
PARSE/merge time, identical to the wave-06 root cause:
- The outfit skin meshes' bone `ObjPtr`s are resolved/consolidated onto the shared
  `char/main/skeleton.milo` root via name resolution (`ObjectDir::FindObject`, `obj/Dir.cpp:531`)
  + `ReplaceRefs` at resource-parse — NOT onto the per-member `char/main/main.milo` →
  `skeleton_unshared.milo` instance the char pipeline poses. (See this doc's
  "2026-06-06 FAITHFUL FIX ATTEMPT §1-4" for the exact share mechanism: `DirLoader::Find`
  on the FilePath string `char/main/skeleton.milo`, preloaded `share=true`.)
- The renderer reads that bound (static) pointer: `BandRnd::DrawMesh`
  (`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` ~L3086) `owner->BoneTransAt(b)` →
  the static magnet. It does NO name lookup, so it cannot reach the animated per-member bone.
- The POSE pipeline DOES animate the per-member skeleton: `CharDriver::Poll`
  (`char/CharDriver.cpp:339`) `mFirst->ScaleAdd(*mBones)` → `CharBonesMeshes::PoseMeshes`
  (`char/CharBonesMeshes.cpp:98`) writes bone LocalXfms → resolved by NAME via
  `CharUtlFindBoneTrans` to the per-member instance (`0x..ca28700`). That instance is the
  one `BandCharacter::Find<RndTransformable>("bone_R-upperArm.mesh")` returns (my BAND_ANIM
  probe), and it moves. The outfit meshes just don't point at it.

**SCOPE: this is a MEDIUM-effort bind/un-share gap, NOT a large unported subsystem.** The
BandDirector choreography, song.anim clip-key evaluation, CharDriver/CharClipDriver, ScaleAdd,
PoseMeshes, IK, and the per-member skeleton load ALL WORK on native (probed live). The ONLY
missing piece is making the band outfit meshes bind the animated per-member skeleton instead
of the static shared magnet — the EXACT same DESIGN-A un-share this doc has scoped for the
fling. Prior sessions found the un-share inert/crash-prone, which is why the SHIPPED state is
the wave-06 static-offset REBAKE (coherent-but-static, accepting the static because it was
believed the whole band was static). **That belief is now FALSE: a working un-share/rebind
would yield a LIVE-ANIMATED band, not just a coherent static one.**

**FIX PATH (in priority order):**
1. **(best, the faithful fix) DESIGN-A band-scoped un-share / per-member rebind** — make the
   outfit skin meshes' bone ObjPtrs resolve to the member's own animated `skeleton_unshared.milo`
   (`0x..ca28700`-class) instead of the shared magnet. Two concrete entry points already
   scoped in this doc: (A) `BandCharacter::FilterSubdir` (`BandCharacter.cpp:1876`) — append the
   member's OWN kInlineCached `skeleton_unshared.milo` as the skeleton subdir so parse-time
   `FindObject` resolves per-member; (B) `BandCharacter::SyncObjects` (`BandCharacter.cpp:593`,
   after `Character::SyncObjects()`) — post-load `m->SetBone(b, Find<RndTransformable>(name),
   false)` rebind onto the per-member bone (the `Character::SyncShadow` precedent,
   `char/Character.cpp:651`). My probe REMOVES the prior blocker on (B): `Find` from the
   BandCharacter DOES reach the live per-member animated bone (`0x..ca28700`, moves) — the
   "Find returns the shared magnet" fear is FALSE for the BandCharacter's own `Find`. The
   make-or-break is whether `SetBone` with `calcOffset=false` keeps the gender-correct invBind
   while binding the moving bone; if the invBind was baked vs the magnet, also rebake the
   offset against the per-member bind frame (compose: the wave-06 rebake already computes the
   correct constant for the female).
2. **(minimum visible motion, low risk) Extend the wave-06 SKEL_REBAKE into a per-frame
   REBIND** — the rebake pre-pass in `BandRnd::DrawMesh` already locates the worst flung bone
   and its owning dir. Instead of (or in addition to) rebaking the static offset, look up the
   member's animated counterpart bone BY NAME in the per-member skeleton and skin against IT.
   This gives SOME visible band motion without touching the loader. Risk: the renderer doing a
   name lookup per draw is slow + needs the per-member dir handle at draw time.
3. **(verification helper)** Confirm DESIGN-A end-to-end with the probes left in tree:
   `BAND_ANIM_PROBE=*` (per-member bone moves), engine `XBONE=bone_R-upperArm` (outfit bonePtr
   == the animated pointer, worldPos no longer (7.4,-0.8,57.5) static), `char-burst-capture.py`
   (visible arm motion + female no longer flung).

**PROBES LEFT IN TREE (all HX_NATIVE, default OFF):**
- `BAND_ANIM_PROBE=<member-substr>|*` (`BandCharacter::Poll`, `BandCharacter.cpp` ~L411) —
  driver/clip state + named-bone (`BAND_ANIM_BONE`, default `bone_R-upperArm.mesh`) worldPos
  pre/post `Character::Poll()` + bonePtr.
- `CHARDRV_PROBE=<cliptype-substr>|*` (`CharDriver::Poll`, `CharDriver.cpp`) — two prints:
  `[CHARDRV]` (top: `mFirst`/`mBones`/beat) + `[CHARDRV_APPLY]` (inside `if(mFirst)`: weight,
  mBones, clip name — proves ScaleAdd reached).
- `SERVO_PROBE=<cliptype-substr>|*` (`CharServoBone::Poll`, `CharServoBone.cpp`) — meshes
  count + a bone LocalXfm pre/post `PoseMeshes`.

(Refutes below: the wave-07 DC3-COMPARE section's "mFirst==nullptr / no clip playing / weight-
bootstrap deadlock / TypeDef select_camera UNHANDLED" hypotheses — all measured FALSE here. The
DC3 freeze was a real DC3 bug, but RB3's native band does NOT have it: the clip plays, weight=1,
the per-member skeleton animates. The symptom looked identical only because the WRONG skeleton
was being measured.)

### wave-07 DC3-COMPARE (2026-06-06, READ-ONLY) — the STATIC-band gap is NOT the bind-divergence; it is the DC3-confirmed CHOREOGRAPHY-DRIVE / async-load gap

**TL;DR.** The on-stage band is static because **no CharClip is ever PLAYING on the band
members' CharDrivers** — the venue choreography that assigns/plays the band clips
(`song.anim` PropAnim → `select_camera` TypeDef → `BandDirector::OnSelectCamera` →
`SetFrame` → `play_group` on each `BandCharacter`) is not firing (or fires with a clip
whose layer weight never bootstraps). With `mFirst == nullptr` in `CharDriver::Poll`,
nothing writes `mBones`, `CharBonesMeshes::PoseMeshes` poses the BIND pose every frame, and
the drawn bone is byte-identical static. This is a DIFFERENT gap from the wave-06 female
gender-bind shard (which is the gender-mismatch offset, already mitigated by the rebake).
The "byte-identical WITH clip+IK" measurement is consistent: the `RB3_NO_CLIP/RB3_NO_IK`
env-flags being OFF (clip+IK "on") does NOT mean a clip is actually PLAYING — it only means
the drive code is not env-disabled. The band has no clip to play.

**DC3 hit this EXACT symptom and fixed it.** DC3's native dancers showed "characters freeze
during gameplay — song.anim advances, beat works, venue loads, but characters stand
motionless in their rest pose" (`dc3-decomp/docs/sessions/2026-03-17-character-animation-freeze.md`
+ `2026-03-23-character-animation-investigation.md` + `2026-03-17-song-anim-advancement.md`).
DC3 root-caused it to **async-load timing + DTA-flow gaps that prevent the choreography
clips from ever being assigned/played**, NOT a bind/skinning problem. DC3's fixes (all
HX_NATIVE, all in the SHARED-engine drive path RB3 also uses):

1. **Venue WorldDir lost its `world` TypeDef → `select_camera` UNHANDLED → PropAnim never
   advances.** `FileMerger::MergeDirs`→`MergeObject`→`Copy(kCopyFromMax)` SKIPS TypeDef
   transfer (`obj/Object.cpp:172`, `if (ty != kCopyFromMax)`); on Xbox a `{$world set_type
   world}` DTA handler restores it post-merge, on native that DTA never fires. With
   `TypeDef=(nil)`, `WorldDir::Poll`→`HandleType("select_camera")` is UNHANDLED, so
   `HamDirector::OnSelectCamera`→`songAnim->SetFrame(beat*30)` never runs → no clip keys
   evaluate → frozen. DC3 fix: `SetType("world")` in `HamDirector::VenueEnter`.
   **RB3 PARALLEL (the #1 suspect):** RB3's identical pipeline is `WorldDir::Poll`
   (`world/Dir.cpp:158`) → `HandleType(select_camera_msg)` → venue `world`-type TypeDef →
   `$banddirector select_camera` → `BandDirector::OnSelectCamera` (`BandDirector.cpp:1376`) →
   `mPropAnim->SetFrame(TheTaskMgr.Seconds(kRealTime)*30, 1)` (:1384). If the native venue
   WorldDir has no `world` TypeDef (same `kCopyFromMax` skip), `select_camera` is UNHANDLED →
   `OnSelectCamera` never runs → `mPropAnim` (song.anim) never advances → the band's
   `play_group`/`set_play` PropKeys never fire → BandCharacters get no clip → static. VERIFY
   FIRST (probe-agent): does `BandDirector::OnSelectCamera` run per-frame in gameplay, and
   does `mPropAnim->GetFrame()` advance?

2. **HamDriver::Poll weight-bootstrap deadlock** (`hamobj/HamDriver.cpp`): `Layer::mWeight`
   is uninit in the ctor; Xbox pool memory gives a nonzero bootstrap, native zero-init heap
   pins it at 0 forever, and the `mWeight > 0.0f` guard then prevents `Eval()` from ever
   running → clip queued but never evaluated → frozen. DC3 fix: HX_NATIVE bootstrap forcing
   `Eval(1.0f)` once when layers exist but weight ≤ 0. **RB3 PARALLEL:** check the analogous
   weight/eval guard in `CharDriver::Poll` / `CharClipDriver::Evaluate` — if the band's first
   clip is queued but its blend weight bootstraps to 0, `mFirst->ScaleAdd(*mBones, f14)` with
   `f14==0` writes nothing. (RB3 uses `CharDriver`/`CharClipDriver` directly, not HamDriver,
   so the exact member differs, but the zero-bootstrap class is the same.)

3. **Post-merge deferred choreography init** (`HamDirector::Initialize`): the choreography
   init (`SongInit`/PropKeys/MoveDir) runs at frame ~20 BEFORE the async merger completes
   (~frame 1600); nobody re-runs it, so the clip-player has no keyframes → 0 clips. DC3 fix:
   a post-merge `Initialize()` hook re-runs the init that failed early. **RB3 PARALLEL:**
   `mPropAnim` is bound in `BandDirector::OnFileLoaded` when `sym==song` (`BandDirector.cpp:1202`,
   `dir->Find<RndPropAnim>("song.anim")`); confirm it is non-null AND carries the per-member
   `play_group` SymbolKeys at gameplay (not the empty `unk110` fallback PropAnim built at :1210
   which only has shot_bg/intensity keys, NO char-clip keys → band would never get a group).

4. **Game beat-freeze** (`Game.cpp PostWaitStart`): when audio failed, `mRealTime=false` →
   `CurrentMs(false)`→`mAudio.GetTime()` returns 0 from a dead stream → beat frozen at 0 →
   clip frames never advance. DC3 fix: `mRealTime=true` + wall-clock offset. **RB3 PARALLEL:**
   `OnSelectCamera` uses `TheTaskMgr.Seconds(kRealTime)` (:1379) — if the realtime clock is
   pinned (audio-fail / no song clock), `SetFrame` gets a constant frame and the choreography
   never plays. (RB3's audio is now working per wave-05, so this is lower-suspect, but the
   `kRealTime` source must be advancing.)

**Why DC3 native ANIMATES the same SHARED engine RB3 has static.** The per-frame char-anim
engine is IDENTICAL across both games: `Character::Poll` (`char/Character.cpp`) → `RndDir::Poll`
iterates `mPolls` → `CharDriver::Poll` (`char/CharDriver.cpp:339`) reads beat from
`TheTaskMgr.Beat()`, advances the playing clip via `mFirst->PreEvaluate/Evaluate`, and writes
the pose into `mBones` via `mFirst->ScaleAdd(*mBones, weight)` → `CharBonesMeshes::PoseMeshes`
(`char/CharBonesMeshes.cpp:98`) writes those bone values into the skeleton `RndTransformable`
LocalXfms (resolved by NAME via `CharUtlFindBoneTrans`). DC3's dancers animate because (a)
their CharDriver HAS a playing clip (the MoveDir/HamDriver choreography assigns it every
song frame) AND (b) DC3 fixed the four native-flow gaps above so that clip actually plays and
advances. RB3's band is static because the equivalent choreography-assign step never fires a
playing clip onto the BandCharacters' CharDrivers — the engine machinery downstream is fine
(it poses the bind pose because there is nothing to blend).

**This SUPERSEDES the wave-06 "band skeleton is physically static / un-animatable" framing
for the ANIMATION axis.** The wave-06 conclusion was correct about the SHARD/FLING (a
gender-bind offset mismatch, fixed by the rebake) but mis-attributed the STATIC-ness to an
un-fixable bind layer. It is fixable: it is a drive-assignment gap of exactly the class DC3
already solved. The two are independent — the rebake handles WHERE the female arm sits
relative to its bone; the drive fix handles WHETHER the bone moves at all (for the whole band,
males included).

**RB3-side gap (concrete) + port path.** The probe-agent must first run the existing
`BAND_ANIM_PROBE` (already in `src/system/bandobj/BandCharacter.cpp:411-471`, env
`BAND_ANIM_PROBE=*`, optional `BAND_ANIM_BONE=`) which prints, per band member per ~30 frames:
the CharDriver pointer, `FirstPlaying` clip name (or `(none)`), the bones pointer, and the
probe bone's worldPos BEFORE vs AFTER `Character::Poll()` with a `moved=` delta. The decision
tree:
- **`clip='(none)'`** (most likely): no clip is playing → the choreography-assign is the gap.
  Then probe whether `BandDirector::OnSelectCamera` runs and `mPropAnim->GetFrame()` advances:
  if NOT → the venue `world` TypeDef / `select_camera` dispatch gap (DC3 gap #1) → port DC3's
  `SetType("world")`-in-VenueEnter pattern (RB3 seam: `BandDirector::EnterVenue`/`LoadVenue`,
  `BandDirector.cpp:593-720`, where the venue WorldDir is force-loaded natively). If
  `mPropAnim` advances but the band still gets `(none)` → confirm the loaded `song.anim` has
  per-member `play_group` SymbolKeys (not the `unk110` empty fallback) and that the song.anim
  PropKeys actually `Handle` the `play_group` message on the BandCharacters (DC3 gap #3 class).
- **`clip='<name>'` but `moved≈0`**: a clip is playing but not driving the bone → the
  weight-bootstrap/blend-weight-0 class (DC3 gap #2) → check `CharDriver::Weight()` /
  `CharClipDriver::mBlendFrac` / `Sigmoid(mBlendFrac)` bootstrapping to 0 on native, OR the
  posed instance ≠ drawn instance (the wave-06 magnet split — but that would ALSO desync the
  males, and the rebake already handles the female offset). Lower-probability; the
  `(none)` branch is the expected outcome.
- Entry points to port from: DC3 `HamDirector::VenueEnter` `SetType("world")` (gap #1);
  DC3 `HamDriver::Poll` HX_NATIVE weight bootstrap (gap #2 pattern); DC3 `HamDirector::
  Initialize` post-merge re-init (gap #3 pattern). All are HX_NATIVE, Wii byte-identical.
  Effort: LOW-MEDIUM for gap #1 (one TypeDef set at the venue seam) if the probe confirms
  `select_camera` is UNHANDLED; MEDIUM if the gap is in the song.anim PropKeys → BandCharacter
  message dispatch. Mirror with exact citations: `docs/native/audio-perf-loop/wave-07-band.md`.

---

Status: **LANDED (2026-06-06, wave-06): scoped static-pose offset REBAKE for the
female band outfit arm/torso (trackjacket skinPos 19.8u→0, default-on, no regression).
The orchestrator's "LIVE-animated female" bar is PHYSICALLY UNMEETABLE at the bind layer:
the whole native band skeleton is provably STATIC (proven below), so DESIGN-A/B's
"rebind to a live per-member skeleton" is impossible without a deep loader un-share that
multiple prior attempts proved inert/crash-prone. The rebake brings the female to the
same static-but-coherent quality the 3 male members already have.**

### wave-06 IMPLEMENT (measured)

PROBE FIRST (built `rb3-native`, `char-burst-capture.py`, clamp OFF for some probes).
Three runtime facts SETTLED the A-vs-B decision and retired BOTH designs as written:

1. **Own == shared (DESIGN-B rebind is a no-op).** Added a probe in
   `BandCharacter::SyncObjects` (HX_NATIVE, env `SKEL_REBIND_PROBE`): for each skin mesh
   reachable from the member (`ObjDirItr<RndMesh>(this,true)`), compare
   `Find<RndTransformable>(boneName)` against the currently-bound bone. Result on the
   member that had its outfit merged: `skinMeshes=4 slots=4 reboundDiff=0 same=4` — `Find`
   returns the SAME shared bone the outfit is already bound to. And `distinct upperArm
   instances in member subtree = 1` (only the magnet, `dirFile='char/main/skeleton_unshared.milo'`).
   So there is NO per-member skeleton instance reachable from the BandCharacter to rebind
   onto. The char POSE pipeline (`CharUtlFindBoneTrans`→`dir->Find<CharBone>`) resolves the
   SAME magnet, so the pose pipeline also poses only the magnet.

2. **The magnet is STATIC even WITH clip+IK (kills constant-correction-as-animated AND
   confirms there's no live female pose).** Added engine `XBONE_TRACK=<bone>` (prints the
   trackjacket bone's worldPos every draw). With clip+IK ON, over a whole song:
   `bone_R-upperArm worldPos = (7.4020,-0.8298,57.5293)` — **byte-identical across 1424
   draws, ONE distinct value.** The shared band skeleton never animates. The 3 males look
   correct only because their outfit invBind matches that static male bind; the female
   trackjacket invBind (female-baked) does not → skinPos (19.8,3.8,0.4).

3. **Band draw flow = pose-ALL-then-draw-ALL** (confirmed from source, App.cpp:506 Poll
   before :551 Draw; RndDir separate mPolls/mDraws). Irrelevant here because the bound
   skeleton is a single STATIC object no member animates.

DECISION: A live-animated female is impossible at the bind/offset layer (the skeleton is
static; there is no per-member posed instance; the only reachable+posed skeleton is the
static male-bind magnet). Both DESIGN-A's `FilterSubdir` un-share and DESIGN-B's
`SyncObjects` rebind require a per-member POSED skeleton that does not exist without the
deep, crowd-affecting loader un-share — which prior sessions proved inert (preload-skip,
LoadSubDir un-share, char_shared clone) or crash-prone (`CharServoBone.cpp:179` on dir
copy). So the shippable REAL fix (better than the clamp, lower risk than the un-share) is
a **constant per-member static-pose REBAKE**: because the magnet is provably static, there
is NO recompute trap — `mOffset = meshWorld * inverse(boneWorld)` is a permanent, stable
correction that makes the female arm compose to identity → a coherent, correctly-POSED arm
(not the clamp's authored-T-pose stub). This matches the static-but-coherent quality the 3
males already have.

IMPLEMENTED (`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`, `BandRnd::DrawMesh`, a
pre-pass before the bone-palette loop, default-on, opt-out `RB3_NO_SKEL_REBAKE=1`):
- BAND-ONLY scope: only act if the mesh's worst flung bone's owning dir is
  `skeleton_unshared.milo` (the static band magnet). Crowd/extras bind their OWN per-char
  ANIMATED skeletons (`char/crowd/*`, `char/extras/*`) — never touched (rebaking those
  would freeze the clap/body animation = the recompute trap).
- DYNAMIC-CHAIN EXCLUSION: skip face/hair/fingernail MESHES (and any hair/finger/face
  bone by name) — those are driven every frame by CharFaceServo/CharHair/CharIKFingers, so
  a static rebake doesn't stick. They keep the shipped per-frame fling clamp (unchanged).
- Rebake only the individual STATIC arm/twist/torso/leg bones whose mesh-local skin > 12u.
  Do NOT set `mNativeBonesRebound` for dynamic meshes, so the clamp stays live for them.
- The clamp (`RB3_NO_SKIN_CLAMP`) is untouched and stays as the backstop.

DECOMP support (`src/system/rndobj/Mesh.{h,cpp}`, HX_NATIVE — Wii BYTE-IDENTICAL, verified:
`Mesh.o`/`BandCharacter.o` mwcc-rebuilt, match% unchanged 62.01% / 77.31%): added
`bool mNativeBonesRebound` (appended after the matched layout, ctor-init false) consumed by
the engine's clamp-skip gate (this was DORMANT WIP referencing a member that didn't exist —
built ON it, did not revert). `BandCharacter::SyncObjects` carries the SKEL_REBIND probe
(diagnostic, default OFF, enable `SET_SKEL_REBIND=1`).

MEASURED (built + run, `char-burst-capture.py --shots 45`, multiple bursts to catch the
band-closeup camera cut; the female draws in ~5/6 bursts but the camera rarely frames her
tightly — XBONE confirms she IS drawn):

| metric | BEFORE (clamp, rebake off) | AFTER (rebake default-on) |
|---|---|---|
| `XBONE` trackjacket_resource bone_R-upperArm skinPos | (19.80, 3.84, 0.36) FLUNG | **(-0.00, 0.00, 0.00)** |
| 3 male outfits (vestdenim/plaidshirt/shred) skinPos | (0,0,0) | (0,0,0) (untouched) |
| `SKIN_CLAMP_PROBE` clamps on trackjacket | n/a (clamp froze it to T-pose) | **0** (rebake does the work) |
| `SKIN_CLAMP_PROBE` clamps on crowd/extras (clap/body/fist) | fires | fires (unchanged backstop) |
| band dynamic meshes in SHARD_CATCH (fingernails/facehair/blownback) | 0 / 0 / 0 | **0 / 0 / 0** (no regression) |
| post-rebake re-fling (clamp on trackjacket across 45-shot burst) | n/a | **0** (rebake is stable; magnet static) |
| meshes rebaked (SKEL_REBAKE_PROBE) | — | trackjacket_resource + trackjacket_skin.2 ONLY |

ANIMATED-not-frozen: the female arm is STATIC after the fix — but so is the ENTIRE native
band (XBONE_TRACK: 1424 identical samples WITH clip+IK; the males are static too). My fix
does not freeze anything that was animating; it makes the female's static pose COHERENT
instead of shattered, matching the males. True animation requires the whole band skeleton
to be posed (the deep loader un-share), which is out of scope here and proved inert/risky.

SCREENSHOTS: band members render coherently (no shards/spikes); venue/HUD/highway/menus
unchanged; no crashes/asserts in a default no-env run. The female-singer tight closeup is
camera-rare in the test song (small_club, guitar), so the objective skinPos→0 + zero
clamp-on-trackjacket + zero re-fling are the decisive proofs.

REGRESSION (checked): males 0→0; crowd/extras still clamped (their animated skeletons
untouched, never rebaked — band-only `skeleton_unshared.milo` scope); dynamic band
hair/face/finger meshes keep the clamp (mesh+bone-name exclusion, SHARD_CATCH counts
unchanged 0/0/0); Wii byte-identical (HX_NATIVE, match% unchanged); DC3-inert (matched-
gender dancers never hit the mixed-gender band path); opt-out `RB3_NO_SKEL_REBAKE=1`
restores the clamp-only shipped behavior.

DIAGNOSTICS LEFT IN-TREE (all default-OFF unless noted): engine `XBONE_TRACK=<bone>` (per-
frame worldPos, settles static-vs-animated), `SKEL_REBAKE_PROBE` (which meshes/bones
rebaked); RB3 `SET_SKEL_REBIND=1`/`SKEL_REBIND_PROBE=1` (the no-op rebind probe).
`RB3_NO_SKEL_REBAKE=1` is the fix opt-out (default ON).

### wave-06 DESIGN-A un-share

Surgical band-only loader UN-SHARE (READ-ONLY, no build). Supersedes the CHAR-1/CHAR-2
rebind/rebake designs below (both REFUTED 2/2 empirically: a `SetBone` rebind to
`Find(name)` is a no-op when `Find` returns the shared magnet, or binds a dormant static
male-bind skeleton — there is no LIVE female-posed instance to bind to UNTIL the magnet is
removed from the band member's name resolution). DESIGN-A removes the magnet so the member's
own skeleton becomes the name winner for BOTH the outfit bind AND the pose pipeline at once.

**The decisive new trace — WHEN the bind is established.** The outfit skin mesh's bone
pointer is fixed at the **resource/char_shared milo's PARSE time**, not at the band-member
merge. `RndMesh::Load` parses `bs >> mBones` (`rndobj/Mesh.cpp:750`); each `RndBone::mBone`
(`ObjPtr<RndTransformable>`) resolves its bone NAME via `mOwner->Dir()->FindObject(name,
false)` (`obj/ObjPtr_p.h:536-542`), which descends `mSubDirs` (`obj/Dir.cpp:531-540`). The
female torso `trackjacket_solid.milo` → `../../../shared/char_shared.milo` subdir →
`../skeleton.milo` subdir → `skeleton_unshared.milo` (316 bones, verified by `strings` on the
`_xbox` milos). At torso-parse, `FindObject("bone_R-upperArm.mesh")` descends to that
`skeleton.milo` — **the SHARED preloaded instance** (`char_shared.milo` band-preload + every
`*_resource.milo`'s `../main/skeleton.milo` subdir resolve through `DirLoader::Find`,
`DirLoader.cpp:78`, to one loader). So the bones are male-bind-bound the moment the shared
skeleton answers the parse-time `FindObject`. This is why "prune after merge" failed (already
resolved) and why a post-merge `SetBone` rebind only helps if `Find` returns the per-member
instance (the orchestrator's split-probe: distinct at OnInstallFilter §2, shared at draw §3/4).

**Why the per-member `skeleton_unshared.milo` is unused.** `char/main/main.milo` references
`skeleton_unshared.milo` directly as **kInlineCached** (fresh per-member copy, 8-9 distinct,
`shared=0`) AND transitively the shared `skeleton.milo` via resource/char_shared subdirs.
Both instances sit under the same bone names in the member's dir; the shared `skeleton.milo`
subdir (appended by `FilterSubdir`→`kReplace`) wins the `FindObject` descent over the
inline-cached copy buried under `main.milo`'s inline subtree.

**Band-only scope (blast-radius containment).** The share key is a FilePath STRING
(`char/main/skeleton.milo`), referenced by 10 files (`char_shared.milo` + 9 `*_resource.milo`:
deform/shell/vignette/extras/viseme/keyboard/vocal/guitar/drum); vignette/extras/shell
resources also serve the CROWD. The containment is that crowd/extras do NOT merge through a
`BandCharacter` filter: `FileMerger::FilterSubdir` (`char/FileMerger.cpp:317`) dispatches to
`mFilter` only when set; `BandCharacter` sets `mFileMerger->mFilter = this`
(`BandCharacter.cpp:1961`, in `OnInstallFilter`); the crowd's `crowd_clips.fm` has none. So
`BandCharacter::FilterSubdir`/`Filter` run ONLY for the 4 band members — the SAME seam as the
existing white-texture shim and `sCharSharedDir`/`sBoneMergeDir` remaps. DC3 dancers never hit
this path (matched-gender, named chars).

**The fix (one HX_NATIVE branch at `BandCharacter::FilterSubdir`, `BandCharacter.cpp:1876`).**
When `FilterSubdir`'s `o1` is the shared `skeleton.milo` root (detect: `o1->mStoredFile`
basename == `skeleton.milo` AND `o1->FindObject("bone_R-upperArm.mesh")`), append the member's
OWN kInlineCached `skeleton_unshared.milo` as the skeleton subdir instead of the shared magnet.
No new copy (reuse the instance `main.milo` already loaded → NO CharServoBone ref-remap crash
that killed the prior dir-`Copy`), no global un-share (band-scoped by the mFilter seam). Then
the outfit bones parse-resolve to the per-member skeleton, AND `CharBonesMeshes::PoseMeshes`
→ `CharUtlFindBoneTrans(name, Dir())` (`char/CharUtl.cpp:183`, same name resolution) poses
that SAME per-member instance to the member's gender bind (deform-clip morph + idle clip + IK).
Both the bind and the pose follow from making the per-member instance the name winner — THE
difference from the refuted SetBone rebind, which moved only the outfit's read pointer.

**The make-or-break (feasibility: MODERATE).** Parse-order: the resource/char_shared milos
must `FindObject`-resolve bones AFTER the per-member skeleton is the name winner. Probe
`XBONE=bone_R-upperArm` post-fix under `RB3_NO_CLIP=1 RB3_NO_IK=1`: trackjacket bonePtr must
DIFFER from vestdenim/plaidshirt/shred (distinct per-member), THEN with clip+IK confirm
skinPos 19.8→0, animated, no re-fling across `char-burst-capture.py --shots 40`. Fallback if
parse-order can't be satisfied at `FilterSubdir`: layer DESIGN-B's `SetBone` rebind in
`SyncObjects` ON TOP (un-share fixes the pose pipeline's pointer; rebind catches any outfit ref
already consolidated). Shipped fling-clamp stays as backstop. Wii path byte-identical (all
edits HX_NATIVE). Full detail + exact entry points:
`docs/native/audio-perf-loop/wave-06-char.md` `### A`.

### wave-06 DESIGN-B drawflow

READ-ONLY draw-flow analysis (no build). Settles the orchestrator's empirical
question on paper: is the band POSE-then-DRAW per member, or POSE-ALL-then-DRAW-ALL,
and can a CONSTANT per-bone correction therefore work, or is a true per-member
skeleton (DESIGN-A) the only viable path? Conclusion up front: **the band is
unambiguously POSE-ALL-then-DRAW-ALL**, so a constant per-bone offset correction
canNOT work IF the female outfit's bound skeleton is the *animated, shared, last-
member-wins* root — but the prior probes show the bound root is actually the *static
male-bind* `skeleton.milo` magnet that no member animates, which changes the calculus.
The honest recommendation is **DESIGN-A (per-member live skeleton) — there is no
clean constant-correction shortcut.** Reasoning, traced to source:

**1. The frame is two GLOBAL passes, not an interleaved per-member pose+draw.**
`App::RunOneFrame` (`src/App.cpp`): `TheBandDirector->Poll()` (line 506) runs to
*completion* — posing the entire venue world graph and all four band members — BEFORE
`TheUI.Draw()` (line 551/558) runs the *entire* draw pass. There is no per-member
pose→draw interleave at the frame level.
- POSE pass: `BandDirector::Poll` (`BandDirector.cpp:242`) → `mCurWorld->Poll()` (:245)
  → `WorldDir::Poll` (`world/Dir.cpp:124`) → `RndDir::Poll` (`rndobj/Dir.cpp:162`)
  iterates `mPolls` and Polls **every** band member (and the rest of the world graph)
  in one sweep; each `BandCharacter::Poll` (`BandCharacter.cpp:315`) →
  `Character::Poll` (`char/Character.cpp:217`) → `RndDir::Poll` drives that member's
  CharDriver/CharClip/CharBones/IK/twist — writing bone LocalXfms/WorldXfms.
- DRAW pass: `WorldDir::DrawShowing` (`world/Dir.cpp:393`) → the HX_NATIVE band-draw
  bridge (`Dir.cpp:448-461`) loops `bi=0..3` calling `bandChar->DrawShowing()` →
  `Character::DrawShowing` → `DrawLod` → `mOutfitDir->DrawLodOrShadow` → the outfit
  skin meshes reach `BandRnd::DrawMesh` (`Rnd_Wgpu_RB3.cpp:3086`), which reads each
  bone's CURRENT `WorldXfm` and the mesh's baked `BoneOffsetAt`. All four are drawn
  *after* all four were posed. **POSE-ALL-then-DRAW-ALL. Confirmed.**

**2. Why "pose-all-then-draw-all" normally damns a constant correction.** If all four
outfit meshes bound the SAME live skeleton object, that skeleton would hold only the
LAST-posed member's pose at draw time, and every member's draw would skin against that
one wrong pose. A constant per-bone offset baked for the female bind can only cancel a
SINGLE fixed world orientation; if the shared skeleton's draw-time world were the
last-male's *animated* pose it would change frame to frame and a constant offset could
not track it. That is the textbook reason a constant correction fails under
pose-all-draw-all and only a per-member skeleton (each holding its own pose at draw)
works.

**3. BUT the bound skeleton is NOT animated — it is the STATIC male-bind magnet.**
The decisive runtime fact (this file, §ROOT CAUSE / 2026-06-05; `XBONE` probe):
`bone_R-upperArm`'s shared bonePtr is identical across all four outfit meshes AND
`worldPos=(7.4,-0.8,57.5)` "never moves — at bind AND during playback" (lines 387-388).
So the object the outfit meshes skin against is `char/main/skeleton.milo`'s static
male-bind root (the name-resolution magnet), which **no band member's Poll animates**.
The per-member char pipeline poses each member's OWN `char/main/main.milo` skeleton
(`skeleton_unshared.milo`, 8-9 distinct kInlineCached instances — `SKEL_LOAD_PROBE`,
2026-06-06 §1), and those ARE live and gender-posed, but the outfit meshes do not read
them (they consolidated their bone ObjPtrs onto the shared magnet via `ReplaceRefs` at
resource-parse, 2026-06-06 §4). The "last-member-wins on one shared pose" failure mode
above therefore does NOT actually occur — the bound pose is constant (static male
bind), independent of any member.

**4. So could a CONSTANT correction work after all? Almost — but it is equivalent to
the recompute trap, and it is NOT the faithful fix.** If the bound skeleton truly never
moves, then `off_female × world_staticMaleBind` is a single constant matrix per bone,
and a constant per-bone delta `D = world_staticMaleBind⁻¹ × world_femaleBind` baked
into `off_female` would make `skin == I` at the female bind and stay there — because
nothing animates. That is exactly what `SetBone(i, bone, /*calcOffset=*/true)` /
`RB3_RECOMPUTE_OFFSETS` computes. BUT this is the **already-refuted recompute path**:
- It produces a **frozen** female (the bound skeleton is static — correcting the offset
  to it gives a posed-but-immobile arm, the male bind expressed as a fixed female-bind
  silhouette, NOT a live animated arm). The female would not move with the song. That
  fails the orchestrator's own bar ("animated not frozen").
- And if the bound skeleton is *not* perfectly static in every venue/closeup state
  (the recompute trap, line 306: idle clip from songMs=0 + IK reachability post-poll),
  the constant bake captures a mid-idle/post-IK pose and re-flings on later frames.
The constant correction is thus either FROZEN-but-coherent (no better than the shipped
clamp, which already gives a coherent model-space pose without a bake) or RE-FLINGS.
Neither is the faithful, live-animated female the goal requires.

**5. Verdict — DESIGN-A is the only path to a LIVE-ANIMATED female.** The reason is
not the pose-all/draw-all ordering per se (that alone would be survivable since the
bound pose is constant); it is that **the outfit meshes are bound to a skeleton NO
MEMBER ANIMATES.** A constant correction can only ever reproduce that static skeleton's
(now female-shaped) pose. To get a *live* female arm, the female outfit meshes must
bind to a skeleton that her OWN Poll animates to her OWN gender bind — i.e. her
per-member `main.milo` instance, posed live by her CharBones/IK. That is precisely
DESIGN-A's "each band member POSES + BINDS its own gender-correct skeleton." The two
sub-problems remain exactly those mapped 2026-06-06: (1) band-scoped UN-SHARE of
`char/main/skeleton.milo` so the female outfit's bone ObjPtrs land on her own
`main.milo` skeleton instead of the magnet; (2) ensure her per-member skeleton is
posed to the FEMALE bind (the gender bind enters via `GetDeformClip(mGender)` at
`SetDeformation`, `BandCharacter.cpp:1087`, which is a load-time body-MORPH, plus the
live clip/IK on her own bones). Because pose-all-draw-all holds, the rebind variant
(wave-05 CHAR-1) is *correct by construction* ONLY IF the per-member skeleton each
outfit rebinds to is the live-posed one — which it now is (the per-member instances
ARE animated; the magnet is the dormant one). That makes the **SyncObjects per-member
rebind onto the LIVE `main.milo` bones (NOT the dormant magnet) the recommended
concrete DESIGN-A landing**, and the orchestrator's "rebind was a no-op / hit a dormant
static skeleton" refutation is avoided specifically by targeting `Find` at the member's
own `main.milo` instance (verified distinct + live, 2026-06-06 §1-2) rather than the
shared `skeleton.milo` root.

**Constant-offset feasibility, stated plainly for the Implement agent's A/B.**
- IF the build shows the bound `bone_R-upperArm` `worldPos` is byte-identical every
  frame during real playback (static magnet, as 388 claims) → a constant correction
  WOULD null the fling but the female stays FROZEN (no win over the clamp). Reject.
- IF the build shows it MOVES during playback (some path animates the magnet) → the
  pose is last-writer-wins across members and a constant correction CANNOT track it.
  Reject. Either way the constant correction loses; **proceed to DESIGN-A rebind.**
- The single build that decides this: `XBONE=bone_R-upperArm` with clip+IK ON
  (no RB3_NO_CLIP/RB3_NO_IK), `char-burst-capture.py --shots 40`, watch whether the
  shared `worldPos` is constant across frames. (DESIGN-A does not depend on the
  answer; this just retires the constant-correction option on evidence.)

Recommended path: **DESIGN-A — per-member rebind in `BandCharacter::SyncObjects`
(`BandCharacter.cpp:593`, after `Character::SyncObjects()` :648) onto the member's own
live `main.milo` skeleton bones (`Find<RndTransformable>(BoneTransAt(b)->Name())`
resolving to the per-member instance, `SetBone(b, own, /*calcOffset=*/false)` to keep
the authored gender-correct invBind), HX_NATIVE-gated; shipped clamp stays as
backstop.** Feasibility: HARD — the make-or-break risk is that `Find` from
`SyncObjects` still resolves to the shared magnet (post-consolidation `ReplaceRefs`),
not the per-member instance; if so the band-scoped loader un-share (2026-06-06 blocker
#1) is required first. The Implement agent must PROVE `own != shared` AND `own` is
live-female-posed (skinPos 19.8→0 WITH clip+IK, animated not frozen) before claiming
the fix. Mirror: `docs/native/audio-perf-loop/wave-06-char.md` `### B`.

---

wave-05 CHAR-2 (below) maps a DRAW/BIND-side fix that does NOT need the loader
un-share: a load-time per-bone offset rebake against the female member's gender
bind, sourced from `GetDeformClip`. wave-05 CHAR-1 (below) RECOMMENDS the single
faithful fix: a per-member bone REBIND in `BandCharacter::SyncObjects` (the engine's
own `Character::SyncShadow` `SetBone` pattern) onto the member's already-live,
gender-posed skeleton — keeping the authored offset, no loader un-share, no offset
recompute. Lowest risk; composes with the shipped clamp.

### wave-05 CHAR-2 draw fix

Second-angle review of the fix at the renderer / inverse-bind-offset layer
(read-only, no build). The audit confirms the documented root cause AND finds a
draw/bind-side fix that sidesteps CHAR-1's loader blocker.

**The renderer does NO name lookup at draw.** `BandRnd::DrawMesh`
(`Rnd_Wgpu_RB3.cpp`) reads the bone pointer and the inverse-bind offset straight
out of the OUTFIT MESH's own `mBones` array:
- `2705` `owner = mesh->GeomOwner()`
- `3086` `RndTransformable* bt = owner->BoneTransAt(b)` = `mBones[b].mBone`
  (`Mesh.h:256`)
- `3137` `Multiply(owner->BoneOffsetAt(b), wt, skin)`, `BoneOffsetAt` =
  `mBones[b].mOffset` (`Mesh.h:257`)
- `3290` `MiloXfmToColMajor(skin, dst)` → GPU palette.
So the skeleton choice is ALREADY baked into the `mBone` ObjPtr the loader
resolved (the shared male-bind `skeleton.milo` root — BAND_DRAW_PROBE). The
renderer cannot pick a different bone; it follows the ObjPtr.

**Why a pure draw-side rebind (fix-a) does NOT stand alone.** The rebind
mechanism is trivially present — `RndMesh::SetBone(idx, bone, calcOffset)`
(`Mesh.cpp:317`): `mBones[idx].mBone = bone;` + optional
`mOffset = WorldXfm() * inverse(bone->WorldXfm())`. Per-member skeletons exist
and are findable (INSTALL_PROBE). BUT the **gender bind is not in the skeleton —
it is a per-member deform CLIP applied at pose time:**
`BandCharacter::SetDeformation` (`BandCharacter.cpp:1086`) →
`BandCharDesc::GetDeformClip(mGender)` → `StuffBones/ScaleAdd/PoseMeshes`.
`skeleton_unshared.milo` is itself male-bind (`BandCharDesc` ctor `mGender`
default `"male"`, `BandCharDesc.cpp:355`). Rebinding the female outfit to a
DORMANT per-member `main.milo` skeleton binds it to a STATIC male-bind pose
(still flung, now also un-animated). Rebinding to a LIVE female-posed skeleton
requires that skeleton to exist and be driven by the char pipeline — which is
CHAR-1's loader problem; the char pipeline currently poses ONE shared root
(`CharBoneDir::FindResource` → shared `sResources`, `CharBoneDir.cpp:36/60`),
male-bind because 3/4 members are male. The draw layer can REBIND cheaply but
cannot MANUFACTURE the female pose.

**Why a draw-time offset recompute (fix-b) does NOT stand alone.**
`SetBone(i,bone,true)` makes `skin=I` by construction at the snapshot frame; the
only frame the outfit mesh is first reachable at draw is post-IK, so it bakes a
mid-idle pose and the IK re-flings later frames (the documented
`RB3_RECOMPUTE_OFFSETS` trap). No clean bind frame exists at draw.

**The faithful draw/BIND-side fix (recommended).** Do the offset rebake at the
ONE frame the female bind is valid — INSIDE the deform pass, at LOAD, before any
idle clip or IK runs. After `SetDeformation`'s `clip->PoseMeshes()` poses the
member's own bones to its GENDER bind (`BandCharacter.cpp:1086`), the runtime
bone worlds momentarily ARE the female bind. Snapshot there and rebake each
female outfit-skin mesh's `mOffset` via `owner->SetBone(b, BoneTransAt(b),
true)`. Result: `skin = invBind_rebaked * boneWorld_live` cancels to identity at
bind AND tracks the live shared-skeleton animation (the female pose is just the
shared skeleton expressed through the corrected offset). This is a CONSTANT
per-bone correction (bind-to-bind delta), independent of the live animation —
not a runtime per-frame recompute — so it does not suffer the post-IK trap.
Touches ONLY the female member's outfit offsets, at load, HX_NATIVE-gated inside
`SetDeformation`. Keeps the SINGLE shared live skeleton (no loader un-share, no
CharServoBone-ref crash).

**Composition with the shipped clamp.** Unchanged. After the rebake the female
arm bones compose <8u → the clamp (`Rnd_Wgpu_RB3.cpp:3200-3230`, fires >12u)
never acts on them, so it stops freezing them at T-pose (the rebake gives a
PROPERLY POSED arm instead). The clamp stays as a pure backstop for crowd/extras
servo flings and any residual hair/face bone. Fixup runs at load (offset edit);
clamp runs at draw (palette reject) — no conflict.

**A/B.** `XBONE=bone_R-upperArm RB3_NO_CLIP=1 RB3_NO_IK=1` (≥70 shots): assert
trackjacket_resource skinPos 19.8→0, males stay 0. Then WITH clip+IK:
`SHARD_CATCH` = 0 trackjacket arm lines across a burst (the runtime recompute
re-flung here; the bind-frame rebake must not). `SKIN_CLAMP_PROBE=1`: clamp now
0× on trackjacket, still fires crowd/extras. Venue: `char-burst-capture.py
--shots 40`, female upper body coherent, males unchanged.

**Risk.** Female-only, load-time, HX_NATIVE; Wii path byte-identical; DC3-inert
(matched-gender dancers never hit the path). Clamp untouched = no regression to
shipped behavior even on mis-fire. Open verify: confirm the `SetDeformation`
pose frame is the same bind the outfit invBinds were authored against (XBONE A/B
before landing); residual per-bone delta is covered by the clamp.

Files: `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (clamp stays, no
edit needed for the fix itself) + `src/system/bandobj/BandCharacter.cpp`
`SetDeformation` (DECOMP, HX_NATIVE-gated — coordinate with CHAR-1, it is a
dormant-WIP file). Detail mirror: `docs/native/audio-perf-loop/wave-05-char.md`
`### angle-2`.

### wave-05 CHAR-1 loader fix

First-angle (loader / name-resolution) review. Pins the share magnet to EXACT
source, finds the engine's OWN post-load mesh-bone rebind precedent, and lands on
a faithful fix that is **lower-risk than the loader un-share AND than CHAR-2's
offset-rebake**: it re-points the outfit meshes' bone ObjPtrs to the member's OWN,
already-live, already-gender-posed skeleton — keeping the 8 per-member skeletons the
loader ALREADY creates. No loader change, no offset recompute, no dir copy.

**Root cause — pinned to exact source.** Share key = the on-disk FilePath string.
- Magnet created: `src/system/obj/Dir.cpp:1021` `PreloadSharedSubdirs()` →
  `gPreloaded[…].LoadFile(FilePath("char/main/skeleton.milo"), async=false,
  share=true, …)` (Dir.cpp:1033). List: `config/preload_subdirs.dta:24` (`char`).
- Shared by NAME: `ObjDirPtr::LoadFile` (`src/system/obj/Dir.h:63`), `share==true` →
  `d = DirLoader::Find(p)` (Dir.h:67) → `DirLoader::Find`
  (`src/system/obj/DirLoader.cpp:78`) returns the first loader with `mFile == fp`.
  (`FindLast`, DirLoader.cpp:92, is the reverse variant for the inlined-dir fixup.)
- Two share entry points: `ObjectDir::LoadSubDir` (Dir.cpp:849 → `LoadFile(subdirpath,
  true, b, …)` :860); and `ObjectDir::PostLoad` shared-inlined fixup (Dir.cpp:402-411,
  `DirLoader::FindLast` → `iDir.dir = last->GetDir()`).
- Bone refs resolve at the resource milo's PARSE time: `ObjectDir::FindObject`
  (Dir.cpp:531, descends `mSubDirs` 535-540) finds `bone_R-upperArm.mesh` in the shared
  `skeleton.milo`; `RndBone::mBone` (`rndobj/Mesh.h:45`, `bs >> mBone >> mOffset` :50);
  `BoneTransAt` (Mesh.h:256) returns it at draw.
- Merge does NOT redo it: `BandCharacter::Filter` `sBoneMergeDir` path
  (`BandCharacter.cpp:1844-1851`) only `ReplaceRefs` for `sBoneMergeDir` bones (the
  outfit's own pelvis dir, set at OnInstallFilter `BandCharacter.cpp:1981`); shared
  skeleton bones fall to `kIgnore` (:1852).

**De-risking precedent.** `Character::SyncShadow` (`src/system/char/Character.cpp:651-
652`) already rebinds skin-mesh bones post-load:
`mesh->SetBone(i, AddShadowBone(mesh->BoneTransAt(i)), false)` — repoint to a different
transform, `calcOffset=false` (keep the authored offset). `RndMesh::SetBone`
(`Mesh.cpp:317`) is the sanctioned API. A per-member rebind is the same pattern.

**The fix — per-member rebind in `BandCharacter::SyncObjects`.** At the END of
`BandCharacter::SyncObjects` (`BandCharacter.cpp:593`, after `Character::SyncObjects()`
:648 — the member's own skeleton is char-pipeline-hooked by then; install-time
`FindObject(boneName)` ALREADY returns the member's own bones), add `#ifdef HX_NATIVE`:
for each outfit/resource SKIN mesh `m`, for each bone `b`,
`own = Find<RndTransformable>(m->BoneTransAt(b)->Name(), false)` (resolves to the
member's OWN skeleton — the 8 distinct `kInlineCached` instances); if `own && own !=
shared` then `m->SetBone(b, own, /*calcOffset=*/false)` (keep the outfit's authored,
gender-correct inverse-bind). The `SyncShadow` pattern, scoped to the band.

**Mechanism.** `BoneTransAt(b)->WorldXfm()` now reads the member's OWN skeleton world,
which the char pipeline poses to the MEMBER's gender bind (`SetDeformation`
`BandCharacter.cpp:1086` → `GetDeformClip(mGender)` + idle clip + IK on the member's
own bones). For the female member that world is the FEMALE bind, so
`off_female * world_female == I` → skinPos→0, and the arm ANIMATES (live skeleton, not
a frozen T-pose). Males: `own` == the member's counterpart of the formerly shared bone,
offset unchanged → still clean.

**Why this beats the dead-ends.** vs loader un-share: no crowd/extras blast radius (they
keep sharing; no per-member `main.milo`), no `DirLoader::Find` surgery. vs dir Copy: no
`CharServoBone` ref-remap crash (touches MESH ObjPtrs via `SetBone` only). vs runtime
offset recompute: we DON'T recompute the offset; we keep it and supply the right LIVE
world, so the recompute-trap doesn't apply. vs CHAR-2's load-time offset rebake: no
dependence on whether the deform pose frame matches the authored DCC bind (CHAR-2's
flagged open unknown) — the rebind keeps the authored offset and pairs it with the
matching live world, so it's correct by construction at the female bind.

**Composition with the shipped clamp.** After rebind band arm bones compose ~0u → the
clamp (`Rnd_Wgpu_RB3.cpp` skin loop, >12u → identity, `RB3_NO_SKIN_CLAMP`) never fires
on them; stays as backstop for crowd/extras + residual hair/face. No conflict (rebind at
SyncObjects/ObjPtr; clamp at draw/palette).

**A/B.** `XBONE=bone_R-upperArm RB3_NO_CLIP=1 RB3_NO_IK=1` (≥70 shots): trackjacket_
resource `bonePtr` now DIFFERS from vestdenim/plaidshirt/shred and skinPos 19.8→0; males
stay 0. WITH clip+IK: `SHARD_CATCH` = 0 trackjacket arm lines across a burst (live
female pose, no re-fling — where the runtime recompute failed). `SKIN_CLAMP_PROBE=1`:
clamp 0× on trackjacket, still fires crowd/extras. Venue: `char-burst-capture.py --shots
40`, female upper body coherent, males unchanged.

**Files + risk.** ONE DECOMP file: `src/system/bandobj/BandCharacter.cpp` `SyncObjects`,
HX_NATIVE → Wii byte-identical. Existing engine API only (`Find`/`BoneTransAt`/
`NumBones`/`SetBone`). NO engine `Rnd_Wgpu_RB3.cpp` edit → does NOT collide with the
audio agent's engine-build ownership (DECOMP edit + the single coordinated build).
Males/crowd/venue/DC3 inert. Shipped clamp untouched → no regression on mis-fire. Open:
confirm every flinging band bone is a SyncObjects-reachable skin mesh — the arm/twist
statics are; hair/goatee dynamic chains (650u via `CharFaceServo`/`CharHair`) may need
the rebind extended to those meshes (verify with SHARD_CATCH after the arm fix; clamp +
`RB3_NO_FACE` cover them meanwhile). Sequence: (1) outfit/resource skin meshes first,
(2) extend to face/hair if SHARD_CATCH still flags them, (3) keep the clamp default-on.
Detail mirror: `docs/native/audio-perf-loop/wave-05-char.md` `### angle-1`.

---

## 2026-06-06 — FAITHFUL FIX ATTEMPT: precise blocker (name-resolution share)

This session chased the loader-level per-member-skeleton fix end to end and mapped the
EXACT mechanism. New ground truth (all from runtime probes on rb3-native, clamp OFF):

1. **The loader is CORRECT.** Each band member's `char/main/main.milo` references
   `char/main/skeleton_unshared.milo` as an INLINED subdir with `dType=1`
   (kInlineCached) and loads it via `LoadInlinedFile` → a FRESH, DISTINCT per-member
   skeleton instance. Verified 8 distinct `resolvedDirPtr`s, all `shared=0`
   (`SKEL_LOAD_PROBE` in Dir.cpp). The prior session's belief that the loader shares
   the skeleton was WRONG.
2. **At `OnInstallFilter` time, each member's `FindObject("bone_R-upperArm.mesh")`
   returns a DISTINCT per-member bone** (`INSTALL_PROBE`: player1=0x..7820,
   player3=0x..b6a0, …). So per-member skeletons exist and are findable at install.
3. **But at DRAW time, all four band outfit meshes bind to ONE shared
   `char/main/skeleton_unshared.milo` instance at the MALE bind**, `parent==nil` (a
   root dir), `bonePtr` identical across trackjacket/vestdenim/plaidshirt/shred
   (`BAND_DRAW_PROBE` / `XBONE`). trackjacket (female) skinPos=(19.8,3.8,0.4) FLUNG;
   the three males skinPos=(0,0,0) CLEAN.
4. **The magnet is `char/main/skeleton.milo`, established by NAME RESOLUTION, not the
   merge.** Every char RESOURCE milo (vocal/viseme/guitar/extras/…_resource.milo) and
   each torso outfit milo (e.g. `trackjacket_solid.milo` → `../../../shared/char_shared.milo`)
   lists `char/main/skeleton.milo` as a `share=true` NON-INLINED subdir. The first
   loader creates it; every later reference shares it via `DirLoader::Find`
   (`ObjectDir::LoadSubDir`, Dir.cpp `LoadFile(p, async=true, share=true, …)`). The
   outfit-resource bones then `ReplaceRefs(bone, FindObject(name))`-consolidate onto
   that one shared `skeleton.milo` root, NOT onto the per-member main.milo skeleton.

### PROVEN dead-ends this session (each built + measured, clamp OFF)
- **Scope `FilterSubdir` shim to kInlineNever palettes (outfits stay kMerge, the
  retail action).** Band stays VISIBLE (NOT the prior "invisible" failure — that was a
  different, blanket narrowing) and textures stay OK, but the female is STILL flung:
  all outfit bones still bind the shared `skeleton.milo` root (`BAND_DRAW_PROBE`
  upArmPtr identical, trackjacket skinPos still 19.8). The merge action does not change
  the name-resolution share. → reverted to the blanket shim.
- **Full shim-OFF (retail kMerge for everything).** Same shared root binding +
  re-introduces the white-texture drain (1 `dummy_torso` cascade). → not viable.
- **Prune char_shared's nested `../skeleton.milo` subdir (one-time or per-encounter).**
  Strips ALL outfit bones (`numBones=0`, `skinned=0`) — the outfit bones had ALREADY
  consolidated onto that shared skeleton, so removing it leaves them bone-less. → not
  viable.
- **Skip the `char/main/skeleton.milo` PRELOAD.** Did not even fire / change the
  binding: the resource milos recreate+share `skeleton.milo` regardless of the preload.

### The remaining blocker (precise)
The faithful fix needs TWO things, both deep/broad:
1. **Un-share `char/main/skeleton.milo` for the band** at the name-resolution / share
   layer (`ObjectDir::LoadSubDir` / the per-subdir `share` flag), so each band member's
   outfit resources bind to that member's OWN main.milo skeleton. This is global-ish:
   the same shared `skeleton.milo` is referenced by the crowd/extras/vignette resource
   milos too, so un-sharing must be scoped to the band without breaking those.
2. **Pose each per-member skeleton to its outfit's gender bind.** `skeleton_unshared.milo`
   is itself the MALE-bind skeleton — a per-member COPY is still male-bind. The female
   bind comes from the outfit/clip data; on kMerge the outfit's own (female-bind) bones
   would need to drive the per-member skeleton. Just instancing per member is not enough.

Because (1) touches shared char loading used by the whole cast and (2) is a separate
posing problem, the safe, shipped fix remains the renderer fling clamp. The
`BandCharacter::FilterSubdir` shim is the original blanket kReplace (white-texture fix);
its NOTE now points here with the corrected root cause.

---

## (Superseded header — kept for history) 2026-06-05 renderer clamp landing

Status: **FIX LANDED (2026-06-05): renderer-side per-bone fling clamp (scoped
fallback B).** The four band members all bind to ONE shared
`char/main/skeleton_unshared.milo` skeleton instance at the MALE bind; the FEMALE
member `player1`'s `trackjacket` outfit (offsets baked for the FEMALE bind) flung her
arms/hands/hair ~20u into shards. The faithful fix (per-member skeleton at the correct
gender bind, loader-level) proved un-landable safely this session (see "WHY THE
FAITHFUL/RECOMPUTE FIXES DON'T WORK" below). The shipped fix is a **scoped renderer
clamp** in `BandRnd::DrawMesh` (engine `Rnd_Wgpu_RB3.cpp`): a skin bone whose composed
skin, in the mesh's own frame, is >12u off bind (the female mismatch is ~20u; a clean
animated arm is <8u) falls back to identity, so its vertices stay at their authored
model-space bind instead of shattering. A/B (40 shots each):
`trackjacket_resource` shards **170 → 0**, `trackjacket_skin` **102 → 0**; male
members untouched (never clamped); no crashes; textures/venue/menus/HUD all intact.
Opt-out: `RB3_NO_SKIN_CLAMP=1`. **Residual:** the female's hair/extras, clamped to
their (male-bind) model-space pose, look mildly splayed at the head — far better than
the baseline whole-figure shatter, but not perfectly posed. The true fix remains the
per-member gender-bind skeleton (loader-level), tracked below.

## LANDED FIX (2026-06-05) — renderer fling clamp (fallback B)

File: `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`, `BandRnd::DrawMesh` skin
loop (~L3165), right after the existing non-finite/runaway `bad` check. For each
skinned bone of a multi-bone (`NumBones>=8`) skin mesh, compute the composed skin in
the MESH's own frame (`skin * inverse(meshWorld)`); a correctly-bound bone is ~0
there at bind, the mismatched female bones are ~20u. If `|meshLocalSkin.v| > 12u`,
fall the bone back to identity (`sFallbackBones++; continue;`) so its weighted
vertices stay at the authored model-space bind rather than flinging. The `NumBones>=8`
gate excludes single-bone props (e.g. the mic, which legitimately translates far with
the performance). Env opt-out `RB3_NO_SKIN_CLAMP=1`; probe `SKIN_CLAMP_PROBE=1`
(prints which mesh/bone is clamped). Verified the clamp hits ONLY the female
member's mismatched meshes (trackjacket/fingernails/goatee/hair/buttflappants) plus
the pre-existing crowd/extras servo-skeleton flings — never the male band outfits
(vestdenim/plaidshirt/shred: 0 clamps). RB3 source `BandCharacter::FilterSubdir`
carries a one-line NOTE pointing here; no other RB3 source change.

### WHY THE FAITHFUL / RECOMPUTE FIXES DON'T WORK (tried & rejected this session)
- **Loader per-member skeleton (faithful A).** The shared skeleton is the PRELOADED
  `char/main/skeleton.milo`→`skeleton_unshared.milo` (`char` preload group,
  `share=true`), referenced by name at outfit-resource load (probed: trackjacket and
  vestdenim resolve `bone_R-upperArm` to the SAME bonePtr, `boneDir` parent=nil = a
  root preloaded dir). `char_shared.milo` is a RED HERRING — its own "skeleton"
  subtree is itself a `share=true` subdir reference to that same preloaded skeleton
  (proven via `SUBLOAD_PROBE`), so deep-cloning `char_shared` per member (tried,
  `CloneDirTree`) was INERT — the outfit still binds the preloaded root skeleton.
  Un-sharing `char/main/skeleton.milo` at `ObjectDir::LoadSubDir` was also inert (the
  outfit resource doesn't reach the skeleton via that path). Per-member kInlineCached
  copies of `skeleton_unshared` DO load (9 distinct, `shared=0`, via `INLINE_PROBE`)
  but are UNUSED — the outfit binds the preloaded one. A safe per-member rebind would
  require rerouting the name resolution / preventing the preload share for the band,
  which is broad and high-risk.
- **Offset recompute at bind (recompute B').** `SetBone(i,bone,true)` /
  `mOffset = meshWorld * inverse(boneBindWorld)` makes skin=identity at bind. It works
  PERFECTLY under `RB3_NO_CLIP RB3_NO_IK` (trackjacket skinPos 19.8u→0). But there is
  **no clean bind frame at runtime**: the band idle clip poses the skeleton from
  songMs=0, the outfit skin meshes only become reachable (`mOutfitDir`) at the first
  POLL — already post-IK — so the recompute bakes a posed frame and the IK/twist
  solvers then re-fling it (trackjacket arms shard again during playback). Capturing
  the bind earlier (load-time snapshot, cross-member male-reference invBind harvest)
  was attempted at length and could not reliably reconstruct the pre-IK bind for the
  shared arm/twist chain. This is the same wall the prior `RB3_RECOMPUTE_OFFSETS`
  diagnostic hit. The clamp sidesteps it entirely (no bind needed — just reject the
  fling).

(Historical root-cause analysis retained below.)

## TL;DR for the next session
- The shared object is `char/main/skeleton_unshared.milo` (316 bones, `kInlineCached`
  = "copy per reference"), reached two ways: (1) nested in the preloaded-shared
  `char/main/shared/char_shared.milo` (→ `skeleton.milo` → `skeleton_unshared.milo`),
  and (2) referenced directly by each member's `char/main/main.milo`. On native ALL
  paths resolve to ONE instance; retail gives each band member its OWN copy
  (honoring `kInlineCached`).
- The native divergence is in the **inline-cached subdir load / preload-shared
  cascade**, NOT the renderer and NOT `BandCharacter::FilterSubdir` alone. The
  `FilterSubdir` shim (kReplace on shared on-disk subdirs, added for white textures)
  is a *contributor* (it keeps char_shared shared) but pruning char_shared's skeleton
  does NOT un-share `skeleton_unshared` (main.milo still resolves the one shared
  instance — verified, `skinPos` stays 19.8u).
- Faithful fix = make `skeleton_unshared` per-band-member at LOAD time (before
  CharServoBone/CharBones hookup in `SyncObjects`). A post-hoc deep-copy of the bone
  tree CRASHES (`CharServoBone.cpp:179 mFacingPos && mPelvis` — bone refs in
  CharBones/CharServoBone are not remapped by a dir `Copy`).

Date: 2026-06-03 (root cause), 2026-06-05 (isolated to shared-instance/gender).
Target: `rb3-native` (small_club venue, guitar, hard), Xbox-360 RB3 assets. Engine
pin `MILO_ENGINE_PIN=59b7307` (HEAD unchanged).

---

## ROOT CAUSE (2026-06-05) — shared skeleton instance + gender mismatch

Decisive new evidence (engine probe `XBONE=bone_R-upperArm RB3_NO_CLIP=1 RB3_NO_IK=1`,
`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` ~L3084):

```
mesh='trackjacket_resource'  bonePtr=0x..fbe0  worldRot=[0.730 ..]  offRot=[0.511 ..]  skinPos=(19.80,3.84,0.36)  FLUNG
mesh='vestdenim_resource'    bonePtr=0x..fbe0  worldRot=[0.730 ..]  offRot=[0.730 ..]  skinPos=(0,0,0)            CLEAN
mesh='plaidshirt_resource'   bonePtr=0x..fbe0  worldRot=[0.730 ..]  offRot=[0.730 ..]  skinPos=(0,0,0)            CLEAN
mesh='shred_resource'        bonePtr=0x..fbe0  worldRot=[0.730 ..]  offRot=[0.730 ..]  skinPos=(0,0,0)            CLEAN
```

1. **All four band outfit meshes share ONE bone object pointer** (`0x..fbe0`) for
   `bone_R-upperArm` (and one shared pelvis at one worldPos) → there is exactly
   **ONE skeleton instance** for the whole band, not one per member.
2. That instance is at the **male bind** (`bone_R-upperArm` worldRot `[0.730 …]`).
3. `trackjacket`'s baked offRot `[0.511 0.038 -0.859 / …]` is **byte-identical to
   the FEMALE crowd/extra bind** (`female_*` meshes probe to exactly that offRot,
   worldRot `[0.511 …]`). `trackjacket_solid.milo` is a **female-only** asset.
   So trackjacket's offset is valid — for a FEMALE skeleton — but it's bound to
   the male-bind shared instance ⇒ `BoneOffsetAt ∘ WorldXfm ≠ I` ⇒ 20u arm flings.
4. The band has mixed genders (probed `BandCharacter::mGender`):
   `player0=male, player1=female, player2=male, player3=male`. player1 wears
   trackjacket. The other 3 (male) wear male outfits whose offset matches → clean.

Retail works because each band member gets its **own** skeleton copy, posed to that
member's gender bind; the female member's skeleton is at the female bind, so
trackjacket's offset cancels to identity there. The native band load instead leaves a
single shared skeleton instance, so the female outfit lands on a male bind.

## ASSET STRUCTURE (2026-06-05 follow-up, definitively dumped)

`SUBDIR_DUMP` (env, in `BandCharacter::FilterSubdir`) walked the actual loaded dirs:

```
char/main/shared/char_shared.milo   (inlineType=0 kInlineNever, PRELOADED share=true)
├── 5 skin/cloth materials (feet_skin.mat, torso_naked.mat, ...)  ← sCharSharedDir
├── char/main/shared/colorpalettes.milo (kInlineNever, 280 objs, 0 bones)  ← TEXTURE palette
└── char/main/skeleton.milo           (kInlineNever, 0 objs, 1 subdir)
    └── char/main/skeleton_unshared.milo (kInlineCached=1, 492 objs, 316 BONES) ← THE SKELETON
```

- `char/main/main.milo` (each band member, player0..3) references **skeleton_unshared.milo
  and colorpalettes.milo directly** (binary-string-confirmed) — NOT char_shared.
- `colorpalettes.milo` is ALSO preloaded separately as a shared subdir
  (`config/preload_subdirs.dta` "band" group, line 27). char_shared is line 29.
- `skeleton_unshared.milo` is `kInlineCached` → retail copies it per reference (each
  band member gets its own). NATIVE shares ONE instance across all four
  (`XBONE` bonePtr identical for all four band meshes, both at bind AND during
  playback — `worldPos=(7.4,-0.8,57.5)` male bind, never moves).

So the shared skeleton is reached two ways and BOTH resolve to one instance on native:
1. nested in the preloaded-shared char_shared (FilterSubdir keeps it shared, kReplace);
2. each main.milo's direct `kInlineCached` ref — which on native still resolves to the
   one shared instance instead of a per-member copy.

## FIX ATTEMPTS — RULED OUT (2026-06-05 follow-up, all hard-evidenced)

1. **Narrow the FilterSubdir shim to texture-only (let char_shared kMerge per-char).**
   → Band goes INVISIBLE. With kMerge, MergeObjectsRecurse descends into char_shared
   and the 316 skeleton bones hit `BandCharacter::Filter`'s `o1->Dir()==sBoneMergeDir`
   path, which does `ReplaceRefs(bone, FindObject(bone)); return kIgnore` — i.e. it
   remaps to an EXISTING per-char bone, never COPIES. With no per-char skeleton yet
   present (`found==null`/self), the bones are dropped and the outfit binds to nothing.
   Verified: 0 band meshes drawn; only a floating hair remnant rendered.

2. **Per-character deep-copy of the skeleton dir (`MergeDirs`/`Copy(kCopyDeep)` in
   `SyncObjects`).** → CRASH: `CharServoBone.cpp:179 Error: mFacingPos && mPelvis`,
   then SIGSEGV. A dir copy duplicates the RndTransformable bones but does NOT remap
   the bone ObjPtrs held by CharBones / CharServoBone / CharBoneDir / CharBoneOffset,
   so the copied skeleton's servo bones have null facing/pelvis. Also `SyncObjects`
   runs MULTIPLE times during the incremental native load with the skeleton in
   different transient states, so a copy fires on the wrong state. Backed out.

3. **Prune char_shared's `skeleton.milo` subtree before sharing (`SKEL_PRUNE` env).**
   → Does NOT fix it. The band still binds to ONE shared `skeleton_unshared` (via
   main.milo's direct reference): `XBONE` still shows identical bonePtr for all four
   and `skinPos` stays `(19.8,3.8,0.4)` for trackjacket. (Some MALE members looked
   "fixed" in screenshots — but males are clean in BOTH modes; the female is the only
   deformed one, and prune leaves her on the shared male bind.) Removed.

### The convergent path forward (next session)
Make `skeleton_unshared.milo` per-band-member, honoring its `kInlineCached` intent,
at LOAD time (so CharBones/CharServoBone hook up to the per-member copy naturally —
avoiding the attempt-2 crash). Candidate levers, in order of faithfulness:
- Fix the native inline-cached subdir load / preload-shared cascade so a
  `kInlineCached` subdir nested under a `share=true` preloaded dir is still COPIED per
  reference (this is the true Wii/Xbox semantics; the divergence lives in
  `ObjectDir::PreLoad`/`PostLoad` inline-dir handling + `DirLoader::FindLast`, not in
  bandobj). Confirm against `../xenia` retail behavior.
- OR give each `BandCharacter` its own `skeleton_unshared` instance during the band
  load (BandWardrobe / FileMerger), before `SyncObjects`, so it merges cleanly like
  the per-outfit resources already do.
The `BandCharacter::Filter` `sCharSharedDir` remap (BandCharacter.cpp:1824) handles
the shared MATERIALS correctly; it is NOT the mechanism for per-member skeletons.

CORRECTION (2026-06-05 follow-up): an earlier writeup claimed `BandCharacter::Filter`'s
`if (o1->Dir() == sCharSharedDir)` remap (BandCharacter.cpp:1824) was the
per-character SKELETON mechanism. This is WRONG — that remap is for the shared MATERIALS
(`sCharSharedDir` = `feet_skin.mat`'s dir = char_shared's own dir, which holds only 5
materials, 0 bones). `Find<Hmx::Object>(name, /*create*/true)` does NOT create — it
ASSERTS the object already exists and FAILs otherwise — so it only re-points refs to a
pre-existing per-char material copy. The skeleton lives in the nested
`skeleton_unshared.milo` (`o1->Dir() != sCharSharedDir`), and reaches `Filter` only via
the `sBoneMergeDir` path (which also `return kIgnore`s, see FIX ATTEMPT 1). The
per-member skeleton must come from the `kInlineCached` load, not from `Filter`.

### Why the two "fix" approaches in the original scope don't apply cleanly
- **Recompute offsets at neutral** (`RB3_RECOMPUTE_OFFSETS`, engine, default OFF):
  PROVES the diagnosis (under `RB3_NO_CLIP=1 RB3_NO_IK=1` it collapses
  trackjacket's `skinPos`→0 while leaving the clean meshes clean) but is **NOT a
  shippable fix**: there is no clean static neutral frame — the band idle clip is
  already animating at songMs=0, so a first-draw recompute bakes a mid-idle pose
  and makes playback visibly WORSE (huge wood-textured shards fly across the
  stage; see /tmp/ab_on). A/B screenshots below.
- **DC3 reference** (`dc3-decomp`): DC3 hit the identical "bind-pose-inverse *
  world produces garbage bone translations" class (commit `d6dffa63`) and its
  `BoneSetup.cpp:218-225` comment explicitly says *"the fix now lives in the
  neutral-skeleton posing path."* But DC3's actual fix (`cb4b0737`,
  `docs/sessions/2026-06-03-ik-ground-truth-comparison.md`) is for the **IK
  foot-plant** (a dynamic neutral-anchor crouch bug), a DIFFERENT symptom. DC3's
  dancers are FIXED-gender named characters (Emilia/Bodie) loaded with
  matched-gender outfits, so DC3 never exercises the mixed-gender-band /
  shared-skeleton path that RB3's `BandWardrobe::LoadMainCharacters` does. The DC3
  skin compose (`Multiply(BoneOffsetAt(i), wt)`) is line-for-line identical to
  RB3's, confirming the renderer is not the divergence.

---

## Symptom

In gameplay, on-stage band members render with **flat triangular shards / spikes**
radiating from the **arms, hands/fingers, hair, and facial hair**. The torso,
pelvis, head, and legs are coherent. It is NOT a simple T-pose; specific bone
chains fling vertices many units away from the body.

## Call chain (RB3 native skinning)

RB3 uses the **BandRnd** renderer, not the DC3 `Mesh_Wgpu`/`BoneSetup` path.
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` `BandRnd::DrawMesh` (~L2700+)
  builds the GPU bone palette: for each bone `b`,
  `skin = Multiply(owner->BoneOffsetAt(b), boneTrans->WorldXfm())`, stored
  column-major via `MiloXfmToColMajor`, consumed by `vs_skinned` in
  `src/gfx/standard_wgsl.inc` as `sum_i( weight_i * bones[idx_i] * vec4(pos,1) )`.
- `owner->BoneOffsetAt(b)` = `RndMesh::mBones[b].mOffset` (`rndobj/Mesh.h`), the
  baked **inverse-bind** Transform, read verbatim from the asset
  (`operator>>(BinStream&, RndBone&)` → `bs >> mBone >> mOffset`).
- `owner->BoneTransAt(b)->WorldXfm()` = the bone's runtime world, posed by the
  char system (`CharDriver`/clip, IK solvers, `CharForeTwist`/`CharUpperTwist`,
  `CharFaceServo`, `CharHair`, all via `CharBones`/`CharBonesMeshes::PoseMeshes`).

The CPU reference (`dc3-decomp` `RndMesh::SkinVertex`, also RB3 `Mesh.cpp`)
composes identically: `Multiply(BoneOffsetAt(i), BoneTransAt(i)->WorldXfm())`,
weighted by `boneWeights.x..w`, indexed by `boneIndices[0..3]`. The GPU path mirrors
this exactly.

## What was RULED OUT (with evidence)

| Hypothesis | Test | Result |
|---|---|---|
| Vertex/weight/index/stride/material decode | `RB3_BONES_IDENTITY=1` (force identity palette) | Renders a coherent T-pose ⇒ geometry/weights/indices/stride/materials all OK. |
| Wrong column/row order in `MiloXfmToColMajor` | Inspected; torso bones compose to identity at bind | Correct (column j = Milo row j; standard row-vec→col-vec transpose). |
| Shader weight/index logic | Read `normalizeBoneWeights`/`boneIndexAt`/`boneWeightAt` | Correct; out-of-range idx → identity palette slot (harmless). |
| `CharBonesMeshes::PoseMeshes` channel partition | `RB3_NO_POSEMESHES=1` (skip writing CharBones data into bone LocalXfms) | Still deformed. Partition logic also matches DC3 line-for-line. |
| Clip animation | `RB3_NO_CLIP=1` | Still deformed (static). |
| IK / twist solvers | `RB3_NO_IK=1` (gates CharIKHand/Foot/Fingers/Midi/Head, Char{Upper,Fore,Neck}Twist, CharLookAt, CharIKSliderMidi) | Still deformed (static). |
| Body-morph deform | `RB3_NO_DEFORM=1` (skip `BandCharacter::SetDeformation`) | Did not remove the goatee/fingernail/arm flings. |
| The skinning MATH | `BONE_PROBE` (per-bone localXfm/worldXfm/offset/skin det+rot+pos) | At bind, torso/spine/pelvis/head **and clean characters' arms** compose to **perfect identity** (skinDet=1, skinRot=I, skinPos=0). The math is correct. |

## CONFIRMED ROOT CAUSE

At the **bind/neutral pose**, the skin matrix `BoneOffsetAt(b) ∘ WorldXfm(b)` must
equal identity (offset is the inverse of the bone's bind world). For most bones it
does. For specific bone chains it does **not** — those bones fling vertices into
shards. Measured displacements (env `SHARD_CATCH`, `|skinPos|` at songMs≈0):

- band `trackjacket_resource` upper-body: `bone_R/L-upperArm` ≈ **20u**,
  `bone_*-foreArm` / `bone_*-foreTwist1/2` ≈ **19u**, `bone_*-shoulderTwist3/4`
  ≈ 6–13u — **STATIC** (present under `RB3_NO_CLIP=1 RB3_NO_IK=1`).
- band `goatee_resource` / hair: face/lip/jaw + hair bones ≈ **650u** — DYNAMIC
  (only with `CharFaceServo::Poll`/`CharHair::Poll` running).
- crowd `clap.mesh` hand/finger bones ≈ **2000u** (separate crowd skeleton).

The decisive proof is a same-world / different-offset comparison of the SAME bone
on two outfit meshes (probed under `RB3_NO_CLIP=1 RB3_NO_IK=1`):

```
bone_R-upperArm   worldRot = [0.730 -0.068 -0.680 / -0.077 -0.997 0.018 / -0.680 0.040 -0.733]  (identical on both)

vestdenim_resource  offRot = [0.730 -0.077 -0.680 / -0.068 -0.997 0.040 / -0.680 0.018 -0.733]  == transpose(worldRot)  ⇒ skinPos=(0,0,0)  CLEAN
trackjacket_resource offRot = [0.511  0.038 -0.859 / -0.052 -0.996 -0.075 / -0.858 0.083 -0.506]  != transpose(worldRot)  ⇒ skinPos=(19.8,3.8,0.4) FLUNG
```

i.e. **the runtime bone world is correct and self-consistent, but
`trackjacket`'s baked inverse-bind offset corresponds to a *different* bind
orientation than the one the runtime skeleton is in.** `vestdenim`'s offset matches
the runtime pose; `trackjacket`'s does not. Both meshes load through the identical
`bs >> mOffset` path and both render correctly on retail Xbox-360, so the offsets
themselves are valid — what differs is **the pose the native char pipeline puts the
shared skeleton into at the neutral moment vs. the pose those offsets were baked
against.** The native pipeline drives the upper-body / face / hair chains into a
neutral pose that disagrees, per-bone, with some meshes' baked inverse-binds.

This is a **char-animation-pipeline bind-pose mismatch**, NOT a renderer, skinning,
weight, index, stride, material, or `PoseMeshes`-partition bug — all of which were
explicitly tested and ruled out above.

### Why `RB3_BONES_IDENTITY` looked "clean" and `RB3_NO_POSEMESHES` didn't help
- Identity palette draws every vertex at its authored **model-space** position
  (band skin meshes are authored in model space — a T-pose; verts span ±21u in X to
  the wrists). That's a coherent T-pose, so identity *looks* clean — but it proves
  only that geometry is fine, NOT that the offsets/poses are right.
- `RB3_NO_POSEMESHES` leaves bones at their loaded LocalXfm, which is ALSO not the
  offset's bind for the affected chains, so it doesn't fix the mismatch.

## The FIX (scoped — next step)

The mathematically-correct fix is to make the runtime neutral skeleton pose agree,
per bone, with the meshes' baked inverse-bind offsets. Two viable approaches:

1. **Match the retail neutral pose.** Find where native diverges in establishing
   the per-character A-pose/idle before skinning (the upper-body lands at a ~35°
   arms-down rest at songMs=0 even under `RB3_NO_CLIP`; some meshes' offsets expect
   a *different* neutral). Likely in the char setup/idle-clip application order or
   `CharBoneDir::StuffBones` / `AcquirePose` timing. This is the faithful fix.
2. **Recompute offsets from the runtime bind** (`RndMesh::SetBone(idx, bone, true)`
   semantics: `mOffset = meshWorldXfm ∘ inverse(boneWorldXfm)`), captured once when
   the skeleton is in its neutral pose. Guarantees skin=identity at neutral by
   construction, independent of the authored offset. Requires reliably identifying
   the neutral frame; risk of baking a non-neutral pose if mis-timed.

Neither is a one-line edit; both need retail-pose ground truth or careful neutral
detection. **Not landed** to avoid guessing wrong and regressing the working
characters (vestdenim et al. are correct today).

## Partial mitigation (VERIFIED)

`RB3_NO_FACE=1` gates `CharFaceServo::Poll` (`char/CharFaceServo.cpp`) and
`CharHair::Poll` (`char/CharHair.cpp`). `SHARD_CATCH` log A/B confirms it removes
the **650u** goatee/band-hair flings (the most visually dramatic shards). It does
NOT fix the static ~20u upper-body mismatch and it DISABLES face/hair animation, so
it is a diagnostic/mitigation flag, **default OFF**, not a shippable fix.

## Diagnostic instrumentation left in the tree (all env-gated, default OFF)

Engine `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (uncommitted, HEAD=59b7307):
- `XBONE=<bonename>` — **the decisive 2026-06-05 probe.** Cross-mesh single-bone
  dump: for every skinned mesh that has the named bone, prints the bone OBJECT
  pointer + root-skeleton pointer + the bone's owning **dir storedFile** + worldRot +
  that mesh's offRot + skinPos, once per (mesh,bone). Run under `RB3_NO_CLIP=1
  RB3_NO_IK=1`. This is what proved all band outfits share ONE bone instance (in
  `char/main/skeleton_unshared.milo`) and only the female `trackjacket` offset
  diverges (== female-crowd bind). NOTE: band-member meshes only enter `DrawMesh`
  when the venue camera cuts to a band closeup, which is non-deterministic — capture
  MANY frames (`--shots 70+`) to reliably catch all four, and grep for the actual
  `mesh='trackjacket_resource'` lines (skinPos is the objective shard metric:
  `(19.8,3.8,0.4)`=flung, `(0,0,0)`=clean).
- `SUBDIR_PROBE` (RB3 `BandCharacter::FilterSubdir`, env-gated, kept) — logs each
  shared on-disk subdir flipped kMerge→kReplace by the native shim, with its
  storedFile + `inlineType` + hasSkeleton. Use to confirm char_shared/colorpalettes/
  skeleton routing. (`SUBDIR_DUMP`, the recursive subtree dumper, was throwaway and
  removed; reconstruct from this file's ASSET STRUCTURE section if needed.)
- `RB3_RECOMPUTE_OFFSETS` — DIAGNOSTIC ONLY (NOT a fix). Once per skinned mesh,
  recomputes `mOffset = meshWorld ∘ inverse(boneWorld)` (= `SetBone(i,bone,true)`)
  at first draw. Under `RB3_NO_CLIP=1 RB3_NO_IK=1` it collapses trackjacket's
  skinPos→0 (confirming the offset/bind mismatch); during real playback it bakes a
  mid-idle pose and makes shards WORSE. Default OFF.
- `BONE_PROBE` + `BONE_PROBE_NAME=<substr>` — per-bone dump for the first matching
  skinned mesh: local/world/offset/skin det + rotation + translation + bone/mesh
  Dir names. `BONE_PROBE_NAME` retargets it (e.g. `goatee`, `clap`, `fingernails`,
  `trackjacket_resource`).
- `SKEW_PROBE` — per skinned mesh, the worst bone deviation from identity (use under
  `RB3_NO_CLIP=1 RB3_NO_IK=1` to find bind-pose mismatches).
- `SHARD_CATCH` — per-bone, prints any composed `|skinPos| > 8u` with mesh+bone
  name (catches the flinging bones during normal play).
- (pre-existing) `RB3_BONES_IDENTITY`, `RB3_SKIP_SKINNED`, `RB3_SKIP_STATIC`,
  `SHARD_*`, `SMASH_DBG`, `VERT_PROBE`.

RB3 `src/system/char/...` (uncommitted):
- `RB3_NO_POSEMESHES` (`CharBonesMeshes.cpp`) — early-return `PoseMeshes`.
- `RB3_NO_CLIP` (`CharDriver.cpp`), `RB3_NO_IK` (the 10 IK/twist Polls).
- `RB3_NO_FACE` (`CharFaceServo.cpp`, `CharHair.cpp`).
- `RB3_NO_DEFORM` (`BandCharacter.cpp`) — skip `SetDeformation`.

## Key screenshots (this session)

- Baseline (deformed) singer-at-mic with upper-body shards: `/tmp/ab_off/shot_04.png`,
  `/tmp/ab_off/shot_06.png`, `/tmp/final_baseline/shot_05.png`.
- `RB3_RECOMPUTE_OFFSETS=1` during PLAYBACK (worse — proves recompute is not a
  fix): `/tmp/ab_on/shot_04.png`, `/tmp/ab_on/shot_06.png` (wood shards across the
  whole stage).
- (Screenshots are throwaway under `/tmp`; regenerate with
  `scripts/native/char-burst-capture.py`. A/B is camera-timing-dependent — the
  venue cuts between note-highway and band closeups, so re-shoot enough frames.)

## A/B summary (2026-06-05)
- **Default (no env):** shards on the singer's upper body — UNCHANGED by this
  session's work (all additions are env-gated, default OFF; verified no
  regression, `/tmp/final_baseline/shot_05.png`).
- **`RB3_RECOMPUTE_OFFSETS=1` (static probe, NO_CLIP+NO_IK):** trackjacket
  `bone_R-upperArm` skinPos (19.80,3.84,0.36) → (0.00,0.00,0.00); clean meshes
  stay (0,0,0). Confirms the offset/bind mismatch.
- **`RB3_RECOMPUTE_OFFSETS=1` (real playback):** WORSE (mid-idle pose baked) —
  not shippable.

## Reproduce

```bash
cmake --build native/build-native --target rb3-native
# the decisive same-world/diff-offset proof:
python3 scripts/native/char-burst-capture.py --shots 3 --interval 0.4 --out /tmp/p \
  --extra-env "BONE_PROBE=1 BONE_PROBE_NAME=trackjacket_resource RB3_NO_CLIP=1 RB3_NO_IK=1"
grep -a -A6 bone_R-upperArm $(ls -t /tmp/rb3-charburst-*.log | head -1)
# vs BONE_PROBE_NAME=vestdenim_resource  → offRot == transpose(worldRot), skinPos 0
```

### wave-08 VERIFY band-animates (2026-06-06, RUN-ONLY — adversarial REFUTE attempt FAILED → claim CONFIRMED)

Read-only Verify agent ran the BUILT binary (native/build-native/rb3-native, mtime 21:53,
newer than both BandCharacter.cpp and Rnd_Wgpu_RB3.cpp — NOT rebuilt) and tried to REFUTE
that the band now animates. Could not. Every measurement confirms the rebind.

MEASUREMENTS (all via char-burst-capture.py to gameplay, my own captures):

(a) THE KEY — outfit-bound bone MOVES (XBONE_TRACK=upperArm on trackjacket, rebind DEFAULT ON):
    978 samples, **469 DISTINCT worldPos values**, 467 frame-to-frame moves, **max frame
    delta 292.9u** (mean 12.3u/frame post-onset). Band ANIMATES.
    NEGATIVE CONTROL (RB3_NO_SKEL_REBIND=1): 481 samples, **EXACTLY 1 distinct value** =
    (7.402,-0.8298,57.5293) — the static magnet — maxDelta 0.0000u, 0 SKEL_REBIND events.
    => (e) the REBIND does the work, not the clamp/rebake. Decisive on/off contrast.

(b) FEMALE no fling (SKEL_REBIND_SKINPOS, draw-time |skinWorld-boneWorld|): trackjacket
    (player1 this run) worstBone delta 50.3-64.8u (clean limb extent, threshold "fling=hundreds").
    Max across ALL torso meshes = 65.4u. **0 SKIN_CLAMP events touch any band torso mesh.**

(c) ALL 4 members rebind + clean + animate: SKEL_REBIND summary body=1 for player0-3
    (vestdenim/trackjacket/shred/plaidshirt + _skin), reboundBones 35-46 each, every magnet ptr
    != own ptr. Per-member skeleton source motion (BAND_ANIM_PROBE) up to 186u/135u with REAL
    venue clips (stand_realtime_idle_c_06 / stand_around_03 / ms_idle_lean_rt_02), not fallback.

(d) NO regression: SKIN_CLAMP fires ONLY on crowd/extras/hair (clap, male/female_crowd_body*,
    male_facehair_lemmy, *_extra_head, female_extras_skin) — band torso never touched; crowd
    scope unchanged. No crash/assert/abort across 3 runs (frames 5548/4575/6182, clean SIGTERM).
    Screenshots valid PNG, highway+HUD+venue intact, band figures coherent (no shards).
    Wii byte-identical: 62.8830% code / 77.4446% fns / BandCharacter 77.3131% (as claimed).

VERDICT: refuted=FALSE. The band ANIMATES (outfit bone moves 292.9u/frame vs 0.0u with rebind
off), the female no longer flings (≤65u, 0 clamp hits), all 3 other members clean+animating, no
regression, Wii match unchanged. The CLAIM holds end-to-end.

### wave-08 VERIFY band-animates (2026-06-06, RUN-ONLY, adversarial) — REFUTED=FALSE: the rebind WORKS, MEASURED

Read-only adversarial verification of the CLAIM that `BandCharacter::RebindOutfitBonesToOwnSkeleton()`
(BandCharacter.cpp:725, called from Poll:491) makes the on-stage band ANIMATE and stops the female
fling. RAN the pre-built binary (native/build-native/rb3-native, built 21:53; did NOT rebuild).
All numbers are my own measurement via char-burst-capture.py + the in-tree probes. Tried to REFUTE;
could not. Verdict: the claim HOLDS.

| test | metric | result |
|---|---|---|
| (a) outfit bone MOVES | XBONE_TRACK=upperArm on trackjacket, rebind ON (default) | **MOVES**: 2358 samples, **1198 distinct worldPos**, max frame-to-frame **351.77u**, range x[-258,115] y[-42,311] z[36,358]. Screenshots distinct + band figure pose changes shot-to-shot. |
| (e) REBIND does the work | same probe, RB3_NO_SKEL_REBIND=1 | **STATIC**: 1549 samples, **exactly 1 distinct worldPos** (7.402,-0.830,57.529) = the documented static magnet, byte-identical whole capture. => the motion is 100% the rebind, NOT clamp/rebake. |
| rebind repoints to a DIFFERENT instance | SKEL_REBIND_PROBE | every per-bone rebind: magnet ptr (0x..bc/bd cluster) -> own ptr (0x..cc cluster), DISTINCT. All 4 members rebound: player0/1/2/3, 2 meshes each, 35-46 bones. Not a no-op. |
| (b) female no fling | SKEL_REBIND_SKINPOS, player1=trackjacket(female) | **CLEAN**: 64.791u / 50.269u skin-to-bone delta (limb extent <~65u). Stable across both latch events (no re-fling). Not in SKIN_CLAMP. |
| (c) 3 males clean + animating | SKINPOS player0 vestdenim / player2 shred / player3 plaidshirt | **CLEAN**: 50-65.4u, identical range to female. All animate (XBONE_TRACK motion is the shared moving instance). |
| no re-fling mid-anim | XBONE_TRACK centroid analysis | max dist from scene centroid 227.9u (normal arm-swing; a fling = thousands). No spike. |
| (d) crowd/extras unchanged | SKIN_CLAMP_PROBE | 4069 clamps, ALL crowd/extras (clap/male_crowd/female_crowd/fist/facehair/youngozzie/extras heads). **ZERO band torso** (trackjacket/vestdenim/plaidshirt/shred) ever clamped. band-only scope held. |
| (d) no crash | 3x ~14-16-shot runs to full gameplay | no FATAL/assert/segv/abort in any run; all reached game_screen, songMs advanced, full-size PNG renders, HUD/highway/venue intact. |
| (d) underlying skeleton motion | BAND_ANIM_PROBE=* | per-member bone moves up to 247.9u (331/899 samples >1u; rest are pre-clip idle). source motion confirmed. |
| (d) Wii byte-identical | report.json + #ifdef audit | overall 62.8830% code / 77.4446% fns, BandCharacter 77.3131% — matches claim. All rebind code + 3 header members (mNativeReboundOnce/Quiet/Body) + method decl are #ifdef HX_NATIVE (header 108/116/244-254, .cpp 448/491/675-916). No Wii layout/vtable change. |

CONCLUSION: REFUTED=FALSE. The band ANIMATES (outfit bone goes 1->1198 distinct positions, 351u/frame),
the rebind (not the clamp/rebake) is the cause (RB3_NO_SKEL_REBIND=1 -> static magnet), the female does
NOT fling (64.8u clean), the 3 males are clean + animate, crowd/extras + Wii match are unregressed. The
wave-07 ground truth (per-member skeleton moves, outfit was bound to the static magnet) + the wave-08 fix
(rebind outfit bones to the Find-resolved animated instance at Poll time) are both CONFIRMED by measurement.
