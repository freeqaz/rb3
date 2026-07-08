# Lane D — STATUS / VERDICT (Wave 20, decomp-fidelity audit)

**Bottom line: the merge/share/binding decomp chain is FAITHFUL. There is NO decomp
bug that disables the skeleton-binding remaps. Zero SEMANTIC-SUSPECT findings.**
Every sub-100% function in the chain is COSMETIC-DIFF (register allocation, instruction
scheduling, SDA/`&TheLoadMgr` relocation representation, or MWCC bool-materialization) —
never a changed dir compare, inverted predicate, wrong branch target, or missing case.
The two behavioral HX_NATIVE divergences on the path (ReplaceRefs reimpl; FilterSubdir
white-texture shim) are semantics-preserving / already-exonerated, not decomp defects.

This directly answers the owner's "is it a bug in decomp code? where?": **No — not in
this chain.** The native binding pathology (shared-static skeleton root) is established
by DATA/runtime name-resolution behavior (Lane N/W), not by any function diverging from
the Wii target.

## Verdict table

| function | unit | match% | verdict | exact suspect instructions | binding consequence |
|---|---|---|---|---|---|
| BandCharacter::**Filter** | bandobj/BandCharacter | 95.60 | **COSMETIC-DIFF** | 3 diff_op (idx62 beq↔bne, idx356 bne↔beq, idx419 beq↔bne) — ALL bool-mat/block-reorder paired w/ same-target branch; root = idx4 `_savegpr_24` vs `_savegpr_23` → uniform regalloc shift; diagnose "Unexplained:0" | NONE — dir-compare chain (sCharSharedDir idx230 / sInstrumentDir idx285 / sInstResourceDir idx288 / sOutfit·sResource·sToDir idx344-351 / **sBoneMergeDir idx357**) byte-faithful; sBoneMergeDir remap branch structurally present + correct |
| ObjDirPtr<ObjectDir>::**__as** (copy-assign FRC) | obj/Dir | 95.17 | **COSMETIC-DIFF** | idx36-63 r0-vs-r3 regalloc + redundant `mr r3,r0` (idx39/60) shifting vtable-deref (insert idx42/63) | NONE — refcount mgmt only; same fields 0x8/0x4/0x0, vtable slots 0x8/0x24; not a name-resolution/dir-identity fn |
| **ReplaceRefs** (free fn) | bandobj/BandCharacter | 98.79 | **COSMETIC-DIFF** (Wii #else path) | idx49/52/55 SDA-reloc on sOutfitDir/sResourceDir/sToDir (same symbols); idx75 redundant `b 0xca74` to loop-cond | NONE — match set + `theirs!=mine` guard + ref->Replace faithful. (Native #ifdef reimpl audited separately = FAITHFUL, see below) |
| BandCharacter::**OnInstallFilter** | bandobj/BandCharacter | 99.11 | **COSMETIC-DIFF** | idx112 beq↔bne driven by idx110 `cntlzw`(TGT) vs `cmpwi`(SRC) pointer-null bool-mat | NONE — the STORES (sBoneMergeDir=xfm->Dir(), sCharSharedDir=feet->Dir(), etc.) are identical; only the null-check materialization differs |
| ObjectDir::**LoadSubDir** | obj/Dir | 99.39 | **COSMETIC-DIFF** | idx49/185/301/461 `subi ...0x2a90` vs `addi ...TheLoadMgr` (same &TheLoadMgr); idx265-267 duplicated `~String` cleanup site vs coalesced | NONE — `LoadFile(subdirpath, share=true, …)` (the shared-skeleton establishment call) is faithful |
| ObjectDir::**PostLoadInlined** | obj/Dir | 99.80 | **COSMETIC-DIFF** | idx137-146 two-halfword std::swap-style slot exchange ([r1+0xc/0xe]↔[r31+0x70/0x72]) | NONE — inline-cache resolution unchanged; same values swapped |
| ObjDirPtr::**LoadFile** | obj/Dir | 99.96 | **COSMETIC-DIFF** | idx40/202 `subi ...0x2a90` vs `addi ...TheLoadMgr` only | NONE |
| ObjDirPtr::**LoadInlinedFile** | obj/Dir | 99.97 | **COSMETIC-DIFF** | idx39 `subi ...0x2a90` vs `addi ...TheLoadMgr` only | NONE |
| FileMerger::MergeAction/Filter/FilterSubdir | char/FileMerger | 100 | **FAITHFUL-BY-MATCH** | — | merge-action dispatch faithful |
| MergeDirs / MergeObjectsRecurse / MergeFilter::DefaultSubdirAction | obj/Utl | 100 | **FAITHFUL-BY-MATCH** | — | recursive merge + default subdir action faithful |
| ObjectDir::PreLoad (owns Dir.cpp:317-327 kInlineCached/kInlineCachedShared) | obj/Dir | 100 | **FAITHFUL-BY-MATCH** | — | **inline-cache subdir-type handling is 100% matched — the kickoff's "kInlineCached-under-share divergence" worry is decomp-clean** |
| ObjectDir::FindObject / DirLoader::Find | obj/Dir, obj/DirLoader | 100 | **FAITHFUL-BY-MATCH** | — | name resolution + loader-share lookup faithful |
| BandCharacter::FilterSubdir (Wii #else) | bandobj/BandCharacter | 100 | **FAITHFUL-BY-MATCH** | — | Wii merge-action path faithful; native shim is the #ifdef arm (Lane N) |

## The four remap branches inside Filter — decomp status
All four remaps the VERDICT/kickoff care about are STRUCTURALLY PRESENT and FAITHFUL in
the compiled Wii target and in the source:
- sCharSharedDir (:4319 region → ReplaceRefs, return kIgnore) — faithful (Filter idx230).
- sInstrumentDir / sInstResourceDir (:4325 → FindObject → SetLocalXfm → ReplaceRefs) — faithful (idx285/288).
- guard `!(sOutfitDir||sResourceDir||sToDir)` (:4353) — faithful (idx344-356; the idx356
  diff_op is the bool-materialization of exactly this negated-OR, NOT a changed condition).
- **sBoneMergeDir (:4356 → RndTransformable dynamic_cast → FindObject → ReplaceRefs)** —
  faithful (idx357-369). The VERDICT §1 calls this remap "never-firing"; **that is a
  RUNTIME/DATA claim about whether `o1->Dir()==sBoneMergeDir` is ever true at load — it is
  NOT a decomp bug.** The branch exists and is correct; whether it is ENTERED depends on
  whether any merged object's Dir() equals sBoneMergeDir at runtime — a Lane N hit-count
  question (the gFilterBranchHits[2]/[3] probes already instrument exactly this). Lane D
  confirms: if it never fires, it is not because the code is wrong.

NB: VERDICT §1's source anchor "BandCharacter.cpp:4159-4181" for the sBoneMergeDir remap
is STALE (confirmed by review A3). The real branch is inside Filter at :4353-4370
(objdiff idx356-369). Do not propagate the stale anchor.

## HX_NATIVE ifdef enumeration (load path) — see evidence/hx_native_ifdef_enumeration.md
Two behavioral HX_NATIVE blocks, both NON-defects:
1. **ReplaceRefs reimpl (:4217-4267)** — FAITHFUL-REIMPL. Reverse-walk index-based rewrite;
   same match set {sOutfitDir,sResourceDir,sToDir}, same `theirs!=mine` guard, replaces
   every qualifying ref, restart-after-replace. Documented reason: clang/LP64 iterator
   invalidation that MWCC/PPC tolerated. Semantics preserved. Wii #else path is the 98.79%
   COSMETIC match above.
2. **FilterSubdir white-texture shim (:4386-4459)** — DELIBERATE divergence (kMerge→kReplace
   on external shared-resource subdirs). Already proven by the 2026-06-06 record to NOT
   change skeleton binding topology (shim-off = same shared root). Wii #else is 100%-matched.
   This is Lane N's A/B reconciliation subject, out of Lane D's read-only scope.
All other HX_NATIVE blocks on the path are default-OFF Wave-20 Lane N probes
(LoadBindProbeOn-gated hit counters) — byte-invisible to the Wii target and to the
native default run.

## Coordinator hand-off
- The decomp of the binding chain is NOT the bug. Rule out "plain decomp bug in the
  merge/share chain" (kickoff suspicion (c)) — it is FALSE for every function that decides
  binding. The divergence lives in mechanism (a)/(b): the FilterSubdir shim and/or
  poll-interleaved name-resolution share, both DATA/ordering behaviors that Lane N/W measure.
- ObjectDir::PreLoad being 100%-matched means the kInlineCached/kInlineCachedShared subdir
  read is decomp-faithful — the per-member fresh skeleton loads correctly (matching the
  doc's "loader is CORRECT" finding); any share is downstream name-resolution, not a
  broken inline-cache read.
- No fix chartered by Lane D (audit-only). No source edits made.

## Evidence
- evidence/objdiff_gate_summary.md — batch_objdiff gate (6 confirmed).
- evidence/per_function_diagnose.md — exact mismatching instructions per function.
- evidence/hx_native_ifdef_enumeration.md — every HX_NATIVE block classified.
- evidence/Filter_full_listing.md — full 484-instr objdiff listing for Filter.
- Checkpoint: /tmp/wave20-checkpoints/D.json.
