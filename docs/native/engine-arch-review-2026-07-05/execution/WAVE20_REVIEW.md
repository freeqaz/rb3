# WAVE20_REVIEW — Fable pre-dispatch review of WAVE20_KICKOFF.md (rb3 `62bc897f`)

**Reviewer:** Fable. **Date:** 2026-07-08. **Verdict: DISPATCH-WITH-AMENDMENTS** (A1–A11
below, binding). Every code anchor below re-derived from the working tree, not trusted
from the kickoff.

## Q1 — Is the central claim sound (load-path class outside the invariant; static reads dodge GT-D)?

**Sound, with one trap the synthesis must discharge (A11).** The §3 invariant
(HANDS-ADJUDICATION/VERDICT.md) kills *post-load bakes*: "no bake evaluated after load can
conjure the missing pairing." A load-time fix that makes the loader produce the Wii object
is definitionally outside that class; the kickoff quotes the verdict's own §1 "(b)-at-load"
statement, so the wave attacks the record's root cause at its source — legitimate.
Static-read validity on the D2 rig is established (R1-DOLPHIN/STATUS.md D2: 989/992 rigid
CharBones, map-valid by construction; V_findings.md: reads live and correct at hub AND
gameplay). GT-D closed the *articulated/animated* question only; binding topology is fixed
at load and readable frozen. No banned citation appears in the kickoff (the HANDS-FIX
"genuine fix is engine reskin" line is not cited; VERDICT §8.4 respected). The closure's
reopen condition (articulated capture) is not claimed — Lane W must state it makes NO
animation claims (A11).

**The trap:** the 8th dead cell (`RB3_HANDS_AUTHORED_REPOINT`) composed exactly
`inv(authored B) · L_own(t)` — authored offsets on the animating per-member bone — and was
VISUAL-refuted (torn blends, HANDS-FIX/STATUS.md). A load-path fix that yields a per-member
instance carrying B and animating like `own` produces the *same composition*. So "the Wii
object is real and native loses it at load" is necessary but NOT sufficient for a Wave-21
fix charter: the synthesis must name what the Wii-loaded object has that (authored offsets
+ own) lacked — candidates: whole-skeleton slot consistency (native is MIXED, 8/46 slots
per-member per VERDICT §1; Wii may be all-46 one dir), gender-posed rest, inter-bone rest
geometry. Lanes must collect slot-level data to answer this (A7, A8, A11).

## Q2 — Lane W: capture state, schema, and what wii_bone_dirboot.py can actually read

- **State:** main_hub alone is insufficient. The shipped bug, all E1 evidence, and the W16
  tear are gameplay-state; per-song loads install instruments/venue and re-run the merge
  chain. VISCAP reached BOTH states by sight (V_findings.md), so cost is marginal →
  capture BOTH (A6). The torso calibration row is right and stays.
- **Tooling truth:** `wii_bone_dirboot.py` today reads **CharBone (`.cb`) objects only**,
  via Bank-8 vtable census (name@+12, mTrans@+72, packed world@+76 — offsets D2-derived).
  It has **no RndMesh, no RndBone/ObjPtr deref, no Hmx::Object::mDir walk, no ObjectDir
  reader**. The lane's real work is a new `binding` subcommand: census `__vt__7RndMesh`
  (map 0x80c235a8), `__vt__16RndTransformable` (0x80c2a1b4), `__vt__9ObjectDir`
  (0x80bb8ad4) — all present in `orig/SZBE69_B8/files/band_r_wii.map` — then derive Bank-8
  offsets for `RndMesh::mBones`, `RndBone::mBone`, `Object::mDir` empirically with
  D2/G2-style validation (Bank-5 DWARF is known-divergent; D2 precedent: name @+12 not
  +24). Binding rows MUST come from mesh→RndBone **pointer deref**, never the name-based
  nearest-neighbor heuristic `cmd_bones` uses (heap carries ~2× instances per name). (A5)
- **Schema:** insufficient as proposed — no status field (the chartered fail-red has
  nowhere to land), no state, no per-slot rows, no dir classification (raw instance ids
  cannot join cross-platform). Amended schema in A10. `boneCount` is in the kickoff prose
  but missing from the R-D field list — restored in A10.
- **Additional fail-reds:** MESH_ABSENT loud (the substrate's band init is anomalous —
  CharClipDrivers=0 everywhere, CharServoBone VT 0 resident at gameplay per V_findings —
  so verify hand meshes are resident per member before reading); explicit GENDER-GAP row
  if the guest-profile lineup has no female. (A5)

## Q3 — Lane N: anchors re-derived, probe interpretability, probe timing

All re-derived against `src/system/bandobj/BandCharacter.cpp` @ 62bc897f-era tree:

- `:4202` **confirmed**: `if (o1->Dir() == sBoneMergeDir)` with `ReplaceRefs(o1, found)`
  at :4207, inside `BandCharacter::Filter` (:4165–4232). Sibling remaps: `sCharSharedDir`
  :4182 (ReplaceRefs :4185, with the `mine->Dir() == this` assert 0xAB8),
  `sInstrumentDir`/`sInstResourceDir` :4188 (ReplaceRefs :4196). All three need hit
  counters (lint 8). **VERDICT §1's ":4159-4181" anchor is stale — do not propagate.**
- Shim **confirmed**: HX_NATIVE block `:4235–4286` (`#ifdef` :4235, override logic
  :4278–4282 — `kMerge`→`kReplace` when `!o1->mStoredFile.empty()`, `#else` :4284).
  Kickoff's ":4236-4286" is accurate enough.
- `OnInstallFilter` **confirmed**: resets `sBoneMergeDir=0` at :4290, sets it at
  :4335–4340 from `sOutfitDir->FindObject("bone_pelvis.mesh", false)->Dir()`. Also sets
  `sCharSharedDir` from `feet_skin.mat`'s dir (:4342-4345) and `mFileMerger->mFilter =
  this` (:4319) — the band-only containment seam.
- **Mechanism note (favors the wave):** a subdir kept via kReplace never has its objects
  iterated through `Filter`, so the :4202/:4182 remaps cannot fire for its contents — the
  shim IS a coherent candidate for disabling the Wii remap path. BUT see Q5: the recorded
  full-shim-off arm says binding topology did NOT change. The A/B arm is therefore a
  *reconciliation* experiment, not discovery — pre-register both outcomes (A1).
- **Interpretability of the bypass arm:** the recorded 2026-06-06 full-shim-off run
  completed load (white textures + probes readable), so the arm is interpretable.
  Control = default boot with identical probes, same lineup, same fixed clock. Run the
  FULL bypass first (matches the recorded arm for reconciliation), skeleton-scoped bypass
  second if the full arm diverges from the record. (A1)
- **"Before any rebind/rebake" is achievable but NOT as a single end-of-load gate.**
  Loads/merges are poll-interleaved and "outfit skin meshes only become reachable
  (mOutfitDir) at the first POLL" (CHAR_SKINNING doc ~:1012). The first mutator is
  `RebindHeadHandsAtRest`, called at Poll `:527` BEFORE `Character::Poll()` (:530);
  `RebindOutfitBonesToOwnSkeleton` follows at :575; meshes are rebound "by frame 3"
  (HANDS-FIX). Therefore the pristine-binding dump must hook the ENTRY of both rebind
  functions (:1254, :1102), per mesh, first-touch — not a SyncObjects-time sweep alone. (A2)

## Q4 — Lane D: list, references, and batch gating

Pre-gated from `build/SZBE69_B8/report.json` (current build) — the chain is mostly at
100%, so **yes, gate first; the semantic audit shortlist is small**:

| function | unit | fuzzy% | audit? |
|---|---|---|---|
| `BandCharacter::Filter` (all four remap branches live here) | bandobj/BandCharacter | **95.60** (1928B) | YES — priority 1 |
| `ObjDirPtr<ObjectDir>::__as` (share/assign semantics) | obj/Dir | **95.17** | YES — add, missing from kickoff |
| `ReplaceRefs` (free fn, BandCharacter.cpp:4092) | bandobj/BandCharacter | 98.79 | YES |
| `BandCharacter::OnInstallFilter` | bandobj/BandCharacter | 99.11 | YES |
| `ObjectDir::LoadSubDir` | obj/Dir | 99.39 | YES |
| `ObjectDir::PostLoadInlined` (inline-cache resolution) | obj/Dir | 99.80 | YES — add |
| `ObjDirPtr::LoadFile` / `LoadInlinedFile` | obj/Dir | 99.96 / 99.97 | cosmetic-scan only |

Already **100%** → FAITHFUL-BY-MATCH, no semantic audit needed: `FileMerger::MergeAction`
/`Filter`/`FilterSubdir` (char/FileMerger.cpp:247/…/317), `MergeFilter::DefaultSubdirAction`
(obj/Utl.cpp:201), `MergeDirs`/`MergeObjectsRecurse` (obj/Utl.cpp), `BandCharacter::
FilterSubdir` Wii path (28B), **`ObjectDir::PreLoad` — which owns the Dir.cpp:317–327
kInlineCached/kInlineCachedShared read the kickoff calls "Dir.cpp:321 region"**,
`ObjectDir::FindObject` (Dir.cpp), `DirLoader::Find`. The kickoff's kInlineCached-handling
worry is answerable nearly for free: the handling lives in a 100%-matched function.

**References (answers R-C):** dc3-decomp has **no `src/system/bandobj/`** — for
BandCharacter functions anchor on Bank-8 asm (`bin/analyze-function`, default bank8) +
`scripts/analysis/bank_divergence.py`; DC3 reference IS valid for `obj/Dir.cpp`,
`obj/Utl.cpp`, `char/FileMerger.cpp` (all present in dc3-decomp). SEMANTIC-SUSPECT
verdicts must cite the exact mismatching instructions (`run_diff_inspect` mode=mismatches/
diagnose), never impressions. (A8, A9)

## Q5 — Does CHAR_SKINNING_DEFORM_INVESTIGATION.md already answer part of Lane N?

**Yes — substantially. Lane N must be reframed "verify + complete the 2026-06-06 causal
chain", not discover-from-scratch (A1).** The doc already establishes, probe-evidenced:

1. **The loader is CORRECT** — kInlineCached loads FRESH per-member skeletons (8–9
   distinct `resolvedDirPtr`s, shared=0; doc §2026-06-06 item 1). This *refutes the
   kickoff's own framing* of "the documented kInlineCached-under-preloaded-share
   divergence" (a VERDICT §1 phrase inherited from the superseded TL;DR at doc ~:1024,
   which the 2026-06-06 section explicitly marks WRONG). Lane N re-verifies rather than
   assumes — good — but the kickoff should not rank loader-share as a live mechanism.
2. **Binding is decided at PARSE-TIME NAME RESOLUTION**, not merge: `RndMesh::Load` →
   ObjPtr resolve via `FindObject` descent into the shared preloaded
   `char/main/skeleton.milo` (doc ~:536-549) — "the bones are male-bind-bound the moment
   the shared skeleton answers the parse-time FindObject."
3. **Shim-off was ALREADY RUN and did NOT change binding**: "full shim-off (retail
   kMerge) — same shared root + white textures"; kInlineNever-scoped variant — same;
   pruning char_shared's skeleton subdir — strips all outfit bones (doc + the :4268-4276
   comment). The kickoff's suspicion order (shim first) contradicts this committed record.
   HOWEVER the recorded arm had **no branch hit counts** (pre-dates lint 8) and Q3's
   mechanism note shows the shim coherently *could* gate the remaps — so the A/B arm is
   retained as a reconciliation with pre-registered expectations: predicted outcome =
   topology UNCHANGED (shim exonerated for binding, mechanism (a) demoted); if topology
   CHANGES, the 2026-06-06 record is superseded and (a) is promoted. Either way the hit
   counters answer WHY. (A1)
4. Outfit meshes bind the shared root while per-member instances sit unused (doc items
   2–4, pointer-evidenced) — Lane N's step (b) will largely re-confirm this on the
   CURRENT build (worth doing: 12 defaults + many rebinds landed since 2026-06-06).

What the doc does NOT answer (Lane N's genuine new ground): hit counts on
:4182/:4188/:4202; whether the :4202 branch is really "never-firing" (that claim has
never been instrumented); a Wii-joinable per-slot binding table; and which load event
establishes the shared binding under TODAY'S build.

## Q6 — Lints, fail-reds, scope hazards

- Probe flags default-OFF: chartered ✓; add: all Lane N probes inside `#ifdef HX_NATIVE`
  (Wii build byte-identical by construction — state it in the lane brief) (A3).
- Lint 8 (hit counts on negatives): chartered ✓ — extend to all three remap branches (A3).
- Lane W fail-reds: "unresolved ≠ shared" chartered ✓; add MESH_ABSENT + GENDER-GAP (A5).
- Lane D: add the "cite exact instructions" rule (A9); step-0 unit-in-build check ✓.
- Scope: rb3_session_trace.cpp / engine FxSendNative.cpp never-stage hazard carried ✓.
- Lane W must not emit any animation-level claim (substrate CharClipDrivers=0; GT-D
  closure + reopen condition untouched by this wave) (A11).

## Q7 — Join schema R-D: AMENDED (see A10)

As proposed it cannot express the fail-red, the state, or slot-level mixing, and raw
`owningDirInstanceId` does not join across platforms. Amended schema blessed in A10.

## AMENDMENTS (binding)

- **A1 (Lane N reframe + suspicion order):** charter = "verify + complete the 2026-06-06
  causal chain" (CHAR_SKINNING doc ~:894–1040). Suspicion order becomes (b) parse-time
  name-resolution share FIRST, (a) shim SECOND. The shim A/B arm is a *reconciliation*
  of the recorded shim-off negative (doc "PROVEN dead-ends"; comment :4268-4276):
  full bypass first (matches the recorded arm), control = default boot, identical probes;
  pre-registered: UNCHANGED topology ⇒ shim exonerated for binding; CHANGED ⇒ record
  superseded. White textures are acceptable in the probe arm (recorded arm completed load).
- **A2 (Lane N probe point):** pristine-binding dump hooks the ENTRY of
  `RebindHeadHandsAtRest` (:1254; called from Poll :527) and
  `RebindOutfitBonesToOwnSkeleton` (:1102; called :575), per mesh, first-touch, plus the
  end-of-merge event. A single "end-of-load" sweep is INSUFFICIENT (poll-interleaved
  loads; meshes reachable only at first Poll, doc ~:1012).
- **A3 (Lane N instrumentation set):** hit counters on ALL THREE remap branches —
  :4182 (sCharSharedDir), :4188 (sInstrumentDir/sInstResourceDir), :4202 (sBoneMergeDir)
  — plus per-merge `FilterSubdir` action log (which subdir → kMerge/kReplace, with
  `mStoredFile`). All probes `#ifdef HX_NATIVE` + env-gated default-OFF. Do not propagate
  VERDICT §1's stale ":4159-4181" anchor.
- **A4 (Lane N step (d) framing):** "name-resolution vs merge" must be answered with the
  doc's mechanism as the null hypothesis (parse-time FindObject descent, doc ~:536-556),
  instrumented on the current build — not re-derived from scratch.
- **A5 (Lane W capability + fail-reds):** build the new mesh-binding reader per Q2
  (RndMesh/RndTransformable/ObjectDir vtable census from the Bank-8 map; empirical Bank-8
  offset derivation with G2-style rigidity/topology validation; pointer-deref only, no
  name-based nearest-neighbor). Fail-reds: UNRESOLVED ≠ SHARED (chartered), MESH_ABSENT
  loud (verify hand meshes resident per member before reading), GENDER-GAP row if the
  lineup lacks a female.
- **A6 (Lane W states):** capture main_hub AND live gameplay (both proven reachable,
  V_findings); binding table per state; torso control row in both.
- **A7 (Lane W basis capture):** per bone slot, also record the bound trans's world+local
  matrices (read_mat3x4 machinery exists; the frozen substrate holds the bind pose, so
  this IS the Wii-side basis B) — enables the A11 discharge (does the Wii object carry
  the authored basis), not just topology.
- **A8 (Lane D gate + list):** run `batch_objdiff` to confirm, then semantic-audit ONLY
  the sub-100 shortlist in Q4's table (adding `ObjDirPtr::__as` and
  `ObjectDir::PostLoadInlined`, missing from the kickoff). 100% functions get
  FAITHFUL-BY-MATCH rows for free, including `ObjectDir::PreLoad` (the Dir.cpp:317-327
  kInlineCached/kInlineCachedShared handling), `FindObject`, `DirLoader::Find`.
- **A9 (Lane D references + rigor):** BandCharacter has NO DC3 analog (no
  dc3-decomp/src/system/bandobj) → Bank-8 asm via `bin/analyze-function` +
  `bank_divergence.py` gate; DC3 valid for obj/Dir, obj/Utl, char/FileMerger.
  SEMANTIC-SUSPECT requires the exact mismatching instructions cited (run_diff_inspect).
- **A10 (join schema R-D, blessed as amended):** one row per (platform, state, member,
  mesh, boneSlot). Fields: `platform{wii|native}`, `state{main_hub|gameplay}`, `member`,
  `memberGender` (38-vs-40 census on Wii), `mesh`, `boneSlotIndex`, `boneName`,
  `status{RESOLVED|UNRESOLVED|MESH_ABSENT}`, `owningDirName`,
  `owningDirClass{OWN_MEMBER|SHARED_ROOT|OTHER}`, `owningDirInstanceId` (platform-local,
  opaque), `boneCount`. JOIN key = (state, member, mesh, boneSlotIndex/boneName);
  COMPARED value = owningDirClass (+ owningDirName). Per-SLOT rows are mandatory — the
  native pathology is slot-mixed (8/46, VERDICT §1); per-mesh aggregates erase it.
- **A11 (synthesis obligation — the 8th-cell equivalence trap):** before chartering any
  Wave-21 fix, the synthesis must state what the Wii-loaded object has that the
  VISUAL-refuted 8th cell (authored offsets repointed to `own` = `inv(B)·L_own(t)`,
  HANDS-FIX/STATUS.md) lacked — using A7/A10 data (slot consistency, rest-basis identity,
  gender pose). A load-path fix that merely reproduces that composition re-enters the
  dead class by the back door. Also: Lane W makes static-binding claims ONLY; the GT-D
  closure and its articulated-capture reopen condition are untouched.

## VERDICT

**DISPATCH-WITH-AMENDMENTS** — dispatch all three lanes once A1–A11 are folded into the
lane briefs; no lane's core charter is blocked; the wave's central claim survives review
with the A11 discharge obligation attached to the synthesis, not to the lanes.
